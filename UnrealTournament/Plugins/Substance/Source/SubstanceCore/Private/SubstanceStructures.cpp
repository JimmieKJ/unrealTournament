//! @file SubstanceStructures.cpp
//! @author Antoine Gonzalez - Allegorithmic
//! @date 20110105
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceCorePrivatePCH.h"
#include "SubstanceCoreHelpers.h"
#include "SubstanceFOutput.h"
#include "SubstanceFGraph.h"
#include "SubstanceGraphInstance.h"
#include "SubstanceInput.h"
#include "SubstanceCallbacks.h"
#include "SubstanceTexture2D.h"
#include "framework/details/detailsrendertoken.h"
#include "substance_public.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

#define LOCTEXT_NAMESPACE "SubstanceCoreStructures"

namespace Substance
{

FOutputDesc::FOutputDesc():
	Uid(0),
	Format(0),
	Channel(0)
{

}


FOutputDesc::FOutputDesc(const FOutputDesc & O):
	Identifier(O.Identifier),
	Label(O.Label),
	Uid(O.Uid),
	Format(O.Format),
	Channel(O.Channel),
	AlteringInputUids(O.AlteringInputUids)
{

}


FOutputInstance FOutputDesc::Instantiate() const
{
	output_inst_t Instance;
	Instance.Uid = Uid;
	Instance.Format = Format;
	
	return Instance;
}


FOutputInstance::FOutputInstance():
	Uid(0),
	Format(0),
	OutputGuid(FGuid::NewGuid()),
	bIsEnabled(false),
	bIsDirty(true),
	Texture(std::shared_ptr<USubstanceTexture2D*>(new USubstanceTexture2D*))
{
	*(Texture.get()) = NULL;
}


FOutputInstance::FOutputInstance(const FOutputInstance& Other)
{
	Uid = Other.Uid;
	Format = Other.Format;
	OutputGuid = Other.OutputGuid;
	bIsEnabled = Other.bIsEnabled;
	ParentInstance = Other.ParentInstance;
	RenderTokens.clear();

	std::shared_ptr<USubstanceTexture2D*> prevTexture = Texture;

	// the new output instance now owns the texture
	Texture = Other.Texture;

	if (prevTexture.get() && prevTexture.unique())
	{
		Helpers::Clear(prevTexture);
	}
}


FOutputInstance::~FOutputInstance()
{
	RenderCallbacks::clearComputedOutputs(this);
	RenderTokens.clear();
}


graph_inst_t* FOutputInstance::GetParentGraphInstance() const
{
	USubstanceGraphInstance* Outer = GetOuter();

	if (Outer && Outer->Instance)
	{
		return Outer->Instance;
	}

	return NULL;
}


graph_desc_t* FOutputInstance::GetParentGraph() const
{
	USubstanceGraphInstance* Outer = GetOuter();

	if (Outer && Outer->Instance && Outer->Instance->Desc)
	{
		return Outer->Instance->Desc;
	}
	
	return NULL;
}


output_desc_t* FOutputInstance::GetOutputDesc() const
{
	USubstanceGraphInstance* Outer = GetOuter();

	if (Outer && Outer->Instance && Outer->Instance->Desc)
	{
		Substance::List<output_desc_t>::TIterator
			ItOut(Outer->Instance->Desc->OutputDescs.itfront());

		for (;ItOut;++ItOut)
		{
			if ((*ItOut).Uid == Uid)
			{
				return &(*ItOut);
			}
		}
	}

	return NULL;
}


void FOutputInstance::push(const Token& rtokenptr)
{
	RenderTokens.push_back(rtokenptr);
}


bool FOutputInstance::queueRender()
{
	check(*Texture);
	bool res = bIsDirty;
	bIsDirty = false;
	(*Texture)->OutputCopy->bIsDirty = true;
	return res;
}


//! @brief Internal use only
void FOutputInstance::releaseTokensOwnedByEngine(uint32 engineUid)
{
	// Remove deprecated tokens: token w/ render results created from engine
	// w/ UID as argument.
	// Not thread safe, can't be called if render ongoing.

	for (auto ite = RenderTokens.itfront(); ite;)
	{
		if ((*ite)->releaseOwnedByEngine(engineUid))
		{
			RenderTokens.removeItem(*ite);
			ite.Reset();
		}
		else
		{
			++ite;
		}
	}
}


TSharedPtr<FInputDescBase> FindInput(
	Substance::List< TSharedPtr<FInputDescBase> >& Inputs, 
	const uint32 uid)
{
	for (int32 i = 0 ; i < Inputs.Num() ; ++i)
	{
		if (uid == Inputs[i]->Uid)
		{
			return Inputs[i];
		}
	}

	return TSharedPtr<FInputDescBase>();
}


TSharedPtr<FInputDescBase> FindInput(
	Substance::List< TSharedPtr<FInputDescBase> >& Inputs, 
	const FString identifier)
{
	for (int32 i = 0 ; i < Inputs.Num() ; ++i)
	{
		if (identifier == Inputs[i]->Identifier)
		{
			return Inputs[i];
		}
	}

	return TSharedPtr<FInputDescBase>();
}


TSharedPtr<FInputDescBase> FindInput(
	const Substance::List< TSharedPtr<FInputDescBase> >& Inputs,
	const uint32 uid)
{
	for (int32 i = 0; i < Inputs.Num(); ++i)
	{
		if (uid == Inputs[i]->Uid)
		{
			return Inputs[i];
		}
	}

	return TSharedPtr<FInputDescBase>();
}


TSharedPtr<FInputInstanceBase> FindInput(
	Substance::List< TSharedPtr<FInputInstanceBase> >& Inputs, 
	const uint32 uid)
{
	for (int32 i = 0 ; i < Inputs.Num() ; ++i)
	{
		if (uid == Inputs[i]->Uid)
		{
			return Inputs[i];
		}
	}

	return TSharedPtr<FInputInstanceBase>();
}


USubstanceGraphInstance* FOutputInstance::GetOuter() const
{
	return ParentInstance;
}


FOutputInstance::Result FOutputInstance::grabResult()
{
	Result res;

	// Remove all canceled render results
	while (RenderTokens.size() != 0 &&
		(*RenderTokens.itfront()).get()->canRemove())
	{
		RenderTokens.pop_front();
	}

	if (RenderTokens.size() == 0)
	{
		return res;
	}

	// Get last valid and delete the others
	res.reset( RenderTokens.back()->grabResult() );

	// Remove all canceled render results
	while (RenderTokens.size() > 1)
	{
		RenderTokens.pop_front();
	}

	return res;
}


template< typename T > TSharedPtr<input_inst_t> instantiateNumericalInput(input_desc_t* Input)
{
	TSharedPtr< input_inst_t > Instance = TSharedPtr< input_inst_t >(new FNumericalInputInstance<T>(Input));
	FNumericalInputInstance< T >* I = (FNumericalInputInstance< T >*)Instance.Get();
	I->Value = ((FNumericalInputDesc< T >*)Input)->DefaultValue;

	return Instance;
}


TSharedPtr< input_inst_t > FInputDescBase::Instantiate()
{
	TSharedPtr< input_inst_t > Instance;

	switch((SubstanceInputType)Type)
	{
		case Substance_IType_Float:
		{
			Instance = instantiateNumericalInput<float>(this);
		}
		break;
	case Substance_IType_Float2:
		{
			Instance = instantiateNumericalInput<vec2float_t>(this);
		}
		break;
	case Substance_IType_Float3:
		{
			Instance = instantiateNumericalInput<vec3float_t>(this);
		}
		break;
	case Substance_IType_Float4:
		{
			Instance = instantiateNumericalInput<vec4float_t>(this);
		}
		break;
	case Substance_IType_Integer:
		{
			Instance = instantiateNumericalInput<int32>(this);
		}
		break;
	case Substance_IType_Integer2:
		{
			Instance = instantiateNumericalInput<vec2int_t>(this);
		}
		break;
	case Substance_IType_Integer3:
		{
			Instance = instantiateNumericalInput<vec3int_t>(this);
		}
		break;
	case Substance_IType_Integer4:
		{
			Instance = instantiateNumericalInput<vec4int_t>(this);
		}
		break;

	case Substance_IType_Image:
		{
			Instance = TSharedPtr<input_inst_t>( new FImageInputInstance(this) );
		}
		break;
	}

	Instance->Desc = this;

	return Instance;
}


template< typename T > TSharedPtr<input_inst_t> cloneInstance(FInputInstanceBase* Input)
{
	TSharedPtr<input_inst_t> Instance = TSharedPtr<input_inst_t>( new FNumericalInputInstance<T>);
	FNumericalInputInstance<T>* I = (FNumericalInputInstance<T>*)Instance.Get();
	I->Value = ((FNumericalInputInstance<T>*)Input)->Value;

	return Instance;
}


TSharedPtr<input_inst_t> FInputInstanceBase::Clone()
{
	TSharedPtr<input_inst_t> Instance;

	switch((SubstanceInputType)Type)
	{
	case Substance_IType_Float:
		{
			Instance = cloneInstance<float>(this);
		}
		break;
	case Substance_IType_Float2:
		{
			Instance = cloneInstance<vec2float_t>(this);
		}
		break;
	case Substance_IType_Float3:
		{
			Instance = cloneInstance<vec3float_t>(this);
		}
		break;
	case Substance_IType_Float4:
		{
			Instance = cloneInstance<vec4float_t>(this);
		}
		break;
	case Substance_IType_Integer:
		{
			Instance = cloneInstance<int32>(this);
		}
		break;
	case Substance_IType_Integer2:
		{
			Instance = cloneInstance<vec2int_t>(this);
		}
		break;
	case Substance_IType_Integer3:
		{
			Instance = cloneInstance<vec3int_t>(this);
		}
		break;
	case Substance_IType_Integer4:
		{
			Instance = cloneInstance<vec4int_t>(this);
		}
		break;

	case Substance_IType_Image:
		{
			Instance = TSharedPtr<input_inst_t>( new FImageInputInstance );
		}
		break;
	}

	Instance->Uid = Uid;
	Instance->Type = Type;
	Instance->IsHeavyDuty = IsHeavyDuty;
	Instance->Parent = Parent;

	return Instance;
}


bool FInputDescBase::IsNumerical() const
{
	return Type != Substance_IType_Image;
}


FNumericalInputDescComboBox::FNumericalInputDescComboBox(FNumericalInputDesc<int32>* Desc)
{
	Identifier = Desc->Identifier;
	Label = Desc->Label;
	Widget = Desc->Widget;
	Uid = Desc->Uid;
	IsHeavyDuty = Desc->IsHeavyDuty;
	Type = Desc->Type;
	AlteredOutputUids = Desc->AlteredOutputUids;
	IsClamped = Desc->IsClamped;
	DefaultValue = Desc->DefaultValue;
	Min = Desc->Min;
	Max = Desc->Max;
	Group = Desc->Group;
}


bool FInputInstanceBase::IsNumerical() const
{
	return Type != Substance_IType_Image;
}


FInputInstanceBase::FInputInstanceBase(input_desc_t* InputDesc):
	UseCache(true),
	Desc(InputDesc),
	Parent(NULL)
{
	if (InputDesc)
	{
		Type = InputDesc->Type;
		Uid = InputDesc->Uid;
		IsHeavyDuty = InputDesc->IsHeavyDuty;
	}
}


bool FNumericalInputInstanceBase::isModified(const void* numeric) const
{
	return FMemory::Memcmp(getRawData(), numeric, getRawSize()) ? true : false;
}


template < typename T > void FNumericalInputInstance<T>::Reset()
{
	FMemory::Memzero(Value);
}


FImageInputInstance::FImageInputInstance(FInputDescBase* Input):
	FInputInstanceBase(Input),
		PtrModified(false),
		ImageSource(NULL)
{

}


FImageInputInstance::FImageInputInstance(const FImageInputInstance& Other)
{
	PtrModified = Other.PtrModified;
	Uid = Other.Uid;
	IsHeavyDuty = Other.IsHeavyDuty;
	Type = Other.Type;
	ImageSource = Other.ImageSource;
	ImageInput = Other.ImageInput;
	Parent = Other.Parent;
}


FImageInputInstance& FImageInputInstance::operator =(const FImageInputInstance& Other)
{
	PtrModified = Other.PtrModified;
	Uid = Other.Uid;
	IsHeavyDuty = Other.IsHeavyDuty;
	Type = Other.Type;
	ImageSource = Other.ImageSource;
	ImageInput = Other.ImageInput;
	Parent = Other.Parent;

	return *this;
}


FImageInputInstance::~FImageInputInstance()
{
	Substance::Helpers::RemoveFromDelayedImageInputs(this);
}


//! @brief Internal use only
bool FImageInputInstance::isModified(const void* numeric) const
{
	bool res = ImageInput!=NULL &&
		ImageInput->resolveDirty();
	res = PtrModified || res;
	PtrModified = false;
	return res;
}


void FImageInputInstance::SetImageInput(
	UObject* InValue,
	FGraphInstance* ParentInstance,
	bool unregisterOutput,
	bool isTransacting)
{
	std::shared_ptr<Substance::ImageInput> NewInput =
		Helpers::PrepareImageInput(InValue, this, ParentInstance);

	if (NewInput != ImageInput || (NULL == NewInput.get() && NULL == InValue))
	{
#if WITH_EDITOR
		if (GIsEditor && isTransacting)
		{
			FScopedTransaction Transaction(	NSLOCTEXT("Editor", "ModifiedInput", "Substance"));
			Parent->ParentInstance->Modify(false);
		}
#endif
		
		ImageInput = NewInput;
		PtrModified = true;
		ImageSource = InValue;
		Parent->bHasPendingImageInputRendering = true;
	}
}


SIZE_T getComponentsCount(SubstanceInputType type)
{
	switch (type)
	{
	case Substance_IType_Float   :
	case Substance_IType_Integer : return 1;
	case Substance_IType_Float2  :
	case Substance_IType_Integer2: return 2;
	case Substance_IType_Float3  :
	case Substance_IType_Integer3: return 3;
	case Substance_IType_Float4  :
	case Substance_IType_Integer4: return 4;
	default                      : return 0;
	}
}

} // namespace Substance

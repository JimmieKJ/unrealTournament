//! @file SubstanceGraphInstance.cpp
//! @brief Implementation of the USubstanceGraphInstance class
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceCorePrivatePCH.h"
#include "SubstanceCoreTypedefs.h"
#include "SubstanceFOutput.h"
#include "SubstanceInput.h"
#include "SubstanceFGraph.h"
#include "SubstanceFPackage.h"
#include "SubstanceTexture2D.h"
#include "SubstanceInstanceFactory.h"
#include "SubstanceCoreHelpers.h"
#include "SubstanceImageInput.h"

#if WITH_EDITOR
#include "ContentBrowserModule.h"
#endif

namespace sbsGraphInstance
{
	TArray< std::shared_ptr< USubstanceTexture2D* > > SavedTextures;
}


USubstanceGraphInstance::USubstanceGraphInstance(class FObjectInitializer const & PCIP) : Super(PCIP)
{
	bCooked = false;
	bFreezed = false;
}


void USubstanceGraphInstance::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FSubstanceCoreCustomVersion::GUID);

	if (Ar.IsLoading() && !Ar.IsTransacting())
	{
		check(NULL == Instance);
		Instance = new Substance::FGraphInstance();
	}

	if(Instance != NULL)
	{
		Ar << *Instance;

		// serialize eventual fields from previous version
		if (Ar.IsLoading())
		{
			const int32 SbsCoreVer = GetLinkerCustomVersion(FSubstanceCoreCustomVersion::GUID);

			if (SbsCoreVer < FSubstanceCoreCustomVersion::FixedGraphFreeze)
			{
				int32 dummyBool;
				Ar << dummyBool;
				Ar << dummyBool;
			}
		}

		if( Ar.IsObjectReferenceCollector() )
		{
			TArray<output_inst_t>::TIterator ItOut(Instance->Outputs.itfront());
			for (; ItOut ; ++ItOut)
			{
				Ar << *(ItOut->Texture.get());
			}
		}
	}

	bCooked = Ar.IsCooking() && Ar.IsSaving();
	
	Ar << bCooked;
	Ar << Parent;
}


void USubstanceGraphInstance::BeginDestroy()
{
	// Route BeginDestroy.
	Super::BeginDestroy();

	if (Instance)
	{
		Substance::Helpers::Cleanup(this);
	}
}


void USubstanceGraphInstance::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (!bDuplicateForPIE)
	{
		// after duplication, we need to recreate a parent instance and set it as outer
		// look for the original object, using the GUID
		USubstanceGraphInstance* RefInstance = NULL;

		for (TObjectIterator<USubstanceGraphInstance> It; It; ++It)
		{
			if ((*It)->Instance->InstanceGuid ==
				Instance->InstanceGuid && (*It)->Instance != this->Instance)
			{
				RefInstance = *It;
				break;
			}
		}

		check(RefInstance);

		Instance->InstanceGuid = FGuid::NewGuid();
		Instance->ParentInstance = this;
		Instance->Desc = RefInstance->Instance->Desc;
		Substance::Helpers::RenderAsync(Instance);

#if WITH_EDITOR
		if (GIsEditor)
		{
			TArray<UObject*> AssetList;
			for (auto itout = Instance->Outputs.itfront(); itout; ++itout)
			{
				AssetList.AddUnique(*(itout->Texture));
			}
			AssetList.AddUnique(this);

			FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().SyncBrowserToAssets(AssetList);
		}
#endif
	}
}


void USubstanceGraphInstance::PostLoad()
{
	// subscribe to the parent object
	if(!Parent || !Parent->SubstancePackage)
	{
		UE_LOG(LogSubstanceCore, Error, TEXT("Impossible to find parent package for \"%s\". You should delete the object."),
			*GetName());
		Super::PostLoad();
		return;
	}

	Parent->ConditionalPostLoad();

	// sanity check
	if(!Instance)
	{
		UE_LOG(LogSubstanceCore, Error, TEXT("Error, no actual instance associated to \"%s\". You should delete the object."),
			*GetName());
		Super::PostLoad();
		return;
	}

	// link the instance to its desc and parent
	Instance->ParentInstance = this;
	Instance->Desc = Substance::Helpers::FindParentGraph(
		Parent->SubstancePackage->Graphs,
		Instance);

	// if nothing was found, we lookup by url and bind the object to the factory
	// this can happen when the factory and the instance are not in the same
	// package and this factory's package was not saved after instancing
	if(NULL == Instance->Desc)
	{
		Instance->Desc = Substance::Helpers::FindParentGraph(
			Parent->SubstancePackage->Graphs,
			Instance->ParentUrl);
		Instance->Desc->InstanceUids.AddUnique(
			Instance->InstanceGuid); // do this manually in that case
	}

	// could be broken if instance and package are in different package not saved synchronously
	if (NULL == Instance->Desc)
	{
		UE_LOG(LogSubstanceCore, Error, TEXT("Error, Impossible to find parent desc for \"%s\", invalid data. You should delete the object."),
			*GetName());
		Super::PostLoad();
		return;
	}

	Instance->Desc->Subscribe(Instance);

	// link the outputs with the graph instance
	TArray<output_inst_t>::TIterator ItOut(Instance->Outputs.itfront());
	for (; ItOut ; ++ItOut)
	{
		ItOut->ParentInstance = this;
	}

	// and the input instances with the graph and their desc
	TArray<TSharedPtr<input_inst_t>>::TIterator ItInInst(Instance->Inputs.itfront());
	for (; ItInInst ; ++ItInInst)
	{
		TArray<input_desc_ptr>::TIterator InIntDesc(Instance->Desc->InputDescs.itfront());
		for (; InIntDesc ; ++InIntDesc)
		{
			if ((*InIntDesc)->Uid == (*ItInInst)->Uid)
			{
				ItInInst->Get()->Desc = InIntDesc->Get();
			}
		}

		if (ItInInst->Get()->Desc && ItInInst->Get()->Desc->IsImage())
		{
			img_input_inst_t* ImgInput = (img_input_inst_t*)(ItInInst->Get());
			if (ImgInput->ImageSource)
			{
				ImgInput->ImageSource->ConditionalPostLoad();

				// delay the set image input, the source is not necessarily ready
				Substance::Helpers::PushDelayedImageInput(ImgInput, Instance);
				Instance->bHasPendingImageInputRendering = true;
				ImageSources.AddUnique(ImgInput->ImageSource);
			}
		}

		(*ItInInst)->Parent = Instance;
	}

	// let the output's texture switch them back on
	if (Instance)
	{
		ItOut.Reset();

		for (; ItOut; ++ItOut)
		{
			if (*(ItOut->Texture.get()) == NULL)
			{
				ItOut->bIsEnabled = false;
			}
		}
	}

	if (Instance->bHasPendingImageInputRendering || 
		(Parent->GetGenerationMode() != ESubstanceGenerationMode::SGM_Baked && bCooked) ||
		(bFreezed && bCooked))
	{
		Substance::Helpers::PushDelayedRender(Instance);
	}

	Super::PostLoad();
}


void USubstanceGraphInstance::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	USubstanceGraphInstance* GI = Cast<USubstanceGraphInstance>(InThis);

	if (NULL == GI->Instance || !GI->IsValidLowLevel())
	{
		return;
	}

	TArray<TSharedPtr<input_inst_t>>::TIterator ItInput(GI->Instance->Inputs.itfront());
	for (; ItInput; ++ItInput)
	{
		if (!ItInput->Get()->IsNumerical())
		{
			Substance::FImageInputInstance* ImgInput = (Substance::FImageInputInstance*)ItInput->Get();
			USubstanceImageInput* SrcImageInput = Cast<USubstanceImageInput>(ImgInput->ImageSource);

			if (SrcImageInput && SrcImageInput->Consumers.Contains(GI))
			{
				Collector.AddReferencedObject(SrcImageInput);
			}
		}
	}
}


#if WITH_EDITOR
bool USubstanceGraphInstance::CanEditChange( const UProperty* InProperty ) const
{
	return true;
}


void USubstanceGraphInstance::PreEditUndo()
{
	// serialization of outputs does not include the shared ptr to textures,
	// save them before undo/redo

	Substance::List<output_inst_t>::TIterator itOut(Instance->Outputs.itfront());
	for (;itOut;++itOut)
	{
		sbsGraphInstance::SavedTextures.Add(itOut->Texture);
	}
}


void USubstanceGraphInstance::PostEditUndo()
{
	Substance::List<output_inst_t>::TIterator itOut(Instance->Outputs.itfront());
	TArray<std::shared_ptr<USubstanceTexture2D*>>::TIterator SavedTexturesIt(sbsGraphInstance::SavedTextures);

	for (;itOut && SavedTexturesIt;++itOut, ++SavedTexturesIt)
	{
		itOut->Texture = std::shared_ptr<USubstanceTexture2D*>(*SavedTexturesIt);
	}

	sbsGraphInstance::SavedTextures.Empty();

	Substance::Helpers::RenderAsync(Instance);
}
#endif // WITH_EDITOR


bool USubstanceGraphInstance::SetInputImg(const FString& Name, class UObject* Value)
{
	if (Instance)
	{
		if (Substance::Helpers::IsSupportedImageInput(Value))
		{
			UObject* PrevSource = GetInputImg(Name);

			Instance->UpdateInput(Name, Value);

			bool bUseOtherInput = false;
			if (PrevSource)
			{
				for (auto InputName : GetInputNames())
				{
					if (PrevSource == GetInputImg(InputName))
					{
						bUseOtherInput = true;
					}
				}
			}
			if (!bUseOtherInput)
			{
				ImageSources.Remove(PrevSource);
			}
			ImageSources.AddUnique(Value);

			Substance::Helpers::RenderAsync(Instance);
			return true;
		}
	}

	return false;
}


class UObject* USubstanceGraphInstance::GetInputImg(const FString& Name)
{
	TArray<TSharedPtr<input_inst_t>>::TIterator ItInInst(Instance->Inputs.itfront());
	for (; ItInInst ; ++ItInInst)
	{
		if (ItInInst->Get()->Desc->Identifier == Name &&
			ItInInst->Get()->Desc->Type == Substance_IType_Image)
		{
			Substance::FImageInputInstance* TypedInst =
				(Substance::FImageInputInstance*) &(*ItInInst->Get());

			return TypedInst->ImageSource;
		}
	}

	return NULL;
}


TArray<FString> USubstanceGraphInstance::GetInputNames()
{
	TArray<FString> Names;

	TArray<TSharedPtr<input_inst_t>>::TIterator ItInInst(Instance->Inputs.itfront());
	for (; ItInInst ; ++ItInInst)
	{
		if (ItInInst->Get()->Desc)
		{
			Names.Add(ItInInst->Get()->Desc->Identifier);
		}
	}

	return Names;
}


ESubstanceInputType USubstanceGraphInstance::GetInputType(FString InputName)
{
	TArray<TSharedPtr<input_inst_t>>::TIterator ItInInst(Instance->Inputs.itfront());
	for (; ItInInst ; ++ItInInst)
	{
		if (ItInInst->Get()->Desc->Identifier == InputName)
		{
			return (ESubstanceInputType)(ItInInst->Get()->Desc->Type);
		}
	}

	return ESubstanceInputType::SIT_MAX;
}


void USubstanceGraphInstance::SetInputInt(FString IntputName, const TArray<int32>& InputValues)
{
	if (Instance)
	{
		if (bFreezed)
		{
			UE_LOG(LogSubstanceCore, Error, TEXT("Cannot modify inputs for Graph Instance \"%s\", instance is freezed."),
				*GetName());
			return;
		}

		Instance->UpdateInput< int32 >(
			IntputName,
			InputValues);
	}
}


void USubstanceGraphInstance::SetInputFloat(FString IntputName, const TArray<float>& InputValues)
{
	if (Instance)
	{
		if (bFreezed)
		{
			UE_LOG(LogSubstanceCore, Error, TEXT("Cannot modify inputs for Graph Instance \"%s\", instance is freezed."),
				*GetName());
			return;
		}

		int32 UpdatedInputs = Instance->UpdateInput< float >(
			IntputName,
			InputValues);
	}
}


TArray< int32 > USubstanceGraphInstance::GetInputInt(FString IntputName)
{
	TArray< int32 > DummyValue;

	TArray< TSharedPtr< input_inst_t > >::TIterator ItInInst(Instance->Inputs.itfront());
	for (; ItInInst ; ++ItInInst)
	{
		if (ItInInst->Get()->Desc->Identifier == IntputName &&
			(ItInInst->Get()->Desc->Type == Substance_IType_Integer ||
			ItInInst->Get()->Desc->Type == Substance_IType_Integer2 ||
			ItInInst->Get()->Desc->Type == Substance_IType_Integer3 ||
			ItInInst->Get()->Desc->Type == Substance_IType_Integer4))
		{
			return Substance::Helpers::GetValueInt(*ItInInst);
		}
	}

	return DummyValue;
}


TArray< float > USubstanceGraphInstance::GetInputFloat(FString IntputName)
{
	TArray< float > DummyValue;

	TArray<TSharedPtr<input_inst_t>>::TIterator ItInInst(Instance->Inputs.itfront());
	for (; ItInInst ; ++ItInInst)
	{
		if (ItInInst->Get()->Desc->Identifier == IntputName &&
			(ItInInst->Get()->Desc->Type == Substance_IType_Float ||
			ItInInst->Get()->Desc->Type == Substance_IType_Float2 ||
			ItInInst->Get()->Desc->Type == Substance_IType_Float3 ||
			ItInInst->Get()->Desc->Type == Substance_IType_Float4))
		{
			return Substance::Helpers::GetValueFloat(*ItInInst);
		}
	}

	return DummyValue;
}

using namespace Substance;

FSubstanceFloatInputDesc USubstanceGraphInstance::GetFloatInputDesc(FString IntputName)
{
	FSubstanceFloatInputDesc K2_InputDesc;

	TArray<TSharedPtr<input_inst_t>>::TIterator ItInInst(Instance->Inputs.itfront());
	for (; ItInInst ; ++ItInInst)
	{
		if (ItInInst->Get()->Desc->Identifier == IntputName &&
			(ItInInst->Get()->Desc->Type == Substance_IType_Float ||
			ItInInst->Get()->Desc->Type == Substance_IType_Float2 ||
			ItInInst->Get()->Desc->Type == Substance_IType_Float3 ||
			ItInInst->Get()->Desc->Type == Substance_IType_Float4))
		{
			input_desc_t * InputDesc = ItInInst->Get()->Desc;

			K2_InputDesc.Name = IntputName;

			switch (ItInInst->Get()->Desc->Type)
			{
			case Substance_IType_Float:
				K2_InputDesc.Min.Add(((FNumericalInputDesc<float>*)InputDesc)->Min);
				K2_InputDesc.Max.Add(((FNumericalInputDesc<float>*)InputDesc)->Max);
				K2_InputDesc.Default.Add(((FNumericalInputDesc<float>*)InputDesc)->DefaultValue);
				break;
			case Substance_IType_Float2:
				K2_InputDesc.Min.Add(((FNumericalInputDesc<vec2float_t>*)InputDesc)->Min.X);
				K2_InputDesc.Min.Add(((FNumericalInputDesc<vec2float_t>*)InputDesc)->Min.Y);

				K2_InputDesc.Max.Add(((FNumericalInputDesc<vec2float_t>*)InputDesc)->Max.X);
				K2_InputDesc.Max.Add(((FNumericalInputDesc<vec2float_t>*)InputDesc)->Max.Y);

				K2_InputDesc.Default.Add(((FNumericalInputDesc<vec2float_t>*)InputDesc)->DefaultValue.X);
				K2_InputDesc.Default.Add(((FNumericalInputDesc<vec2float_t>*)InputDesc)->DefaultValue.Y);
				break;
			case Substance_IType_Float3:
				K2_InputDesc.Min.Add(((FNumericalInputDesc<vec3float_t>*)InputDesc)->Min.X);
				K2_InputDesc.Min.Add(((FNumericalInputDesc<vec3float_t>*)InputDesc)->Min.Y);
				K2_InputDesc.Min.Add(((FNumericalInputDesc<vec3float_t>*)InputDesc)->Min.Z);

				K2_InputDesc.Max.Add(((FNumericalInputDesc<vec3float_t>*)InputDesc)->Max.X);
				K2_InputDesc.Max.Add(((FNumericalInputDesc<vec3float_t>*)InputDesc)->Max.Y);
				K2_InputDesc.Max.Add(((FNumericalInputDesc<vec3float_t>*)InputDesc)->Max.Z);

				K2_InputDesc.Default.Add(((FNumericalInputDesc<vec3float_t>*)InputDesc)->DefaultValue.X);
				K2_InputDesc.Default.Add(((FNumericalInputDesc<vec3float_t>*)InputDesc)->DefaultValue.Y);
				K2_InputDesc.Default.Add(((FNumericalInputDesc<vec3float_t>*)InputDesc)->DefaultValue.Z);
				break;
			case Substance_IType_Float4:
				K2_InputDesc.Min.Add(((FNumericalInputDesc<vec4float_t>*)InputDesc)->Min.X);
				K2_InputDesc.Min.Add(((FNumericalInputDesc<vec4float_t>*)InputDesc)->Min.Y);
				K2_InputDesc.Min.Add(((FNumericalInputDesc<vec4float_t>*)InputDesc)->Min.Z);
				K2_InputDesc.Min.Add(((FNumericalInputDesc<vec4float_t>*)InputDesc)->Min.W);
				
				K2_InputDesc.Max.Add(((FNumericalInputDesc<vec4float_t>*)InputDesc)->Max.X);
				K2_InputDesc.Max.Add(((FNumericalInputDesc<vec4float_t>*)InputDesc)->Max.Y);
				K2_InputDesc.Max.Add(((FNumericalInputDesc<vec4float_t>*)InputDesc)->Max.Z);
				K2_InputDesc.Min.Add(((FNumericalInputDesc<vec4float_t>*)InputDesc)->Max.W);

				K2_InputDesc.Default.Add(((FNumericalInputDesc<vec4float_t>*)InputDesc)->DefaultValue.X);
				K2_InputDesc.Default.Add(((FNumericalInputDesc<vec4float_t>*)InputDesc)->DefaultValue.Y);
				K2_InputDesc.Default.Add(((FNumericalInputDesc<vec4float_t>*)InputDesc)->DefaultValue.Z);
				K2_InputDesc.Default.Add(((FNumericalInputDesc<vec4float_t>*)InputDesc)->DefaultValue.W);
				break;
			}
		}
	}

	return K2_InputDesc;
}


FSubstanceIntInputDesc USubstanceGraphInstance::GetIntInputDesc(FString IntputName)
{
	FSubstanceIntInputDesc K2_InputDesc;

	TArray<TSharedPtr<input_inst_t>>::TIterator ItInInst(Instance->Inputs.itfront());
	for (; ItInInst ; ++ItInInst)
	{
		if (ItInInst->Get()->Desc->Identifier == IntputName &&
			(ItInInst->Get()->Desc->Type == Substance_IType_Integer ||
			ItInInst->Get()->Desc->Type == Substance_IType_Integer2 ||
			ItInInst->Get()->Desc->Type == Substance_IType_Integer3 ||
			ItInInst->Get()->Desc->Type == Substance_IType_Integer4))
		{
			input_desc_t * InputDesc = ItInInst->Get()->Desc;

			K2_InputDesc.Name = IntputName;

			switch (ItInInst->Get()->Desc->Type)
			{
			case Substance_IType_Integer:
				K2_InputDesc.Min.Add(((FNumericalInputDesc<int32>*)InputDesc)->Min);
				K2_InputDesc.Max.Add(((FNumericalInputDesc<int32>*)InputDesc)->Max);
				K2_InputDesc.Default.Add(((FNumericalInputDesc<int32>*)InputDesc)->DefaultValue);
				break;
			case Substance_IType_Integer2:
				K2_InputDesc.Min.Add(((FNumericalInputDesc<vec2int_t>*)InputDesc)->Min.X);
				K2_InputDesc.Min.Add(((FNumericalInputDesc<vec2int_t>*)InputDesc)->Min.Y);

				K2_InputDesc.Max.Add(((FNumericalInputDesc<vec2int_t>*)InputDesc)->Max.X);
				K2_InputDesc.Max.Add(((FNumericalInputDesc<vec2int_t>*)InputDesc)->Max.Y);

				K2_InputDesc.Default.Add(((FNumericalInputDesc<vec2int_t>*)InputDesc)->DefaultValue.X);
				K2_InputDesc.Default.Add(((FNumericalInputDesc<vec2int_t>*)InputDesc)->DefaultValue.Y);
				break;
			case Substance_IType_Integer3:
				K2_InputDesc.Min.Add(((FNumericalInputDesc<vec3int_t>*)InputDesc)->Min.X);
				K2_InputDesc.Min.Add(((FNumericalInputDesc<vec3int_t>*)InputDesc)->Min.Y);
				K2_InputDesc.Min.Add(((FNumericalInputDesc<vec3int_t>*)InputDesc)->Min.Z);

				K2_InputDesc.Max.Add(((FNumericalInputDesc<vec3int_t>*)InputDesc)->Max.X);
				K2_InputDesc.Max.Add(((FNumericalInputDesc<vec3int_t>*)InputDesc)->Max.Y);
				K2_InputDesc.Max.Add(((FNumericalInputDesc<vec3int_t>*)InputDesc)->Max.Z);

				K2_InputDesc.Default.Add(((FNumericalInputDesc<vec3int_t>*)InputDesc)->DefaultValue.X);
				K2_InputDesc.Default.Add(((FNumericalInputDesc<vec3int_t>*)InputDesc)->DefaultValue.Y);
				K2_InputDesc.Default.Add(((FNumericalInputDesc<vec3int_t>*)InputDesc)->DefaultValue.Z);
				break;
			case Substance_IType_Integer4:
				K2_InputDesc.Min.Add(((FNumericalInputDesc<vec4int_t>*)InputDesc)->Min.X);
				K2_InputDesc.Min.Add(((FNumericalInputDesc<vec4int_t>*)InputDesc)->Min.Y);
				K2_InputDesc.Min.Add(((FNumericalInputDesc<vec4int_t>*)InputDesc)->Min.Z);
				K2_InputDesc.Min.Add(((FNumericalInputDesc<vec4int_t>*)InputDesc)->Min.W);

				K2_InputDesc.Max.Add(((FNumericalInputDesc<vec4int_t>*)InputDesc)->Max.X);
				K2_InputDesc.Max.Add(((FNumericalInputDesc<vec4int_t>*)InputDesc)->Max.Y);
				K2_InputDesc.Max.Add(((FNumericalInputDesc<vec4int_t>*)InputDesc)->Max.Z);
				K2_InputDesc.Min.Add(((FNumericalInputDesc<vec4int_t>*)InputDesc)->Max.W);

				K2_InputDesc.Default.Add(((FNumericalInputDesc<vec4int_t>*)InputDesc)->DefaultValue.X);
				K2_InputDesc.Default.Add(((FNumericalInputDesc<vec4int_t>*)InputDesc)->DefaultValue.Y);
				K2_InputDesc.Default.Add(((FNumericalInputDesc<vec4int_t>*)InputDesc)->DefaultValue.Z);
				K2_InputDesc.Default.Add(((FNumericalInputDesc<vec4int_t>*)InputDesc)->DefaultValue.W);
				break;
			}
		}
	}

	return K2_InputDesc;
}


FSubstanceInstanceDesc USubstanceGraphInstance::GetInstanceDesc()
{
	FSubstanceInstanceDesc K2_InstanceDesc;
	K2_InstanceDesc.Name = Instance->Desc->Label;
	return K2_InstanceDesc;
}

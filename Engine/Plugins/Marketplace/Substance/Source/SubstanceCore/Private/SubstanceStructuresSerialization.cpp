//! @file SubstanceStructuresSerialization.cpp
//! @author Antoine Gonzalez - Allegorithmic
//! @date 20110105
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceCorePrivatePCH.h"
#include "UObjectBaseUtility.h"
#include "SubstanceCoreCustomVersion.h"
#include "SubstanceFGraph.h"
#include "SubstanceGraphInstance.h"
#include "SubstanceInput.h"
#include "SubstanceFPackage.h"
#include "framework/details/detailslinkdata.h"
#include "substance_public.h"

using Substance::FNumericalInputInstance;
using Substance::FNumericalInputDesc;


FArchive& operator<<(FArchive& Ar, package_t*& P)
{
	Ar << P->SubstanceUids << P->Guid;
	Ar << P->SourceFilePath << P->SourceFileTimestamp;

	TArray<uint8> arArchive;
	if (Ar.IsLoading())
	{
		arArchive.BulkSerialize(Ar);
		P->LinkData.reset(new Substance::Details::LinkDataAssembly(&arArchive[0], arArchive.Num()));
	}
	else if(Ar.IsSaving())
	{
		Substance::Details::LinkDataAssembly *linkdata = 
			static_cast<Substance::Details::LinkDataAssembly*>(
				P->getLinkData().get());

		check(linkdata);

		arArchive.AddZeroed(linkdata->getAssembly().size());

		FMemory::Memcpy(
			&arArchive[0],
			(void*)&linkdata->getAssembly()[0],
			linkdata->getAssembly().size());

		arArchive.BulkSerialize(Ar);
	}

	int32 GraphCount = 0;

	if (Ar.IsSaving())
	{
		GraphCount =  P->Graphs.Num();
	}
	
	Ar << GraphCount;

	if (Ar.IsLoading())
	{
		P->Graphs.AddZeroed(GraphCount);
	}

	for (int32 Idx=0 ; Idx<GraphCount ; ++Idx)
	{
		Ar << P->Graphs[Idx];

		if (Ar.IsLoading())
		{
			P->Graphs[Idx]->Parent = P;

			Substance::List<output_desc_t>::TIterator itO(P->Graphs[Idx]->OutputDescs.itfront());
			for (; itO ; ++itO)
			{
				Substance::Details::LinkDataAssembly *linkdata = 
					(Substance::Details::LinkDataAssembly *)P->getLinkData().get();

				if (linkdata)
				{
					linkdata->setOutputFormat(
						(*itO).Uid,
						(*itO).Format);
				}
			}
		}
	}

	return Ar;
}


template <class T> void serialize(
	FArchive& Ar,
	TSharedPtr< Substance::FInputDescBase >& I)
{
	FNumericalInputDesc<T>* Input = 
		(FNumericalInputDesc<T>*)I.Get();

	// replace combobox by special struct
	if (I->Widget == Substance::Input_Combobox)
	{
		// when loading, the refcount_ptr<input_desc_t> has to be rebuilt
		// with the special type FNumericalInputDescComboBox
		if (Ar.IsLoading())
		{
			input_desc_ptr NewI(
				new Substance::FNumericalInputDescComboBox(
				(FNumericalInputDesc<int32>*)Input));		
			I = NewI;

			// and the Input ptr updated !
			Input = (FNumericalInputDesc<T>*)I.Get();
		}

		Substance::FNumericalInputDescComboBox* InputComboBox = 
			(Substance::FNumericalInputDescComboBox*)I.Get();
		Ar<<InputComboBox->ValueText;
	}

	int32 Clamped = Input->IsClamped ? 1 : 0;
	Ar<<Clamped;
	Input->IsClamped = Clamped == 1 ? true : false;

	Ar<<Input->DefaultValue;
	Ar<<Input->Min;
	Ar<<Input->Max;
	Ar<<Input->Group;
}


FArchive& operator<<(
	FArchive& Ar,
	TSharedPtr< Substance::FInputDescBase >& I)
{
	Ar<<I->Identifier<<I->Label;
	Ar<<I->Uid<<I->Type;
	Ar << I->Index;

	I->AlteredOutputUids.BulkSerialize( Ar );

	int32 UseHints = 0;
	int32 HeavyDuty = I->IsHeavyDuty;
	int32 Widget = I->Widget;

	Ar<<UseHints<<HeavyDuty<<Widget;
	I->IsHeavyDuty = HeavyDuty;
	I->Widget = (Substance::InputWidget)Widget;

	switch((SubstanceInputType)I->Type)
	{
	case Substance_IType_Float:
		{
			serialize<float>(Ar, I);
		}
		break;
	case Substance_IType_Float2:
		{
			serialize<vec2float_t>(Ar, I);
		}
		break;
	case Substance_IType_Float3:
		{
			serialize<vec3float_t>(Ar, I);
		}
		break;
	case Substance_IType_Float4:
		{
			serialize<vec4float_t>(Ar, I);
		}
		break;
	case Substance_IType_Integer:
		{
			serialize<int32>(Ar, I);
		}
		break;
	case Substance_IType_Integer2:
		{
			serialize<vec2int_t>(Ar, I);
		}
		break;
	case Substance_IType_Integer3:
		{
			serialize<vec3int_t>(Ar, I);
		}
		break;
	case Substance_IType_Integer4:
		{
			serialize<vec4int_t>(Ar, I);
		}
		break;

	case Substance_IType_Image:
		{
			Substance::FImageInputDesc* ImgInput =
				(Substance::FImageInputDesc*)I.Get();

			Ar << ImgInput->Desc << ImgInput->Label ;
		}
		break;
	}

	return Ar;
}


FArchive& operator<<(FArchive& Ar, Substance::FOutputDesc& O)
{
	Ar << O.Identifier << O.Label << O.Uid << O.Format << O.Channel;
	O.AlteringInputUids.BulkSerialize( Ar );

	return Ar;
}


FArchive& operator<<(FArchive& Ar, Substance::FGraphInstance& G)
{
	if (Ar.IsTransacting())
	{
		for (int32 Idx=0 ; Idx<G.Inputs.Num() ; ++Idx)
		{
			Ar << G.Inputs[Idx];
		}

		Ar << G.Outputs.getArray();

		return Ar;
	}
	
	if (Ar.IsSaving())
	{
		int32 Count = G.Inputs.Num();
		Ar << Count;

		for (int32 Idx=0 ; Idx<G.Inputs.Num() ; ++Idx)
		{
			// start by the type to be able to allocate 
			// the good type when loading back in memory
			Ar << G.Inputs[Idx]->Type;
			Ar << G.Inputs[Idx];
		}

		Count = G.Outputs.Num();
		Ar << Count;
		Ar << G.Outputs.getArray();
	}
	else if (Ar.IsLoading())
	{
		int32 Count;
		Ar << Count;
		
		G.Inputs.AddZeroed(Count);

		for (int32 Idx=0 ; Idx<G.Inputs.Num() ; ++Idx)
		{
			int32 Type;
			Ar << Type;

			switch((SubstanceInputType)Type)
			{
			case Substance_IType_Float:
				G.Inputs[Idx] = TSharedPtr<input_inst_t>(
					new FNumericalInputInstance<float>());
				break;
			case Substance_IType_Float2:
				G.Inputs[Idx] = TSharedPtr<input_inst_t>(
					new FNumericalInputInstance<vec2float_t>());
				break;
			case Substance_IType_Float3:
				G.Inputs[Idx] = TSharedPtr<input_inst_t>(
					new FNumericalInputInstance<vec3float_t>());
				break;
			case Substance_IType_Float4:
				G.Inputs[Idx] = TSharedPtr<input_inst_t>(
					new FNumericalInputInstance<vec4float_t>());
				break;
			case Substance_IType_Integer:
				G.Inputs[Idx] = TSharedPtr<input_inst_t>(
					new FNumericalInputInstance<int32>());
				break;
			case Substance_IType_Integer2:
				G.Inputs[Idx] = TSharedPtr<input_inst_t>(
					new FNumericalInputInstance<vec2int_t>());
				break;
			case Substance_IType_Integer3:
				G.Inputs[Idx] = TSharedPtr<input_inst_t>(
					new FNumericalInputInstance<vec3int_t>());
				break;
			case Substance_IType_Integer4:
				G.Inputs[Idx] = TSharedPtr<input_inst_t>(
					new FNumericalInputInstance<vec4int_t>());
				break;
			case Substance_IType_Image:
				G.Inputs[Idx] = TSharedPtr<input_inst_t>(
					new Substance::FImageInputInstance());
				break;
			default:
				break;
			}

			// the type is known so it will
			// serialize the good amount of data
			Ar<<G.Inputs[Idx];
		}

		Ar << Count;
		G.Outputs.AddZeroed(Count);
		Ar << G.Outputs.getArray();
	}
	else
	{
		for (int32 Idx=0 ; Idx<G.Inputs.Num() ; ++Idx)
		{
			Ar<<G.Inputs[Idx];
		}
	}

	Ar << G.InstanceGuid;
	Ar << G.ParentUrl;

	if (!Ar.IsSaving() && !Ar.IsLoading())
	{
		Ar << G.ParentInstance; // to make sure it does not get garbage collected
	}

	return Ar;
}


FArchive& operator<<(FArchive& Ar, Substance::FOutputInstance& O)
{
	Ar << O.Uid << O.Format << O.OutputGuid;

	int32 IsEnabled = O.bIsEnabled;
	Ar << IsEnabled;
	O.bIsEnabled = IsEnabled;
	
	return Ar;
}


FArchive& operator<<(FArchive& Ar, Substance::FImageInputInstance* Instance)
{
	if (Ar.IsTransacting())
	{
		Ar << Instance->ImageSource;

		if (Ar.IsLoading())
		{
			Instance->SetImageInput(Instance->ImageSource, Instance->Parent, false, false);
		}

		return Ar;
	}

	if (!Ar.IsSaving() && !Ar.IsLoading())
	{
		Ar << Instance->ImageSource;
	}
	else
	{
		int32 bHasImage = 0;

		if (Ar.IsSaving() && Instance->ImageSource != NULL)
		{
			bHasImage = 1;
		}

		Ar << bHasImage;

		if (bHasImage)
		{
			Ar << Instance->ImageSource;

			if (Ar.IsLoading() && !Ar.IsTransacting())
			{
				USubstanceImageInput* BmpImageInput = NULL;
				
				if (BmpImageInput)
				{	// link the image input with the input
					Substance::Helpers::LinkImageInput(Instance, BmpImageInput);
				}
			}
		}
	}

	return Ar;
}


FArchive& operator<<(FArchive& Ar, TSharedPtr<Substance::FInputInstanceBase>& I)
{
	int32 HeavyDuty = I->IsHeavyDuty;
	Ar << I->Uid << I->Type << HeavyDuty;
	I->IsHeavyDuty = HeavyDuty;
	
	switch((SubstanceInputType)I->Type)
	{
	case Substance_IType_Float:
		{
			FNumericalInputInstance<float>* Instance = (FNumericalInputInstance<float>*)I.Get();
			Ar << Instance->Value;
		}
		break;
	case Substance_IType_Float2:
		{
			FNumericalInputInstance<vec2float_t>* Instance = (FNumericalInputInstance<vec2float_t>*)I.Get();
			Ar << Instance->Value;
		}
		break;
	case Substance_IType_Float3:
		{
			FNumericalInputInstance<vec3float_t>* Instance = (FNumericalInputInstance<vec3float_t>*)I.Get();
			Ar << Instance->Value;
		}
		break;
	case Substance_IType_Float4:
		{
			FNumericalInputInstance<vec4float_t>* Instance = (FNumericalInputInstance<vec4float_t>*)I.Get();
			Ar << Instance->Value;
		}
		break;
	case Substance_IType_Integer:
		{
			FNumericalInputInstance<int32>* Instance = (FNumericalInputInstance<int32>*)I.Get();
			Ar << Instance->Value;
		}
		break;
	case Substance_IType_Integer2:
		{
			FNumericalInputInstance<vec2int_t>* Instance = (FNumericalInputInstance<vec2int_t>*)I.Get();
			Ar << Instance->Value;
		}
		break;
	case Substance_IType_Integer3:
		{
			FNumericalInputInstance<vec3int_t>* Instance = (FNumericalInputInstance<vec3int_t>*)I.Get();
			Ar << Instance->Value;
		}
		break;
	case Substance_IType_Integer4:
		{
			FNumericalInputInstance<vec4int_t>* Instance = (FNumericalInputInstance<vec4int_t>*)I.Get();
			Ar << Instance->Value;
		}
		break;

	case Substance_IType_Image:
		{
			Substance::FImageInputInstance* Instance = (Substance::FImageInputInstance*)I.Get();
			Ar << Instance;
		}
		break;
	}

	return Ar;
}


FArchive& operator<<(FArchive& Ar, Substance::FGraphDesc*& G)
{
	if (Ar.IsLoading())
	{
		G = new Substance::FGraphDesc();
	}

	Ar << G->PackageUrl << G->Label << G->Description;
	Ar << G->OutputDescs.getArray() << G->InstanceUids.getArray();

	if (Ar.IsSaving())
	{
		int32 Count = G->InputDescs.Num();
		Ar << Count;

		for (int32 Idx=0 ; Idx<G->InputDescs.Num() ; ++Idx)
		{
			// start by the type to be able to allocate 
			// the good type when loading back in memory
			Ar << G->InputDescs[Idx]->Type;
			Ar << G->InputDescs[Idx];
		}
	}
	else if (Ar.IsLoading())
	{
		int32 InputCount;
		Ar << InputCount;
		
		G->InputDescs.AddZeroed(InputCount);

		for (int32 Idx=0 ; Idx<G->InputDescs.Num() ; ++Idx)
		{
			int32 Type;
			Ar << Type;

			switch((SubstanceInputType)Type)
			{
			case Substance_IType_Float:
				G->InputDescs[Idx] = TSharedPtr<input_desc_t>(new FNumericalInputDesc<float>());
				break;
			case Substance_IType_Float2:
				G->InputDescs[Idx] = TSharedPtr<input_desc_t>(new FNumericalInputDesc<vec2float_t>());
				break;
			case Substance_IType_Float3:
				G->InputDescs[Idx] = TSharedPtr<input_desc_t>(new FNumericalInputDesc<vec3float_t>());
				break;
			case Substance_IType_Float4:
				G->InputDescs[Idx] = TSharedPtr<input_desc_t>(new FNumericalInputDesc<vec4float_t>());
				break;
			case Substance_IType_Integer:
				G->InputDescs[Idx] = TSharedPtr<input_desc_t>(new FNumericalInputDesc<int32>());
				break;
			case Substance_IType_Integer2:
				G->InputDescs[Idx] = TSharedPtr<input_desc_t>(new FNumericalInputDesc<vec2int_t>());
				break;
			case Substance_IType_Integer3:
				G->InputDescs[Idx] = TSharedPtr<input_desc_t>(new FNumericalInputDesc<vec3int_t>());
				break;
			case Substance_IType_Integer4:
				G->InputDescs[Idx] = TSharedPtr<input_desc_t>(new FNumericalInputDesc<vec4int_t>());
				break;
			case Substance_IType_Image:
				G->InputDescs[Idx] = TSharedPtr<input_desc_t>(new Substance::FImageInputDesc());
				break;
			default:
				break;
			}

			// the type is known so it will
			// serialize the good amount of data
			Ar << G->InputDescs[Idx];

			G->InputDescs[Idx]->Index = Idx;
		}
	}

	return Ar;
}

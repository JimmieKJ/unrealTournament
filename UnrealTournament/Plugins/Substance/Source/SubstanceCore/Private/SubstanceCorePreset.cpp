//! @file preset.cpp
//! @brief Substance preset implementation
//! @author Christophe Soum - Allegorithmic
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceCorePrivatePCH.h"
#include "SubstanceCorePreset.h"
#include "SubstanceCoreClasses.h"
#include "SubstanceCoreXmlHelper.h"
#include "SubstanceInput.h"
#include "SubstanceFGraph.h"


namespace Substance
{

//! @brief Apply this preset to a graph instance
//! @param graph The target graph instance to apply this preset
//! @param mode Reset to default other inputs of merge w/ previous values
//! @return Return true whether at least one input value is applied
bool FPreset::Apply(graph_inst_t* Instance, FPreset::ApplyMode mode) const
{
	bool AtLeastOneSet = false;
	graph_desc_t* Desc = Instance->Desc;
	
	for(int32 Idx = 0 ; Idx < mInputValues.Num() ; ++Idx)
	{
		const FInputValue& inp_value = mInputValues[Idx];

		// lookup by Input UID
		TSharedPtr<input_inst_t> inp_found =
			FindInput(Instance->Inputs, inp_value.mUid);

		// or by input identifier if no match was found
		if (!inp_found.Get())
		{
			inp_found =
				FindInput(Instance->Inputs, inp_value.mUid);
		}

		if (inp_found.Get() &&
		    inp_found->Type == inp_value.mType)
		{
			// Process only numerical (additional check)
			if (inp_found->IsNumerical())
			{
				TArray<FString> StrValueArray;
				inp_value.mValue.ParseIntoArray(StrValueArray,TEXT(","), true);

				TArray<float> ValueArray;

				for (TArray<FString>::TIterator ItV(StrValueArray); ItV; ++ItV)
				{
					ValueArray.Add(FCString::Atof(*(*	ItV)));
				}

				Instance->UpdateInput(inp_found->Uid, ValueArray);
				AtLeastOneSet = true;
			}
			else
			{
				if (inp_value.mValue != FString(TEXT("NULL")))
				{
					UObject* Object = FindObject<UTexture2D>(NULL, *inp_value.mValue);
					if (!Object)
					{
						Object = FindObject<USubstanceImageInput>(NULL, *inp_value.mValue);
					}

					if (Object)
					{
						Object->ConditionalPostLoad();
						Instance->UpdateInput(inp_found->Uid, Object);
					}
					/*else
					{
						debugf(
							TEXT("Unable to restore graph instance image input, object not found: %s"),
							*inp_value.mValue);
					}*/
				}
				else
				{
					Instance->UpdateInput(inp_found->Uid, NULL);
				}
			}
		}
	}

	if (AtLeastOneSet)
	{
		Instance->ParentInstance->MarkPackageDirty();
	}
	
	return AtLeastOneSet;
}


void FPreset::ReadFrom(graph_inst_t* Graph)
{
	mPackageUrl = Graph->Desc->PackageUrl;
	mLabel = Graph->ParentInstance->GetName();
	mDescription = Graph->Desc->Description;

	for (Substance::List<TSharedPtr<input_inst_t>>::TIterator 
		ItInp(Graph->Inputs.itfront()) ; ItInp ; ++ItInp)
	{
		uint32 Idx = mInputValues.AddZeroed(1);
		FPreset::FInputValue& inpvalue = mInputValues[Idx];

		inpvalue.mUid = (*ItInp)->Uid;
		inpvalue.mIdentifier = 
			Graph->Desc->GetInputDesc((*ItInp)->Uid)->Identifier;
		inpvalue.mValue = Helpers::GetValueString(*ItInp);
		inpvalue.mType = (SubstanceInputType)(*ItInp)->Type;
	}
}


bool ParsePresets(
	presets_t& Presets,
	const FString& XmlPreset)
{
	const int32 initialsize = Presets.Num();

	Helpers::ParseSubstanceXmlPreset(Presets,XmlPreset,NULL);
		
	return Presets.Num()>initialsize;
}


void WritePreset(preset_t& Preset, FString& XmlPreset)
{
	Helpers::WriteSubstanceXmlPreset(Preset, XmlPreset);
}

} // namespace Substance

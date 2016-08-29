// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

#if WITH_EDITOR
#include "StructureEditorUtils.h"
#endif //WITH_EDITOR
#include "Engine/UserDefinedStruct.h"
#include "TextReferenceCollector.h"

#if WITH_EDITORONLY_DATA
namespace
{
	void GatherUserDefinedStructForLocalization(const UObject* const Object, FPropertyLocalizationDataGatherer& PropertyLocalizationDataGatherer, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
	{
		const UUserDefinedStruct* const UserDefinedStruct = CastChecked<UUserDefinedStruct>(Object);

		PropertyLocalizationDataGatherer.GatherLocalizationDataFromObject(UserDefinedStruct, GatherTextFlags);

		const FString PathToObject = UserDefinedStruct->GetPathName();

		FStructOnScope StructData(UserDefinedStruct);
		FStructureEditorUtils::Fill_MakeStructureDefaultValue(UserDefinedStruct, StructData.GetStructMemory());

		// Iterate over all fields of the object's class.
		for (TFieldIterator<const UProperty> PropIt(StructData.GetStruct(), EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::ExcludeDeprecated, EFieldIteratorFlags::IncludeInterfaces); PropIt; ++PropIt)
		{
			PropertyLocalizationDataGatherer.GatherLocalizationDataFromChildTextProperties(PathToObject, *PropIt, PropIt->ContainerPtrToValuePtr<void>(StructData.GetStructMemory()), GatherTextFlags);
		}
	}

	void CollectUserDefinedStructTextReferences(UObject* Object, FArchive& Ar)
	{
		UUserDefinedStruct* const UserDefinedStruct = CastChecked<UUserDefinedStruct>(Object);

		// User Defined Structs need some special handling as they store their default data in a way that serialize doesn't pick up
		UUserDefinedStructEditorData* UDSEditorData = Cast<UUserDefinedStructEditorData>(UserDefinedStruct->EditorData);
		if (UDSEditorData)
		{
			for (const FStructVariableDescription& StructVariableDesc : UDSEditorData->VariablesDescriptions)
			{
				static const FString TextCategory = TEXT("text"); // Must match UEdGraphSchema_K2::PC_Text
				if (StructVariableDesc.Category == TextCategory)
				{
					FText StructVariableValue;
					if (FTextStringHelper::ReadFromString(*StructVariableDesc.DefaultValue, StructVariableValue))
					{
						Ar << StructVariableValue;
					}
				}
			}
		}

		UserDefinedStruct->Serialize(Ar);
	}
}
#endif

UUserDefinedStruct::UUserDefinedStruct(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	{ static const FAutoRegisterLocalizationDataGatheringCallback AutomaticRegistrationOfLocalizationGatherer(UUserDefinedStruct::StaticClass(), &GatherUserDefinedStructForLocalization); }
	{ static const FAutoRegisterTextReferenceCollectorCallback AutomaticRegistrationOfTextReferenceCollector(UUserDefinedStruct::StaticClass(), &CollectUserDefinedStructTextReferences); }
#endif
}

#if WITH_EDITOR

void UUserDefinedStruct::Serialize(FArchive& Ar)
{
	Super::Serialize( Ar );

	if (Ar.IsLoading() && (EUserDefinedStructureStatus::UDSS_UpToDate == Status))
	{
		// We need to force the editor data to be preload in case anyone needs to extract variable
		// information at editor time about the user structure.
		if ( EditorData != nullptr )
		{
			Ar.Preload(EditorData);
			if (!(Ar.GetPortFlags() & PPF_Duplicate))
			{
				FStructureEditorUtils::RecreateDefaultInstanceInEditorData(this);
			}
		}

		const FStructureEditorUtils::EStructureError Result = FStructureEditorUtils::IsStructureValid(this, NULL, &ErrorMessage);
		if (FStructureEditorUtils::EStructureError::Ok != Result)
		{
			Status = EUserDefinedStructureStatus::UDSS_Error;
			UE_LOG(LogClass, Log, TEXT("UUserDefinedStruct.Serialize '%s' validation: %s"), *GetName(), *ErrorMessage);
		}
	}
}

void UUserDefinedStruct::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	if (!bDuplicateForPIE)
	{
		Guid = FGuid::NewGuid();
	}
	if (!bDuplicateForPIE && (GetOuter() != GetTransientPackage()))
	{
		SetMetaData(TEXT("BlueprintType"), TEXT("true"));
		FStructureEditorUtils::OnStructureChanged(this);
	}
}

void UUserDefinedStruct::PostLoad()
{
	Super::PostLoad();

	ValidateGuid();
}

void UUserDefinedStruct::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);

	OutTags.Add(FAssetRegistryTag(TEXT("Tooltip"), FStructureEditorUtils::GetTooltip(this), FAssetRegistryTag::TT_Hidden));
}

UProperty* UUserDefinedStruct::CustomFindProperty(const FName Name) const
{
	const FGuid PropertyGuid = FStructureEditorUtils::GetGuidFromPropertyName(Name);
	UProperty* Property = PropertyGuid.IsValid() ? FStructureEditorUtils::GetPropertyByGuid(this, PropertyGuid) : NULL;
	ensure(!Property || PropertyGuid == FStructureEditorUtils::GetGuidForProperty(Property));
	return Property;
}

void UUserDefinedStruct::InitializeDefaultValue(uint8* StructData) const
{
	FStructureEditorUtils::Fill_MakeStructureDefaultValue(this, StructData);
}

void UUserDefinedStruct::ValidateGuid()
{
	// Backward compatibility:
	// The guid is created in an deterministic way using existing name.
	if (!Guid.IsValid() && (GetFName() != NAME_None))
	{
		const FString HashString = GetFName().ToString();
		ensure(HashString.Len());

		const uint32 BufferLength = HashString.Len() * sizeof(HashString[0]);
		uint32 HashBuffer[5];
		FSHA1::HashBuffer(*HashString, BufferLength, reinterpret_cast<uint8*>(HashBuffer));
		Guid = FGuid(HashBuffer[1], HashBuffer[2], HashBuffer[3], HashBuffer[4]);
	}
}

#endif	// WITH_EDITOR

FString UUserDefinedStruct::PropertyNameToDisplayName(FName Name) const
{
#if WITH_EDITOR
	FGuid PropertyGuid = FStructureEditorUtils::GetGuidFromPropertyName(Name);
	return FStructureEditorUtils::GetVariableDisplayName(this, PropertyGuid);
#endif	// WITH_EDITOR

	const int32 GuidStrLen = 32;
	const int32 MinimalPostfixlen = GuidStrLen + 3;
	const FString OriginalName = Name.ToString();
	if (OriginalName.Len() > MinimalPostfixlen)
	{
		FString DisplayName = OriginalName.LeftChop(GuidStrLen + 1);
		int FirstCharToRemove = -1;
		const bool bCharFound = DisplayName.FindLastChar(TCHAR('_'), FirstCharToRemove);
		if(bCharFound && (FirstCharToRemove > 0))
		{
			return DisplayName.Mid(0, FirstCharToRemove);
		}
	}
	return OriginalName;
}

void UUserDefinedStruct::SerializeTaggedProperties(FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, uint8* Defaults, const UObject* BreakRecursionIfFullyLoad) const
{
#if WITH_EDITOR
	/*	The following code is responsible for UUserDefinedStruct's default values serialization.	*/

	auto UDDefaultsStruct = Cast<UUserDefinedStruct>(DefaultsStruct);

	const bool bDuplicate = (0 != (Ar.GetPortFlags() & PPF_Duplicate));

	/*	When saving delta, we want the difference between current data and true structure's default values. 
		When Defaults is NULL then zeroed data will be used for comparison.*/
	const bool bUseNewDefaults = !Defaults
		&& UDDefaultsStruct
		&& Ar.DoDelta()
		&& Ar.IsSaving()
		&& !bDuplicate
		&& !Ar.IsCooking();

	/*	Object serialized from delta will have missing properties filled with zeroed data, 
		we want structure's default data instead */
	const bool bLoadDefaultFirst = UDDefaultsStruct
		&& !bDuplicate
		&& Ar.IsLoading();

	const bool bPrepareDefaultStruct = bUseNewDefaults || bLoadDefaultFirst;
	FStructOnScope StructDefaultMem(bPrepareDefaultStruct ? UDDefaultsStruct : NULL);
	if (bPrepareDefaultStruct)
	{
		FStructureEditorUtils::Fill_MakeStructureDefaultValue(UDDefaultsStruct, StructDefaultMem.GetStructMemory());
	}

	if (bUseNewDefaults)
	{
		Defaults = StructDefaultMem.GetStructMemory();
	}
	if (bLoadDefaultFirst)
	{
		if (Defaults == nullptr)
		{
			Defaults = StructDefaultMem.GetStructMemory();
		}
		UDDefaultsStruct->CopyScriptStruct(Data, Defaults);
	}
#endif // WITH_EDITOR
	Super::SerializeTaggedProperties(Ar, Data, DefaultsStruct, Defaults);
}

void UUserDefinedStruct::RecursivelyPreload()
{
	FLinkerLoad* Linker = GetLinker();
	if( Linker && (NULL == PropertyLink) )
	{
		TArray<UObject*> AllChildMembers;
		GetObjectsWithOuter(this, AllChildMembers);
		for (int32 Index = 0; Index < AllChildMembers.Num(); ++Index)
		{
			UObject* Member = AllChildMembers[Index];
			Linker->Preload(Member);
		}

		Linker->Preload(this);
		if (NULL == PropertyLink)
		{
			StaticLink(true);
		}
	}
}

FGuid UUserDefinedStruct::GetCustomGuid() const
{
	return Guid;
}
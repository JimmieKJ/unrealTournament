// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

#if WITH_EDITOR
#include "StructureEditorUtils.h"
#endif //WITH_EDITOR
#include "Engine/UserDefinedStruct.h"

UUserDefinedStruct::UUserDefinedStruct(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR

void UUserDefinedStruct::Serialize(FArchive& Ar)
{
	Super::Serialize( Ar );

	if (Ar.IsLoading() && (EUserDefinedStructureStatus::UDSS_UpToDate == Status))
	{
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
	if (!bDuplicateForPIE && (GetOuter() != GetTransientPackage()))
	{
		SetMetaData(TEXT("BlueprintType"), TEXT("true"));
		FStructureEditorUtils::OnStructureChanged(this);
	}
}

void UUserDefinedStruct::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);

	OutTags.Add(FAssetRegistryTag(TEXT("Tooltip"), FStructureEditorUtils::GetTooltip(this), FAssetRegistryTag::TT_Hidden));
}

UProperty* UUserDefinedStruct::CustomFindProperty(const FName Name) const
{
	const FGuid Guid = FStructureEditorUtils::GetGuidFromPropertyName(Name);
	UProperty* Property = Guid.IsValid() ? FStructureEditorUtils::GetPropertyByGuid(this, Guid) : NULL;
	ensure(!Property || Guid == FStructureEditorUtils::GetGuidForProperty(Property));
	return Property;
}

void UUserDefinedStruct::InitializeDefaultValue(uint8* StructData) const
{
	FStructureEditorUtils::Fill_MakeStructureDefaultValue(this, StructData);
}

#endif	// WITH_EDITOR

FString UUserDefinedStruct::PropertyNameToDisplayName(FName Name) const
{
#if WITH_EDITOR
	auto Guid = FStructureEditorUtils::GetGuidFromPropertyName(Name);
	return FStructureEditorUtils::GetVariableDisplayName(this, Guid);
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
		UDDefaultsStruct->CopyScriptStruct(Data, StructDefaultMem.GetStructMemory());
	}
#endif // WITH_EDITOR
	Super::SerializeTaggedProperties(Ar, Data, DefaultsStruct, Defaults);
}

void UUserDefinedStruct::RecursivelyPreload()
{
	ULinkerLoad* Linker = GetLinker();
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
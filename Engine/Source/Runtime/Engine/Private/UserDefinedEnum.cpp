// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Engine/UserDefinedEnum.h"
#if WITH_EDITOR
#include "Editor/UnrealEd/Public/Kismet2/EnumEditorUtils.h"
#endif	// WITH_EDITOR

UUserDefinedEnum::UUserDefinedEnum(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UUserDefinedEnum::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITOR
	if (Ar.IsLoading() && Ar.IsPersistent())
	{
		for (int32 i = 0; i < Names.Num(); ++i)
		{
			Names[i].Value = i;
		}
	}
#endif // WITH_EDITOR
}

FString UUserDefinedEnum::GenerateFullEnumName(const TCHAR* InEnumName) const
{
	check(CppForm == ECppForm::Namespaced);

	FString PathName;
	GetPathName(NULL, PathName);
	return UEnum::GenerateFullEnumName(this, InEnumName);
}

#if WITH_EDITOR
bool UUserDefinedEnum::Rename(const TCHAR* NewName, UObject* NewOuter, ERenameFlags Flags)
{
	if (!FEnumEditorUtils::IsNameAvailebleForUserDefinedEnum(FName(NewName)))
	{
		UE_LOG(LogClass, Warning, TEXT("UEnum::Rename: Name '%s' is already used."), NewName);
		return false;
	}
	
	const bool bSucceeded = Super::Rename(NewName, NewOuter, Flags);

	if (bSucceeded)
	{
		FEnumEditorUtils::UpdateAfterPathChanged(this);
	}

	return bSucceeded;
}

void UUserDefinedEnum::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	if(!bDuplicateForPIE)
	{
		FEnumEditorUtils::UpdateAfterPathChanged(this);
	}
}

void UUserDefinedEnum::PostLoad()
{
	Super::PostLoad();
	FEnumEditorUtils::UpdateAfterPathChanged(this);

	if (GIsEditor && !IsRunningCommandlet())
	{
		FEnumEditorUtils::EnsureAllDisplayNamesExist(this);
	}
}

void UUserDefinedEnum::PostEditUndo()
{
	Super::PostEditUndo();
	FEnumEditorUtils::UpdateAfterPathChanged(this);
}

FString UUserDefinedEnum::GenerateNewEnumeratorName()
{
	const TCHAR* EnumNameBase = TEXT("NewEnumerator");
	FString EnumNameString;
	do 
	{
		EnumNameString = FString::Printf(TEXT("%s%u"), EnumNameBase, UniqueNameIndex);
		++UniqueNameIndex;
	} 
	while (!FEnumEditorUtils::IsProperNameForUserDefinedEnumerator(this, EnumNameString));
	return EnumNameString;
}

#endif	// WITH_EDITOR

int32 UUserDefinedEnum::ResolveEnumerator(FArchive& Ar, int32 EnumeratorValue) const
{
#if WITH_EDITOR
	return FEnumEditorUtils::ResolveEnumerator(this, Ar, EnumeratorValue);
#else // WITH_EDITOR
	ensure(false);
	return EnumeratorValue;
#endif // WITH_EDITOR
}

FText UUserDefinedEnum::GetEnumText(int32 InIndex) const
{
	if (DisplayNames.IsValidIndex(InIndex))
	{
		return DisplayNames[InIndex];
	}

	return Super::GetEnumText(InIndex);
}

bool UUserDefinedEnum::SetEnums(TArray<TPair<FName, uint8>>& InNames, ECppForm InCppForm, bool bAddMaxKeyIfMissing)
{
	ensure(bAddMaxKeyIfMissing);
	if (Names.Num() > 0)
	{
		RemoveNamesFromMasterList();
	}
	Names = InNames;
	CppForm = InCppForm;

	const FString BaseEnumPrefix = GenerateEnumPrefix();
	checkSlow(BaseEnumPrefix.Len());

	const int32 MaxTryNum = 1024;
	for (int32 TryNum = 0; TryNum < MaxTryNum; ++TryNum)
	{
		const FString EnumPrefix = (TryNum == 0) ? BaseEnumPrefix : FString::Printf(TEXT("%s_%d"), *BaseEnumPrefix, TryNum - 1);
		const FName MaxEnumItem = *GenerateFullEnumName(*(EnumPrefix + TEXT("_MAX")));
		const int32 MaxEnumItemIndex = GetValueByName(MaxEnumItem);
		if ((MaxEnumItemIndex == INDEX_NONE) && (LookupEnumName(MaxEnumItem) == INDEX_NONE))
		{
			int MaxEnumValue = (InNames.Num() == 0)? 0 : GetMaxEnumValue() + 1;
			Names.Add(TPairInitializer<FName, uint8>(MaxEnumItem, MaxEnumValue));
			AddNamesToMasterList();
			return true;
		}
	}

	UE_LOG(LogClass, Error, TEXT("Unable to generate enum MAX entry due to name collision. Enum: %s"), *GetPathName());

	return false;
}

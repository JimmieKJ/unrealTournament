// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Engine/UserDefinedEnum.h"
#if WITH_EDITOR
#include "Editor/UnrealEd/Public/Kismet2/EnumEditorUtils.h"
#endif	// WITH_EDITOR

UUserDefinedEnum::UUserDefinedEnum(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

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
	FEnumEditorUtils::EnsureAllDisplayNamesExist(this);
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

int32 UUserDefinedEnum::ResolveEnumerator(FArchive& Ar, int32 EnumeratorIndex) const
{
#if WITH_EDITOR
	return FEnumEditorUtils::ResolveEnumerator(this, Ar, EnumeratorIndex);
#else // WITH_EDITOR
	ensure(false);
	return EnumeratorIndex;
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
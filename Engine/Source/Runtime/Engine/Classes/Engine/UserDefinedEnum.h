// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/** 
 *	User defined enumerations:
 *	- EnumType is always UEnum::ECppForm::Namespaced (to comfortable handle names collisions)
 *	- always have the last '_MAX' enumerator, that cannot be changed by user
 *	- Full enumerator name has form: '<enumeration path>::<short, user defined enumerator name>'
 */

#include "UserDefinedEnum.generated.h"

/** 
 *	An Enumeration is a list of named values.
 */
UCLASS()
class ENGINE_API UUserDefinedEnum : public UEnum
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	uint32 UniqueNameIndex;
#endif //WITH_EDITORONLY_DATA

	/**
	 * Names stored in "DisplayName" meta data. They are duplicated here, 
	 * so functions like UKismetNodeHelperLibrary::GetEnumeratorUserFriendlyName can use them
	 * outside the editor. (When meta data are not loaded).
	 * To sync DisplayNames with meta-data use FEnumEditorUtils::EnsureAllDisplayNamesExist.
	 */
	UPROPERTY()
	TArray<FText> DisplayNames;

public:
	//~ Begin UObject Interface.
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface.
	/**
	 * Generates full enum name give enum name.
	 * For UUserDefinedEnum full enumerator name has form: '<enumeration path>::<short, user defined enumerator name>'
	 *
	 * @param InEnumName Enum name.
	 * @return Full enum name.
	 */
	virtual FString GenerateFullEnumName(const TCHAR* InEnumName) const override;

	/*
	 *	Try to update value in enum variable after an enum's change.
	 *
	 *	@param EnumeratorIndex	old index
	 *	@return	new index
	 */
	virtual int32 ResolveEnumerator(FArchive& Ar, int32 EnumeratorValue) const override;

	/**
	 * @return	The enum string at the specified index.
	 */
	virtual FText GetEnumText(int32 InIndex) const override;

	virtual bool SetEnums(TArray<TPair<FName, uint8>>& InNames, ECppForm InCppForm, bool bAddMaxKeyIfMissing = true) override;

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual bool Rename(const TCHAR* NewName = NULL, UObject* NewOuter = NULL, ERenameFlags Flags = REN_None) override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostLoad() override;
	virtual void PostEditUndo() override;
	//~ End UObject Interface

	FString GenerateNewEnumeratorName();
#endif	// WITH_EDITOR
};

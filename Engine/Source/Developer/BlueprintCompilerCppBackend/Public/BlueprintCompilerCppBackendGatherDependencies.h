// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma  once

#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/UserDefinedStruct.h"
#include "Engine/UserDefinedEnum.h"

/** The struct gathers dependencies of a converted BPGC */
struct BLUEPRINTCOMPILERCPPBACKEND_API FGatherConvertedClassDependencies
{
protected:
	UStruct* OriginalStruct;

public:
	// Dependencies:
	TArray<UObject*> Assets; 

	TSet<UBlueprintGeneratedClass*> ConvertedClasses;
	TSet<UUserDefinedStruct*> ConvertedStructs;
	TSet<UUserDefinedEnum*> ConvertedEnum;

	// What to include/declare in the generated code:
	TSet<UField*> IncludeInHeader;
	TSet<UField*> DeclareInHeader;
	TSet<UField*> IncludeInBody;
public:
	FGatherConvertedClassDependencies(UStruct* InStruct);

	UStruct* GetActualStruct() const
	{
		return OriginalStruct;
	}

	UClass* FindOriginalClass(const UClass* InClass) const;

	UClass* GetFirstNativeOrConvertedClass(UClass* InClass, bool bExcludeBPDataOnly = false) const;

public:
	bool WillClassBeConverted(const UBlueprintGeneratedClass* InClass) const;

protected:
	void DependenciesForHeader();
};

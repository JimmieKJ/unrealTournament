// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma  once

#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/UserDefinedStruct.h"
#include "Engine/UserDefinedEnum.h"

/** The struct gathers dependencies of a converted BPGC */
struct BLUEPRINTCOMPILERCPPBACKEND_API FGatherConvertedClassDependencies : public FReferenceCollector
{
protected:
	TSet<UObject*> SerializedObjects;
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

	virtual bool IsIgnoringArchetypeRef() const override { return false; }
	virtual bool IsIgnoringTransient() const override { return true; }

	virtual void HandleObjectReference(UObject*& InObject, const UObject* InReferencingObject, const UProperty* InReferencingProperty) override;

	UStruct* GetOriginalStruct() const
	{
		return OriginalStruct;
	}

public:
	bool WillClassBeConverted(const UBlueprintGeneratedClass* InClass) const;

protected:
	void FindReferences(UObject* Object);
	void DependenciesForHeader();
	bool ShouldIncludeHeaderFor(UObject* Object) const;
};

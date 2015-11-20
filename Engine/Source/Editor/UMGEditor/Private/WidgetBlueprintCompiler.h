// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KismetCompiler.h"

class UWidgetBlueprint;
class UWidgetBlueprintGeneratedClass;

//////////////////////////////////////////////////////////////////////////
// FWidgetBlueprintCompiler

class FWidgetBlueprintCompiler : public FKismetCompilerContext
{
protected:
	typedef FKismetCompilerContext Super;

public:
	FWidgetBlueprintCompiler(UWidgetBlueprint* SourceSketch, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions, TArray<UObject*>* InObjLoaded);
	virtual ~FWidgetBlueprintCompiler();

	// FKismetCompilerContext
	virtual void Compile() override;
	// End FKismetCompilerContext

protected:
	UWidgetBlueprint* WidgetBlueprint() const { return Cast<UWidgetBlueprint>(Blueprint); }

	void ValidateWidgetNames();

	// FKismetCompilerContext
	//virtual UEdGraphSchema_K2* CreateSchema() override;
	virtual void CreateFunctionList() override;
	virtual void SpawnNewClass(const FString& NewClassName) override;
	virtual void PrecompileFunction(FKismetFunctionContext& Context) override;
	virtual void CleanAndSanitizeClass(UBlueprintGeneratedClass* ClassToClean, UObject*& OldCDO) override;
	virtual void SaveSubObjectsFromCleanAndSanitizeClass(FSubobjectCollection& SubObjectsToSave, UBlueprintGeneratedClass* ClassToClean, UObject*& OldCDO) override;
	virtual void EnsureProperGeneratedClass(UClass*& TargetClass) override;
	virtual void CreateClassVariablesFromBlueprint() override;
	virtual void FinishCompilingClass(UClass* Class) override;
	virtual bool ValidateGeneratedClass(UBlueprintGeneratedClass* Class) override;
	// End FKismetCompilerContext

	void VerifyEventReplysAreNotEmpty(FKismetFunctionContext& Context);

protected:
	UWidgetBlueprintGeneratedClass* NewWidgetBlueprintClass;

	// Map of properties created for widgets; to aid in debug data generation
	TMap<class UWidget*, class UProperty*> WidgetToMemberVariableMap;

	///----------------------------------------------------------------
};


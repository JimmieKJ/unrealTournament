// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine.h"

class FCompilerResultsLog;
class UBlueprint;

#define KISMET_COMPILER_MODULENAME "KismetCompiler"

//////////////////////////////////////////////////////////////////////////
// IKismetCompilerInterface

class IBlueprintCompiler
{
public:
	virtual void PreCompile(UBlueprint* Blueprint) = 0;

	virtual bool CanCompile(const UBlueprint* Blueprint) = 0;
	virtual void Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results, TArray<UObject*>* ObjLoaded) = 0;
	
	virtual void PostCompile(UBlueprint* Blueprint) = 0;
};

class IKismetCompilerInterface : public IModuleInterface
{
public:
	/**
	 * Compiles a blueprint.
	 *
	 * @param	Blueprint	The blueprint to compile.
	 * @param	Results  	The results log for warnings and errors.
	 * @param	Options		Compiler options.
	 */
	virtual void CompileBlueprint(class UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results, TSharedPtr<class FBlueprintCompileReinstancer> ParentReinstancer = NULL, TArray<UObject*>* ObjLoaded = NULL)=0;

	/**
	 * Compiles a user defined structure.
	 *
	 * @param	Struct		The structure to compile.
	 * @param	Results  	The results log for warnings and errors.
	 */
	virtual void CompileStructure(class UUserDefinedStruct* Struct, FCompilerResultsLog& Results)=0;

	/**
	 * Attempts to recover a corrupted blueprint package.
	 *
	 * @param	Blueprint	The blueprint to recover.
	 */
	virtual void RecoverCorruptedBlueprint(class UBlueprint* Blueprint)=0;

	/**
	 * Clears the blueprint's generated classes, and consigns them to oblivion
	 *
	 * @param	Blueprint	The blueprint to clear the classes for
	 */
	virtual void RemoveBlueprintGeneratedClasses(class UBlueprint* Blueprint)=0;

	/**
	 * Gets a list of all compilers for blueprints.  You can register new compilers through this list.
	 */
	virtual TArray<IBlueprintCompiler*>& GetCompilers() = 0;
};

//////////////////////////////////////////////////////////////////////////
// FKismet2CompilerModule

/**
 * The Kismet 2 Compiler module
 */
class FKismet2CompilerModule : public IKismetCompilerInterface
{
public:
	// Implementation of the IKismetCompilerInterface
	virtual void CompileBlueprint(class UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results, TSharedPtr<class FBlueprintCompileReinstancer> ParentReinstancer = NULL, TArray<UObject*>* ObjLoaded = NULL) override;
	virtual void CompileStructure(class UUserDefinedStruct* Struct, FCompilerResultsLog& Results) override;
	virtual void RecoverCorruptedBlueprint(class UBlueprint* Blueprint) override;
	virtual void RemoveBlueprintGeneratedClasses(class UBlueprint* Blueprint) override;
	virtual TArray<IBlueprintCompiler*>& GetCompilers() override { return Compilers; }
	// End implementation
private:
	void CompileBlueprintInner(class UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results, TSharedPtr<FBlueprintCompileReinstancer> Reinstancer, TArray<UObject*>* ObjLoaded);

	TArray<IBlueprintCompiler*> Compilers;
};


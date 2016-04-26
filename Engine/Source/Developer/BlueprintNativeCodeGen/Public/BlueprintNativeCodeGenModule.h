// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"
#include "Paths.h"			// for GetCleanFilename()
#include "ModuleManager.h"	// for GetModuleChecked<>
#include "Blueprint/BlueprintSupport.h"

class UBlueprint;

struct FNativeCodeGenInitData
{
	// This is an array of pairs, the pairs are PlatformName/PlatformTargetDirectory. These
	// are determined by the cooker:
	TArray< TPair< FString, FString > > CodegenTargets;

	// Optional Manifest ManifestIdentifier, used for child cook processes that need a unique manifest name.
	// The identifier is used to make a unique name for each platform that is converted.
	int32 ManifestIdentifier;
};

/** 
 * 
 */
class IBlueprintNativeCodeGenModule : public IModuleInterface
{
public:
	FORCEINLINE static void InitializeModule(const FNativeCodeGenInitData& InitData);

	/**
	* Utility function to reconvert all assets listed in a manifest, used to make fixes to
	* the code generator itself and quickly test them with an already converted project.
	*
	* Not for use with any kind of incremental cooking.
	*/
	FORCEINLINE static void InitializeModuleForRerunDebugOnly(const TArray< TPair< FString, FString > >& CodegenTargets);

	/**
	 * Wrapper function that retrieves the interface to this module from the 
	 * module-manager (so we can keep dependent code free of hardcoded strings,
	 * used to lookup this module by name).
	 * 
	 * @return A reference to this module interface (a singleton).
	 */
	static IBlueprintNativeCodeGenModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IBlueprintNativeCodeGenModule>(GetModuleName());
	}

	static bool IsNativeCodeGenModuleLoaded()
	{
		return FModuleManager::Get().IsModuleLoaded(GetModuleName());
	}
	
	/**
	 * Creates a centralized point where the name of this module is supplied 
	 * from (so we can avoid littering code with hardcoded strings that 
	 * all reference this module - in case we want to rename it).
	 * 
	 * @return The name of the module that this file is part of.
	 */
	static FName GetModuleName()
	{
		return TEXT("BlueprintNativeCodeGen");
	}

	virtual void Convert(UPackage* Package, ESavePackageResult ReplacementType, const TCHAR* PlatformName) = 0;
	virtual void SaveManifest(int32 Id = -1) = 0;
	virtual void MergeManifest(int32 ManifestIdentifier) = 0;
	virtual void FinalizeManifest() = 0;
	virtual void GenerateStubs() = 0;
	virtual void GenerateFullyConvertedClasses() = 0;
	virtual void MarkUnconvertedBlueprintAsNecessary(TAssetPtr<UBlueprint> BPPtr) = 0;
	virtual const TMultiMap<FName, TAssetSubclassOf<UObject>>& GetFunctionsBoundToADelegate() = 0;
protected:
	virtual void Initialize(const FNativeCodeGenInitData& InitData) = 0;
	virtual void InitializeForRerunDebugOnly(const TArray< TPair< FString, FString > >& CodegenTargets) = 0;
};

void IBlueprintNativeCodeGenModule::InitializeModule(const FNativeCodeGenInitData& InitData)
{
	IBlueprintNativeCodeGenModule& Module = FModuleManager::LoadModuleChecked<IBlueprintNativeCodeGenModule>(GetModuleName());
	Module.Initialize(InitData);
}

void IBlueprintNativeCodeGenModule::InitializeModuleForRerunDebugOnly(const TArray< TPair< FString, FString > >& CodegenTargets)
{
	IBlueprintNativeCodeGenModule& Module = FModuleManager::LoadModuleChecked<IBlueprintNativeCodeGenModule>(GetModuleName());
	Module.InitializeForRerunDebugOnly(CodegenTargets);
}


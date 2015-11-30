// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"
#include "Paths.h"			// for GetCleanFilename()
#include "ModuleManager.h"	// for GetModuleChecked<>
#include "Blueprint/BlueprintSupport.h"

#include "BlueprintNativeCodeGenModule.generated.h"

struct BLUEPRINTNATIVECODEGEN_API FCookCommandParams
{
	FCookCommandParams(const FString& CommandLine);

	bool    bRunConversion;
	FString ManifestFilePath;
	FString OutputPath;
	FString PluginName;

	TArray<FString> ToConversionParams() const;
};

/*******************************************************************************
* UBlueprintNativeCodeGenConfig
******************************************************************************/
UCLASS(config = Editor)
class BLUEPRINTNATIVECODEGEN_API UBlueprintNativeCodeGenConfig : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(globalconfig)
	TArray<FString> PackagesToNeverConvert;

	UPROPERTY(globalconfig)
	TArray<FString> ExcludedAssetTypes;

	UPROPERTY(globalconfig)
	TArray<FString> ExcludedBlueprintTypes;
};


/** 
 * 
 */
class IBlueprintNativeCodeGenModule : public IModuleInterface
{
public:
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

	virtual void Convert(UPackage* Package, EReplacementResult ReplacementType) = 0;
	virtual void SaveManifest(const TCHAR* Filename) = 0;
	virtual void MergeManifest(const TCHAR* Filename) = 0;
	virtual void FinalizeManifest() = 0;
};

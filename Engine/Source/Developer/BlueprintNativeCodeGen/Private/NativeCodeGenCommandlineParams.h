// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NativeCodeGenCommandlineParams.generated.h"

/*******************************************************************************
 * EDependencyConversionRule
 ******************************************************************************/
 
UENUM()
enum class EDependencyConversionRule
{
	None,
	RequiredOnly,
	All,
};

/*******************************************************************************
 * FNativeCodeGenCommandlineParams
 ******************************************************************************/

/**  */
struct FNativeCodeGenCommandlineParams
{
public: 
	static const FString HelpMessage;

public:
	FNativeCodeGenCommandlineParams(const TArray<FString>& CommandlineSwitches);
	
public:
	bool            bHelpRequested : 1;
	TArray<FString> WhiteListedAssetPaths;
	TArray<FString> BlackListedAssetPaths;
	FString         OutputDir;
	FString			PluginName;
	FString			ManifestFilePath;
	bool            bWipeRequested : 1;
	bool            bPreviewRequested : 1;
	EDependencyConversionRule DependencyConversionRule;
};


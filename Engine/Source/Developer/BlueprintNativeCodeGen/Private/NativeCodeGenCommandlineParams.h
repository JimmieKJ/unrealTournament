// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

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
};


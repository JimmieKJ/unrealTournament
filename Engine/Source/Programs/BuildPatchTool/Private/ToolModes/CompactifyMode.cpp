// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PrivatePch.h"
#include "BuildPatchTool.h"
#include "CompactifyMode.h"
using namespace BuildPatchTool;

class FCompactifyToolMode : public IToolMode
{
public:
	FCompactifyToolMode(const TSharedRef<IBuildPatchServicesModule>& InBpsInterface)
		: BpsInterface(InBpsInterface)
	{}

	virtual ~FCompactifyToolMode()
	{}

	virtual EReturnCode Execute() override
	{
		// Parse commandline
		if (ProcessCommandline() == false)
		{
			return EReturnCode::ArgumentProcessingError;
		}

		// Print help if requested
		if (bHelp)
		{
			UE_LOG(LogBuildPatchTool, Log, TEXT("COMPACTIFY MODE"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("This tool supports the removal of redundant patch data from a cloud directory."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			UE_LOG(LogBuildPatchTool, Log, TEXT("Required arguments:"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -mode=Compactify       Must be specified to launch the tool in compactify mode."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -CloudDir=\"\"           Specifies in quotes the cloud directory where manifest files and chunks to be compactified can be found."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			UE_LOG(LogBuildPatchTool, Log, TEXT("Optional arguments:"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -preview               Log all the actions it will take to update internal structures, but don't actually execute them."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -DataAgeThreshold=2    The minimum age in days of chunk files that will be deleted. Any unreferenced chunks older than this will be deleted."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			UE_LOG(LogBuildPatchTool, Log, TEXT("NB: If -DataAgeThreshold is not supplied, then all unreferenced existing data is eligible for deletion by the compactify process."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			return EReturnCode::OK;
		}

		// Grab options
		float DataAgeThresholdFloat = TCString<TCHAR>::Atod(*DataAgeThreshold);
		ECompactifyMode::Type CompactifyMode = bPreview ? ECompactifyMode::Preview : ECompactifyMode::Full;

		// Setup the module
		BpsInterface->SetCloudDirectory(CloudDir);

		// Run the compactify routine
		bool bSuccess = BpsInterface->CompactifyCloudDirectory(DataAgeThresholdFloat, CompactifyMode);
		return bSuccess ? EReturnCode::OK : EReturnCode::ToolFailure;
	}

private:

	bool ProcessCommandline()
	{
#define PARSE_SWITCH(Switch) ParseSwitch(TEXT(#Switch L"="), Switch, Switches)
		TArray<FString> Tokens, Switches;
		FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);

		bHelp = ParseOption(TEXT("help"), Switches);
		if (bHelp)
		{
			return true;
		}

		// Get all required parameters
		if (!(PARSE_SWITCH(CloudDir)
		   && PARSE_SWITCH(DataAgeThreshold)))
		{
			UE_LOG(LogBuildPatchTool, Error, TEXT("CloudDir and DataAgeThreshold are required parameters"));
			return false;
		}
		FPaths::NormalizeDirectoryName(CloudDir);

		// Check required numeric values
		if (!DataAgeThreshold.IsNumeric())
		{
			UE_LOG(LogBuildPatchTool, Error, TEXT("An error occurred processing numeric token from commandline -DataAgeThreshold=%s"), *DataAgeThreshold);
			return false;
		}

		// Get optional parameters
		bPreview = ParseOption(TEXT("preview"), Switches);

		return true;
#undef PARSE_SWITCH
	}

private:
	TSharedRef<IBuildPatchServicesModule> BpsInterface;
	bool bHelp;
	FString CloudDir;
	FString DataAgeThreshold;
	bool bPreview;
};

BuildPatchTool::IToolModeRef BuildPatchTool::FCompactifyToolModeFactory::Create(const TSharedRef<IBuildPatchServicesModule>& BpsInterface)
{
	return MakeShareable(new FCompactifyToolMode(BpsInterface));
}

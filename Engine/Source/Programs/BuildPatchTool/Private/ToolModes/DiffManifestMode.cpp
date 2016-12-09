// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DiffManifestMode.h"
#include "Interfaces/IBuildPatchServicesModule.h"
#include "BuildPatchTool.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"

using namespace BuildPatchTool;

class FDiffManifestToolMode : public IToolMode
{
public:
	FDiffManifestToolMode(const TSharedRef<IBuildPatchServicesModule>& InBpsInterface)
		: BpsInterface(InBpsInterface)
	{}

	virtual ~FDiffManifestToolMode()
	{}

	virtual EReturnCode Execute() override
	{
		// Parse commandline.
		if (ProcessCommandline() == false)
		{
			return EReturnCode::ArgumentProcessingError;
		}

		// Print help if requested.
		if (bHelp)
		{
			UE_LOG(LogBuildPatchTool, Log, TEXT("DIFF MANIFEST MODE"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("This tool mode reports the changes between two existing manifest files."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			UE_LOG(LogBuildPatchTool, Log, TEXT("Required arguments:"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -mode=DiffManifests    Must be specified to launch the tool in diff manifests mode."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -ManifestA=\"\"          Specifies in quotes the file path to the base manifest."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -ManifestB=\"\"          Specifies in quotes the file path to the update manifest."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			UE_LOG(LogBuildPatchTool, Log, TEXT("Optional arguments:"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -OutputFile=\"\"         Specifies in quotes the file path where the diff will be exported as a JSON object."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			return EReturnCode::OK;
		}

		// Run the merge manifest routine.
		bool bSuccess = BpsInterface->DiffManifests(ManifestA, ManifestB, OutputFile);
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

		// Get all required parameters.
		if (!(PARSE_SWITCH(ManifestA)
		   && PARSE_SWITCH(ManifestB)))
		{
			UE_LOG(LogBuildPatchTool, Error, TEXT("ManifestA and ManifestB are required parameters."));
			return false;
		}
		FPaths::NormalizeDirectoryName(ManifestA);
		FPaths::NormalizeDirectoryName(ManifestB);

		// Get optional parameters
		PARSE_SWITCH(OutputFile);
		FPaths::NormalizeDirectoryName(OutputFile);

		return true;
#undef PARSE_SWITCH
	}

private:
	TSharedRef<IBuildPatchServicesModule> BpsInterface;
	bool bHelp;
	FString ManifestA;
	FString ManifestB;
	FString OutputFile;
};

BuildPatchTool::IToolModeRef BuildPatchTool::FDiffManifestToolModeFactory::Create(const TSharedRef<IBuildPatchServicesModule>& BpsInterface)
{
	return MakeShareable(new FDiffManifestToolMode(BpsInterface));
}

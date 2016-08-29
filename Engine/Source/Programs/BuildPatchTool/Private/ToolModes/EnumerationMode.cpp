// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PrivatePch.h"
#include "BuildPatchTool.h"
#include "EnumerationMode.h"
using namespace BuildPatchTool;

class FEnumerationToolMode : public IToolMode
{
public:
	FEnumerationToolMode(const TSharedRef<IBuildPatchServicesModule>& InBpsInterface)
		: BpsInterface(InBpsInterface)
	{}

	virtual ~FEnumerationToolMode()
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
			UE_LOG(LogBuildPatchTool, Log, TEXT("ENUMERATION MODE"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("This tool supports enumerating patch data referenced by a build manifest."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			UE_LOG(LogBuildPatchTool, Log, TEXT("Required arguments:"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -mode=Enumeration    Must be specified to launch the tool in enumeration mode."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -ManifestFile=\"\"     Specifies in quotes the file path to the manifest to enumerate from."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -OutputFile=\"\"       Specifies in quotes the file path to a file where the list will be saved out, \\r\\n separated cloud relative paths."));

			UE_LOG(LogBuildPatchTool, Log, TEXT("Optional arguments:"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -includesizes        When specified, the size of each file (in bytes) will be output following the filename and a tab.  E.g. path/to/chunk\\t1233."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			return EReturnCode::OK;
		}

		// Run the enumeration routine
		bool bSuccess = BpsInterface->EnumerateManifestData(ManifestFile, OutputFile, bIncludeSizes);
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
		if (!(PARSE_SWITCH(ManifestFile)
		   && PARSE_SWITCH(OutputFile)))
		{
			UE_LOG(LogBuildPatchTool, Error, TEXT("ManifestFile and OutputFile are required parameters"));
			return false;
		}
		FPaths::NormalizeDirectoryName(ManifestFile);
		FPaths::NormalizeDirectoryName(OutputFile);

		// Get optional parameters
		bIncludeSizes = ParseOption(TEXT("includesizes"), Switches);

		return true;
#undef PARSE_SWITCH
	}

private:
	TSharedRef<IBuildPatchServicesModule> BpsInterface;
	bool bHelp;
	FString ManifestFile;
	FString OutputFile;
	bool bIncludeSizes;
};

BuildPatchTool::IToolModeRef BuildPatchTool::FEnumerationToolModeFactory::Create(const TSharedRef<IBuildPatchServicesModule>& BpsInterface)
{
	return MakeShareable(new FEnumerationToolMode(BpsInterface));
}

// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PrivatePch.h"
#include "BuildPatchTool.h"
#include "ToolMode.h"
#include "PatchGenerationMode.h"
#include "CompactifyMode.h"
#include "EnumerationMode.h"
#include "MergeManifestMode.h"


namespace BuildPatchTool
{
	class FHelpToolMode : public IToolMode
	{
	public:
		FHelpToolMode()
		{}

		virtual ~FHelpToolMode()
		{}

		virtual EReturnCode Execute() override
		{
			bool bRequestedHelp = FParse::Param(FCommandLine::Get(), TEXT("help"));

			// Output generic help info
			if (!bRequestedHelp)
			{
				UE_LOG(LogBuildPatchTool, Error, TEXT("No supported mode detected."));
			}
			UE_LOG(LogBuildPatchTool, Log, TEXT("-help can be added with any mode selection to get extended information."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("Supported modes are:"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -mode=PatchGeneration    Mode that generates patch data for the a new build."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -mode=Compactify         Mode that can clean up unneeded patch data from a given cloud directory with redundant data."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -mode=Enumeration        Mode that outputs the paths to referenced patch data given a single manifest."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -mode=MergeManifests     Mode that can combine two manifest files to create a new one, primarily used to create hotfixes."));

			// Error if this wasn't just a help request
			return bRequestedHelp ? EReturnCode::OK : EReturnCode::UnknownToolMode;
		}
	};

	IToolModeRef FToolModeFactory::Create(const TSharedRef<IBuildPatchServicesModule>& BpsInterface)
	{
		// Create the correct tool mode for the commandline given
		FString ToolModeValue;
		if (FParse::Value(FCommandLine::Get(), TEXT("mode="), ToolModeValue))
		{
			if (ToolModeValue == TEXT("patchgeneration"))
			{
				return FPatchGenerationToolModeFactory::Create(BpsInterface);
			}
			else if (ToolModeValue == TEXT("compactify"))
			{
				return FCompactifyToolModeFactory::Create(BpsInterface);
			}
			else if (ToolModeValue == TEXT("enumeration"))
			{
				return FEnumerationToolModeFactory::Create(BpsInterface);
			}
			else if (ToolModeValue == TEXT("mergemanifests"))
			{
				return FMergeManifestToolModeFactory::Create(BpsInterface);
			}
		}
		
		// No mode provided so create the generic help, which will return ok if -help was provided else return UnknownToolMode error
		return MakeShareable(new FHelpToolMode());
	}
}

// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

// ShaderCacheTool.cpp: Driver for testing compilation of an individual shader

#include "ShaderCacheTool.h"
#include "ShaderCache.h"

#include "RequiredProgramMainCPPInclude.h"

DEFINE_LOG_CATEGORY(LogShaderCacheTool);

IMPLEMENT_APPLICATION(ShaderCacheTool, "ShaderCacheTool");

namespace SCT
{
	FRunInfo::FRunInfo() :
	InputFile1(""),
	InputFile2(""),
	OutputFile("ShaderDrawCache.ushadercache"),
	GameVersion(0)
	{
	}
	
	void PrintUsage()
	{
		UE_LOG(LogShaderCacheTool, Display, TEXT("Usage:"));
		UE_LOG(LogShaderCacheTool, Display, TEXT("\tShaderCacheTool[.exe] FilePath1 FilePath2 [OutputPath] [-version=%d]"));
		UE_LOG(LogShaderCacheTool, Display, TEXT("\t\tOptions:"));
		UE_LOG(LogShaderCacheTool, Display, TEXT("\t\t\t-o=version\tThe game version to set."));
	}
	
	bool FRunInfo::Setup(const TArray<FString>& InOptions, const TArray<FString>& InSwitches)
	{
		if(InOptions.Num() >= 2)
		{
			InputFile1 = InOptions[0];
			InputFile2 = InOptions[1];
			if(InOptions.Num() >= 3)
			{
				OutputFile = InOptions[2];
			}
			for(FString Switch : InSwitches)
			{
				if(Switch.StartsWith(TEXT("version=")))
				{
					FString Vers = Switch.Mid(8);
					if(Vers.IsNumeric())
					{
						LexicalConversion::FromString(GameVersion, *Vers);
					}
				}
				else
				{
					return false;
				}
			}
			return true;
		}
		return false;
	}
	
	static int32 Run(const FRunInfo& RunInfo)
	{
		FShaderCache::SetGameVersion(RunInfo.GameVersion);
		
		bool bOK = FShaderCache::MergeShaderCacheFiles(RunInfo.InputFile1, RunInfo.InputFile2, RunInfo.OutputFile);

		return ((int32)!bOK);
	}
}

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	GEngineLoop.PreInit(ArgC, ArgV, TEXT("-NOPACKAGECACHE -Multiprocess"));

	TArray<FString> Tokens, Switches;
	FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);

	if (Tokens.Num() < 2)
	{
		UE_LOG(LogShaderCacheTool, Error, TEXT("Missing input files!"));
		SCT::PrintUsage();
		return 1;
	}
	
	if (Tokens.Num() == 2)
	{
		UE_LOG(LogShaderCacheTool, Warning,TEXT("Output set to ./ShaderDrawCache.ushadercache!"));
	}

	if (Tokens.Num() > 3)
	{
		UE_LOG(LogShaderCacheTool, Warning,TEXT("Ignoring extra command line arguments!"));
	}

	SCT::FRunInfo RunInfo;
	if (!RunInfo.Setup(Tokens, Switches))
	{
		SCT::PrintUsage();
		return 1;
	}

	int32 Value = SCT::Run(RunInfo);
	return Value;
}

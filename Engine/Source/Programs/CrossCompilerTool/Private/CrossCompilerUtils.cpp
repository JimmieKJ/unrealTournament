// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "CrossCompilerTool.h"

namespace CCT
{
	FRunInfo::FRunInfo() :
		Frequency(HSF_InvalidFrequency),
		Target(HCT_InvalidTarget),
		Entry(""),
		InputFile(""),
		OutputFile(""),
		BackEnd(BE_Invalid),
		bRunCPP(true),
		bUseNew(false),
		bList(false),
		bPreprocessOnly(false),
		bForcePackedUBs(false)
	{
	}

	void PrintUsage()
	{
		UE_LOG(LogCrossCompilerTool, Display, TEXT("Usage:"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\tCrossCompilerTool.exe input.hlsl {options}"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\tOptions:"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-o=file\tOutput filename"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-entry=function\tMain entry point (defaults to Main())"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-cpp\tOnly run C preprocessor"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-nocpp\tDo not run C preprocessor"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-flattenub\tRemoves/flattens UBs and puts them in a global packed array"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\tProfiles:"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-vs\tCompile as a Vertex Shader"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-ps\tCompile as a Pixel Shader"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-cs\tCompile as a Compute Shader"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-gs\tCompile as a Geomtry Shader"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-hs\tCompile as a Hull Shader"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-ds\tCompile as a Domain Shader"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\tTargets:"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-metal\tCompile for Metal"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-es2\tCompile for OpenGL ES 2"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-gl3\tCompile for OpenGL 3.2"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-gl4\tCompile for OpenGL 4.3"));
	}

	EHlslShaderFrequency FRunInfo::ParseFrequency(TArray<FString>& InOutSwitches)
	{
		TArray<FString> OutSwitches;
		EHlslShaderFrequency Frequency = HSF_InvalidFrequency;

		// Validate profile and target
		for(FString& Switch : InOutSwitches)
		{
			if (Switch == "vs")
			{
				if (Frequency != HSF_InvalidFrequency)
				{
					UE_LOG(LogCrossCompilerTool, Warning, TEXT("Ignoring extra command line argument -vs!"));
				}
				else
				{
					Frequency = HSF_VertexShader;
				}
			}
			else if (Switch == "ps")
			{
				if (Frequency != HSF_InvalidFrequency)
				{
					UE_LOG(LogCrossCompilerTool, Warning, TEXT("Ignoring extra command line argument -ps!"));
				}
				else
				{
					Frequency = HSF_PixelShader;
				}
			}
			else if (Switch == "cs")
			{
				if (Frequency != HSF_InvalidFrequency)
				{
					UE_LOG(LogCrossCompilerTool, Warning, TEXT("Ignoring extra command line argument -cs!"));
				}
				else
				{
					Frequency = HSF_ComputeShader;
				}
			}
			else if (Switch == "gs")
			{
				if (Frequency != HSF_InvalidFrequency)
				{
					UE_LOG(LogCrossCompilerTool, Warning, TEXT("Ignoring extra command line argument -gs!"));
				}
				else
				{
					Frequency = HSF_GeometryShader;
				}
			}
			else if (Switch == "hs")
			{
				if (Frequency != HSF_InvalidFrequency)
				{
					UE_LOG(LogCrossCompilerTool, Warning, TEXT("Ignoring extra command line argument -hs!"));
				}
				else
				{
					Frequency = HSF_HullShader;
				}
			}
			else if (Switch == "ds")
			{
				if (Frequency != HSF_InvalidFrequency)
				{
					UE_LOG(LogCrossCompilerTool, Warning, TEXT("Ignoring extra command line argument -ds!"));
				}
				else
				{
					Frequency = HSF_HullShader;
				}
			}
			else
			{
				OutSwitches.Add(Switch);
			}
		}

		// Default to PS
		if (Frequency == HSF_InvalidFrequency)
		{
			UE_LOG(LogCrossCompilerTool, Warning, TEXT("No profile given, assuming Pixel Shader (-ps)!"));
			Frequency = HSF_PixelShader;
		}

		Swap(InOutSwitches, OutSwitches);
		return Frequency;
	}


	EHlslCompileTarget FRunInfo::ParseTarget(TArray<FString>& InOutSwitches, EBackend& OutBackEnd)
	{
		TArray<FString> OutSwitches;
		EHlslCompileTarget Target = HCT_InvalidTarget;

		// Validate profile and target
		for (FString& Switch : InOutSwitches)
		{
			if (Switch == "es2")
			{
				if (Target != HCT_InvalidTarget)
				{
					UE_LOG(LogCrossCompilerTool, Warning, TEXT("Ignoring extra command line argument -es2!"));
				}
				else
				{
					Target = HCT_FeatureLevelES2;
					OutBackEnd = BE_OpenGL;
				}
			}
			else if (Switch == "metal")
			{
				if (Target != HCT_InvalidTarget)
				{
					UE_LOG(LogCrossCompilerTool, Warning, TEXT("Ignoring extra command line argument -metal!"));
				}
				else
				{
					Target = HCT_FeatureLevelES3_1;
					OutBackEnd = BE_Metal;
				}
			}
			else if (Switch == "gl3")
			{
				if (Target != HCT_InvalidTarget)
				{
					UE_LOG(LogCrossCompilerTool, Warning, TEXT("Ignoring extra command line argument -gl3!"));
				}
				else
				{
					Target = HCT_FeatureLevelSM4;
					OutBackEnd = BE_OpenGL;
				}
			}
			else if (Switch == "gl4")
			{
				if (Target != HCT_InvalidTarget)
				{
					UE_LOG(LogCrossCompilerTool, Warning, TEXT("Ignoring extra command line argument -gl4!"));
				}
				else
				{
					Target = HCT_FeatureLevelSM5;
					OutBackEnd = BE_OpenGL;
				}
			}
			else
			{
				OutSwitches.Add(Switch);
			}
		}

		// Defaults to Metal
		if (Target == HCT_InvalidTarget)
		{
			UE_LOG(LogCrossCompilerTool, Warning, TEXT("No profile given, assuming (-gl4)!"));
			Target = HCT_FeatureLevelSM5;
			OutBackEnd = BE_OpenGL;
		}
		check(OutBackEnd != BE_Invalid);

		Swap(InOutSwitches, OutSwitches);
		return Target;
	}

	bool FRunInfo::Setup(const FString& InFilename, const TArray<FString>& InSwitches)
	{
		TArray<FString> Switches = InSwitches;
		Frequency = ParseFrequency(Switches);
		Target = ParseTarget(Switches, BackEnd);
		InputFile = InFilename;
		bRunCPP = true;

		// Now find entry point and output filename
		for (const FString& Switch : InSwitches)
		{
			if (Switch.StartsWith(TEXT("o=")))
			{
				if (OutputFile != "")
				{
					UE_LOG(LogCrossCompilerTool, Warning, TEXT("Ignoring extra command line argument -o!"));
				}
				else
				{
					OutputFile = Switch.Mid(2);
				}
			}
			else if (Switch.StartsWith(TEXT("entry=")))
			{
				if (Entry != TEXT(""))
				{
					UE_LOG(LogCrossCompilerTool, Warning, TEXT("Ignoring extra command line argument -entry!"));
				}
				else
				{
					Entry = Switch.Mid(6);
				}
			}
			else if (Switch.StartsWith(TEXT("nocpp")))
			{
				bRunCPP = false;
			}
			else if (Switch.StartsWith(TEXT("cpp")))
			{
				bPreprocessOnly = true;
			}
			else if (Switch.StartsWith(TEXT("new")))
			{
				bUseNew = true;
			}
			else if (Switch.StartsWith(TEXT("list")))
			{
				bList = true;
			}
			else if (Switch.StartsWith(TEXT("flattenub")))
			{
				bForcePackedUBs = true;
			}
		}

		// Default to PS
		if (Entry == TEXT(""))
		{
			UE_LOG(LogCrossCompilerTool, Warning, TEXT("No entry point given, assuming Main (-entry=Main)!"));
			Entry = TEXT("Main");
		}

		return true;
	}
}

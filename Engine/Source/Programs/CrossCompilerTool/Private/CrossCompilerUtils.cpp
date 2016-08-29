// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


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
		bValidate(true),
		bRunCPP(true),
		bUseNew(false),
		bList(false),
		bPreprocessOnly(false),
		bTestRemoveUnused(false),
		bPackIntoUBs(false),
		bUseDX11Clip(false),
		bFlattenUBs(false),
		bFlattenUBStructs(false),
		bGroupFlattenUBs(false),
		bCSE(false),
		bExpandExpressions(false),
		bFixAtomics(false),
		bSeparateShaders(false),
		bUseFullPrecisionInPS(false)
	{
	}

	void PrintUsage()
	{
		UE_LOG(LogCrossCompilerTool, Display, TEXT("Usage:"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\tCrossCompilerTool.exe input.[usf|hlsl] {options}"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\tOptions:"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-o=file\tOutput filename"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-entry=function\tMain entry point (defaults to Main())"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-cpp\tOnly run C preprocessor"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\tFlags:"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-nocpp\tDo not run C preprocessor"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-novalidate\tDo not run AST/IR validation"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-dx11clip\tUse DX11 Clip space fixup at end of VS"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-flattenub\tRemoves/flattens UBs and puts them in a global packed array"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-flattenubstruct\tFlatten UB structures"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-groupflatub\tGroup flattened UBs"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-cse\tRun common subexpression elimination"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-xpxpr\tExpand expressions"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-separateshaders\tUse the separate shaders flags"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-packintoubs\tMove packed global uniform arrays into a uniform buffer"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-fixatomics\tConvert accesses to atomic variable into intrinsics"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-usefullprecision\tSet default precision to highp in a pixel shader (default is mediump on ES2 platforms)"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\tProfiles:"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-vs\tCompile as a Vertex Shader"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-ps\tCompile as a Pixel Shader"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-cs\tCompile as a Compute Shader"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-gs\tCompile as a Geometry Shader"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-hs\tCompile as a Hull Shader"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-ds\tCompile as a Domain Shader"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\tTargets:"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-metal\tCompile for Metal"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-vulkan\tCompile for Vulkan ES3.1"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-vulkansm4\tCompile for Vulkan SM4"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-vulkansm5\tCompile for Vulkan SM5"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-es2\tCompile for OpenGL ES 2"));
		UE_LOG(LogCrossCompilerTool, Display, TEXT("\t\t\t-es31ext\tCompile for OpenGL ES 3.1 with AEP"));
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
			else if (Switch == "es31")
			{
				if (Target != HCT_InvalidTarget)
				{
					UE_LOG(LogCrossCompilerTool, Warning, TEXT("Ignoring extra command line argument -es31!"));
				}
				else
				{
					Target = HCT_FeatureLevelES3_1;
					OutBackEnd = BE_OpenGL;
				}
			}
			else if (Switch == "es31ext")
			{
				if (Target != HCT_InvalidTarget)
				{
					UE_LOG(LogCrossCompilerTool, Warning, TEXT("Ignoring extra command line argument -es31ext!"));
				}
				else
				{
					Target = HCT_FeatureLevelES3_1Ext;
					OutBackEnd = BE_OpenGL;
				}
			}
			else if (Switch == "vulkan")
			{
				if (Target != HCT_InvalidTarget)
				{
					UE_LOG(LogCrossCompilerTool, Warning, TEXT("Ignoring extra command line argument -vulkan!"));
				}
				else
				{
					Target = HCT_FeatureLevelES3_1;
					OutBackEnd = BE_Vulkan;
				}
			}
			else if (Switch == "vulkansm4")
			{
				if (Target != HCT_InvalidTarget)
				{
					UE_LOG(LogCrossCompilerTool, Warning, TEXT("Ignoring extra command line argument -vulkansm4!"));
				}
				else
				{
					Target = HCT_FeatureLevelSM4;
					OutBackEnd = BE_Vulkan;
				}
			}
			else if (Switch == "vulkansm5")
			{
				if (Target != HCT_InvalidTarget)
				{
					UE_LOG(LogCrossCompilerTool, Warning, TEXT("Ignoring extra command line argument -vulkansm5!"));
				}
				else
				{
					Target = HCT_FeatureLevelSM5;
					OutBackEnd = BE_Vulkan;
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
			else if (Switch == "metalmrt" || Switch == "metalsm4")
			{
				if (Target != HCT_InvalidTarget)
				{
					UE_LOG(LogCrossCompilerTool, Warning, TEXT("Ignoring extra command line argument -%s!"), *Switch);
				}
				else
				{
					Target = HCT_FeatureLevelSM4;
					OutBackEnd = BE_Metal;
				}
			}
			else if (Switch == "metalsm5")
			{
				if (Target != HCT_InvalidTarget)
				{
					UE_LOG(LogCrossCompilerTool, Warning, TEXT("Ignoring extra command line argument -metalsm5!"));
				}
				else
				{
					Target = HCT_FeatureLevelSM5;
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
			else if (Switch.StartsWith(TEXT("flattenubstruct")))
			{
				bFlattenUBStructs = true;
			}
			else if (Switch.StartsWith(TEXT("flattenub")))
			{
				bFlattenUBs = true;
			}
			else if (Switch.StartsWith(TEXT("groupflatub")))
			{
				bGroupFlattenUBs = true;
			}
			else if (Switch.StartsWith(TEXT("removeunused")))
			{
				bTestRemoveUnused = true;
			}
			else if (Switch.StartsWith(TEXT("packintoubs")))
			{
				bPackIntoUBs = true;
			}
			else if (Switch.StartsWith(TEXT("fixatomics")))
			{
				bFixAtomics = true;
			}
			else if (Switch.StartsWith(TEXT("cse")))
			{
				bCSE = true;
			}
			else if (Switch.StartsWith(TEXT("novalidate")))
			{
				bValidate = false;
			}
			else if (Switch.StartsWith(TEXT("dx11clip")))
			{
				bUseDX11Clip = true;
			}
			else if (Switch.StartsWith(TEXT("xpxpr")))
			{
				bExpandExpressions = true;
			}
			else if (Switch.StartsWith(TEXT("separateshaders")))
			{
				bSeparateShaders = true;
			}
			else if (Switch.StartsWith(TEXT("usefullprecision")))
			{
				bUseFullPrecisionInPS = true;
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

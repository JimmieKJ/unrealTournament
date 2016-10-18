// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

// CrossCompilerTool.cpp: Driver for testing compilation of an individual shader

#include "CrossCompilerTool.h"
#include "hlslcc.h"
#include "MetalBackend.h"
#include "GlslBackend.h"
#define PLATFORM_SUPPORTS_VULKAN			PLATFORM_WINDOWS		// for now just Windows
#if PLATFORM_SUPPORTS_VULKAN
#include "VulkanBackend.h"
#endif
#include "HlslAST.h"
#include "HlslLexer.h"
#include "HlslParser.h"
#include "ShaderPreprocessor.h"
#include "ShaderCompilerCommon.h"

#include "RequiredProgramMainCPPInclude.h"

DEFINE_LOG_CATEGORY(LogCrossCompilerTool);

IMPLEMENT_APPLICATION(CrossCompilerTool, "CrossCompilerTool");

namespace CCT
{
	static bool Preprocess(const FString& InputFile, FString& Output)
	{
		FShaderCompilerInput CompilerInput;
		CompilerInput.SourceFilename = InputFile;
		FShaderCompilerOutput CompilerOutput;
		TArray<FShaderCompilerError> Errors;
		return PreprocessShaderFile(Output, Errors, InputFile);
	}

	static void DumpMessages(CrossCompiler::FCompilerMessages& Messages)
	{
		for (auto& Message : Messages.MessageList)
		{
			if (Message.bIsError)
			{
				UE_LOG(LogCrossCompilerTool, Error, TEXT("%s"), *Message.Message);
			}
			else
			{
				UE_LOG(LogCrossCompilerTool, Warning, TEXT("%s"), *Message.Message);
			}
		}
	}

	static int32 Run(const FRunInfo& RunInfo)
	{
		ILanguageSpec* Language = nullptr;
		FCodeBackend* Backend = nullptr;

		EHlslCompileTarget CompileTarget = RunInfo.Target;

		uint32 Flags = HLSLCC_PackUniforms;
		Flags |= RunInfo.bValidate ? 0 : HLSLCC_NoValidation;
		Flags |= RunInfo.bRunCPP ? 0 : HLSLCC_NoPreprocess;
		Flags |= RunInfo.bPackIntoUBs ? HLSLCC_PackUniformsIntoUniformBuffers : 0;
		Flags |= RunInfo.bUseDX11Clip ? HLSLCC_DX11ClipSpace : 0;
		Flags |= RunInfo.bFlattenUBs ? HLSLCC_FlattenUniformBuffers : 0;
		Flags |= RunInfo.bFlattenUBStructs ? HLSLCC_FlattenUniformBufferStructures : 0;
		Flags |= RunInfo.bGroupFlattenUBs ? HLSLCC_GroupFlattenedUniformBuffers : 0;
		Flags |= RunInfo.bCSE ? HLSLCC_ApplyCommonSubexpressionElimination : 0;
		Flags |= RunInfo.bExpandExpressions ? HLSLCC_ExpandSubexpressions : 0;
		Flags |= RunInfo.bFixAtomics ? HLSLCC_FixAtomicReferences : 0;
		Flags |= RunInfo.bSeparateShaders ? HLSLCC_SeparateShaderObjects : 0;
		Flags |= RunInfo.bUseFullPrecisionInPS ? HLSLCC_UseFullPrecisionInPS : 0;

		FGlslLanguageSpec GlslLanguage(RunInfo.Target == HCT_FeatureLevelES2);
		FGlslCodeBackend GlslBackend(Flags, RunInfo.Target);
		FMetalLanguageSpec MetalLanguage;
		FMetalCodeBackend MetalBackend(Flags, CompileTarget, (RunInfo.Target >= HCT_FeatureLevelSM4), true, (RunInfo.Target >= HCT_FeatureLevelSM4));
#if PLATFORM_SUPPORTS_VULKAN
		FVulkanBindingTable VulkanBindingTable(RunInfo.Frequency);
		FVulkanLanguageSpec VulkanLanguage(false);
		FVulkanCodeBackend VulkanBackend(Flags, VulkanBindingTable, CompileTarget);
#endif

		switch (RunInfo.BackEnd)
		{
		case CCT::FRunInfo::BE_Metal:
			Language = &MetalLanguage;
			Backend = &MetalBackend;
			CompileTarget = HCT_FeatureLevelES3_1;
			break;

		case CCT::FRunInfo::BE_OpenGL:
			Language = &GlslLanguage;
			Backend = &GlslBackend;
			Flags |= HLSLCC_DX11ClipSpace;
			break;

#if PLATFORM_SUPPORTS_VULKAN
		case CCT::FRunInfo::BE_Vulkan:
			Language = &VulkanLanguage;
			Backend = &VulkanBackend;
			Flags |= HLSLCC_DX11ClipSpace;
			break;
#endif

		default:
			return 1;
		}

		FString HLSLShaderSource;
		if (RunInfo.bUseNew)
		{
			if (RunInfo.bList)
			{
				if (!FFileHelper::LoadFileToString(HLSLShaderSource, *RunInfo.InputFile))
				{
					UE_LOG(LogCrossCompilerTool, Error, TEXT("Couldn't load Input file '%s'!"), *RunInfo.InputFile);
					return 1;
				}

				TArray<FString> List;

				if (!FFileHelper::LoadANSITextFileToStrings(*RunInfo.InputFile, &IFileManager::Get(), List))
				{
					return 1;
				}

				int32 Count = 0;
				for (auto& File : List)
				{
					FString HLSLShader;
					if (!FFileHelper::LoadFileToString(HLSLShader, *File))
					{
						UE_LOG(LogCrossCompilerTool, Error, TEXT("Couldn't load Input file '%s'!"), *File);
						continue;
					}
					UE_LOG(LogCrossCompilerTool, Log, TEXT("%d: %s"), Count++, *File);

					CrossCompiler::FCompilerMessages Messages;
					if (!CrossCompiler::Parser::Parse(HLSLShader, File, Messages))
					{
						DumpMessages(Messages);
						UE_LOG(LogCrossCompilerTool, Log, TEXT("Error compiling '%s'!"), *File);
						return 1;
					}

					DumpMessages(Messages);
				}
			}
			else
			{
				if (RunInfo.bRunCPP)
				{
					//GMalloc->DumpAllocatorStats(*FGenericPlatformOutputDevices::GetLog());
					if (!Preprocess(RunInfo.InputFile, HLSLShaderSource))
					{
						UE_LOG(LogCrossCompilerTool, Log, TEXT("Error during preprocessor on '%s'!"), *RunInfo.InputFile);
						return 1;
					}
					//GMalloc->DumpAllocatorStats(*FGenericPlatformOutputDevices::GetLog());
				}
				else
				{
					if (!FFileHelper::LoadFileToString(HLSLShaderSource, *RunInfo.InputFile))
					{
						UE_LOG(LogCrossCompilerTool, Error, TEXT("Couldn't load Input file '%s'!"), *RunInfo.InputFile);
						return 1;
					}
				}

				if (RunInfo.bPreprocessOnly)
				{
					return 0;
				}

				CrossCompiler::FCompilerMessages Messages;
				if (!CrossCompiler::Parser::Parse(HLSLShaderSource, *RunInfo.InputFile, Messages))
				{
					UE_LOG(LogCrossCompilerTool, Log, TEXT("Error compiling '%s'!"), *RunInfo.InputFile);
					DumpMessages(Messages);
					return 1;
				}

				DumpMessages(Messages);
			}
			//Scanner.Dump();
			return 0;
		}
		else
		{
			if (!FFileHelper::LoadFileToString(HLSLShaderSource, *RunInfo.InputFile))
			{
				UE_LOG(LogCrossCompilerTool, Error, TEXT("Couldn't load Input file '%s'!"), *RunInfo.InputFile);
				return 1;
			}
		}

		ANSICHAR* ShaderSource = 0;
		ANSICHAR* ErrorLog = 0;

		FHlslCrossCompilerContext Context(Flags, RunInfo.Frequency, CompileTarget);
		if (Context.Init(TCHAR_TO_ANSI(*RunInfo.InputFile), Language))
		{
			Context.Run(
				TCHAR_TO_ANSI(*HLSLShaderSource),
				TCHAR_TO_ANSI(*RunInfo.Entry),
				Backend,
				&ShaderSource,
				&ErrorLog);
		}

		if (ErrorLog)
		{
			FString OutError(ANSI_TO_TCHAR(ErrorLog));
			UE_LOG(LogCrossCompilerTool, Warning, TEXT("%s"), *OutError);
		}

		if (ShaderSource)
		{
			FString OutSource(ANSI_TO_TCHAR(ShaderSource));
			UE_LOG(LogCrossCompilerTool, Display, TEXT("%s"), *OutSource);
			if (RunInfo.OutputFile.Len() > 0)
			{
				FFileHelper::SaveStringToFile(OutSource, *RunInfo.OutputFile);
			}
		}

		if (ShaderSource)
		{
			const ANSICHAR* USFSource = ShaderSource;
			CrossCompiler::FHlslccHeader CCHeader;
			int32 Len = FCStringAnsi::Strlen(USFSource);
			if (!CCHeader.Read(USFSource, Len))
			{
				UE_LOG(LogCrossCompilerTool, Error, TEXT("Bad hlslcc header found"));
			}

			if (Language == &GlslLanguage && *USFSource != '#')
			{
				UE_LOG(LogCrossCompilerTool, Error, TEXT("Bad hlslcc header found! Missing '#'!"));
			}
		}

		free(ShaderSource);
		free(ErrorLog);

		return 0;
	}
}

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	GEngineLoop.PreInit(ArgC, ArgV, TEXT("-NOPACKAGECACHE -Multiprocess"));

	TArray<FString> Tokens, Switches;
	FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);

	if (Tokens.Num() < 1)
	{
		UE_LOG(LogCrossCompilerTool, Error, TEXT("Missing input file!"));
		CCT::PrintUsage();
		return 1;
	}

	if (Tokens.Num() > 1)
	{
		UE_LOG(LogCrossCompilerTool, Warning,TEXT("Ignoring extra command line arguments!"));
	}

	CCT::FRunInfo RunInfo;
	if (!RunInfo.Setup(Tokens[0], Switches))
	{
		return 1;
	}

	int32 Value = CCT::Run(RunInfo);
	//FGenericPlatformOutputDevices::GetLog()->Flush();
	return Value;
}

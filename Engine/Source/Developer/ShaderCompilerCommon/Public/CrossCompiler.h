// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "../../RHI/Public/RHIDefinitions.h"

// Cross compiler support/common functionality
extern SHADERCOMPILERCOMMON_API FString CreateCrossCompilerBatchFileContents(
											const FString& ShaderFile,
											const FString& OutputFile,
											const FString& FrequencySwitch,
											const FString& EntryPoint,
											const FString& VersionSwitch,
											const FString& ExtraArguments = TEXT(""));

extern SHADERCOMPILERCOMMON_API int32 HlslCrossCompile(
	const FString& InSourceFilename, 
	const FString& InShaderSource,
	const FString& InEntryPoint,
	EShaderFrequency InShaderFrequency,
	class FCodeBackend* InShaderBackEnd,
	struct ILanguageSpec* InLanguageSpec,
	unsigned int InFlags,
	int32/*EHlslCompileTarget*/ InCompileTarget,
	FString& OutShaderSource,
	char** OutErrorLog);

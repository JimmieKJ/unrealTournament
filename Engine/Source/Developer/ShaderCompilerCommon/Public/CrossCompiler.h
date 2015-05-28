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

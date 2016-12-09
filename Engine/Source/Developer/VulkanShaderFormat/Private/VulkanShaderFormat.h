// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "hlslcc.h"

enum class EVulkanShaderVersion
{
	ES3_1,
	SM4_UB,
	ES3_1_ANDROID,
	SM4,
	SM5,
};
extern void CompileShader_Windows_Vulkan(const struct FShaderCompilerInput& Input,struct FShaderCompilerOutput& Output,const class FString& WorkingDirectory, EVulkanShaderVersion Version);


// Hold information to be able to call the compilers
struct FCompilerInfo
{
	const struct FShaderCompilerInput& Input;
	FString WorkingDirectory;
	FString Profile;
	uint32 CCFlags;
	EHlslShaderFrequency Frequency;
	bool bDebugDump;
	FString BaseSourceFilename;

	FCompilerInfo(const struct FShaderCompilerInput& InInput, const FString& InWorkingDirectory, EHlslShaderFrequency InFrequency);
};
 
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"

#include "hlslcc.h"

namespace CCT
{
	class FRunInfo
	{
	public:
		enum EBackend
		{
			BE_Metal,
			BE_OpenGL,
			BE_Vulkan,
			BE_Invalid,
		};
		EHlslShaderFrequency Frequency;
		EHlslCompileTarget Target;
		FString Entry;
		FString InputFile;
		FString OutputFile;
		EBackend BackEnd;
		bool bValidate;
		bool bRunCPP;
		bool bUseNew;
		bool bList;
		bool bPreprocessOnly;
		bool bTestRemoveUnused;
		bool bPackIntoUBs;
		bool bUseDX11Clip;
		bool bFlattenUBs;
		bool bFlattenUBStructs;
		bool bGroupFlattenUBs;
		bool bCSE;
		bool bExpandExpressions;
		bool bFixAtomics;
		bool bSeparateShaders;
		bool bUseFullPrecisionInPS;

		FRunInfo();
		bool Setup(const FString& InOptions, const TArray<FString>& InSwitches);

	protected:
		static EHlslShaderFrequency ParseFrequency(TArray<FString>& InOutSwitches);
		static EHlslCompileTarget ParseTarget(TArray<FString>& InOutSwitches, EBackend& OutBackEnd);
	};

	void PrintUsage();
}

DECLARE_LOG_CATEGORY_EXTERN(LogCrossCompilerTool, Log, All);

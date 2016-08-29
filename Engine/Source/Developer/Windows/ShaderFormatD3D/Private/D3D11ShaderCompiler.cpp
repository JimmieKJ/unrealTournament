// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ShaderFormatD3D.h"
#include "ShaderPreprocessor.h"
#include "ShaderCompilerCommon.h"
#include "D3D11ShaderResources.h"

DEFINE_LOG_CATEGORY_STATIC(LogD3D11ShaderCompiler, Log, All);

#define DEBUG_SHADERS 0

// D3D headers.
#define D3D_OVERLOADS 1

// D3D doesn't define a mask for this, so we do so here
#define SHADER_OPTIMIZATION_LEVEL_MASK (D3D10_SHADER_OPTIMIZATION_LEVEL0 | D3D10_SHADER_OPTIMIZATION_LEVEL1 | D3D10_SHADER_OPTIMIZATION_LEVEL2 | D3D10_SHADER_OPTIMIZATION_LEVEL3)

// Disable macro redefinition warning for compatibility with Windows SDK 8+
#pragma warning(push)
#pragma warning(disable : 4005)	// macro redefinition

#include "AllowWindowsPlatformTypes.h"
	#include <D3D11.h>
	#include <D3Dcompiler.h>
	#include <d3d11Shader.h>
#include "HideWindowsPlatformTypes.h"
#undef DrawText

#pragma warning(pop)

static int32 GD3DAllowRemoveUnused = 0;


static int32 GD3DCheckForDoubles = 1;
static FAutoConsoleVariableRef CVarD3DCheckForDoubles(
	TEXT("r.D3DCheckShadersForDouble"),
	GD3DCheckForDoubles,
	TEXT("Enables checking the D3D microcode for uses of double. This is not allowed on all D3D11 cards.\n")
	TEXT(" 0: Do not check for faster compilation\n")
	TEXT(" 1: Enable checking and error if found (default)"),
	ECVF_Default
	);

static int32 GD3DDumpAMDCodeXLFile = 0;
static FAutoConsoleVariableRef CVarD3DDumpAMDCodeXLFile(
	TEXT("r.D3DDumpAMDCodeXLFile"),
	GD3DDumpAMDCodeXLFile,
	TEXT("When r.DumpShaderDebugInfo is enabled, this will generate a batch file for running CodeXL.\n")
	TEXT(" 0: Do not generate extra batch file (default)\n")
	TEXT(" 1: Enable generating extra batch file"),
	ECVF_Default
	);

static int32 GD3DDumpD3DAsmFile = 0;
static FAutoConsoleVariableRef CVarD3DDumpD3DAsmFile(
	TEXT("r.D3DDumpD3DAsm"),
	GD3DDumpD3DAsmFile,
	TEXT("When r.DumpShaderDebugInfo is enabled, this will generate a text file with the fxc assembly.\n")
	TEXT(" 0: Do not generate extra file (default)\n")
	TEXT(" 1: Enable generating extra disassembly file"),
	ECVF_Default
	);

/**
 * TranslateCompilerFlag - translates the platform-independent compiler flags into D3DX defines
 * @param CompilerFlag - the platform-independent compiler flag to translate
 * @return uint32 - the value of the appropriate D3DX enum
 */
static uint32 TranslateCompilerFlagD3D11(ECompilerFlags CompilerFlag)
{
	// @TODO - currently d3d11 uses d3d10 shader compiler flags... update when this changes in DXSDK
	switch(CompilerFlag)
	{
	case CFLAG_PreferFlowControl: return D3D10_SHADER_PREFER_FLOW_CONTROL;
	case CFLAG_AvoidFlowControl: return D3D10_SHADER_AVOID_FLOW_CONTROL;
	default: return 0;
	};
}

/**
 * Filters out unwanted shader compile warnings
 */
static void D3D11FilterShaderCompileWarnings(const FString& CompileWarnings, TArray<FString>& FilteredWarnings)
{
	TArray<FString> WarningArray;
	FString OutWarningString = TEXT("");
	CompileWarnings.ParseIntoArray(WarningArray, TEXT("\n"), true);
	
	//go through each warning line
	for (int32 WarningIndex = 0; WarningIndex < WarningArray.Num(); WarningIndex++)
	{
		//suppress "warning X3557: Loop only executes for 1 iteration(s), forcing loop to unroll"
		if (!WarningArray[WarningIndex].Contains(TEXT("X3557"))
			// "warning X3205: conversion from larger type to smaller, possible loss of data"
			// Gets spammed when converting from float to half
			&& !WarningArray[WarningIndex].Contains(TEXT("X3205")))
		{
			FilteredWarnings.AddUnique(WarningArray[WarningIndex]);
		}
	}
}

// @return 0 if not recognized
static const TCHAR* GetShaderProfileName(FShaderTarget Target)
{
	if(Target.Platform == SP_PCD3D_SM5)
	{
		checkSlow(Target.Frequency == SF_Vertex ||
			Target.Frequency == SF_Pixel ||
			Target.Frequency == SF_Hull ||
			Target.Frequency == SF_Domain ||
			Target.Frequency == SF_Compute ||
			Target.Frequency == SF_Geometry);

		//set defines and profiles for the appropriate shader paths
		switch(Target.Frequency)
		{
		case SF_Pixel:
			return TEXT("ps_5_0");
		case SF_Vertex:
			return TEXT("vs_5_0");
		case SF_Hull:
			return TEXT("hs_5_0");
		case SF_Domain:
			return TEXT("ds_5_0");
		case SF_Geometry:
			return TEXT("gs_5_0");
		case SF_Compute:
			return TEXT("cs_5_0");
		}
	}
	else if(Target.Platform == SP_PCD3D_SM4 || Target.Platform == SP_PCD3D_ES2 || Target.Platform == SP_PCD3D_ES3_1)
	{
		checkSlow(Target.Frequency == SF_Vertex ||
			Target.Frequency == SF_Pixel ||
			Target.Frequency == SF_Geometry);

		//set defines and profiles for the appropriate shader paths
		switch(Target.Frequency)
		{
		case SF_Pixel:
			return TEXT("ps_4_0");
		case SF_Vertex:
			return TEXT("vs_4_0");
		case SF_Geometry:
			return TEXT("gs_4_0");
		}
	}

	return NULL;
}

/**
 * D3D11CreateShaderCompileCommandLine - takes shader parameters used to compile with the DX11
 * compiler and returns an fxc command to compile from the command line
 */
static FString D3D11CreateShaderCompileCommandLine(
	const FString& ShaderPath, 
	const TCHAR* EntryFunction, 
	const TCHAR* ShaderProfile, 
	uint32 CompileFlags,
	FShaderCompilerOutput& Output
	)
{
	// fxc is our command line compiler
	FString FXCCommandline = FString(TEXT("\"%DXSDK_DIR%\\Utilities\\bin\\x86\\fxc\" ")) + ShaderPath;

	// add the entry point reference
	FXCCommandline += FString(TEXT(" /E ")) + EntryFunction;

	// @TODO - currently d3d11 uses d3d10 shader compiler flags... update when this changes in DXSDK
	// go through and add other switches
	if(CompileFlags & D3D10_SHADER_PREFER_FLOW_CONTROL)
	{
		CompileFlags &= ~D3D10_SHADER_PREFER_FLOW_CONTROL;
		FXCCommandline += FString(TEXT(" /Gfp"));
	}

	if(CompileFlags & D3D10_SHADER_DEBUG)
	{
		CompileFlags &= ~D3D10_SHADER_DEBUG;
		FXCCommandline += FString(TEXT(" /Zi"));
	}

	if(CompileFlags & D3D10_SHADER_SKIP_OPTIMIZATION)
	{
		CompileFlags &= ~D3D10_SHADER_SKIP_OPTIMIZATION;
		FXCCommandline += FString(TEXT(" /Od"));
	}

	if (CompileFlags & D3D10_SHADER_SKIP_VALIDATION)
	{
		CompileFlags &= ~D3D10_SHADER_SKIP_VALIDATION;
		FXCCommandline += FString(TEXT(" /Vd"));
	}

	if(CompileFlags & D3D10_SHADER_AVOID_FLOW_CONTROL)
	{
		CompileFlags &= ~D3D10_SHADER_AVOID_FLOW_CONTROL;
		FXCCommandline += FString(TEXT(" /Gfa"));
	}

	if(CompileFlags & D3D10_SHADER_PACK_MATRIX_ROW_MAJOR)
	{
		CompileFlags &= ~D3D10_SHADER_PACK_MATRIX_ROW_MAJOR;
		FXCCommandline += FString(TEXT(" /Zpr"));
	}

	if(CompileFlags & D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY)
	{
		CompileFlags &= ~D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY;
		FXCCommandline += FString(TEXT(" /Gec"));
	}

	switch (CompileFlags & SHADER_OPTIMIZATION_LEVEL_MASK)
	{
		case D3D10_SHADER_OPTIMIZATION_LEVEL2:
		CompileFlags &= ~D3D10_SHADER_OPTIMIZATION_LEVEL2;
		FXCCommandline += FString(TEXT(" /O2"));
			break;

		case D3D10_SHADER_OPTIMIZATION_LEVEL3:
		CompileFlags &= ~D3D10_SHADER_OPTIMIZATION_LEVEL3;
		FXCCommandline += FString(TEXT(" /O3"));
			break;

		case D3D10_SHADER_OPTIMIZATION_LEVEL1:
		CompileFlags &= ~D3D10_SHADER_OPTIMIZATION_LEVEL1;
		FXCCommandline += FString(TEXT(" /O1"));
			break;

		case D3D10_SHADER_OPTIMIZATION_LEVEL0:
			CompileFlags &= ~D3D10_SHADER_OPTIMIZATION_LEVEL0;
			break;

		default:
			Output.Errors.Emplace(TEXT("Unknown D3D10 optimization level"));
			break;
	}

	checkf(CompileFlags == 0, TEXT("Unhandled d3d11 shader compiler flag!"));

	// add the target instruction set
	FXCCommandline += FString(TEXT(" /T ")) + ShaderProfile;

	// Assembly instruction numbering
	FXCCommandline += TEXT(" /Ni");

	// Output to ShaderPath.d3dasm
	if (FPaths::GetExtension(ShaderPath) == TEXT("usf"))
	{
		FXCCommandline += FString::Printf(TEXT(" /Fc%sd3dasm"), *ShaderPath.LeftChop(3));
	}

	// add a pause on a newline
	FXCCommandline += FString(TEXT(" \r\n pause"));
	return FXCCommandline;
}

/** Creates a batch file string to call the AMD shader analyzer. */
static FString CreateAMDCodeXLCommandLine(
	const FString& ShaderPath, 
	const TCHAR* EntryFunction, 
	const TCHAR* ShaderProfile,
	uint32 DXFlags
	)
{
	// Hardcoded to the default install path since there's no Env variable or addition to PATH
	FString Commandline = FString(TEXT("\"C:\\Program Files (x86)\\AMD\\CodeXL\\CodeXLAnalyzer.exe\" -c Pitcairn")) 
		+ TEXT(" -f ") + EntryFunction
		+ TEXT(" -s HLSL")
		+ TEXT(" -p ") + ShaderProfile
		+ TEXT(" -a AnalyzerStats.csv")
		+ TEXT(" --isa ISA.txt")
		+ *FString::Printf(TEXT(" --DXFlags %u "), DXFlags)
		+ ShaderPath;

	// add a pause on a newline
	Commandline += FString(TEXT(" \r\n pause"));
	return Commandline;
}

// @return pointer to the D3DCompile function
pD3DCompile GetD3DCompileFunc(const FString& NewCompilerPath)
{
	static FString CurrentCompiler;
	static HMODULE CompilerDLL = 0;

	if(CurrentCompiler != *NewCompilerPath)
	{
		CurrentCompiler = *NewCompilerPath;

		if(CompilerDLL)
		{
			FreeLibrary(CompilerDLL);
			CompilerDLL = 0;
		}

		if(CurrentCompiler.Len())
		{
			CompilerDLL = LoadLibrary(*CurrentCompiler);
		}

		if(!CompilerDLL && NewCompilerPath.Len())
		{
			// Couldn't find HLSL compiler in specified path. We fail the first compile.
			return 0;
		}	
	}

	if(CompilerDLL)
	{
		// from custom folder e.g. "C:/DXWin8/D3DCompiler_44.dll"
		return (pD3DCompile)(void*)GetProcAddress(CompilerDLL, "D3DCompile");
	}

	// D3D SDK we compiled with (usually D3DCompiler_43.dll from windows folder)
	return &D3DCompile;
}

HRESULT D3DCompileWrapper(
	pD3DCompile				D3DCompileFunc,
	bool&					bException,
	LPCVOID					pSrcData,
	SIZE_T					SrcDataSize,
	LPCSTR					pFileName,
	CONST D3D_SHADER_MACRO*	pDefines,
	ID3DInclude*			pInclude,
	LPCSTR					pEntrypoint,
	LPCSTR					pTarget,
	uint32					Flags1,
	uint32					Flags2,
	ID3DBlob**				ppCode,
	ID3DBlob**				ppErrorMsgs
	)
{
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
	__try
#endif
	{
		return D3DCompileFunc(
			pSrcData,
			SrcDataSize,
			pFileName,
			pDefines,
			pInclude,
			pEntrypoint,
			pTarget,
			Flags1,
			Flags2,
			ppCode,
			ppErrorMsgs
		);
	}
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
	__except( EXCEPTION_EXECUTE_HANDLER )
	{
		bException = true;
		return E_FAIL;
	}
#endif
}

// Utility variable so we can place a breakpoint while debugging
static int32 GBreakpoint = 0;

// Generate the dumped usf file; call the D3D compiler, gather reflection information and generate the output data
static bool CompileAndProcessD3DShader(FString& PreprocessedShaderSource, const FString& CompilerPath,
	uint32 CompileFlags, const FShaderCompilerInput& Input, FString& EntryPointName,
	const TCHAR* ShaderProfile, bool bProcessingSecondTime, TArray<FString>& FilteredErrors, FShaderCompilerOutput& Output)
{
	auto AnsiSourceFile = StringCast<ANSICHAR>(*PreprocessedShaderSource);

	// Write out the preprocessed file and a batch file to compile it if requested (DumpDebugInfoPath is valid)
	if (Input.DumpDebugInfoPath.Len() > 0 && IFileManager::Get().DirectoryExists(*Input.DumpDebugInfoPath))
	{
		FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*(Input.DumpDebugInfoPath / Input.SourceFilename + TEXT(".usf")));
		if (FileWriter)
		{
			FileWriter->Serialize((ANSICHAR*)AnsiSourceFile.Get(), AnsiSourceFile.Length());
			FileWriter->Close();
			delete FileWriter;
		}

		const FString BatchFileContents = D3D11CreateShaderCompileCommandLine((Input.SourceFilename + TEXT(".usf")), *EntryPointName, ShaderProfile, CompileFlags, Output);
		FFileHelper::SaveStringToFile(BatchFileContents, *(Input.DumpDebugInfoPath / TEXT("CompileD3D.bat")));

		if (GD3DDumpAMDCodeXLFile)
		{
			const FString BatchFileContents2 = CreateAMDCodeXLCommandLine((Input.SourceFilename + TEXT(".usf")), *EntryPointName, ShaderProfile, CompileFlags);
			FFileHelper::SaveStringToFile(BatchFileContents2, *(Input.DumpDebugInfoPath / TEXT("CompileAMD.bat")));
		}

		if (Input.bGenerateDirectCompileFile)
		{
			FFileHelper::SaveStringToFile(CreateShaderCompilerWorkerDirectCommandLine(Input), *(Input.DumpDebugInfoPath / TEXT("DirectCompile.txt")));
		}
	}

	TRefCountPtr<ID3DBlob> Shader;
	TRefCountPtr<ID3DBlob> Errors;

	HRESULT Result;
	pD3DCompile D3DCompileFunc = GetD3DCompileFunc(CompilerPath);

	if (D3DCompileFunc)
	{
		bool bException = false;

		Result = D3DCompileWrapper(
			D3DCompileFunc,
			bException,
			AnsiSourceFile.Get(),
			AnsiSourceFile.Length(),
			TCHAR_TO_ANSI(*Input.SourceFilename),
			/*pDefines=*/ NULL,
			/*pInclude=*/ NULL,
			TCHAR_TO_ANSI(*EntryPointName),
			TCHAR_TO_ANSI(ShaderProfile),
			CompileFlags,
			0,
			Shader.GetInitReference(),
			Errors.GetInitReference()
			);

		if (bException)
		{
			FilteredErrors.Add(TEXT("D3DCompile exception"));
		}
	}
	else
	{
		FilteredErrors.Add(FString::Printf(TEXT("Couldn't find shader compiler: %s"), *CompilerPath));
		Result = E_FAIL;
	}

	// Filter any errors.
	void* ErrorBuffer = Errors ? Errors->GetBufferPointer() : NULL;
	if (ErrorBuffer)
	{
		D3D11FilterShaderCompileWarnings(ANSI_TO_TCHAR(ErrorBuffer), FilteredErrors);
	}

	// Fail the compilation if double operations are being used, since those are not supported on all D3D11 cards
	if (SUCCEEDED(Result))
	{
		if (GD3DCheckForDoubles || GD3DDumpD3DAsmFile)
		{
			TRefCountPtr<ID3DBlob> Dissasembly;
			if (SUCCEEDED(D3DDisassemble(Shader->GetBufferPointer(), Shader->GetBufferSize(), 0, "", Dissasembly.GetInitReference())))
			{
				ANSICHAR* DissasemblyString = new ANSICHAR[Dissasembly->GetBufferSize() + 1];
				FMemory::Memcpy(DissasemblyString, Dissasembly->GetBufferPointer(), Dissasembly->GetBufferSize());
				DissasemblyString[Dissasembly->GetBufferSize()] = 0;
				FString DissasemblyStringW(DissasemblyString);
				delete[] DissasemblyString;

				if (GD3DDumpD3DAsmFile)
				{
					FFileHelper::SaveStringToFile(DissasemblyStringW, *(Input.DumpDebugInfoPath / TEXT("Output.d3dasm")));
				}
				else if (GD3DCheckForDoubles)
				{
					// dcl_globalFlags will contain enableDoublePrecisionFloatOps when the shader uses doubles, even though the docs on dcl_globalFlags don't say anything about this
					if (DissasemblyStringW.Contains(TEXT("enableDoublePrecisionFloatOps")))
					{
						FilteredErrors.Add(TEXT("Shader uses double precision floats, which are not supported on all D3D11 hardware!"));
						return false;
					}
				}
			}
		}

		// Gather reflection information
		int32 NumInterpolants = 0;
		TIndirectArray<FString> InterpolantNames;
		TArray<FString> ShaderInputs;
		if (SUCCEEDED(Result))
		{
			Output.bSucceeded = true;

			ID3D11ShaderReflection* Reflector = NULL;
			Result = D3DReflect(Shader->GetBufferPointer(), Shader->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&Reflector);
			if (FAILED(Result))
			{
				UE_LOG(LogD3D11ShaderCompiler, Fatal, TEXT("D3DReflect failed: Result=%08x"), Result);
			}

			// Read the constant table description.
			D3D11_SHADER_DESC ShaderDesc;
			Reflector->GetDesc(&ShaderDesc);

			bool bGlobalUniformBufferUsed = false;
			uint32 NumSamplers = 0;
			uint32 NumSRVs = 0;
			uint32 NumCBs = 0;
			uint32 NumUAVs = 0;
			TArray<FString> UniformBufferNames;
			TArray<FString> ShaderOutputs;

			TBitArray<> UsedUniformBufferSlots;
			UsedUniformBufferSlots.Init(false, 32);

			if (Input.Target.Frequency == SF_Vertex)
			{
				for (uint32 Index = 0; Index < ShaderDesc.OutputParameters; ++Index)
				{
					D3D11_SIGNATURE_PARAMETER_DESC ParamDesc;
					Reflector->GetOutputParameterDesc(Index, &ParamDesc);
					if (ParamDesc.SystemValueType == D3D_NAME_UNDEFINED && ParamDesc.Mask != 0)
					{
						++NumInterpolants;
						new(InterpolantNames) FString(FString::Printf(TEXT("%s%d"), ANSI_TO_TCHAR(ParamDesc.SemanticName), ParamDesc.SemanticIndex));
						ShaderOutputs.Add(*InterpolantNames.Last());
					}
				}
			}
			else if (Input.Target.Frequency == SF_Pixel)
			{
				if (GD3DAllowRemoveUnused != 0 && Input.bCompilingForShaderPipeline)
				{
					// Handy place for a breakpoint for debugging...
					++GBreakpoint;
				}

				bool bFoundUnused = false;
				for (uint32 Index = 0; Index < ShaderDesc.InputParameters; ++Index)
				{
					D3D11_SIGNATURE_PARAMETER_DESC ParamDesc;
					Reflector->GetInputParameterDesc(Index, &ParamDesc);
					if (ParamDesc.SystemValueType == D3D_NAME_UNDEFINED)
					{
						if (ParamDesc.ReadWriteMask != 0)
						{
							FString SemanticName = ANSI_TO_TCHAR(ParamDesc.SemanticName);

							ShaderInputs.AddUnique(SemanticName);

							// Add the number (for the case of TEXCOORD)
							FString SemanticIndexName = FString::Printf(TEXT("%s%d"), *SemanticName, ParamDesc.SemanticIndex);
							ShaderInputs.AddUnique(SemanticIndexName);

							// Add _centroid
							ShaderInputs.AddUnique(SemanticName + TEXT("_centroid"));
							ShaderInputs.AddUnique(SemanticIndexName + TEXT("_centroid"));
						}
						else
						{
							bFoundUnused = true;
						}
					}
					else
					{
						//if (ParamDesc.ReadWriteMask != 0)
						{
							// Keep system values
							ShaderInputs.AddUnique(FString(ANSI_TO_TCHAR(ParamDesc.SemanticName)));
						}
					}
				}

				if (GD3DAllowRemoveUnused && Input.bCompilingForShaderPipeline && bFoundUnused && !bProcessingSecondTime)
				{
					// Rewrite the source removing the unused inputs so the bindings will match
					TArray<FString> RemoveErrors;
					if (RemoveUnusedInputs(PreprocessedShaderSource, ShaderInputs, EntryPointName, RemoveErrors))
					{
						return CompileAndProcessD3DShader(PreprocessedShaderSource, CompilerPath, CompileFlags, Input, EntryPointName, ShaderProfile, true, FilteredErrors, Output);
					}
					else
					{
						UE_LOG(LogD3D11ShaderCompiler, Warning, TEXT("Failed to Remove unused inputs [%s]!"), *Input.DumpDebugInfoPath);
						for (int32 Index = 0; Index < RemoveErrors.Num(); ++Index)
						{
							FShaderCompilerError NewError;
							NewError.StrippedErrorMessage = RemoveErrors[Index];
							Output.Errors.Add(NewError);
						}
						Output.bFailedRemovingUnused = true;
					}
				}
			}

			// Add parameters for shader resources (constant buffers, textures, samplers, etc. */
			for (uint32 ResourceIndex = 0; ResourceIndex < ShaderDesc.BoundResources; ResourceIndex++)
			{
				D3D11_SHADER_INPUT_BIND_DESC BindDesc;
				Reflector->GetResourceBindingDesc(ResourceIndex, &BindDesc);

				if (BindDesc.Type == D3D10_SIT_CBUFFER || BindDesc.Type == D3D10_SIT_TBUFFER)
				{
					const uint32 CBIndex = BindDesc.BindPoint;
					ID3D11ShaderReflectionConstantBuffer* ConstantBuffer = Reflector->GetConstantBufferByName(BindDesc.Name);
					D3D11_SHADER_BUFFER_DESC CBDesc;
					ConstantBuffer->GetDesc(&CBDesc);
					bool bGlobalCB = (FCStringAnsi::Strcmp(CBDesc.Name, "$Globals") == 0);

					if (bGlobalCB)
					{
						// Track all of the variables in this constant buffer.
						for (uint32 ConstantIndex = 0; ConstantIndex < CBDesc.Variables; ConstantIndex++)
						{
							ID3D11ShaderReflectionVariable* Variable = ConstantBuffer->GetVariableByIndex(ConstantIndex);
							D3D11_SHADER_VARIABLE_DESC VariableDesc;
							Variable->GetDesc(&VariableDesc);
							if (VariableDesc.uFlags & D3D10_SVF_USED)
							{
								bGlobalUniformBufferUsed = true;

								Output.ParameterMap.AddParameterAllocation(
									ANSI_TO_TCHAR(VariableDesc.Name),
									CBIndex,
									VariableDesc.StartOffset,
									VariableDesc.Size
									);
								UsedUniformBufferSlots[CBIndex] = true;
							}
						}
					}
					else
					{
						// Track just the constant buffer itself.
						Output.ParameterMap.AddParameterAllocation(
							ANSI_TO_TCHAR(CBDesc.Name),
							CBIndex,
							0,
							0
							);
						UsedUniformBufferSlots[CBIndex] = true;

						if (UniformBufferNames.Num() <= (int32)CBIndex)
						{
							UniformBufferNames.AddDefaulted(CBIndex - UniformBufferNames.Num() + 1);
						}
						UniformBufferNames[CBIndex] = CBDesc.Name;
					}

					NumCBs = FMath::Max(NumCBs, BindDesc.BindPoint + BindDesc.BindCount);
				}
				else if (BindDesc.Type == D3D10_SIT_TEXTURE || BindDesc.Type == D3D10_SIT_SAMPLER)
				{
					TCHAR OfficialName[1024];
					uint32 BindCount = BindDesc.BindCount;
					FCString::Strcpy(OfficialName, ANSI_TO_TCHAR(BindDesc.Name));

					if (Input.Target.Platform == SP_PCD3D_SM5)
					{
						BindCount = 1;

						// Assign the name and optionally strip any "[#]" suffixes
						TCHAR *BracketLocation = FCString::Strchr(OfficialName, TEXT('['));
						if (BracketLocation)
						{
							*BracketLocation = 0;

							const int32 NumCharactersBeforeArray = BracketLocation - OfficialName;

							// In SM5, for some reason, array suffixes are included in Name, i.e. "LightMapTextures[0]", rather than "LightMapTextures"
							// Additionally elements in an array are listed as SEPERATE bound resources.
							// However, they are always contiguous in resource index, so iterate over the samplers and textures of the initial association
							// and count them, identifying the bindpoint and bindcounts

							while (ResourceIndex + 1 < ShaderDesc.BoundResources)
							{
								D3D11_SHADER_INPUT_BIND_DESC BindDesc2;
								Reflector->GetResourceBindingDesc(ResourceIndex + 1, &BindDesc2);

								if (BindDesc2.Type == BindDesc.Type && FCStringAnsi::Strncmp(BindDesc2.Name, BindDesc.Name, NumCharactersBeforeArray) == 0)
								{
									BindCount++;
									// Skip over this resource since it is part of an array
									ResourceIndex++;
								}
								else
								{
									break;
								}
							}
						}
					}

					if (BindDesc.Type == D3D10_SIT_SAMPLER)
					{
						NumSamplers = FMath::Max(NumSamplers, BindDesc.BindPoint + BindDesc.BindCount);
					}
					else if (BindDesc.Type == D3D10_SIT_TEXTURE)
					{
						NumSRVs = FMath::Max(NumSRVs, BindDesc.BindPoint + BindDesc.BindCount);
					}

					// Add a parameter for the texture only, the sampler index will be invalid
					Output.ParameterMap.AddParameterAllocation(
						OfficialName,
						0,
						BindDesc.BindPoint,
						BindCount
						);
				}
				else if (BindDesc.Type == D3D11_SIT_UAV_RWTYPED || BindDesc.Type == D3D11_SIT_UAV_RWSTRUCTURED ||
					BindDesc.Type == D3D11_SIT_UAV_RWBYTEADDRESS || BindDesc.Type == D3D11_SIT_UAV_RWSTRUCTURED_WITH_COUNTER ||
					BindDesc.Type == D3D11_SIT_UAV_APPEND_STRUCTURED)
				{
					TCHAR OfficialName[1024];
					FCString::Strcpy(OfficialName, ANSI_TO_TCHAR(BindDesc.Name));

					Output.ParameterMap.AddParameterAllocation(
						OfficialName,
						0,
						BindDesc.BindPoint,
						1
						);

					NumUAVs = FMath::Max(NumUAVs, BindDesc.BindPoint + BindDesc.BindCount);
				}
				else if (BindDesc.Type == D3D11_SIT_STRUCTURED || BindDesc.Type == D3D11_SIT_BYTEADDRESS)
				{
					TCHAR OfficialName[1024];
					FCString::Strcpy(OfficialName, ANSI_TO_TCHAR(BindDesc.Name));

					Output.ParameterMap.AddParameterAllocation(
						OfficialName,
						0,
						BindDesc.BindPoint,
						1
						);

					NumSRVs = FMath::Max(NumSRVs, BindDesc.BindPoint + BindDesc.BindCount);
				}
			}

			TRefCountPtr<ID3DBlob> CompressedData;

			if (Input.Environment.CompilerFlags.Contains(CFLAG_KeepDebugInfo))
			{
				CompressedData = Shader;
			}
			else
			{
				// Strip shader reflection and debug info
				D3D_SHADER_DATA ShaderData;
				ShaderData.pBytecode = Shader->GetBufferPointer();
				ShaderData.BytecodeLength = Shader->GetBufferSize();
				Result = D3DStripShader(Shader->GetBufferPointer(),
					Shader->GetBufferSize(),
					D3DCOMPILER_STRIP_REFLECTION_DATA | D3DCOMPILER_STRIP_DEBUG_INFO | D3DCOMPILER_STRIP_TEST_BLOBS,
					CompressedData.GetInitReference());

				if (FAILED(Result))
				{
					UE_LOG(LogD3D11ShaderCompiler, Fatal, TEXT("D3DStripShader failed: Result=%08x"), Result);
				}
			}

			// Build the SRT for this shader.
			FD3D11ShaderResourceTable SRT;

			TArray<uint8> UniformBufferNameBytes;

			{
				// Build the generic SRT for this shader.
				FShaderCompilerResourceTable GenericSRT;
				BuildResourceTableMapping(Input.Environment.ResourceTableMap, Input.Environment.ResourceTableLayoutHashes, UsedUniformBufferSlots, Output.ParameterMap, GenericSRT);

				if (UniformBufferNames.Num() < GenericSRT.ResourceTableLayoutHashes.Num())
				{
					UniformBufferNames.AddDefaulted(GenericSRT.ResourceTableLayoutHashes.Num() - UniformBufferNames.Num() + 1);
				}

				for (int32 Index = 0; Index < GenericSRT.ResourceTableLayoutHashes.Num(); ++Index)
				{
					if (GenericSRT.ResourceTableLayoutHashes[Index] != 0 && UniformBufferNames[Index].Len() == 0)
					{
						auto* Name = Input.Environment.ResourceTableLayoutHashes.FindKey(GenericSRT.ResourceTableLayoutHashes[Index]);
						check(Name);
						UniformBufferNames[Index] = *Name;
					}
				}

				FMemoryWriter UniformBufferNameWriter(UniformBufferNameBytes);
				UniformBufferNameWriter << UniformBufferNames;

				// Copy over the bits indicating which resource tables are active.
				SRT.ResourceTableBits = GenericSRT.ResourceTableBits;

				SRT.ResourceTableLayoutHashes = GenericSRT.ResourceTableLayoutHashes;

				// Now build our token streams.
				BuildResourceTableTokenStream(GenericSRT.TextureMap, GenericSRT.MaxBoundResourceTable, SRT.TextureMap);
				BuildResourceTableTokenStream(GenericSRT.ShaderResourceViewMap, GenericSRT.MaxBoundResourceTable, SRT.ShaderResourceViewMap);
				BuildResourceTableTokenStream(GenericSRT.SamplerMap, GenericSRT.MaxBoundResourceTable, SRT.SamplerMap);
				BuildResourceTableTokenStream(GenericSRT.UnorderedAccessViewMap, GenericSRT.MaxBoundResourceTable, SRT.UnorderedAccessViewMap);
			}

			if (GD3DAllowRemoveUnused != 0 && Input.Target.Frequency == SF_Pixel && Input.bCompilingForShaderPipeline && bProcessingSecondTime)
			{
				Output.bSupportsQueryingUsedAttributes = true;
				if (GD3DAllowRemoveUnused == 1)
				{
					Output.UsedAttributes = ShaderInputs;
				}
			}

			// Generate the final Output
			FMemoryWriter Ar(Output.ShaderCode.GetWriteAccess(), true);
			Ar << SRT;
			Ar.Serialize(CompressedData->GetBufferPointer(), CompressedData->GetBufferSize());

			// append data that is generate from the shader code and assist the usage, mostly needed for DX12 
			{
				FShaderCodePackedResourceCounts PackedResourceCounts = { bGlobalUniformBufferUsed, NumSamplers, NumSRVs, NumCBs, NumUAVs };

				Output.ShaderCode.AddOptionalData(PackedResourceCounts);
				Output.ShaderCode.AddOptionalData('u', UniformBufferNameBytes.GetData(), UniformBufferNameBytes.Num());
			}

			// store data we can pickup later with ShaderCode.FindOptionalData('n'), could be removed for shipping
			// Daniel L: This GenerateShaderName does not generate a deterministic output among shaders as the shader code can be shared. 
			//			uncommenting this will cause the project to have non deterministic materials and will hurt patch sizes
			//Output.ShaderCode.AddOptionalData('n', TCHAR_TO_UTF8(*Input.GenerateShaderName()));

			// Set the number of instructions.
			Output.NumInstructions = ShaderDesc.InstructionCount;

			Output.NumTextureSamplers = NumSamplers;

			// Reflector is a com interface, so it needs to be released.
			Reflector->Release();

			// Pass the target through to the output.
			Output.Target = Input.Target;
		}

		if (Input.Target.Platform == SP_PCD3D_ES2)
		{
			if (Output.NumTextureSamplers > 16)
			{
				FilteredErrors.Add(FString::Printf(TEXT("Shader uses more than 8 texture samplers which is not supported by ES2!  Used: %u"), Output.NumTextureSamplers));
				Result = E_FAIL;
				Output.bSucceeded = false;
			}
			// Disabled for now while we work out some issues with it. A compiler bug is causing 
			// Landscape to require a 9th interpolant even though the pixel shader never reads from
			// it. Search for LANDSCAPE_BUG_WORKAROUND.
			else if (false && NumInterpolants > 8)
			{
				FString InterpolantsStr;
				for (int32 i = 0; i < InterpolantNames.Num(); ++i)
				{
					InterpolantsStr += FString::Printf(TEXT("\n\t%s"), *InterpolantNames[i]);
				}
				FilteredErrors.Add(FString::Printf(TEXT("Shader uses more than 8 interpolants which is not supported by ES2!  Used: %u%s"), NumInterpolants, *InterpolantsStr));
				Result = E_FAIL;
				Output.bSucceeded = false;
			}
		}
	}
	else
	{
		++GBreakpoint;
	}

	return SUCCEEDED(Result);
}

void CompileD3D11Shader(const FShaderCompilerInput& Input,FShaderCompilerOutput& Output, FShaderCompilerDefinitions& AdditionalDefines, const FString& WorkingDirectory)
{
	FString PreprocessedShaderSource;
	FString CompilerPath;
	const TCHAR* ShaderProfile = GetShaderProfileName(Input.Target);

	if(!ShaderProfile)
	{
		Output.Errors.Add(FShaderCompilerError(TEXT("Unrecognized shader frequency")));
		return;
	}

	// Set additional defines.
	AdditionalDefines.SetDefine(TEXT("COMPILER_HLSL"), 1);

	if (Input.bSkipPreprocessedCache)
	{
		FFileHelper::LoadFileToString(PreprocessedShaderSource, *Input.SourceFilename);
	}
	else
	{
		// Preprocess the shader.
		if (PreprocessShader(PreprocessedShaderSource, Output, Input, AdditionalDefines) != true)
		{
			// The preprocessing stage will add any relevant errors.
			return;
		}
	}

	GD3DAllowRemoveUnused = Input.Environment.CompilerFlags.Contains(CFLAG_ForceRemoveUnusedInterpolators) ? 1 : 0;

	FString EntryPointName = Input.EntryPointName;

	Output.bFailedRemovingUnused = false;
	if (GD3DAllowRemoveUnused == 1 && Input.Target.Frequency == SF_Vertex && Input.bCompilingForShaderPipeline)
	{
		// Always add SV_Position
		TArray<FString> UsedOutputs = Input.UsedOutputs;
		UsedOutputs.AddUnique(TEXT("SV_POSITION"));

		// We can't remove any of the output-only system semantics
		//@todo - there are a bunch of tessellation ones as well
		TArray<FString> Exceptions;
		Exceptions.AddUnique(TEXT("SV_ClipDistance"));
		Exceptions.AddUnique(TEXT("SV_ClipDistance0"));
		Exceptions.AddUnique(TEXT("SV_ClipDistance1"));
		Exceptions.AddUnique(TEXT("SV_ClipDistance2"));
		Exceptions.AddUnique(TEXT("SV_ClipDistance3"));
		Exceptions.AddUnique(TEXT("SV_ClipDistance4"));
		Exceptions.AddUnique(TEXT("SV_ClipDistance5"));
		Exceptions.AddUnique(TEXT("SV_ClipDistance6"));
		Exceptions.AddUnique(TEXT("SV_ClipDistance7"));

		Exceptions.AddUnique(TEXT("SV_CullDistance"));
		Exceptions.AddUnique(TEXT("SV_CullDistance0"));
		Exceptions.AddUnique(TEXT("SV_CullDistance1"));
		Exceptions.AddUnique(TEXT("SV_CullDistance2"));
		Exceptions.AddUnique(TEXT("SV_CullDistance3"));
		Exceptions.AddUnique(TEXT("SV_CullDistance4"));
		Exceptions.AddUnique(TEXT("SV_CullDistance5"));
		Exceptions.AddUnique(TEXT("SV_CullDistance6"));
		Exceptions.AddUnique(TEXT("SV_CullDistance7"));
		
		TArray<FString> Errors;
		if (!RemoveUnusedOutputs(PreprocessedShaderSource, UsedOutputs, Exceptions, EntryPointName, Errors))
		{
			UE_LOG(LogD3D11ShaderCompiler, Warning, TEXT("Failed to Remove unused outputs [%s]!"), *Input.DumpDebugInfoPath);
			for (int32 Index = 0; Index < Errors.Num(); ++Index)
			{
				FShaderCompilerError NewError;
				NewError.StrippedErrorMessage = Errors[Index];
				Output.Errors.Add(NewError);
			}
			Output.bFailedRemovingUnused = true;
		}
	}

	if (!RemoveUniformBuffersFromSource(PreprocessedShaderSource))
	{
		return;
	}

	// Override default compiler path to newer dll
	CompilerPath = FPaths::EngineDir();
#if !PLATFORM_64BITS
	CompilerPath.Append(TEXT("Binaries/ThirdParty/Windows/DirectX/x86/d3dcompiler_47.dll"));
#else
	CompilerPath.Append(TEXT("Binaries/ThirdParty/Windows/DirectX/x64/d3dcompiler_47.dll"));
#endif

	// @TODO - currently d3d11 uses d3d10 shader compiler flags... update when this changes in DXSDK
	// @TODO - implement different material path to allow us to remove backwards compat flag on sm5 shaders
	uint32 CompileFlags = D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY
		// Unpack uniform matrices as row-major to match the CPU layout.
		| D3D10_SHADER_PACK_MATRIX_ROW_MAJOR;

	if (DEBUG_SHADERS || Input.Environment.CompilerFlags.Contains(CFLAG_Debug)) 
	{
		//add the debug flags
		CompileFlags |= D3D10_SHADER_DEBUG | D3D10_SHADER_SKIP_OPTIMIZATION;
	}
	else
	{
		if (Input.Environment.CompilerFlags.Contains(CFLAG_StandardOptimization))
		{
			CompileFlags |= D3D10_SHADER_OPTIMIZATION_LEVEL1;
		}
		else
		{
			CompileFlags |= D3D10_SHADER_OPTIMIZATION_LEVEL3;
		}
	}

	for (int32 FlagIndex = 0; FlagIndex < Input.Environment.CompilerFlags.Num(); FlagIndex++)
	{
		//accumulate flags set by the shader
		CompileFlags |= TranslateCompilerFlagD3D11((ECompilerFlags)Input.Environment.CompilerFlags[FlagIndex]);
	}

	TArray<FString> FilteredErrors;
	if (!CompileAndProcessD3DShader(PreprocessedShaderSource, CompilerPath, CompileFlags, Input, EntryPointName, ShaderProfile, false, FilteredErrors, Output))
	{
		if (!FilteredErrors.Num())
		{
			FilteredErrors.Add(TEXT("Compile Failed without errors!"));
		}
	}

	// Process errors
	for (int32 ErrorIndex = 0; ErrorIndex < FilteredErrors.Num(); ErrorIndex++)
	{
		const FString& CurrentError = FilteredErrors[ErrorIndex];
		FShaderCompilerError NewError;
		// Extract the filename and line number from the shader compiler error message for PC whose format is:
		// "d:\UE4\Binaries\BasePassPixelShader(30,7): error X3000: invalid target or usage string"
		int32 FirstParenIndex = CurrentError.Find(TEXT("("));
		int32 LastParenIndex = CurrentError.Find(TEXT("):"));
		if (FirstParenIndex != INDEX_NONE 
			&& LastParenIndex != INDEX_NONE
			&& LastParenIndex > FirstParenIndex)
		{
			FString ErrorFileAndPath = CurrentError.Left(FirstParenIndex);
			if (FPaths::GetExtension(ErrorFileAndPath).ToUpper() == TEXT("USF"))
			{
				NewError.ErrorFile = FPaths::GetCleanFilename(ErrorFileAndPath);
			}
			else
			{
				NewError.ErrorFile = FPaths::GetCleanFilename(ErrorFileAndPath) + TEXT(".usf");
			}

			NewError.ErrorLineString = CurrentError.Mid(FirstParenIndex + 1, LastParenIndex - FirstParenIndex - FCString::Strlen(TEXT("(")));
			NewError.StrippedErrorMessage = CurrentError.Right(CurrentError.Len() - LastParenIndex - FCString::Strlen(TEXT("):")));
		}
		else
		{
			NewError.StrippedErrorMessage = CurrentError;
		}
		Output.Errors.Add(NewError);
	}
}

void CompileShader_Windows_SM5(const FShaderCompilerInput& Input,FShaderCompilerOutput& Output,const FString& WorkingDirectory)
{
	check(Input.Target.Platform == SP_PCD3D_SM5);

	FShaderCompilerDefinitions AdditionalDefines;
	AdditionalDefines.SetDefine(TEXT("SM5_PROFILE"), 1);
	CompileD3D11Shader(Input, Output, AdditionalDefines, WorkingDirectory);
}

void CompileShader_Windows_SM4(const FShaderCompilerInput& Input,FShaderCompilerOutput& Output,const FString& WorkingDirectory)
{
	check(Input.Target.Platform == SP_PCD3D_SM4);

	FShaderCompilerDefinitions AdditionalDefines;
	AdditionalDefines.SetDefine(TEXT("SM4_PROFILE"), 1);
	CompileD3D11Shader(Input, Output, AdditionalDefines, WorkingDirectory);
}

void CompileShader_Windows_ES2(const FShaderCompilerInput& Input,FShaderCompilerOutput& Output,const FString& WorkingDirectory)
{
	check(Input.Target.Platform == SP_PCD3D_ES2);

	FShaderCompilerDefinitions AdditionalDefines;
	AdditionalDefines.SetDefine(TEXT("ES2_PROFILE"), 1);
	CompileD3D11Shader(Input, Output, AdditionalDefines, WorkingDirectory);
}

void CompileShader_Windows_ES3_1(const FShaderCompilerInput& Input, FShaderCompilerOutput& Output, const FString& WorkingDirectory)
{
	check(Input.Target.Platform == SP_PCD3D_ES3_1);

	FShaderCompilerDefinitions AdditionalDefines;
	AdditionalDefines.SetDefine(TEXT("ES3_1_PROFILE"), 1);
	CompileD3D11Shader(Input, Output, AdditionalDefines, WorkingDirectory);
}

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ShaderFormatD3D.h"
#include "ShaderPreprocessor.h"
#include "ShaderCompilerCommon.h"
#include "D3D11ShaderResources.h"

DEFINE_LOG_CATEGORY_STATIC(LogD3D11ShaderCompiler, Log, All);

#define DEBUG_SHADERS 0

// D3D headers.
#define D3D_OVERLOADS 1

// Disable macro redefinition warning for compatibility with Windows SDK 8+
#pragma warning(push)
#if _MSC_VER >= 1700
#pragma warning(disable : 4005)	// macro redefinition
#endif

#include "AllowWindowsPlatformTypes.h"
	#include <D3D11.h>
	#include <D3Dcompiler.h>
	#include <d3d11Shader.h>
#include "HideWindowsPlatformTypes.h"
#undef DrawText

#pragma warning(pop)

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
	uint32 CompileFlags
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

	if(CompileFlags & D3D10_SHADER_OPTIMIZATION_LEVEL1)
	{
		CompileFlags &= ~D3D10_SHADER_OPTIMIZATION_LEVEL1;
		FXCCommandline += FString(TEXT(" /O1"));
	}

	if(CompileFlags & D3D10_SHADER_OPTIMIZATION_LEVEL3)
	{
		CompileFlags &= ~D3D10_SHADER_OPTIMIZATION_LEVEL3;
		FXCCommandline += FString(TEXT(" /O3"));
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

	// Preprocess the shader.
	if (PreprocessShader(PreprocessedShaderSource, Output, Input, AdditionalDefines) != true)
	{
		// The preprocessing stage will add any relevant errors.
		return;
	}

	if (!RemoveUniformBuffersFromSource(PreprocessedShaderSource))
	{
		return;
	}

	// Search definitions for a custom D3D compiler path.
	for(TMap<FString,FString>::TConstIterator DefinitionIt(Input.Environment.GetDefinitions());DefinitionIt;++DefinitionIt)
	{
		const FString& Name = DefinitionIt.Key();
		const FString& Definition = DefinitionIt.Value();

		if(Name == TEXT("D3DCOMPILER_PATH"))
		{
			CompilerPath = Definition;
		}
	}

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

	for(int32 FlagIndex = 0;FlagIndex < Input.Environment.CompilerFlags.Num();FlagIndex++)
	{
		//accumulate flags set by the shader
		CompileFlags |= TranslateCompilerFlagD3D11((ECompilerFlags)Input.Environment.CompilerFlags[FlagIndex]);
	}

	auto AnsiSourceFile = StringCast<ANSICHAR>(*PreprocessedShaderSource);
	TArray<FString> FilteredErrors;
	{
		TRefCountPtr<ID3DBlob> Shader;
		TRefCountPtr<ID3DBlob> Errors;

		HRESULT Result;
		pD3DCompile D3DCompileFunc = GetD3DCompileFunc(CompilerPath);
			
		if(D3DCompileFunc)
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
				TCHAR_TO_ANSI(*Input.EntryPointName),
				TCHAR_TO_ANSI(ShaderProfile),
				CompileFlags,
				0,
				Shader.GetInitReference(),
				Errors.GetInitReference()
			);

			if( bException )
			{
				FilteredErrors.Add( TEXT("D3DCompile exception") );
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
			TRefCountPtr<ID3DBlob> Dissasembly;

			if (SUCCEEDED(D3DDisassemble(Shader->GetBufferPointer(), Shader->GetBufferSize(), 0, "", Dissasembly.GetInitReference())))
			{
				ANSICHAR* DissasemblyString = new ANSICHAR[Dissasembly->GetBufferSize() + 1];
				FMemory::Memcpy(DissasemblyString, Dissasembly->GetBufferPointer(), Dissasembly->GetBufferSize());
				DissasemblyString[Dissasembly->GetBufferSize()] = 0;
				FString DissasemblyStringW(DissasemblyString);
				delete [] DissasemblyString;

				// dcl_globalFlags will contain enableDoublePrecisionFloatOps when the shader uses doubles, even though the docs on dcl_globalFlags don't say anything about this
				if (DissasemblyStringW.Contains(TEXT("enableDoublePrecisionFloatOps")))
				{
					FilteredErrors.Add(TEXT("Shader uses double precision floats, which are not supported on all D3D11 hardware!"));
					Result = E_FAIL;
				}
			}
		}

		// Write out the preprocessed file and a batch file to compile it if requested (DumpDebugInfoPath is valid)
		if (Input.DumpDebugInfoPath != TEXT("") 
			&& IFileManager::Get().DirectoryExists(*Input.DumpDebugInfoPath))
		{
			FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*(Input.DumpDebugInfoPath / Input.SourceFilename + TEXT(".usf")));

			if (FileWriter)
			{
				FileWriter->Serialize((ANSICHAR*)AnsiSourceFile.Get(), AnsiSourceFile.Length());
				FileWriter->Close();
				delete FileWriter;
			}

			const FString BatchFileContents = D3D11CreateShaderCompileCommandLine((Input.SourceFilename + TEXT(".usf")), *Input.EntryPointName, ShaderProfile, CompileFlags);
			FFileHelper::SaveStringToFile(BatchFileContents, *(Input.DumpDebugInfoPath / TEXT("CompileD3D.bat")));

			const FString BatchFileContents2 = CreateAMDCodeXLCommandLine((Input.SourceFilename + TEXT(".usf")), *Input.EntryPointName, ShaderProfile, CompileFlags);
			FFileHelper::SaveStringToFile(BatchFileContents2, *(Input.DumpDebugInfoPath / TEXT("CompileAMD.bat")));
		}

		int32 NumInterpolants = 0;
		TIndirectArray<FString> InterpolantNames;
		if (SUCCEEDED(Result))
		{
			Output.bSucceeded = true;

			ID3D11ShaderReflection* Reflector = NULL;
			Result = D3DReflect( Shader->GetBufferPointer(), Shader->GetBufferSize(), IID_ID3D11ShaderReflection, (void**) &Reflector );
			if(FAILED(Result))
			{
				UE_LOG(LogD3D11ShaderCompiler, Fatal,TEXT("D3DReflect failed: Result=%08x"),Result);
			}

			// Read the constant table description.
			D3D11_SHADER_DESC ShaderDesc;
			Reflector->GetDesc(&ShaderDesc);

			bool bGlobalUniformBufferUsed = false;
			uint32 NumSamplers = 0;

			TBitArray<> UsedUniformBufferSlots;
			UsedUniformBufferSlots.Init(false,32);

			if (Input.Target.Frequency == SF_Vertex)
			{
				for (uint32 Index = 0; Index < ShaderDesc.OutputParameters; ++Index)
				{
					D3D11_SIGNATURE_PARAMETER_DESC ParamDesc;
					Reflector->GetOutputParameterDesc(Index, &ParamDesc);
					if (ParamDesc.SystemValueType == D3D_NAME_UNDEFINED && ParamDesc.Mask != 0)
					{
						++NumInterpolants;
						new(InterpolantNames) FString(FString::Printf(TEXT("%s%d"),ANSI_TO_TCHAR(ParamDesc.SemanticName),ParamDesc.SemanticIndex));
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
					}
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
						NumSamplers += BindCount;
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
					UE_LOG(LogD3D11ShaderCompiler, Fatal,TEXT("D3DStripShader failed: Result=%08x"),Result);
				}
			}
			
			// Build the SRT for this shader.
			FD3D11ShaderResourceTable SRT;
			{
				// Build the generic SRT for this shader.
				FShaderResourceTable GenericSRT;
				BuildResourceTableMapping(Input.Environment.ResourceTableMap, Input.Environment.ResourceTableLayoutHashes, UsedUniformBufferSlots, Output.ParameterMap, GenericSRT);

				// At runtime textures are just SRVs, so combine them for the purposes of building token streams.
				GenericSRT.ShaderResourceViewMap.Append(GenericSRT.TextureMap);

				// Copy over the bits indicating which resource tables are active.
				SRT.ResourceTableBits = GenericSRT.ResourceTableBits;

				SRT.ResourceTableLayoutHashes = GenericSRT.ResourceTableLayoutHashes;

				// Now build our token streams.
				BuildResourceTableTokenStream(GenericSRT.ShaderResourceViewMap, GenericSRT.MaxBoundResourceTable, SRT.ShaderResourceViewMap);
				BuildResourceTableTokenStream(GenericSRT.SamplerMap, GenericSRT.MaxBoundResourceTable, SRT.SamplerMap);
				BuildResourceTableTokenStream(GenericSRT.UnorderedAccessViewMap, GenericSRT.MaxBoundResourceTable, SRT.UnorderedAccessViewMap);

			}

			FMemoryWriter Ar( Output.Code, true );
			Ar << SRT;
			Ar.Serialize( CompressedData->GetBufferPointer(), CompressedData->GetBufferSize() );

			// Pack bGlobalUniformBufferUsed in the last byte
			Output.Code.Add( bGlobalUniformBufferUsed );

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
			if (Output.NumTextureSamplers > 8)
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
					InterpolantsStr += FString::Printf(TEXT("\n\t%s"),*InterpolantNames[i]);
				}
				FilteredErrors.Add(FString::Printf(TEXT("Shader uses more than 8 interpolants which is not supported by ES2!  Used: %u%s"), NumInterpolants, *InterpolantsStr));
				Result = E_FAIL;
				Output.bSucceeded = false;
			}
		}

		if(FAILED(Result) && !FilteredErrors.Num())
		{
			FilteredErrors.Add(TEXT("Compile Failed without errors!"));
		}

		for (int32 ErrorIndex = 0; ErrorIndex < FilteredErrors.Num(); ErrorIndex++)
		{
			const FString& CurrentError = FilteredErrors[ErrorIndex];
			FShaderCompilerError NewError;
			// Extract the filename and line number from the shader compiler error message for PC whose format is:
			// "d:\UnrealEngine3\Binaries\BasePassPixelShader(30,7): error X3000: invalid target or usage string"
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

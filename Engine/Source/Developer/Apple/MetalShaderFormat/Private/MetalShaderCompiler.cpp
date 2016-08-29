// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// ..

#include "Core.h"
#include "MetalShaderFormat.h"
#include "ShaderCore.h"
#include "MetalShaderResources.h"
#include "ShaderCompilerCommon.h"

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
	#include "Windows/PreWindowsApi.h"
	#include <objbase.h>
	#include <assert.h>
	#include <stdio.h>
	#include "Windows/PostWindowsApi.h"
	#include "Windows/MinWindows.h"
#include "HideWindowsPlatformTypes.h"
#endif
#include "ShaderPreprocessor.h"
#include "hlslcc.h"
#include "MetalBackend.h"

DEFINE_LOG_CATEGORY_STATIC(LogMetalShaderCompiler, Log, All); 

/*------------------------------------------------------------------------------
	Shader compiling.
------------------------------------------------------------------------------*/

static inline uint32 ParseNumber(const TCHAR* Str)
{
	uint32 Num = 0;
	while (*Str && *Str >= '0' && *Str <= '9')
	{
		Num = Num * 10 + *Str++ - '0';
	}
	return Num;
}

static inline uint32 ParseNumber(const ANSICHAR* Str)
{
	uint32 Num = 0;
	while (*Str && *Str >= '0' && *Str <= '9')
	{
		Num = Num * 10 + *Str++ - '0';
	}
	return Num;
}

/**
 * Construct the final microcode from the compiled and verified shader source.
 * @param ShaderOutput - Where to store the microcode and parameter map.
 * @param InShaderSource - Metal source with input/output signature.
 * @param SourceLen - The length of the Metal source code.
 */
static void BuildMetalShaderOutput(
	FShaderCompilerOutput& ShaderOutput,
	const FShaderCompilerInput& ShaderInput, 
	const ANSICHAR* InShaderSource,
	int32 SourceLen,
	TCHAR const* Standard,
	TArray<FShaderCompilerError>& OutErrors
	)
{
	const ANSICHAR* USFSource = InShaderSource;
	CrossCompiler::FHlslccHeader CCHeader;
	if (!CCHeader.Read(USFSource, SourceLen))
	{
		UE_LOG(LogMetalShaderCompiler, Fatal, TEXT("Bad hlslcc header found"));
	}
	
	const ANSICHAR* SideTableString = FCStringAnsi::Strstr(USFSource, "@SideTable: ");

	FMetalCodeHeader Header = {0};
	Header.bFastMath = !ShaderInput.Environment.CompilerFlags.Contains(CFLAG_NoFastMath);
	Header.SideTable = -1;
	if (SideTableString)
	{
		int32 SideTableLoc = -1;
		while (*SideTableString && *SideTableString != '\n')
		{
			if (*SideTableString == '(')
			{
				SideTableString++;
				if (*SideTableString && *SideTableString != '\n')
				{
					SideTableLoc = (int32)ParseNumber(SideTableString);
				}
			}
			else
			{
				SideTableString++;
			}
		}
		if (SideTableLoc >= 0)
		{
			Header.SideTable = SideTableLoc;
		}
		else
		{
			UE_LOG(LogMetalShaderCompiler, Fatal, TEXT("Couldn't parse the SideTable buffer index for bounds checking"));
		}
	}
	
	FShaderParameterMap& ParameterMap = ShaderOutput.ParameterMap;
	EShaderFrequency Frequency = (EShaderFrequency)ShaderOutput.Target.Frequency;

	TBitArray<> UsedUniformBufferSlots;
	UsedUniformBufferSlots.Init(false,32);

	// Write out the magic markers.
	Header.Frequency = Frequency;

	// Only inputs for vertex shaders must be tracked.
	if (Frequency == SF_Vertex)
	{
		static const FString AttributePrefix = TEXT("in_ATTRIBUTE");
		for (auto& Input : CCHeader.Inputs)
		{
			// Only process attributes.
			if (Input.Name.StartsWith(AttributePrefix))
			{
				uint8 AttributeIndex = ParseNumber(*Input.Name + AttributePrefix.Len());
				Header.Bindings.InOutMask |= (1 << AttributeIndex);
			}
		}
	}

	// Then the list of outputs.
	static const FString TargetPrefix = "out_Target";
	static const FString GL_FragDepth = "FragDepth";
	// Only outputs for pixel shaders must be tracked.
	if (Frequency == SF_Pixel)
	{
		for (auto& Output : CCHeader.Outputs)
		{
			// Handle targets.
			if (Output.Name.StartsWith(TargetPrefix))
			{
				uint8 TargetIndex = ParseNumber(*Output.Name + TargetPrefix.Len());
				Header.Bindings.InOutMask |= (1 << TargetIndex);
			}
			// Handle depth writes.
			else if (Output.Name.Equals(GL_FragDepth))
			{
				Header.Bindings.InOutMask |= 0x8000;
			}
		}
	}

	bool bHasRegularUniformBuffers = false;

	// Then 'normal' uniform buffers.
	for (auto& UniformBlock : CCHeader.UniformBlocks)
	{
		uint16 UBIndex = UniformBlock.Index;
		if (UBIndex >= Header.Bindings.NumUniformBuffers)
		{
			Header.Bindings.NumUniformBuffers = UBIndex + 1;
		}
		UsedUniformBufferSlots[UBIndex] = true;
		ParameterMap.AddParameterAllocation(*UniformBlock.Name, UBIndex, 0, 0);
		bHasRegularUniformBuffers = true;
	}

	// Packed global uniforms
	const uint16 BytesPerComponent = 4;
	TMap<ANSICHAR, uint16> PackedGlobalArraySize;
	for (auto& PackedGlobal : CCHeader.PackedGlobals)
	{
		ParameterMap.AddParameterAllocation(
			*PackedGlobal.Name,
			PackedGlobal.PackedType,
			PackedGlobal.Offset * BytesPerComponent,
			PackedGlobal.Count * BytesPerComponent
			);

		uint16& Size = PackedGlobalArraySize.FindOrAdd(PackedGlobal.PackedType);
		Size = FMath::Max<uint16>(BytesPerComponent * (PackedGlobal.Offset + PackedGlobal.Count), Size);
	}

	// Packed Uniform Buffers
	TMap<int, TMap<ANSICHAR, uint16> > PackedUniformBuffersSize;
	for (auto& PackedUB : CCHeader.PackedUBs)
	{
		check(PackedUB.Attribute.Index == Header.Bindings.NumUniformBuffers);
		UsedUniformBufferSlots[PackedUB.Attribute.Index] = true;
		ParameterMap.AddParameterAllocation(*PackedUB.Attribute.Name, Header.Bindings.NumUniformBuffers++, 0, 0);

		// Nothing else...
		//for (auto& Member : PackedUB.Members)
		//{
		//}
	}

	// Packed Uniform Buffers copy lists & setup sizes for each UB/Precision entry
	for (auto& PackedUBCopy : CCHeader.PackedUBCopies)
	{
		CrossCompiler::FUniformBufferCopyInfo CopyInfo;
		CopyInfo.SourceUBIndex = PackedUBCopy.SourceUB;
		CopyInfo.SourceOffsetInFloats = PackedUBCopy.SourceOffset;
		CopyInfo.DestUBIndex = PackedUBCopy.DestUB;
		CopyInfo.DestUBTypeName = PackedUBCopy.DestPackedType;
		CopyInfo.DestUBTypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(CopyInfo.DestUBTypeName);
		CopyInfo.DestOffsetInFloats = PackedUBCopy.DestOffset;
		CopyInfo.SizeInFloats = PackedUBCopy.Count;

		Header.UniformBuffersCopyInfo.Add(CopyInfo);

		auto& UniformBufferSize = PackedUniformBuffersSize.FindOrAdd(CopyInfo.DestUBIndex);
		uint16& Size = UniformBufferSize.FindOrAdd(CopyInfo.DestUBTypeName);
		Size = FMath::Max<uint16>(BytesPerComponent * (CopyInfo.DestOffsetInFloats + CopyInfo.SizeInFloats), Size);
	}

	for (auto& PackedUBCopy : CCHeader.PackedUBGlobalCopies)
	{
		CrossCompiler::FUniformBufferCopyInfo CopyInfo;
		CopyInfo.SourceUBIndex = PackedUBCopy.SourceUB;
		CopyInfo.SourceOffsetInFloats = PackedUBCopy.SourceOffset;
		CopyInfo.DestUBIndex = PackedUBCopy.DestUB;
		CopyInfo.DestUBTypeName = PackedUBCopy.DestPackedType;
		CopyInfo.DestUBTypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(CopyInfo.DestUBTypeName);
		CopyInfo.DestOffsetInFloats = PackedUBCopy.DestOffset;
		CopyInfo.SizeInFloats = PackedUBCopy.Count;

		Header.UniformBuffersCopyInfo.Add(CopyInfo);

		uint16& Size = PackedGlobalArraySize.FindOrAdd(CopyInfo.DestUBTypeName);
		Size = FMath::Max<uint16>(BytesPerComponent * (CopyInfo.DestOffsetInFloats + CopyInfo.SizeInFloats), Size);
	}
	Header.Bindings.bHasRegularUniformBuffers = bHasRegularUniformBuffers;

	// Setup Packed Array info
	Header.Bindings.PackedGlobalArrays.Reserve(PackedGlobalArraySize.Num());
	for (auto Iterator = PackedGlobalArraySize.CreateIterator(); Iterator; ++Iterator)
	{
		ANSICHAR TypeName = Iterator.Key();
		uint16 Size = Iterator.Value();
		Size = (Size + 0xf) & (~0xf);
		CrossCompiler::FPackedArrayInfo Info;
		Info.Size = Size;
		Info.TypeName = TypeName;
		Info.TypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(TypeName);
		Header.Bindings.PackedGlobalArrays.Add(Info);
	}

	// Setup Packed Uniform Buffers info
	Header.Bindings.PackedUniformBuffers.Reserve(PackedUniformBuffersSize.Num());
	for (auto Iterator = PackedUniformBuffersSize.CreateIterator(); Iterator; ++Iterator)
	{
		int BufferIndex = Iterator.Key();
		auto& ArraySizes = Iterator.Value();
		TArray<CrossCompiler::FPackedArrayInfo> InfoArray;
		InfoArray.Reserve(ArraySizes.Num());
		for (auto IterSizes = ArraySizes.CreateIterator(); IterSizes; ++IterSizes)
		{
			ANSICHAR TypeName = IterSizes.Key();
			uint16 Size = IterSizes.Value();
			Size = (Size + 0xf) & (~0xf);
			CrossCompiler::FPackedArrayInfo Info;
			Info.Size = Size;
			Info.TypeName = TypeName;
			Info.TypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(TypeName);
			InfoArray.Add(Info);
		}

		Header.Bindings.PackedUniformBuffers.Add(InfoArray);
	}

    uint32 NumTextures = 0;
    
    // Then samplers.
	TMap<FString, uint32> SamplerMap;
	for (auto& Sampler : CCHeader.Samplers)
	{
		ParameterMap.AddParameterAllocation(
			*Sampler.Name,
			0,
			Sampler.Offset,
			Sampler.Count
			);

        NumTextures += Sampler.Count;

		for (auto& SamplerState : Sampler.SamplerStates)
		{
			SamplerMap.Add(SamplerState, Sampler.Count);
		}
    }
    
    Header.Bindings.NumSamplers = CCHeader.SamplerStates.Num();

	// Then UAVs (images in Metal)
	for (auto& UAV : CCHeader.UAVs)
	{
		ParameterMap.AddParameterAllocation(
			*UAV.Name,
			0,
			UAV.Offset,
			UAV.Count
			);

		Header.Bindings.NumUAVs = FMath::Max<uint8>(
			Header.Bindings.NumSamplers,
			UAV.Offset + UAV.Count
			);
	}

	for (auto& SamplerState : CCHeader.SamplerStates)
	{
		ParameterMap.AddParameterAllocation(
			*SamplerState.Name,
			0,
			SamplerState.Index,
			SamplerMap[SamplerState.Name]
			);
	}

	Header.NumThreadsX = CCHeader.NumThreads[0];
	Header.NumThreadsY = CCHeader.NumThreads[1];
	Header.NumThreadsZ = CCHeader.NumThreads[2];

	// Build the SRT for this shader.
	{
		// Build the generic SRT for this shader.
		FShaderCompilerResourceTable GenericSRT;
		BuildResourceTableMapping(ShaderInput.Environment.ResourceTableMap, ShaderInput.Environment.ResourceTableLayoutHashes, UsedUniformBufferSlots, ShaderOutput.ParameterMap, GenericSRT);

		// Copy over the bits indicating which resource tables are active.
		Header.Bindings.ShaderResourceTable.ResourceTableBits = GenericSRT.ResourceTableBits;

		Header.Bindings.ShaderResourceTable.ResourceTableLayoutHashes = GenericSRT.ResourceTableLayoutHashes;

		// Now build our token streams.
		BuildResourceTableTokenStream(GenericSRT.TextureMap, GenericSRT.MaxBoundResourceTable, Header.Bindings.ShaderResourceTable.TextureMap);
		BuildResourceTableTokenStream(GenericSRT.ShaderResourceViewMap, GenericSRT.MaxBoundResourceTable, Header.Bindings.ShaderResourceTable.ShaderResourceViewMap);
		BuildResourceTableTokenStream(GenericSRT.SamplerMap, GenericSRT.MaxBoundResourceTable, Header.Bindings.ShaderResourceTable.SamplerMap);
		BuildResourceTableTokenStream(GenericSRT.UnorderedAccessViewMap, GenericSRT.MaxBoundResourceTable, Header.Bindings.ShaderResourceTable.UnorderedAccessViewMap);

		Header.Bindings.NumUniformBuffers = FMath::Max((uint8)GetNumUniformBuffersUsed(GenericSRT), Header.Bindings.NumUniformBuffers);
	}

	const int32 MaxSamplers = GetFeatureLevelMaxTextureSamplers(ERHIFeatureLevel::ES3_1);

	FString MetalCode = FString(USFSource);
	if (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_KeepDebugInfo) || ShaderInput.Environment.CompilerFlags.Contains(CFLAG_Debug))
	{
		MetalCode.InsertAt(0, FString::Printf(TEXT("// %s\n"), *CCHeader.Name));
		Header.ShaderName = CCHeader.Name;
	}
	
	if (Header.Bindings.NumSamplers > MaxSamplers)
	{
		ShaderOutput.bSucceeded = false;
		FShaderCompilerError* NewError = new(ShaderOutput.Errors) FShaderCompilerError();
		NewError->StrippedErrorMessage =
			FString::Printf(TEXT("shader uses %d samplers exceeding the limit of %d"),
				Header.Bindings.NumSamplers, MaxSamplers);
	}
	else if(ShaderInput.Environment.CompilerFlags.Contains(CFLAG_Debug))
	{
		// Write out the header and shader source code.
		FMemoryWriter Ar(ShaderOutput.ShaderCode.GetWriteAccess(), true);
		uint8 PrecompiledFlag = 0;
		Ar << PrecompiledFlag;
		Ar << Header;
		Ar.Serialize((void*)USFSource, SourceLen + 1 - (USFSource - InShaderSource));
		
		// store data we can pickup later with ShaderCode.FindOptionalData('n'), could be removed for shipping
		ShaderOutput.ShaderCode.AddOptionalData('n', TCHAR_TO_UTF8(*ShaderInput.GenerateShaderName()));

		ShaderOutput.NumInstructions = 0;
		ShaderOutput.NumTextureSamplers = Header.Bindings.NumSamplers;
		ShaderOutput.bSucceeded = true;
	}
	else
	{
		// at this point, the shader source is ready to be compiled
		// We need to use a temp directory path that will be consistent across devices so that debug info
		// can be loaded (as it must be at a consistent location).
#if PLATFORM_MAC
		TCHAR const* TempDir = TEXT("/tmp");
#else
		TCHAR const* TempDir = FPlatformProcess::UserTempDir();
#endif
		FString InputFilename = FPaths::CreateTempFilename(TempDir, TEXT("ShaderIn"), TEXT(""));
		FString ObjFilename = InputFilename + TEXT(".o");
		FString ArFilename = InputFilename + TEXT(".ar");
		FString OutputFilename = InputFilename + TEXT(".lib");
		InputFilename = InputFilename + TEXT(".metal");
		
		if (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_KeepDebugInfo))
		{
			Header.ShaderCode = MetalCode;
			Header.ShaderPath = InputFilename;
		}
		
		// write out shader source
		FFileHelper::SaveStringToFile(MetalCode, *InputFilename);
		
		int32 ReturnCode = 0;
		FString Results;
		FString Errors;
		bool bCompileAtRuntime = true;
		bool bSucceeded = false;

#if METAL_OFFLINE_COMPILE
	#if PLATFORM_MAC
		const bool bIsMobile = (ShaderInput.Target.Platform == SP_METAL || ShaderInput.Target.Platform == SP_METAL_MRT);
		FString XcodePath = FPlatformMisc::GetXcodePath();
		if (XcodePath.Len() > 0 && (bIsMobile || FPlatformMisc::MacOSXVersionCompare(10, 11, 0) >= 0))
		{
			FString MetalToolsPath = FString::Printf(TEXT("%s/Toolchains/XcodeDefault.xctoolchain/usr/bin"), *XcodePath);
			FString MetalPath = MetalToolsPath + TEXT("/metal");
			if (IFileManager::Get().FileSize(*MetalPath) <= 0)
			{
				if(bIsMobile)
				{
					MetalToolsPath = FString::Printf(TEXT("%s/Platforms/iPhoneOS.platform/usr/bin"), *XcodePath);
				}
				else
				{
					MetalToolsPath = FString::Printf(TEXT("%s/Platforms/MacOSX.platform/usr/bin"), *XcodePath);
				}
				MetalPath = MetalToolsPath + TEXT("/metal");
			}

			if (IFileManager::Get().FileSize(*MetalPath) > 0)
			{
				// metal commandlines
				FString DebugInfo = ShaderInput.Environment.CompilerFlags.Contains(CFLAG_KeepDebugInfo) ? TEXT("-gline-tables-only") : TEXT("");
				FString MathMode = Header.bFastMath ? TEXT("-ffast-math") : TEXT("-fno-fast-math");
				
				FString Params = FString::Printf(TEXT("%s %s -Wno-null-character %s %s -o %s"), *DebugInfo, *MathMode, Standard, *InputFilename, *ObjFilename);
				
				FPlatformProcess::ExecProcess( *MetalPath, *Params, &ReturnCode, &Results, &Errors );

				// handle compile error
				if (ReturnCode != 0 || IFileManager::Get().FileSize(*ObjFilename) <= 0)
				{
					FShaderCompilerError* Error = new(OutErrors) FShaderCompilerError();
					Error->ErrorFile = InputFilename;
					Error->ErrorLineString = TEXT("0");
					Error->StrippedErrorMessage = Results + Errors;
					bSucceeded = false;
				}
				else
				{
					Params = FString::Printf(TEXT("r %s %s"), *ArFilename, *ObjFilename);
					FString MetalArPath = MetalToolsPath + TEXT("/metal-ar");
					FPlatformProcess::ExecProcess( *MetalArPath, *Params, &ReturnCode, &Results, &Errors );

					// handle compile error
					if (ReturnCode != 0 || IFileManager::Get().FileSize(*ArFilename) <= 0)
					{
						FShaderCompilerError* Error = new(OutErrors) FShaderCompilerError();
						Error->ErrorFile = InputFilename;
						Error->ErrorLineString = TEXT("0");
						Error->StrippedErrorMessage = Results + Errors;
						bSucceeded = false;
					}
					else
					{
						Params = FString::Printf(TEXT("-o %s %s"), *OutputFilename, *ArFilename);
						FString MetalLibPath = MetalToolsPath + TEXT("/metallib");
						FPlatformProcess::ExecProcess( *MetalLibPath, *Params, &ReturnCode, &Results, &Errors );
				
						// handle compile error
						if (ReturnCode != 0 || IFileManager::Get().FileSize(*OutputFilename) <= 0)
						{
							FShaderCompilerError* Error = new(OutErrors) FShaderCompilerError();
							Error->ErrorFile = InputFilename;
							Error->ErrorLineString = TEXT("0");
							Error->StrippedErrorMessage = Results + Errors;
							bSucceeded = false;
						}
						else
						{
							bCompileAtRuntime = false;
							
							// Write out the header and compiled shader code
							FMemoryWriter Ar(ShaderOutput.ShaderCode.GetWriteAccess(), true);
							uint8 PrecompiledFlag = 1;
							Ar << PrecompiledFlag;
							Ar << Header;

							// load output
							TArray<uint8> CompiledShader;
							FFileHelper::LoadFileToArray(CompiledShader, *OutputFilename);
							
							// jam it into the output bytes
							Ar.Serialize(CompiledShader.GetData(), CompiledShader.Num());
							
							// store data we can pickup later with ShaderCode.FindOptionalData('n'), could be removed for shipping
							ShaderOutput.ShaderCode.AddOptionalData('n', TCHAR_TO_UTF8(*ShaderInput.GenerateShaderName()));

							ShaderOutput.NumInstructions = 0;
							ShaderOutput.NumTextureSamplers = Header.Bindings.NumSamplers;
							ShaderOutput.bSucceeded = true;
							bSucceeded = true;
						}
					}
				}
			}
			else
			{
				UE_LOG(LogMetalShaderCompiler, Warning, TEXT("Could not find offline 'metal' shader compiler - falling back to slower online compiled text shaders."));
				bCompileAtRuntime = true;
				bSucceeded = true;
			}
		}
		else
		{
			UE_LOG(LogMetalShaderCompiler, Warning, TEXT("Could not find offline 'metal' shader compiler - falling back to slower online compiled text shaders."));
			bSucceeded = true;
		}
	#else
		// do not compile on non-Windows
		UE_LOG(LogMetalShaderCompiler, Fatal, TEXT("Metal shader compilation is not supported on this platform"));
		bSucceeded = false;
	#endif // PLATFORM_MAC
#else
		// Assume success for non-Mac
		bSucceeded = true;
#endif	// PLATFORM_MAC

		if (bCompileAtRuntime)
		{
			// Write out the header and shader source code.
			FMemoryWriter Ar(ShaderOutput.ShaderCode.GetWriteAccess(), true);
			uint8 PrecompiledFlag = 0;
			Ar << PrecompiledFlag;
			Ar << Header;
			Ar.Serialize((void*)USFSource, SourceLen + 1 - (USFSource - InShaderSource));
			
			// store data we can pickup later with ShaderCode.FindOptionalData('n'), could be removed for shipping
			// Daniel L: This GenerateShaderName does not generate a deterministic output among shaders as the shader code can be shared. 
			//			uncommenting this will cause the project to have non deterministic materials and will hurt patch sizes
			//ShaderOutput.ShaderCode.AddOptionalData('n', TCHAR_TO_UTF8(*ShaderInput.GenerateShaderName()));
			
			ShaderOutput.NumInstructions = 0;
			ShaderOutput.NumTextureSamplers = Header.Bindings.NumSamplers;
			ShaderOutput.bSucceeded = bSucceeded || ShaderOutput.bSucceeded;
		}
		else
		{
			IFileManager::Get().Delete(*InputFilename);
			IFileManager::Get().Delete(*ObjFilename);
			IFileManager::Get().Delete(*ArFilename);
			IFileManager::Get().Delete(*OutputFilename);
		}
	}
}

/*------------------------------------------------------------------------------
	External interface.
------------------------------------------------------------------------------*/

static FString CreateCommandLineHLSLCC( const FString& ShaderFile, const FString& OutputFile, const FString& EntryPoint, EHlslCompileTarget Target, EHlslShaderFrequency Frequency, uint32 CCFlags )
{
	const TCHAR* VersionSwitch = TEXT("-metal");
	switch (Target)
	{
		case HCT_FeatureLevelES3_1:
			VersionSwitch = TEXT("-metal");
			break;
		case HCT_FeatureLevelSM4:
			VersionSwitch = TEXT("-metalsm4");
			break;
		case HCT_FeatureLevelSM5:
			VersionSwitch = TEXT("-metalsm5");
			break;
			
		default:
			check(0);
	}
	return CrossCompiler::CreateBatchFileContents(ShaderFile, OutputFile, Frequency, EntryPoint, VersionSwitch, CCFlags, TEXT(""));
}

void CompileShader_Metal(const FShaderCompilerInput& Input,FShaderCompilerOutput& Output,const FString& WorkingDirectory)
{
	FString PreprocessedShader;
	FShaderCompilerDefinitions AdditionalDefines;
	EHlslCompileTarget HlslCompilerTarget = HCT_FeatureLevelES3_1; // Always ES3.1 for now due to the way RCO has configured the MetalBackend
	EHlslCompileTarget MetalCompilerTarget = HCT_FeatureLevelES3_1; // Varies depending on the actual intended Metal target.
	ECompilerFlags PlatformFlowControl = CFLAG_AvoidFlowControl;

	// Work out which standard we need, this is dependent on the shader platform.
	const bool bIsMobile = (Input.Target.Platform == SP_METAL || Input.Target.Platform == SP_METAL_MRT);
	if (bIsMobile)
	{
		AdditionalDefines.SetDefine(TEXT("IOS"), 1);
	}
	else
	{
		AdditionalDefines.SetDefine(TEXT("MAC"), 1);
		// On OS X it is always better to leave the flow control statements in the MetalSL & let the MetalSL->GPU compilers
		// optimise it appropriately for each GPU. This gives a performance gain on pretty much all vendors & GPUs.
		// @todo The Metal shader compiler shipped for Nvidia GPUs in 10.11 DP7 & later can't deal with flow-control statements and local constant array variables so we have to sacrifice performance for correctness.
		// PlatformFlowControl = CFLAG_PreferFlowControl;
	}
	
	AdditionalDefines.SetDefine(TEXT("COMPILER_HLSLCC"), 1 );
	AdditionalDefines.SetDefine(TEXT("row_major"), TEXT(""));
	AdditionalDefines.SetDefine(TEXT("COMPILER_METAL"), 1);

	static FName NAME_SF_METAL(TEXT("SF_METAL"));
	static FName NAME_SF_METAL_MRT(TEXT("SF_METAL_MRT"));
	static FName NAME_SF_METAL_SM4(TEXT("SF_METAL_SM4"));
	static FName NAME_SF_METAL_SM5(TEXT("SF_METAL_SM5"));
	static FName NAME_SF_METAL_MACES3_1(TEXT("SF_METAL_MACES3_1"));
	static FName NAME_SF_METAL_MACES2(TEXT("SF_METAL_MACES2"));
	
	TCHAR const* Standard = TEXT("-std=ios-metal1.0");
	bool bIsDesktop = false;

	if (Input.ShaderFormat == NAME_SF_METAL)
	{
		AdditionalDefines.SetDefine(TEXT("METAL_PROFILE"), 1);
	}
	else if (Input.ShaderFormat == NAME_SF_METAL_MRT)
	{
		AdditionalDefines.SetDefine(TEXT("METAL_MRT_PROFILE"), 1);
	}
	else if (Input.ShaderFormat == NAME_SF_METAL_MACES2)
	{
		AdditionalDefines.SetDefine(TEXT("METAL_ES2_PROFILE"), 1);
		AdditionalDefines.SetDefine(TEXT("FORCE_FLOATS"), 1); // Force floats to avoid radr://24884199 & radr://24884860
		Standard = TEXT("-std=osx-metal1.1 -mmacosx-version-min=10.11");
		MetalCompilerTarget = HCT_FeatureLevelES2;
		bIsDesktop = true;
	}
	else if (Input.ShaderFormat == NAME_SF_METAL_MACES3_1)
	{
		AdditionalDefines.SetDefine(TEXT("METAL_PROFILE"), 1);
		AdditionalDefines.SetDefine(TEXT("FORCE_FLOATS"), 1); // Force floats to avoid radr://24884199 & radr://24884860
		Standard = TEXT("-std=osx-metal1.1 -mmacosx-version-min=10.11");
		MetalCompilerTarget = HCT_FeatureLevelES3_1;
		bIsDesktop = true;
	}
	else if (Input.ShaderFormat == NAME_SF_METAL_SM4)
	{
		AdditionalDefines.SetDefine(TEXT("METAL_SM4_PROFILE"), 1);
		AdditionalDefines.SetDefine(TEXT("USING_VERTEX_SHADER_LAYER"), 1);
		Standard = TEXT("-std=osx-metal1.1 -mmacosx-version-min=10.11");
		MetalCompilerTarget = HCT_FeatureLevelSM4;
		bIsDesktop = true;
	}
	else if (Input.ShaderFormat == NAME_SF_METAL_SM5)
	{
		AdditionalDefines.SetDefine(TEXT("METAL_SM5_PROFILE"), 1);
		AdditionalDefines.SetDefine(TEXT("USING_VERTEX_SHADER_LAYER"), 1);
		Standard = TEXT("-std=osx-metal1.1 -mmacosx-version-min=10.11");
		MetalCompilerTarget = HCT_FeatureLevelSM5;
		bIsDesktop = true;
	}
	else
	{
		Output.bSucceeded = false;
		new(Output.Errors) FShaderCompilerError(*FString::Printf(TEXT("Invalid shader format '%s' passed to compiler."), *Input.ShaderFormat.ToString()));
		return;
	}
	
	const bool bDumpDebugInfo = (Input.DumpDebugInfoPath != TEXT("") && IFileManager::Get().DirectoryExists(*Input.DumpDebugInfoPath));

	if(Input.Environment.CompilerFlags.Contains(CFLAG_AvoidFlowControl) || PlatformFlowControl == CFLAG_AvoidFlowControl)
	{
		AdditionalDefines.SetDefine(TEXT("COMPILER_SUPPORTS_ATTRIBUTES"), (uint32)1);
	}
	else
	{
		AdditionalDefines.SetDefine(TEXT("COMPILER_SUPPORTS_ATTRIBUTES"), (uint32)0);
	}

	if (PreprocessShader(PreprocessedShader, Output, Input, AdditionalDefines))
	{
		// Disable instanced stereo until supported for metal
		StripInstancedStereo(PreprocessedShader);

		char* MetalShaderSource = NULL;
		char* ErrorLog = NULL;

		const EHlslShaderFrequency FrequencyTable[] =
		{
			HSF_VertexShader,
			HSF_InvalidFrequency,
			HSF_InvalidFrequency,
			HSF_PixelShader,
			HSF_InvalidFrequency,
			HSF_ComputeShader
		};

		const EHlslShaderFrequency Frequency = FrequencyTable[Input.Target.Frequency];
		if (Frequency == HSF_InvalidFrequency)
		{
			Output.bSucceeded = false;
			FShaderCompilerError* NewError = new(Output.Errors) FShaderCompilerError();
			NewError->StrippedErrorMessage = FString::Printf(
				TEXT("%s shaders not supported for use in Metal."),
				CrossCompiler::GetFrequencyName((EShaderFrequency)Input.Target.Frequency)
				);
			return;
		}


		// This requires removing the HLSLCC_NoPreprocess flag later on!
		if (!RemoveUniformBuffersFromSource(PreprocessedShader))
		{
			return;
		}

		// Write out the preprocessed file and a batch file to compile it if requested (DumpDebugInfoPath is valid)
		if (bDumpDebugInfo)
		{
			FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*(Input.DumpDebugInfoPath / Input.SourceFilename + TEXT(".usf")));
			if (FileWriter)
			{
				auto AnsiSourceFile = StringCast<ANSICHAR>(*PreprocessedShader);
				FileWriter->Serialize((ANSICHAR*)AnsiSourceFile.Get(), AnsiSourceFile.Length());
				FileWriter->Close();
				delete FileWriter;
			}

			if (Input.bGenerateDirectCompileFile)
			{
				FFileHelper::SaveStringToFile(CreateShaderCompilerWorkerDirectCommandLine(Input), *(Input.DumpDebugInfoPath / TEXT("DirectCompile.txt")));
			}
		}

		uint32 CCFlags = HLSLCC_NoPreprocess | HLSLCC_PackUniforms;
		if (!bIsMobile)
		{
			CCFlags |= HLSLCC_FixAtomicReferences;
		}
		//CCFlags |= HLSLCC_FlattenUniformBuffers | HLSLCC_FlattenUniformBufferStructures;

		if (bDumpDebugInfo)
		{
			const FString MetalFile = (Input.DumpDebugInfoPath / TEXT("Output.metal"));
			const FString USFFile = (Input.DumpDebugInfoPath / Input.SourceFilename) + TEXT(".usf");
			const FString CCBatchFileContents = CreateCommandLineHLSLCC(USFFile, MetalFile, *Input.EntryPointName, MetalCompilerTarget, Frequency, CCFlags);
			if (!CCBatchFileContents.IsEmpty())
			{
				FFileHelper::SaveStringToFile(CCBatchFileContents, *(Input.DumpDebugInfoPath / TEXT("CrossCompile.bat")));
			}
		}

		// Required as we added the RemoveUniformBuffersFromSource() function (the cross-compiler won't be able to interpret comments w/o a preprocessor)
		CCFlags &= ~HLSLCC_NoPreprocess;

		bool const bZeroInitialise = Input.Environment.CompilerFlags.Contains(CFLAG_ZeroInitialise);
		bool const bBoundsChecks = Input.Environment.CompilerFlags.Contains(CFLAG_BoundsChecking);
		
		FMetalCodeBackend MetalBackEnd(CCFlags, MetalCompilerTarget, bIsDesktop, bZeroInitialise, bBoundsChecks);
		FMetalLanguageSpec MetalLanguageSpec;

		int32 Result = 0;
		FHlslCrossCompilerContext CrossCompilerContext(CCFlags, Frequency, HlslCompilerTarget);
		if (CrossCompilerContext.Init(TCHAR_TO_ANSI(*Input.SourceFilename), &MetalLanguageSpec))
		{
			Result = CrossCompilerContext.Run(
				TCHAR_TO_ANSI(*PreprocessedShader),
				TCHAR_TO_ANSI(*Input.EntryPointName),
				&MetalBackEnd,
				&MetalShaderSource,
				&ErrorLog
				) ? 1 : 0;
		}

		int32 SourceLen = MetalShaderSource ? FCStringAnsi::Strlen(MetalShaderSource) : 0;
		if(MetalShaderSource)
		{
			uint32 Len = FCStringAnsi::Strlen(TCHAR_TO_ANSI(*Input.SourceFilename)) + FCStringAnsi::Strlen(TCHAR_TO_ANSI(*Input.DebugGroupName)) + FCStringAnsi::Strlen(TCHAR_TO_ANSI(*Input.EntryPointName)) + FCStringAnsi::Strlen(MetalShaderSource) + 21;
			char* Dest = (char*)malloc(Len);
			FCStringAnsi::Snprintf(Dest, Len, "// ! %s/%s.usf:%s\n%s", (const char*)TCHAR_TO_ANSI(*Input.DebugGroupName), (const char*)TCHAR_TO_ANSI(*Input.SourceFilename), (const char*)TCHAR_TO_ANSI(*Input.EntryPointName), (const char*)MetalShaderSource);
			free(MetalShaderSource);
			MetalShaderSource = Dest;
			SourceLen = FCStringAnsi::Strlen(MetalShaderSource);
		}
		if (bDumpDebugInfo)
		{
			if (SourceLen > 0)
			{
				FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*(Input.DumpDebugInfoPath / Input.SourceFilename + TEXT(".metal")));
				if (FileWriter)
				{
					FileWriter->Serialize(MetalShaderSource, SourceLen + 1);
					FileWriter->Close();
					delete FileWriter;
				}
			}
		}

		if (Result != 0)
		{
			Output.Target = Input.Target;
			BuildMetalShaderOutput(Output, Input, MetalShaderSource, SourceLen, Standard, Output.Errors);
		}
		else
		{
			FString Tmp = ANSI_TO_TCHAR(ErrorLog);
			TArray<FString> ErrorLines;
			Tmp.ParseIntoArray(ErrorLines, TEXT("\n"), true);
			for (int32 LineIndex = 0; LineIndex < ErrorLines.Num(); ++LineIndex)
			{
				const FString& Line = ErrorLines[LineIndex];
				CrossCompiler::ParseHlslccError(Output.Errors, Line);
			}
		}

		if (MetalShaderSource)
		{
			free(MetalShaderSource);
		}
		if (ErrorLog)
		{
			free(ErrorLog);
		}
	}
}

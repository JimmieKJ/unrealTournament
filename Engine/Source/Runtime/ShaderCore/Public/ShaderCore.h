// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCore.h: Shader core module definitions.
=============================================================================*/

#pragma once

#include "Core.h"
#include "../../RHI/Public/RHIDefinitions.h"
#include "SecureHash.h"
#include "../../RenderCore/Public/UniformBuffer.h"

/** 
 * Controls whether shader related logs are visible.  
 * Note: The runtime verbosity is driven by the console variable 'r.ShaderDevelopmentMode'
 */
#if UE_BUILD_DEBUG && PLATFORM_LINUX
SHADERCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogShaders, Log, All);
#else
SHADERCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogShaders, Error, All);
#endif

DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Total Material Shader Compiling Time"),STAT_ShaderCompiling_MaterialShaders,STATGROUP_ShaderCompiling, SHADERCORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Total Global Shader Compiling Time"),STAT_ShaderCompiling_GlobalShaders,STATGROUP_ShaderCompiling, SHADERCORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("RHI Compile Time"),STAT_ShaderCompiling_RHI,STATGROUP_ShaderCompiling, SHADERCORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Loading Shader Files"),STAT_ShaderCompiling_LoadingShaderFiles,STATGROUP_ShaderCompiling, SHADERCORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("CRCing Shader Files"),STAT_ShaderCompiling_HashingShaderFiles,STATGROUP_ShaderCompiling, SHADERCORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("HLSL Translation"),STAT_ShaderCompiling_HLSLTranslation,STATGROUP_ShaderCompiling, SHADERCORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("DDC Loading"),STAT_ShaderCompiling_DDCLoading,STATGROUP_ShaderCompiling, SHADERCORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Material Loading"),STAT_ShaderCompiling_MaterialLoading,STATGROUP_ShaderCompiling, SHADERCORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Material Compiling"),STAT_ShaderCompiling_MaterialCompiling,STATGROUP_ShaderCompiling, SHADERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Total Material Shaders"),STAT_ShaderCompiling_NumTotalMaterialShaders,STATGROUP_ShaderCompiling, SHADERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Special Material Shaders"),STAT_ShaderCompiling_NumSpecialMaterialShaders,STATGROUP_ShaderCompiling, SHADERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Particle Material Shaders"),STAT_ShaderCompiling_NumParticleMaterialShaders,STATGROUP_ShaderCompiling, SHADERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Skinned Material Shaders"),STAT_ShaderCompiling_NumSkinnedMaterialShaders,STATGROUP_ShaderCompiling, SHADERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Lit Material Shaders"),STAT_ShaderCompiling_NumLitMaterialShaders,STATGROUP_ShaderCompiling, SHADERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Unlit Material Shaders"),STAT_ShaderCompiling_NumUnlitMaterialShaders,STATGROUP_ShaderCompiling, SHADERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Transparent Material Shaders"),STAT_ShaderCompiling_NumTransparentMaterialShaders,STATGROUP_ShaderCompiling, SHADERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Opaque Material Shaders"),STAT_ShaderCompiling_NumOpaqueMaterialShaders,STATGROUP_ShaderCompiling, SHADERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Masked Material Shaders"),STAT_ShaderCompiling_NumMaskedMaterialShaders,STATGROUP_ShaderCompiling, SHADERCORE_API);

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Shaders Loaded"),STAT_Shaders_NumShadersLoaded,STATGROUP_Shaders, SHADERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Shader Resources Loaded"),STAT_Shaders_NumShaderResourcesLoaded,STATGROUP_Shaders, SHADERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Shader Maps Registered"),STAT_Shaders_NumShaderMaps,STATGROUP_Shaders, SHADERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("RT Shader Load Time"),STAT_Shaders_RTShaderLoadTime,STATGROUP_Shaders, SHADERCORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Shaders Used"),STAT_Shaders_NumShadersUsedForRendering,STATGROUP_Shaders, SHADERCORE_API);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Total RT Shader Init Time"),STAT_Shaders_TotalRTShaderInitForRenderingTime,STATGROUP_Shaders, SHADERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Frame RT Shader Init Time"),STAT_Shaders_FrameRTShaderInitForRenderingTime,STATGROUP_Shaders, SHADERCORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Shader Memory"),STAT_Shaders_ShaderMemory,STATGROUP_Shaders, SHADERCORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Shader Resource Mem"),STAT_Shaders_ShaderResourceMemory,STATGROUP_Shaders, SHADERCORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Shader MapMemory"),STAT_Shaders_ShaderMapMemory,STATGROUP_Shaders, SHADERCORE_API);

inline TStatId GetMemoryStatType(EShaderFrequency ShaderFrequency)
{
	static_assert(6 == SF_NumFrequencies, "EShaderFrequency has a bad size.");
	switch(ShaderFrequency)
	{
		case SF_Pixel:		return GET_STATID(STAT_PixelShaderMemory);
		case SF_Compute:	return GET_STATID(STAT_PixelShaderMemory);
	}
	return GET_STATID(STAT_VertexShaderMemory);
}

/** Initializes cached shader type data.  This must be called before creating any FShaderType. */
extern SHADERCORE_API void InitializeShaderTypes();

/** Uninitializes cached shader type data.  This is needed before unloading modules that contain FShaderTypes. */
extern SHADERCORE_API void UninitializeShaderTypes();

/** Returns true if debug viewmodes are allowed for the given platform. */
extern SHADERCORE_API bool AllowDebugViewmodes();

struct FShaderTarget
{
	uint32 Frequency : SF_NumBits;
	uint32 Platform : SP_NumBits;

	FShaderTarget()
	{}

	FShaderTarget(EShaderFrequency InFrequency,EShaderPlatform InPlatform)
	:	Frequency(InFrequency)
	,	Platform(InPlatform)
	{}

	friend bool operator==(const FShaderTarget& X, const FShaderTarget& Y)
	{
		return X.Frequency == Y.Frequency && X.Platform == Y.Platform;
	}

	friend FArchive& operator<<(FArchive& Ar,FShaderTarget& Target)
	{
		uint32 TargetFrequency = Target.Frequency;
		uint32 TargetPlatform = Target.Platform;
		Ar << TargetFrequency << TargetPlatform;
		Target.Frequency = TargetFrequency;
		Target.Platform = TargetPlatform;
		return Ar;
	}
};

enum ECompilerFlags
{
	CFLAG_PreferFlowControl = 0,
	CFLAG_Debug,
	CFLAG_AvoidFlowControl,
	/** Disable shader validation */
	CFLAG_SkipValidation,
	/** Only allows standard optimizations, not the longest compile times. */
	CFLAG_StandardOptimization,
	/** Shader should use on chip memory instead of main memory ring buffer memory. */
	CFLAG_OnChip,
	CFLAG_KeepDebugInfo
};

/**
 * A map of shader parameter names to registers allocated to that parameter.
 */
class FShaderParameterMap
{
public:

	FShaderParameterMap()
	{}

	SHADERCORE_API bool FindParameterAllocation(const TCHAR* ParameterName,uint16& OutBufferIndex,uint16& OutBaseIndex,uint16& OutSize) const;
	SHADERCORE_API bool ContainsParameterAllocation(const TCHAR* ParameterName) const;
	SHADERCORE_API void AddParameterAllocation(const TCHAR* ParameterName,uint16 BufferIndex,uint16 BaseIndex,uint16 Size);
	SHADERCORE_API void RemoveParameterAllocation(const TCHAR* ParameterName);
	/** Checks that all parameters are bound and asserts if any aren't in a debug build
	* @param InVertexFactoryType can be 0
	*/
	SHADERCORE_API void VerifyBindingsAreComplete(const TCHAR* ShaderTypeName, EShaderFrequency Frequency, class FVertexFactoryType* InVertexFactoryType) const;

	/** Updates the hash state with the contents of this parameter map. */
	void UpdateHash(FSHA1& HashState) const;

	friend FArchive& operator<<(FArchive& Ar,FShaderParameterMap& InParameterMap)
	{
		// Note: this serialize is used to pass between UE4 and the shader compile worker, recompile both when modifying
		return Ar << InParameterMap.ParameterMap;
	}

private:
	struct FParameterAllocation
	{
		uint16 BufferIndex;
		uint16 BaseIndex;
		uint16 Size;
		mutable bool bBound;

		FParameterAllocation() :
			bBound(false)
		{}
			
		friend FArchive& operator<<(FArchive& Ar,FParameterAllocation& Allocation)
		{
			return Ar << Allocation.BufferIndex << Allocation.BaseIndex << Allocation.Size << Allocation.bBound;
		}
	};

	TMap<FString,FParameterAllocation> ParameterMap;
};

/** Container for shader compiler definitions. */
class FShaderCompilerDefinitions
{
public:

	FShaderCompilerDefinitions()
	{
		// Presize to reduce re-hashing while building shader jobs
		Definitions.Empty(50);
	}

	/**
	 * Works for TCHAR
	 * e.g. SetDefine(TEXT("NUM_SAMPLES"), TEXT("1"));
	 */
	void SetDefine(const TCHAR* Name, const TCHAR* Value)
	{
		Definitions.Add(Name, Value);
	}

	/**
	 * Works for uint32 and bool
	 * e.g. OutEnvironment.SetDefine(TEXT("REALLY"), bReally);
	 * e.g. OutEnvironment.SetDefine(TEXT("NUM_SAMPLES"), NumSamples);
	 */
	void SetDefine(const TCHAR* Name, uint32 Value)
	{
		// can be optimized
		Definitions.Add(Name, *FString::Printf(TEXT("%u"), Value));
	}

	/**
	 * Works for float
	 */
	void SetFloatDefine(const TCHAR* Name, float Value)
	{
		// can be optimized
		Definitions.Add(Name, *FString::Printf(TEXT("%f"), Value));
	}

	const TMap<FString,FString>& GetDefinitionMap() const
	{
		return Definitions;
	}

	friend FArchive& operator<<(FArchive& Ar,FShaderCompilerDefinitions& Defs)
	{
		return Ar << Defs.Definitions;
	}

	void Merge(const FShaderCompilerDefinitions& Other)
	{
		Definitions.Append(Other.Definitions);
	}

private:

	/** Map: definition -> value. */
	TMap<FString,FString> Definitions;
};

struct FBaseShaderResourceTable
{
	/** Bits indicating which resource tables contain resources bound to this shader. */
	uint32 ResourceTableBits;

	/** Mapping of bound SRVs to their location in resource tables. */
	TArray<uint32> ShaderResourceViewMap;

	/** Mapping of bound sampler states to their location in resource tables. */
	TArray<uint32> SamplerMap;

	/** Mapping of bound UAVs to their location in resource tables. */
	TArray<uint32> UnorderedAccessViewMap;

	/** Hash of the layouts of resource tables at compile time, used for runtime validation. */
	TArray<uint32> ResourceTableLayoutHashes;

	FBaseShaderResourceTable() :
		ResourceTableBits(0)
	{
	}

	friend bool operator==(const FBaseShaderResourceTable &A, const FBaseShaderResourceTable& B)
	{
		bool bEqual = true;
		bEqual &= (A.ResourceTableBits == B.ResourceTableBits);
		bEqual &= (A.ShaderResourceViewMap.Num() == B.ShaderResourceViewMap.Num());
		bEqual &= (A.SamplerMap.Num() == B.SamplerMap.Num());
		bEqual &= (A.UnorderedAccessViewMap.Num() == B.UnorderedAccessViewMap.Num());
		bEqual &= (A.ResourceTableLayoutHashes.Num() == B.ResourceTableLayoutHashes.Num());
		if (!bEqual)
		{
			return false;
		}
		bEqual &= (FMemory::Memcmp(A.ShaderResourceViewMap.GetData(), B.ShaderResourceViewMap.GetData(), A.ShaderResourceViewMap.GetTypeSize()*A.ShaderResourceViewMap.Num()) == 0);
		bEqual &= (FMemory::Memcmp(A.SamplerMap.GetData(), B.SamplerMap.GetData(), A.SamplerMap.GetTypeSize()*A.SamplerMap.Num()) == 0);
		bEqual &= (FMemory::Memcmp(A.UnorderedAccessViewMap.GetData(), B.UnorderedAccessViewMap.GetData(), A.UnorderedAccessViewMap.GetTypeSize()*A.UnorderedAccessViewMap.Num()) == 0);
		bEqual &= (FMemory::Memcmp(A.ResourceTableLayoutHashes.GetData(), B.ResourceTableLayoutHashes.GetData(), A.ResourceTableLayoutHashes.GetTypeSize()*A.ResourceTableLayoutHashes.Num()) == 0);
		return bEqual;
	}
};

inline FArchive& operator<<(FArchive& Ar, FBaseShaderResourceTable& SRT)
{
	Ar << SRT.ResourceTableBits;
	Ar << SRT.ShaderResourceViewMap;
	Ar << SRT.SamplerMap;
	Ar << SRT.UnorderedAccessViewMap;
	Ar << SRT.ResourceTableLayoutHashes;

	return Ar;
}

struct FShaderResourceTable
{
	/** Bits indicating which resource tables contain resources bound to this shader. */
	uint32 ResourceTableBits;

	/** The max index of a uniform buffer from which resources are bound. */
	uint32 MaxBoundResourceTable;

	/** Mapping of bound Textures to their location in resource tables. */
	TArray<uint32> TextureMap;

	/** Mapping of bound SRVs to their location in resource tables. */
	TArray<uint32> ShaderResourceViewMap;

	/** Mapping of bound sampler states to their location in resource tables. */
	TArray<uint32> SamplerMap;

	/** Mapping of bound UAVs to their location in resource tables. */
	TArray<uint32> UnorderedAccessViewMap;

	/** Hash of the layouts of resource tables at compile time, used for runtime validation. */
	TArray<uint32> ResourceTableLayoutHashes;

	FShaderResourceTable()
		: ResourceTableBits(0)
		, MaxBoundResourceTable(0)
	{
	}
};

inline FArchive& operator<<(FArchive& Ar, FResourceTableEntry& Entry)
{
	Ar << Entry.UniformBufferName;
	Ar << Entry.Type;
	Ar << Entry.ResourceIndex;
	return Ar;
}

/** The environment used to compile a shader. */
struct FShaderCompilerEnvironment : public FRefCountedObject
{
	TMap<FString,FString> IncludeFileNameToContentsMap;
	TArray<uint32> CompilerFlags;
	TMap<uint32,uint8> RenderTargetOutputFormatsMap;
	TMap<FString,FResourceTableEntry> ResourceTableMap;
	TMap<FString,uint32> ResourceTableLayoutHashes;

	/** Default constructor. */
	FShaderCompilerEnvironment() 
	{
		// Presize to reduce re-hashing while building shader jobs
		IncludeFileNameToContentsMap.Empty(15);
	}

	/** Initialization constructor. */
	explicit FShaderCompilerEnvironment(const FShaderCompilerDefinitions& InDefinitions)
		: Definitions(InDefinitions)
	{
	}
	
	/**
	 * Works for TCHAR
	 * e.g. SetDefine(TEXT("NUM_SAMPLES"), TEXT("1"));
	 */
	void SetDefine(const TCHAR* Name, const TCHAR* Value)
	{
		Definitions.SetDefine(Name, Value);
	}

	/**
	 * Works for uint32 and bool
	 * e.g. OutEnvironment.SetDefine(TEXT("REALLY"), bReally);
	 * e.g. OutEnvironment.SetDefine(TEXT("NUM_SAMPLES"), NumSamples);
	 */
	void SetDefine(const TCHAR* Name, uint32 Value)
	{
		Definitions.SetDefine(Name, Value);
	}

	/**
	 * Works for float
	 */
	void SetFloatDefine(const TCHAR* Name, float Value)
	{
		Definitions.SetFloatDefine(Name, Value);
	}

	const TMap<FString,FString>& GetDefinitions() const
	{
		return Definitions.GetDefinitionMap();
	}
	
	void SetRenderTargetOutputFormat(uint32 RenderTargetIndex, EPixelFormat PixelFormat)
	{
		RenderTargetOutputFormatsMap.Add(RenderTargetIndex, PixelFormat);
	}
	
	friend FArchive& operator<<(FArchive& Ar,FShaderCompilerEnvironment& Environment)
	{
		// Note: this serialize is used to pass between UE4 and the shader compile worker, recompile both when modifying
		return Ar << Environment.IncludeFileNameToContentsMap << Environment.Definitions << Environment.CompilerFlags << Environment.RenderTargetOutputFormatsMap << Environment.ResourceTableMap << Environment.ResourceTableLayoutHashes;
	}

	void Merge(const FShaderCompilerEnvironment& Other)
	{
		// Merge the include maps
		// Merge the values of any existing keys
		for (TMap<FString,FString>::TConstIterator It(Other.IncludeFileNameToContentsMap); It; ++It )
		{
			FString* ExistingContents = IncludeFileNameToContentsMap.Find(It.Key());

			if (ExistingContents)
			{
				(*ExistingContents) += It.Value();
			}
			else
			{
				IncludeFileNameToContentsMap.Add(It.Key(), It.Value());
			}
		}

		CompilerFlags.Append(Other.CompilerFlags);
		ResourceTableMap.Append(Other.ResourceTableMap);
		ResourceTableLayoutHashes.Append(Other.ResourceTableLayoutHashes);
		Definitions.Merge(Other.Definitions);
		RenderTargetOutputFormatsMap.Append(Other.RenderTargetOutputFormatsMap);
	}

private:

	FShaderCompilerDefinitions Definitions;
};

/** Struct that gathers all readonly inputs needed for the compilation of a single shader. */
struct FShaderCompilerInput
{
	FShaderTarget Target;
	FName ShaderFormat;
	FString SourceFilePrefix;
	FString SourceFilename;
	FString EntryPointName;
	FString DumpDebugInfoRootPath;	// Dump debug path (up to platform)
	FString DumpDebugInfoPath;		// Dump debug path (platform/groupname)
	FShaderCompilerEnvironment Environment;
	TRefCountPtr<FShaderCompilerEnvironment> SharedEnvironment;

	friend FArchive& operator<<(FArchive& Ar,FShaderCompilerInput& Input)
	{
		// Note: this serialize is used to pass between UE4 and the shader compile worker, recompile both when modifying

		Ar << Input.Target;
		{
			FString ShaderFormatString(Input.ShaderFormat.ToString());
			Ar << ShaderFormatString;
			Input.ShaderFormat = FName(*ShaderFormatString);
		}
		Ar << Input.SourceFilePrefix;
		Ar << Input.SourceFilename;
		Ar << Input.EntryPointName;
		Ar << Input.DumpDebugInfoRootPath;
		Ar << Input.DumpDebugInfoPath;
		Ar << Input.Environment;

		bool bHasSharedEnvironment = IsValidRef(Input.SharedEnvironment);
		Ar << bHasSharedEnvironment;

		if (bHasSharedEnvironment)
		{
			if (Ar.IsSaving())
			{
				// Inline the shared environment when saving
				Ar << *(Input.SharedEnvironment);
			}
			 
			if (Ar.IsLoading())
			{
				// Create a new environment when loading, no sharing is happening anymore
				Input.SharedEnvironment = new FShaderCompilerEnvironment();
				Ar << *(Input.SharedEnvironment);
			}
		}
		
		return Ar;
	}
};

/** A shader compiler error or warning. */
struct FShaderCompilerError
{
	FShaderCompilerError(const TCHAR* InStrippedErrorMessage = TEXT(""))
	:	ErrorFile(TEXT(""))
	,	ErrorLineString(TEXT(""))
	,	StrippedErrorMessage(InStrippedErrorMessage)
	{}

	FString ErrorFile;
	FString ErrorLineString;
	FString StrippedErrorMessage;

	FString GetErrorString() const
	{
		return ErrorFile + TEXT("(") + ErrorLineString + TEXT("): ") + StrippedErrorMessage;
	}

	friend FArchive& operator<<(FArchive& Ar,FShaderCompilerError& Error)
	{
		return Ar << Error.ErrorFile << Error.ErrorLineString << Error.StrippedErrorMessage;
	}
};

/** The output of the shader compiler. */
struct FShaderCompilerOutput
{
	FShaderCompilerOutput()
	:	NumInstructions(0)
	,	NumTextureSamplers(0)
	,	bSucceeded(false)
	{
	}

	FShaderParameterMap ParameterMap;
	TArray<FShaderCompilerError> Errors;
	FShaderTarget Target;
	TArray<uint8> Code;
	FSHAHash OutputHash;
	uint32 NumInstructions;
	uint32 NumTextureSamplers;
	bool bSucceeded;

	/** Generates OutputHash from the compiler output. */
	SHADERCORE_API void GenerateOutputHash();
	
	friend FArchive& operator<<(FArchive& Ar,FShaderCompilerOutput& Output)
	{
		// Note: this serialize is used to pass between UE4 and the shader compile worker, recompile both when modifying
		return Ar << Output.ParameterMap << Output.Errors << Output.Target << Output.Code << Output.NumInstructions << Output.NumTextureSamplers << Output.bSucceeded;
	}
};

/**
 * Converts an absolute or relative shader filename to a filename relative to
 * the shader directory.
 * @param InFilename - The shader filename.
 * @returns a filename relative to the shaders directory.
 */
extern SHADERCORE_API FString GetRelativeShaderFilename(const FString& InFilename);

/**
 * Loads the shader file with the given name.
 * @param Filename - The filename of the shader to load.
 * @param OutFileContents - If true is returned, will contain the contents of the shader file.
 * @return True if the file was successfully loaded.
 */
extern SHADERCORE_API bool LoadShaderSourceFile(const TCHAR* Filename,FString& OutFileContents);

/** Loads the shader file with the given name.  If the shader file couldn't be loaded, throws a fatal error. */
extern SHADERCORE_API void LoadShaderSourceFileChecked(const TCHAR* Filename, FString& OutFileContents);

/**
 * Recursively populates IncludeFilenames with the include filenames from Filename
 */
extern SHADERCORE_API void GetShaderIncludes(const TCHAR* Filename, TArray<FString>& IncludeFilenames, uint32 DepthLimit=7);

/**
 * Calculates a Hash for the given filename if it does not already exist in the Hash cache.
 * @param Filename - shader file to Hash
 */
extern SHADERCORE_API const class FSHAHash& GetShaderFileHash(const TCHAR* Filename);

extern void BuildShaderFileToUniformBufferMap(TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables);

/**
 * Flushes the shader file and CRC cache, and regenerates the binary shader files if necessary.
 * Allows shader source files to be re-read properly even if they've been modified since startup.
 */
extern SHADERCORE_API void FlushShaderFileCache();

extern SHADERCORE_API void VerifyShaderSourceFiles();

struct FCachedUniformBufferDeclaration
{
	FString Declaration[SP_NumPlatforms];
};

/** Parses the given source file and its includes for references of uniform buffers, which are then stored in UniformBufferEntries. */
extern void GenerateReferencedUniformBuffers(
	const TCHAR* SourceFilename, 
	const TCHAR* ShaderTypeName, 
	const TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables,
	TMap<const TCHAR*,FCachedUniformBufferDeclaration>& UniformBufferEntries);

/** Records information about all the uniform buffer layouts referenced by UniformBufferEntries. */
extern SHADERCORE_API void SerializeUniformBufferInfo(class FShaderSaveArchive& Ar, const TMap<const TCHAR*,FCachedUniformBufferDeclaration>& UniformBufferEntries);

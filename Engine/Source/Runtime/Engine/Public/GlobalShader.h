// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GlobalShader.h: Shader manager definitions.
=============================================================================*/

#pragma once

#include "Shader.h"
#include "SceneManagement.h"

class FShaderCommonCompileJob;
class FShaderCompileJob;

/** Used to identify the global shader map in compile queues. */
extern ENGINE_API const int32 GlobalShaderMapId;

/** Class that encapsulates logic to create a DDC key for the global shader map. */
class FGlobalShaderMapId
{
public:

	/** Create a global shader map Id for the given platform. */
	FGlobalShaderMapId(EShaderPlatform Platform);

	/** Append to a string that will be used as a DDC key. */
	void AppendKeyString(FString& KeyString) const;

private:
	/** Shader types that this shader map is dependent on and their stored state. */
	TArray<FShaderTypeDependency> ShaderTypeDependencies;

	/** Shader pipeline types that this shader map is dependent on and their stored state. */
	TArray<FShaderPipelineTypeDependency> ShaderPipelineTypeDependencies;
};

/**
 * A shader meta type for the simplest shaders; shaders which are not material or vertex factory linked.
 * There should only a single instance of each simple shader type.
 */
class FGlobalShaderType : public FShaderType
{
public:

	typedef FShader::CompiledShaderInitializerType CompiledShaderInitializerType;
	typedef FShader* (*ConstructCompiledType)(const CompiledShaderInitializerType&);
	typedef bool (*ShouldCacheType)(EShaderPlatform);
	typedef void (*ModifyCompilationEnvironmentType)(EShaderPlatform, FShaderCompilerEnvironment&);

	FGlobalShaderType(
		const TCHAR* InName,
		const TCHAR* InSourceFilename,
		const TCHAR* InFunctionName,
		uint32 InFrequency,
		ConstructSerializedType InConstructSerializedRef,
		ConstructCompiledType InConstructCompiledRef,
		ModifyCompilationEnvironmentType InModifyCompilationEnvironmentRef,
		ShouldCacheType InShouldCacheRef,
		GetStreamOutElementsType InGetStreamOutElementsRef
		):
		FShaderType(EShaderTypeForDynamicCast::Global, InName, InSourceFilename, InFunctionName, InFrequency, InConstructSerializedRef, InGetStreamOutElementsRef),
		ConstructCompiledRef(InConstructCompiledRef),
		ShouldCacheRef(InShouldCacheRef),
		ModifyCompilationEnvironmentRef(InModifyCompilationEnvironmentRef)
	{}

	/**
	 * Enqueues compilation of a shader of this type.  
	 */
	ENGINE_API class FShaderCompileJob* BeginCompileShader(EShaderPlatform Platform, const FShaderPipelineType* ShaderPipeline, TArray<FShaderCommonCompileJob*>& NewJobs);

	/**
	 * Enqueues compilation of a shader pipeline of this type.  
	 */
	ENGINE_API static void BeginCompileShaderPipeline(EShaderPlatform Platform, const FShaderPipelineType* ShaderPipeline, const TArray<FGlobalShaderType*>& ShaderStages, TArray<FShaderCommonCompileJob*>& NewJobs);

	/** Either returns an equivalent existing shader of this type, or constructs a new instance. */
	FShader* FinishCompileShader(const FShaderCompileJob& CompileJob, const FShaderPipelineType* ShaderPipelineType);

	/**
	 * Checks if the shader type should be cached for a particular platform.
	 * @param Platform - The platform to check.
	 * @return True if this shader type should be cached.
	 */
	bool ShouldCache(EShaderPlatform Platform) const
	{
		return (*ShouldCacheRef)(Platform);
	}

	/**
	 * Sets up the environment used to compile an instance of this shader type.
	 * @param Platform - Platform to compile for.
	 * @param Environment - The shader compile environment that the function modifies.
	 */
	void SetupCompileEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& Environment)
	{
		// Allow the shader type to modify its compile environment.
		(*ModifyCompilationEnvironmentRef)(Platform, Environment);
	}

private:
	ConstructCompiledType ConstructCompiledRef;
	ShouldCacheType ShouldCacheRef;
	ModifyCompilationEnvironmentType ModifyCompilationEnvironmentRef;
};

/**
 * FGlobalShader
 * 
 * Global shaders derive from this class to set their default recompile group as a global one
 */
class FGlobalShader : public FShader
{
	DECLARE_SHADER_TYPE(FGlobalShader,Global);
public:

	FGlobalShader() : FShader() {}

	ENGINE_API FGlobalShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer);
	
	template<typename ShaderRHIParamRef, typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FSceneView& View)
	{
		const auto& ViewUniformBufferParameter = GetUniformBufferParameter<FViewUniformShaderParameters>();
		const auto& BuiltinSamplersUBParameter = GetUniformBufferParameter<FBuiltinSamplersParameters>();
		CheckShaderIsValid();
		SetUniformBufferParameter(RHICmdList, ShaderRHI, ViewUniformBufferParameter, View.ViewUniformBuffer);
#if USE_GBuiltinSamplersUniformBuffer
		SetUniformBufferParameter(RHICmdList, ShaderRHI, BuiltinSamplersUBParameter, GBuiltinSamplersUniformBuffer.GetUniformBufferRHI());
#endif
	}

	typedef void (*ModifyCompilationEnvironmentType)(EShaderPlatform, FShaderCompilerEnvironment&);

	// FShader interface.
	ENGINE_API static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment) {}
};

/**
 * An internal dummy pixel shader to use when the user calls RHISetPixelShader(NULL).
 */
class FNULLPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FNULLPS,Global,ENGINE_API);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	FNULLPS( )	{ }
	FNULLPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
	}
};

/**
* Container for Backup/RestoreGlobalShaderMap functions.
* Includes shader data from any populated feature levels.
*/
struct FGlobalShaderBackupData
{
	TScopedPointer<TArray<uint8>> FeatureLevelShaderData[ERHIFeatureLevel::Num];
};

/** Backs up all global shaders to memory through serialization, and removes all references to FShaders from the global shader map. */
extern ENGINE_API void BackupGlobalShaderMap(FGlobalShaderBackupData& OutGlobalShaderBackup);

/** Recreates shaders in the global shader map from the serialized memory. */
extern ENGINE_API void RestoreGlobalShaderMap(const FGlobalShaderBackupData& GlobalShaderData);

/**
 * Makes sure all global shaders are loaded and/or compiled for the passed in platform.
 * Note: if compilation is needed, this only kicks off the compile.
 *
 * @param	Platform	Platform to verify global shaders for
 */
extern ENGINE_API void VerifyGlobalShaders(EShaderPlatform Platform, bool bLoadedFromCacheFile);

/**
 * Saves the global shader map as a file for the target platform. 
 * @return the name of the file written
 */
extern ENGINE_API FString SaveGlobalShaderFile(EShaderPlatform Platform, FString SavePath);

/**
 * Accesses the global shader map.  This is a global TShaderMap<FGlobalShaderType> which contains an instance of each global shader type.
 *
 * @param Platform Which platform's global shader map to use
 * @param bRefreshShaderMap If true, the existing global shader map will be tossed first
 * @return A reference to the global shader map.
 */
extern ENGINE_API TShaderMap<FGlobalShaderType>* GetGlobalShaderMap(EShaderPlatform Platform, bool bRefreshShaderMap = false);

/**
  * Overload for the above GetGlobalShaderMap which takes a feature level and translates to the appropriate shader platform
  *
  * @param FeatureLevel - Which feature levels shader map to use
  * @param bRefreshShaderMap If true, the existing global shader map will be tossed first
  * @return A reference to the global shader map.
  *
  **/
inline TShaderMap<FGlobalShaderType>* GetGlobalShaderMap(ERHIFeatureLevel::Type FeatureLevel, bool bRefreshShaderMap = false) 
{ 
	return GetGlobalShaderMap(GShaderPlatformForFeatureLevel[FeatureLevel], bRefreshShaderMap); 
}

/**
 * Forces a recompile of the global shaders.
 */
extern ENGINE_API void RecompileGlobalShaders();

/**
 * Begins recompiling the specified global shader types, and flushes their bound shader states.
 * FinishRecompileGlobalShaders must be called after this and before using the global shaders for anything.
 */
extern ENGINE_API void BeginRecompileGlobalShaders(const TArray<FShaderType*>& OutdatedShaderTypes, const TArray<const FShaderPipelineType*>& OutdatedShaderPipelineTypes, EShaderPlatform ShaderPlatform);

/** Finishes recompiling global shaders.  Must be called after BeginRecompileGlobalShaders. */
extern ENGINE_API void FinishRecompileGlobalShaders();

/** Called by the shader compiler to process completed global shader jobs. */
extern void ProcessCompiledGlobalShaders(const TArray<FShaderCommonCompileJob*>& CompilationResults);

extern ENGINE_API FString GetGlobalShaderMapDDCKey();

extern ENGINE_API FString GetMaterialShaderMapDDCKey();


/**
 * Recompiles global shaders and material shaders
 * rebuilds global shaders and also 
 * clears the cooked platform data for all materials if there is a global shader change detected
 * can be slow
 */
extern ENGINE_API bool RecompileChangedShadersForPlatform(const FString& PlatformName);


/** 
 * Recompiles global shaders
 *
 * @param PlatformName					Name of the Platform the shaders are compiled for
 * @param OutputDirectory				The directory the compiled data will be stored to
 * @param MaterialsToLoad				List of Materials that need to be loaded and compiled
 * @param SerializedShaderResources		Serialized shader resources
 * @param MeshMaterialMaps				Mesh material maps
 * @param ModifiedFiles					Returns the list of modified files if not NULL
 * @param bCompileChangedShaders		Whether to compile all changed shaders or the specific material that is passed
 **/
extern ENGINE_API void RecompileShadersForRemote( 
	const FString& PlatformName, 
	EShaderPlatform ShaderPlatform,
	const FString& OutputDirectory, 
	const TArray<FString>& MaterialsToLoad, 
	const TArray<uint8>& SerializedShaderResources, 
	TArray<uint8>* MeshMaterialMaps, 
	TArray<FString>* ModifiedFiles,
	bool bCompileChangedShaders = true);

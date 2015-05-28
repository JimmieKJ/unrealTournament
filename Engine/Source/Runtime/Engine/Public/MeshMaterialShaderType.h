// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshMaterialShader.h: Mesh material shader definitions.
=============================================================================*/

#pragma once

#include "Shader.h"
#include "MaterialShaderType.h"

/**
 * A shader meta type for material-linked shaders which use a vertex factory.
 */
class FMeshMaterialShaderType : public FShaderType
{
public:

	/**
	* Finds a FMeshMaterialShaderType by name.
	*/
	static FMeshMaterialShaderType* GetTypeByName(const FString& TypeName);

	struct CompiledShaderInitializerType : FMaterialShaderType::CompiledShaderInitializerType
	{
		FVertexFactoryType* VertexFactoryType;
		CompiledShaderInitializerType(
			FShaderType* InType,
			const FShaderCompilerOutput& CompilerOutput,
			FShaderResource* InResource,
			const FUniformExpressionSet& InUniformExpressionSet,
			const FSHAHash& InMaterialShaderMapHash,
			const FString& InDebugDescription,
			FVertexFactoryType* InVertexFactoryType
			):
			FMaterialShaderType::CompiledShaderInitializerType(InType,CompilerOutput,InResource,InUniformExpressionSet,InMaterialShaderMapHash,InVertexFactoryType,InDebugDescription),
			VertexFactoryType(InVertexFactoryType)
		{}
	};
	typedef FShader* (*ConstructCompiledType)(const CompiledShaderInitializerType&);
	typedef bool (*ShouldCacheType)(EShaderPlatform,const FMaterial*,const FVertexFactoryType* VertexFactoryType);
	typedef void (*ModifyCompilationEnvironmentType)(EShaderPlatform, const FMaterial*, FShaderCompilerEnvironment&);

	FMeshMaterialShaderType(
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
		FShaderType(InName,InSourceFilename,InFunctionName,InFrequency,InConstructSerializedRef,InGetStreamOutElementsRef),
		ConstructCompiledRef(InConstructCompiledRef),
		ShouldCacheRef(InShouldCacheRef),
		ModifyCompilationEnvironmentRef(InModifyCompilationEnvironmentRef)
	{}

	/**
	 * Enqueues a compilation for a new shader of this type.
	 * @param Platform - The platform to compile for.
	 * @param Material - The material to link the shader with.
	 * @param VertexFactoryType - The vertex factory to compile with.
	 */
	void BeginCompileShader(
		uint32 ShaderMapId,
		EShaderPlatform Platform,
		const FMaterial* Material,
		FShaderCompilerEnvironment* MaterialEnvironment,
		FVertexFactoryType* VertexFactoryType,
		TArray<FShaderCompileJob*>& NewJobs
		);

	/**
	 * Either creates a new instance of this type or returns an equivalent existing shader.
	 * @param Material - The material to link the shader with.
	 * @param CurrentJob - Compile job that was enqueued by BeginCompileShader.
	 */
	FShader* FinishCompileShader(
		const FUniformExpressionSet& UniformExpressionSet, 
		const FSHAHash& MaterialShaderMapHash,
		const FShaderCompileJob& CurrentJob,
		const FString& InDebugDescription
		);

	/**
	 * Checks if the shader type should be cached for a particular platform, material, and vertex factory type.
	 * @param Platform - The platform to check.
	 * @param Material - The material to check.
	 * @param VertexFactoryType - The vertex factory type to check.
	 * @return True if this shader type should be cached.
	 */
	bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType) const
	{
		return (*ShouldCacheRef)(Platform,Material,VertexFactoryType);
	}

	// Dynamic casting.
	virtual FMeshMaterialShaderType* GetMeshMaterialShaderType() { return this; }

protected:

	/**
	 * Sets up the environment used to compile an instance of this shader type.
	 * @param Platform - Platform to compile for.
	 * @param Environment - The shader compile environment that the function modifies.
	 */
	void SetupCompileEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& Environment)
	{
		// Allow the shader type to modify its compile environment.
		(*ModifyCompilationEnvironmentRef)(Platform, Material, Environment);
	}

private:
	ConstructCompiledType ConstructCompiledRef;
	ShouldCacheType ShouldCacheRef;
	ModifyCompilationEnvironmentType ModifyCompilationEnvironmentRef;
};


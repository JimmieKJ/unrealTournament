// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshMaterialShader.cpp: Mesh material shader implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "ShaderCompiler.h"
#include "MeshMaterialShaderType.h"


static inline bool ShouldCacheMeshShader(const FMeshMaterialShaderType* ShaderType, EShaderPlatform Platform, const FMaterial* Material, FVertexFactoryType* InVertexFactoryType)
{
	return ShaderType->ShouldCache(Platform, Material, InVertexFactoryType) &&
		Material->ShouldCache(Platform, ShaderType, InVertexFactoryType) &&
		InVertexFactoryType->ShouldCache(Platform, Material, ShaderType);
}

/**
* Finds a FMeshMaterialShaderType by name.
*/
FMeshMaterialShaderType* FMeshMaterialShaderType::GetTypeByName(const FString& TypeName)
{
	//#todo-rco: Unused?
	for(TLinkedList<FShaderType*>::TIterator It(FShaderType::GetTypeList()); It; It.Next())
	{
		FString CurrentTypeName = FString(It->GetName());
		FMeshMaterialShaderType* CurrentType = It->GetMeshMaterialShaderType();
		if (CurrentType && CurrentTypeName == TypeName)
		{
			return CurrentType;
		}
	}
	return NULL;
}

/**
 * Enqueues a compilation for a new shader of this type.
 * @param Platform - The platform to compile for.
 * @param Material - The material to link the shader with.
 * @param VertexFactoryType - The vertex factory to compile with.
 */
void FMeshMaterialShaderType::BeginCompileShader(
	uint32 ShaderMapId,
	EShaderPlatform Platform,
	const FMaterial* Material,
	FShaderCompilerEnvironment* MaterialEnvironment,
	FVertexFactoryType* VertexFactoryType,
	const FShaderPipelineType* ShaderPipeline,
	TArray<FShaderCommonCompileJob*>& NewJobs
	)
{
	FShaderCompileJob* NewJob = new FShaderCompileJob(ShaderMapId, VertexFactoryType, this);

	NewJob->Input.SharedEnvironment = MaterialEnvironment;
	FShaderCompilerEnvironment& ShaderEnvironment = NewJob->Input.Environment;

	// apply the vertex factory changes to the compile environment
	check(VertexFactoryType);
	VertexFactoryType->ModifyCompilationEnvironment(Platform, Material, ShaderEnvironment);

	//update material shader stats
	UpdateMaterialShaderCompilingStats(Material);

	UE_LOG(LogShaders, Verbose, TEXT("			%s"), GetName());
	
	// Allow the shader type to modify the compile environment.
	SetupCompileEnvironment(Platform, Material, ShaderEnvironment);

	bool bAllowDevelopmentShaderCompile = Material->GetAllowDevelopmentShaderCompile();

	// Compile the shader environment passed in with the shader type's source code.
	::GlobalBeginCompileShader(
		Material->GetFriendlyName(),
		VertexFactoryType,
		this,
		GetShaderFilename(),
		GetFunctionName(),
		FShaderTarget(GetFrequency(),Platform),
		NewJob,
		NewJobs,
		bAllowDevelopmentShaderCompile
		);
}

void FMeshMaterialShaderType::BeginCompileShaderPipeline(
	uint32 ShaderMapId,
	EShaderPlatform Platform,
	const FMaterial* Material,
	FShaderCompilerEnvironment* MaterialEnvironment,
	FVertexFactoryType* VertexFactoryType,
	const FShaderPipelineType* ShaderPipeline,
	const TArray<FMeshMaterialShaderType*>& ShaderStages,
	TArray<FShaderCommonCompileJob*>& NewJobs)
{
	check(ShaderStages.Num() > 0);
	check(ShaderPipeline);
	UE_LOG(LogShaders, Verbose, TEXT("	Pipeline: %s"), ShaderPipeline->GetName());

	// Add all the jobs as individual first, then add the dependencies into a pipeline job
	auto* NewPipelineJob = new FShaderPipelineCompileJob(ShaderMapId, ShaderPipeline, ShaderStages.Num());
	for (int32 Index = 0; Index < ShaderStages.Num(); ++Index)
	{
		auto* ShaderStage = ShaderStages[Index];
		ShaderStage->BeginCompileShader(ShaderMapId, Platform, Material, MaterialEnvironment, VertexFactoryType, ShaderPipeline, NewPipelineJob->StageJobs);
	}

	NewJobs.Add(NewPipelineJob);
}

/**
 * Either creates a new instance of this type or returns an equivalent existing shader.
 * @param Material - The material to link the shader with.
 * @param CurrentJob - Compile job that was enqueued by BeginCompileShader.
 */
FShader* FMeshMaterialShaderType::FinishCompileShader(
	const FUniformExpressionSet& UniformExpressionSet, 
	const FSHAHash& MaterialShaderMapHash,
	const FShaderCompileJob& CurrentJob,
	const FShaderPipelineType* ShaderPipeline,
	const FString& InDebugDescription)
{
	check(CurrentJob.bSucceeded);
	check(CurrentJob.VFType);

	FShaderType* SpecificType = CurrentJob.ShaderType->LimitShaderResourceToThisType() ? CurrentJob.ShaderType : NULL;

	// Reuse an existing resource with the same key or create a new one based on the compile output
	// This allows FShaders to share compiled bytecode and RHI shader references
	FShaderResource* Resource = FShaderResource::FindOrCreateShaderResource(CurrentJob.Output, SpecificType);

	// Find a shader with the same key in memory
	FShader* Shader = CurrentJob.ShaderType->FindShaderById(FShaderId(MaterialShaderMapHash, ShaderPipeline, CurrentJob.VFType, CurrentJob.ShaderType, CurrentJob.Input.Target));

	// There was no shader with the same key so create a new one with the compile output, which will bind shader parameters
	if (!Shader)
	{
		Shader = (*ConstructCompiledRef)(CompiledShaderInitializerType(this, CurrentJob.Output, Resource, UniformExpressionSet, MaterialShaderMapHash, InDebugDescription, ShaderPipeline, CurrentJob.VFType));
		CurrentJob.Output.ParameterMap.VerifyBindingsAreComplete(GetName(), (EShaderFrequency)CurrentJob.Output.Target.Frequency, CurrentJob.VFType);
	}

	return Shader;
}

/**
 * Enqueues compilation for all shaders for a material and vertex factory type.
 * @param Material - The material to compile shaders for.
 * @param VertexFactoryType - The vertex factory type to compile shaders for.
 * @param Platform - The platform to compile for.
 */
uint32 FMeshMaterialShaderMap::BeginCompile(
	uint32 ShaderMapId,
	const FMaterialShaderMapId& InShaderMapId, 
	const FMaterial* Material,
	FShaderCompilerEnvironment* MaterialEnvironment,
	EShaderPlatform Platform,
	TArray<FShaderCommonCompileJob*>& NewJobs
	)
{
	if (!VertexFactoryType)
	{
		return 0;
	}

	uint32 NumShadersPerVF = 0;

	for (TLinkedList<FShaderPipelineType*>::TIterator ShaderPipelineIt(FShaderPipelineType::GetTypeList());ShaderPipelineIt;ShaderPipelineIt.Next())
	{
		const FShaderPipelineType* Pipeline = *ShaderPipelineIt;
		if (Pipeline->IsMeshMaterialTypePipeline())
		{
			auto& Stages = Pipeline->GetStages();

			// Verify all the ShouldCache are in sync
			int32 NumShouldCache = 0;
			for (int32 Index = 0; Index < Stages.Num(); ++Index)
			{
				auto* ShaderType = Stages[Index]->GetMeshMaterialShaderType();
				if (ShouldCacheMeshShader(ShaderType, Platform, Material, VertexFactoryType))
				{
					++NumShouldCache;
				}
			}

			if (NumShouldCache > 0)
			{
				NumShadersPerVF += Stages.Num();
				if (NumShouldCache != Stages.Num())
				{
					//#todo-rco: Error or Warning?
					UE_LOG(LogShaders, Error, TEXT("Mismatched ShouldCache() function on the shaders for MeshMaterial ShaderPipeline %s"), Pipeline->GetName());
				}

				TArray<FMeshMaterialShaderType*> ShaderStagesToCompile;
				ShaderStagesToCompile.Reserve(SF_NumFrequencies);
				for (int32 Index = 0; Index < Stages.Num(); ++Index)
				{
					FMeshMaterialShaderType* ShaderType = (FMeshMaterialShaderType*)(Stages[Index]->GetMeshMaterialShaderType());

					// No need to ShouldCache again as we tested each stage beforehand

					// Verify that the shader map Id contains inputs for any shaders that will be put into this shader map
					check(InShaderMapId.ContainsVertexFactoryType(VertexFactoryType));
					check(InShaderMapId.ContainsShaderType(ShaderType));
					ShaderStagesToCompile.Add(ShaderType);
				}

				//#todo-rco: Check if the MeshShaderMap already contains this pipeline

				// Make a pipeline job with all the stages
				FMeshMaterialShaderType::BeginCompileShaderPipeline(ShaderMapId, Platform, Material, MaterialEnvironment, VertexFactoryType, Pipeline, ShaderStagesToCompile, NewJobs);
			}
		}
	}

	// Iterate over all mesh material shader types.
	for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		FMeshMaterialShaderType* ShaderType = ShaderTypeIt->GetMeshMaterialShaderType();
		if (ShaderType && ShouldCacheMeshShader(ShaderType, Platform, Material, VertexFactoryType))
		{
			// Verify that the shader map Id contains inputs for any shaders that will be put into this shader map
			check(InShaderMapId.ContainsVertexFactoryType(VertexFactoryType));
			check(InShaderMapId.ContainsShaderType(ShaderType));

			NumShadersPerVF++;
			// only compile the shader if we don't already have it
			if (!HasShader(ShaderType))
			{
			    // Compile this mesh material shader for this material and vertex factory type.
				ShaderType->BeginCompileShader(
					ShaderMapId,
					Platform,
					Material,
					MaterialEnvironment,
					VertexFactoryType,
					nullptr,
					NewJobs
					);
			}
		}
	}

	if (NumShadersPerVF > 0)
	{
		UE_LOG(LogShaders, Verbose, TEXT("			%s - %u shaders"), VertexFactoryType->GetName(), NumShadersPerVF);
	}

	return NumShadersPerVF;
}

/**
 * Creates shaders for all of the compile jobs and caches them in this shader map.
 * @param Material - The material to compile shaders for.
 * @param CompilationResults - The compile results that were enqueued by BeginCompile.
 */
void FMeshMaterialShaderMap::FinishCompile(uint32 ShaderMapId, const FUniformExpressionSet& UniformExpressionSet, const FSHAHash& MaterialShaderMapHash, const TArray<FShaderCompileJob*>& CompilationResults, const FString& InDebugDescription)
{
	//#todo-rco: Unused?
	// Find the matching FMeshMaterialShaderType for each compile job
	for (int32 JobIndex = 0; JobIndex < CompilationResults.Num(); JobIndex++)
	{
		const FShaderCompileJob& CurrentJob = *CompilationResults[JobIndex];
		if (CurrentJob.Id == ShaderMapId && CurrentJob.VFType == VertexFactoryType)
		{
			for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
			{
				FMeshMaterialShaderType* MeshMaterialShaderType = ShaderTypeIt->GetMeshMaterialShaderType();
				if (*ShaderTypeIt == CurrentJob.ShaderType && MeshMaterialShaderType != NULL)
				{
					//#todo-rco: Pipeline?
					FShader* Shader = MeshMaterialShaderType->FinishCompileShader(UniformExpressionSet, MaterialShaderMapHash, CurrentJob, nullptr, InDebugDescription);
					check(Shader);
					AddShader(MeshMaterialShaderType,Shader);
				}
			}
		}
	}
}

bool FMeshMaterialShaderMap::IsComplete(
	const FMeshMaterialShaderMap* MeshShaderMap,
	EShaderPlatform Platform,
	const FMaterial* Material,
	FVertexFactoryType* InVertexFactoryType,
	bool bSilent
	)
{
	auto IsMeshShaderComplete = [&](const FMeshMaterialShaderType* ShaderType, const FShaderPipelineType* Pipeline) -> bool
		{
			// If we should cache this shader then the map is empty IF
			//		The shadermap is empty
			//		OR If it doesn't have a pipeline and needs one
			//		OR it's not in the shadermap
			//#todo-rco: Remove cast
			if (ShouldCacheMeshShader(ShaderType, Platform, Material, InVertexFactoryType) &&
				(!MeshShaderMap || (Pipeline && !MeshShaderMap->GetShaderPipeline(Pipeline)) || (!Pipeline && !MeshShaderMap->HasShader((FShaderType*)ShaderType))))
			{
				if (!bSilent)
				{
					if (Pipeline)
					{
						UE_LOG(LogShaders, Warning, TEXT("Incomplete material %s, missing pipeline %s from %s."), *Material->GetFriendlyName(), Pipeline->GetName(), InVertexFactoryType->GetName());
					}
					else
					{
						UE_LOG(LogShaders, Warning, TEXT("Incomplete material %s, missing %s from %s."), *Material->GetFriendlyName(), ShaderType->GetName(), InVertexFactoryType->GetName());
					}
				}
				return false;
			}

			return true;
		};

	// Iterate over all pipeline types
	for (TLinkedList<FShaderPipelineType*>::TIterator ShaderPipelineIt(FShaderPipelineType::GetTypeList());ShaderPipelineIt;ShaderPipelineIt.Next())
	{
		const FShaderPipelineType* ShaderPipelineType = *ShaderPipelineIt;
		if (ShaderPipelineType->IsMeshMaterialTypePipeline())
		{
			auto& Stages = ShaderPipelineType->GetStages();

			// Verify all the ShouldCache are in sync
			int32 NumShouldCache = 0;
			for (int32 Index = 0; Index < Stages.Num(); ++Index)
			{
				auto* ShaderType = Stages[Index]->GetMeshMaterialShaderType();
				if (ShouldCacheMeshShader(ShaderType, Platform, Material, InVertexFactoryType))
				{
					++NumShouldCache;
				}
			}

			if (NumShouldCache > 0 && NumShouldCache != Stages.Num())
			{
				//#todo-rco: Error or Warning?
				UE_LOG(LogShaders, Error, TEXT("Mismatched ShouldCache() function on the shaders for MeshMaterial ShaderPipeline %s"), ShaderPipelineType->GetName());
			}

			// Now check the completeness of the shader map
			for (int32 Index = 0; Index < Stages.Num(); ++Index)
			{
				auto* ShaderType = Stages[Index]->GetMeshMaterialShaderType();
				if (ShaderType && !IsMeshShaderComplete(ShaderType, ShaderPipelineType))
				{
					return false;
				}
			}
		}
	}

	// Iterate over all mesh material shader types.
	for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		FMeshMaterialShaderType* ShaderType = ShaderTypeIt->GetMeshMaterialShaderType();
		if (ShaderType && !IsMeshShaderComplete(ShaderType, nullptr))
		{
			return false;
		}
	}

	return true;
}

void FMeshMaterialShaderMap::LoadMissingShadersFromMemory(
	const FSHAHash& MaterialShaderMapHash, 
	const FMaterial* Material, 
	EShaderPlatform Platform)
{
	//#todo-rco: Pipelines
	for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		FMeshMaterialShaderType* ShaderType = ShaderTypeIt->GetMeshMaterialShaderType();
		//#todo-rco: Pass FNullShaderPipeline instead of nullptr
		const bool bShaderAlreadyExists = HasShader(ShaderType);

		if (ShaderType && ShouldCacheMeshShader(ShaderType, Platform, Material, VertexFactoryType) && !bShaderAlreadyExists) 
		{
			const FShaderId ShaderId(MaterialShaderMapHash, nullptr, VertexFactoryType, ShaderType, FShaderTarget(ShaderType->GetFrequency(), Platform));
			FShader* FoundShader = ShaderType->FindShaderById(ShaderId);	

			if (FoundShader)
			{
				AddShader(ShaderType, FoundShader);
			}
		}
	}
}

/**
 * Removes all entries in the cache with exceptions based on a shader type
 * @param ShaderType - The shader type to flush
 */
void FMeshMaterialShaderMap::FlushShadersByShaderType(FShaderType* ShaderType)
{
	if (ShaderType->GetMeshMaterialShaderType())
	{
		RemoveShaderType(ShaderType);
	}
}

void FMeshMaterialShaderMap::FlushShadersByShaderPipelineType(const FShaderPipelineType* ShaderPipelineType)
{
	if (ShaderPipelineType->IsMeshMaterialTypePipeline())
	{
		RemoveShaderPipelineType(ShaderPipelineType);
	}
}

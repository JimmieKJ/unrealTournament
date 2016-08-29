// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GlobalShader.cpp: Global shader implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "GlobalShader.h"
#include "StaticBoundShaderState.h"
#include "ShaderCompiler.h"
#include "DerivedDataCacheInterface.h"
#include "ShaderDerivedDataVersion.h"
#include "TargetPlatform.h"
#include "CookStats.h"

#if ENABLE_COOK_STATS
namespace GlobalShaderCookStats
{
	FCookStats::FDDCResourceUsageStats UsageStats;
	static int32 ShadersCompiled = 0;

	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		UsageStats.LogStats(AddStat, TEXT("GlobalShader.Usage"), TEXT(""));
		AddStat(TEXT("GlobalShader.Misc"), FCookStatsManager::CreateKeyValueArray(
			TEXT("ShadersCompiled"), ShadersCompiled
			));
	});
}
#endif

/** The global shader map. */
TShaderMap<FGlobalShaderType>* GGlobalShaderMap[SP_NumPlatforms];

IMPLEMENT_SHADER_TYPE(,FNULLPS,TEXT("NullPixelShader"),TEXT("Main"),SF_Pixel);

/** Used to identify the global shader map in compile queues. */
const int32 GlobalShaderMapId = 0;

FGlobalShaderMapId::FGlobalShaderMapId(EShaderPlatform Platform)
{
	TArray<FShaderType*> ShaderTypes;
	TArray<const FShaderPipelineType*> ShaderPipelineTypes;

	for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList()); ShaderTypeIt; ShaderTypeIt.Next())
	{
		FGlobalShaderType* GlobalShaderType = ShaderTypeIt->GetGlobalShaderType();
		if (GlobalShaderType && GlobalShaderType->ShouldCache(Platform))
		{
			ShaderTypes.Add(GlobalShaderType);
		}
	}

	for (TLinkedList<FShaderPipelineType*>::TIterator ShaderPipelineIt(FShaderPipelineType::GetTypeList()); ShaderPipelineIt; ShaderPipelineIt.Next())
	{
		const FShaderPipelineType* Pipeline = *ShaderPipelineIt;
		if (Pipeline->IsGlobalTypePipeline())
		{
			int32 NumStagesNeeded = 0;
			auto& StageTypes = Pipeline->GetStages();
			for (const FShaderType* Shader : StageTypes)
			{
				const FGlobalShaderType* GlobalShaderType = Shader->GetGlobalShaderType();
				if (GlobalShaderType->ShouldCache(Platform))
				{
					++NumStagesNeeded;
				}
				else
				{
					break;
				}
			}

			if (NumStagesNeeded == StageTypes.Num())
			{
				ShaderPipelineTypes.Add(Pipeline);
			}
		}
	}

	// Individual shader dependencies
	ShaderTypes.Sort(FCompareShaderTypes());
	for (int32 TypeIndex = 0; TypeIndex < ShaderTypes.Num(); TypeIndex++)
	{
		FShaderTypeDependency Dependency;
		Dependency.ShaderType = ShaderTypes[TypeIndex];
		Dependency.SourceHash = ShaderTypes[TypeIndex]->GetSourceHash();
		ShaderTypeDependencies.Add(Dependency);
	}

	// Shader pipeline dependencies
	ShaderPipelineTypes.Sort(FCompareShaderPipelineNameTypes());
	for (int32 TypeIndex = 0; TypeIndex < ShaderPipelineTypes.Num(); TypeIndex++)
	{
		const FShaderPipelineType* Pipeline = ShaderPipelineTypes[TypeIndex];
		FShaderPipelineTypeDependency Dependency;
		Dependency.ShaderPipelineType = Pipeline;
		Dependency.StagesSourceHash = Pipeline->GetSourceHash();
		ShaderPipelineTypeDependencies.Add(Dependency);
	}
}

void FGlobalShaderMapId::AppendKeyString(FString& KeyString) const
{
	TMap<const TCHAR*,FCachedUniformBufferDeclaration> ReferencedUniformBuffers;

	for (int32 ShaderIndex = 0; ShaderIndex < ShaderTypeDependencies.Num(); ShaderIndex++)
	{
		const FShaderTypeDependency& ShaderTypeDependency = ShaderTypeDependencies[ShaderIndex];

		KeyString += TEXT("_");
		KeyString += ShaderTypeDependency.ShaderType->GetName();

		// Add the type's source hash so that we can invalidate cached shaders when .usf changes are made
		KeyString += ShaderTypeDependency.SourceHash.ToString();

		// Add the serialization history to the key string so that we can detect changes to global shader serialization without a corresponding .usf change
		ShaderTypeDependency.ShaderType->GetSerializationHistory().AppendKeyString(KeyString);

		const TMap<const TCHAR*,FCachedUniformBufferDeclaration>& ReferencedUniformBufferStructsCache = ShaderTypeDependency.ShaderType->GetReferencedUniformBufferStructsCache();

		// Gather referenced uniform buffers
		for (TMap<const TCHAR*,FCachedUniformBufferDeclaration>::TConstIterator It(ReferencedUniformBufferStructsCache); It; ++It)
		{
			ReferencedUniformBuffers.Add(It.Key(), It.Value());
		}
	}

	for (int32 Index = 0; Index < ShaderPipelineTypeDependencies.Num(); ++Index)
	{
		const FShaderPipelineTypeDependency& Dependency = ShaderPipelineTypeDependencies[Index];

		KeyString += TEXT("_");
		KeyString += Dependency.ShaderPipelineType->GetName();

		// Add the type's source hash so that we can invalidate cached shaders when .usf changes are made
		KeyString += Dependency.StagesSourceHash.ToString();

		for (const FShaderType* ShaderType : Dependency.ShaderPipelineType->GetStages())
		{
			const TMap<const TCHAR*, FCachedUniformBufferDeclaration>& ReferencedUniformBufferStructsCache = ShaderType->GetReferencedUniformBufferStructsCache();

			// Gather referenced uniform buffers
			for (TMap<const TCHAR*, FCachedUniformBufferDeclaration>::TConstIterator It(ReferencedUniformBufferStructsCache); It; ++It)
			{
				ReferencedUniformBuffers.Add(It.Key(), It.Value());
			}
		}
	}

	{
		TArray<uint8> TempData;
		FSerializationHistory SerializationHistory;
		FMemoryWriter Ar(TempData, true);
		FShaderSaveArchive SaveArchive(Ar, SerializationHistory);

		// Save uniform buffer member info so we can detect when layout has changed
		SerializeUniformBufferInfo(SaveArchive, ReferencedUniformBuffers);

		SerializationHistory.AppendKeyString(KeyString);
	}
}

FShaderCompileJob* FGlobalShaderType::BeginCompileShader(EShaderPlatform Platform, const FShaderPipelineType* ShaderPipeline, TArray<FShaderCommonCompileJob*>& NewJobs)
{
	FShaderCompileJob* NewJob = new FShaderCompileJob(GlobalShaderMapId, nullptr, this);
	FShaderCompilerEnvironment& ShaderEnvironment = NewJob->Input.Environment;

	UE_LOG(LogShaders, Verbose, TEXT("	%s"), GetName());
	COOK_STAT(GlobalShaderCookStats::ShadersCompiled++);

	// Allow the shader type to modify the compile environment.
	SetupCompileEnvironment(Platform, ShaderEnvironment);

	static FString GlobalName(TEXT("Global"));

	// Compile the shader environment passed in with the shader type's source code.
	::GlobalBeginCompileShader(
		GlobalName,
		nullptr,
		this,
		ShaderPipeline,
		GetShaderFilename(),
		GetFunctionName(),
		FShaderTarget(GetFrequency(),Platform),
		NewJob,
		NewJobs
		);

	return NewJob;
}

void FGlobalShaderType::BeginCompileShaderPipeline(EShaderPlatform Platform, const FShaderPipelineType* ShaderPipeline, const TArray<FGlobalShaderType*>& ShaderStages, TArray<FShaderCommonCompileJob*>& NewJobs)
{
	check(ShaderStages.Num() > 0);
	check(ShaderPipeline);
	UE_LOG(LogShaders, Verbose, TEXT("	Pipeline: %s"), ShaderPipeline->GetName());

	// Add all the jobs as individual first, then add the dependencies into a pipeline job
	auto* NewPipelineJob = new FShaderPipelineCompileJob(GlobalShaderMapId, ShaderPipeline, ShaderStages.Num());
	for (int32 Index = 0; Index < ShaderStages.Num(); ++Index)
	{
		auto* ShaderStage = ShaderStages[Index];
		ShaderStage->BeginCompileShader(Platform, ShaderPipeline, NewPipelineJob->StageJobs);
	}

	NewJobs.Add(NewPipelineJob);
}

FShader* FGlobalShaderType::FinishCompileShader(const FShaderCompileJob& CurrentJob, const FShaderPipelineType* ShaderPipelineType)
{
	FShader* Shader = nullptr;
	if (CurrentJob.bSucceeded)
	{
		FShaderType* SpecificType = CurrentJob.ShaderType->LimitShaderResourceToThisType() ? CurrentJob.ShaderType : nullptr;

		// Reuse an existing resource with the same key or create a new one based on the compile output
		// This allows FShaders to share compiled bytecode and RHI shader references
		FShaderResource* Resource = FShaderResource::FindOrCreateShaderResource(CurrentJob.Output, SpecificType);
		check(Resource);

		if (ShaderPipelineType && !ShaderPipelineType->ShouldOptimizeUnusedOutputs())
		{
			// If sharing shaders in this pipeline, remove it from the type/id so it uses the one in the shared shadermap list
			ShaderPipelineType = nullptr;
		}

		// Find a shader with the same key in memory
		Shader = CurrentJob.ShaderType->FindShaderById(FShaderId(GGlobalShaderMapHash, ShaderPipelineType, nullptr, CurrentJob.ShaderType, CurrentJob.Input.Target));

		// There was no shader with the same key so create a new one with the compile output, which will bind shader parameters
		if (!Shader)
		{
			Shader = (*ConstructCompiledRef)(CompiledShaderInitializerType(this, CurrentJob.Output, Resource, GGlobalShaderMapHash, ShaderPipelineType, nullptr));
			CurrentJob.Output.ParameterMap.VerifyBindingsAreComplete(GetName(), (EShaderFrequency)CurrentJob.Output.Target.Frequency, CurrentJob.VFType);
		}
	}

	static auto* CVarShowShaderWarnings = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ShowShaderCompilerWarnings"));
	if (CVarShowShaderWarnings && CVarShowShaderWarnings->GetInt() && CurrentJob.Output.Errors.Num() > 0)
	{
		UE_LOG(LogShaderCompilers, Warning, TEXT("Warnings compiling global shader %s %s %s:\n"), CurrentJob.ShaderType->GetName(), ShaderPipelineType ? TEXT("ShaderPipeline") : TEXT(""), ShaderPipelineType ? ShaderPipelineType->GetName() : TEXT(""));
		for (int32 ErrorIndex = 0; ErrorIndex < CurrentJob.Output.Errors.Num(); ErrorIndex++)
		{
			UE_LOG(LogShaderCompilers, Warning, TEXT("	%s"), *CurrentJob.Output.Errors[ErrorIndex].GetErrorString());
		}
	}

	return Shader;
}

FGlobalShader::FGlobalShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
:	FShader(Initializer)
{
}

void BackupGlobalShaderMap(FGlobalShaderBackupData& OutGlobalShaderBackup)
{
	for (int32 i = (int32)ERHIFeatureLevel::ES2; i < (int32)ERHIFeatureLevel::Num; ++i)
	{
		EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform((ERHIFeatureLevel::Type)i);
		if (ShaderPlatform < EShaderPlatform::SP_NumPlatforms && GGlobalShaderMap[ShaderPlatform] != nullptr)
		{
			TArray<uint8>* ShaderData = new TArray<uint8>();
			FMemoryWriter Ar(*ShaderData);
			GGlobalShaderMap[ShaderPlatform]->SerializeInline(Ar, true, true);
			GGlobalShaderMap[ShaderPlatform]->RegisterSerializedShaders();
			GGlobalShaderMap[ShaderPlatform]->Empty();
			OutGlobalShaderBackup.FeatureLevelShaderData[i] = ShaderData;
		}
	}

	// Remove cached references to global shaders
	for (TLinkedList<FGlobalBoundShaderStateResource*>::TIterator It(FGlobalBoundShaderStateResource::GetGlobalBoundShaderStateList()); It; It.Next())
	{
		BeginUpdateResourceRHI(*It);
	}
}

void RestoreGlobalShaderMap(const FGlobalShaderBackupData& GlobalShaderBackup)
{
	for (int32 i = (int32)ERHIFeatureLevel::ES2; i < (int32)ERHIFeatureLevel::Num; ++i)
	{
		EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform((ERHIFeatureLevel::Type)i);		
		if (GlobalShaderBackup.FeatureLevelShaderData[i] != nullptr
			&& ShaderPlatform < EShaderPlatform::SP_NumPlatforms
			&& GGlobalShaderMap[ShaderPlatform] != nullptr)
		{
			FMemoryReader Ar(*GlobalShaderBackup.FeatureLevelShaderData[i]);
			GGlobalShaderMap[ShaderPlatform]->SerializeInline(Ar, true, true);
			GGlobalShaderMap[ShaderPlatform]->RegisterSerializedShaders();
		}
	}
}

/**
 * Makes sure all global shaders are loaded and/or compiled for the passed in platform.
 * Note: if compilation is needed, this only kicks off the compile.
 *
 * @param	Platform	Platform to verify global shaders for
 */
void VerifyGlobalShaders(EShaderPlatform Platform, bool bLoadedFromCacheFile)
{
	check(IsInGameThread());
	check(!FPlatformProperties::IsServerOnly());
	check(GGlobalShaderMap[Platform]);

	UE_LOG(LogShaders, Log, TEXT("Verifying Global Shaders for %s"), *LegacyShaderPlatformToShaderFormat(Platform).ToString());

	// Ensure that the global shader map contains all global shader types.
	TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(Platform);
	const bool bEmptyMap = GlobalShaderMap->IsEmpty();
	if (bEmptyMap)
	{
		UE_LOG(LogShaders, Warning, TEXT("	Empty global shader map, recompiling all global shaders"));
	}
	
	bool bErrorOnMissing = bLoadedFromCacheFile;
	if (FPlatformProperties::RequiresCookedData())
	{
		// We require all shaders to exist on cooked platforms because we can't compile them.
		bErrorOnMissing = true;
	}

	// All jobs, single & pipeline
	TArray<FShaderCommonCompileJob*> GlobalShaderJobs;

	// Add the single jobs first
	TMap<FShaderType*, FShaderCompileJob*> SharedShaderJobs;
	for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		FGlobalShaderType* GlobalShaderType = ShaderTypeIt->GetGlobalShaderType();
		if (GlobalShaderType && GlobalShaderType->ShouldCache(Platform))
		{
			if (!GlobalShaderMap->HasShader(GlobalShaderType))
			{
				if (bErrorOnMissing)
				{
					UE_LOG(LogShaders, Fatal,TEXT("Missing global shader %s, Please make sure cooking was successful."), GlobalShaderType->GetName());
				}

				if (!bEmptyMap)
				{
					UE_LOG(LogShaders, Warning, TEXT("	%s"), GlobalShaderType->GetName());
				}
	
				// Compile this global shader type.
				auto* Job = GlobalShaderType->BeginCompileShader(Platform, nullptr, GlobalShaderJobs);
				check(!SharedShaderJobs.Find(GlobalShaderType));
				SharedShaderJobs.Add(GlobalShaderType, Job);
			}
		}
	}

	// Now the pipeline jobs; if it's a shareable pipeline, do not add duplicate jobs
	for (TLinkedList<FShaderPipelineType*>::TIterator ShaderPipelineIt(FShaderPipelineType::GetTypeList());ShaderPipelineIt;ShaderPipelineIt.Next())
	{
		const FShaderPipelineType* Pipeline = *ShaderPipelineIt;
		if (Pipeline->IsGlobalTypePipeline())
		{
			if (!GlobalShaderMap->GetShaderPipeline(Pipeline))
			{
				auto& StageTypes = Pipeline->GetStages();
				TArray<FGlobalShaderType*> ShaderStages;
				for (int32 Index = 0; Index < StageTypes.Num(); ++Index)
				{
					FGlobalShaderType* GlobalShaderType = ((FShaderType*)(StageTypes[Index]))->GetGlobalShaderType();
					if (GlobalShaderType->ShouldCache(Platform))
					{
						ShaderStages.Add(GlobalShaderType);
					}
					else
					{
						break;
					}
				}

				if (ShaderStages.Num() == StageTypes.Num())
				{
					if (bErrorOnMissing)
					{
						UE_LOG(LogShaders, Fatal, TEXT("Missing global shader pipeline %s, Please make sure cooking was successful."), Pipeline->GetName());
					}

					if (!bEmptyMap)
					{
						UE_LOG(LogShaders, Warning, TEXT("	%s"), Pipeline->GetName());
					}

					if (Pipeline->ShouldOptimizeUnusedOutputs())
					{
						// Make a pipeline job with all the stages
						FGlobalShaderType::BeginCompileShaderPipeline(Platform, Pipeline, ShaderStages, GlobalShaderJobs);
					}
					else
					{
						// If sharing shaders amongst pipelines, add this pipeline as a dependency of an existing individual job
						for (const FShaderType* ShaderType : StageTypes)
						{
							FShaderCompileJob** Job = SharedShaderJobs.Find(ShaderType);
							checkf(Job, TEXT("Couldn't find existing shared job for global shader %s on pipeline %s!"), ShaderType->GetName(), Pipeline->GetName());
							auto* SingleJob = (*Job)->GetSingleShaderJob();
							check(SingleJob);
							auto& SharedPipelinesInJob = SingleJob->SharingPipelines.FindOrAdd(nullptr);
							check(!SharedPipelinesInJob.Contains(Pipeline));
							SharedPipelinesInJob.Add(Pipeline);
						}
					}
				}
			}
		}
	}

	if (GlobalShaderJobs.Num() > 0)
	{
		GShaderCompilingManager->AddJobs(GlobalShaderJobs, true, true, false);

		const bool bAllowAsynchronousGlobalShaderCompiling =
			// OpenGL requires that global shader maps are compiled before attaching
			// primitives to the scene as it must be able to find FNULLPS.
			// TODO_OPENGL: Allow shaders to be compiled asynchronously.
			// Metal also needs this when using RHI thread because it uses TOneColorVS very early in RHIPostInit()
			!IsOpenGLPlatform(GMaxRHIShaderPlatform) && !IsVulkanPlatform(GMaxRHIShaderPlatform) &&
			(!IsMetalPlatform(GMaxRHIShaderPlatform) || !GUseRHIThread) &&
			GShaderCompilingManager->AllowAsynchronousShaderCompiling();

		if (!bAllowAsynchronousGlobalShaderCompiling)
		{
			TArray<int32> ShaderMapIds;
			ShaderMapIds.Add(GlobalShaderMapId);

			GShaderCompilingManager->FinishCompilation(TEXT("Global"), ShaderMapIds);
		}
	}
}

/** Serializes the global shader map to an archive. */
static void SerializeGlobalShaders(FArchive& Ar, TShaderMap<FGlobalShaderType>* GlobalShaderMap)
{
	check(IsInGameThread());

	// Serialize the global shader map binary file tag.
	static const uint32 ReferenceTag = 0x47534D42;
	if (Ar.IsLoading())
	{
		// Initialize Tag to 0 as it won't be written to if the serialize fails (ie the global shader cache file is empty)
		uint32 Tag = 0;
		Ar << Tag;
		checkf(Tag == ReferenceTag, TEXT("Global shader map binary file is missing GSMB tag."));
	}
	else
	{
		uint32 Tag = ReferenceTag;
		Ar << Tag;
	}

	// Serialize the global shaders.
	GlobalShaderMap->SerializeInline(Ar, true, false);
	// And now register them.
	GlobalShaderMap->RegisterSerializedShaders();
}

static FString GetGlobalShaderCacheFilename(EShaderPlatform Platform)
{
	return FString(TEXT("Engine")) / TEXT("GlobalShaderCache-") + LegacyShaderPlatformToShaderFormat(Platform).ToString() + TEXT(".bin");
}

/** Saves the global shader map as a file for the target platform. */
FString SaveGlobalShaderFile(EShaderPlatform Platform, FString SavePath)
{
	TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(Platform);

	// Wait until all global shaders are compiled
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->ProcessAsyncResults(false, true);
	}

	TArray<uint8> GlobalShaderData;
	FMemoryWriter MemoryWriter(GlobalShaderData);
	SerializeGlobalShaders(MemoryWriter, GlobalShaderMap);

	// make the final name
	FString FullPath = SavePath / GetGlobalShaderCacheFilename(Platform);
	if (!FFileHelper::SaveArrayToFile(GlobalShaderData, *FullPath))
	{
		UE_LOG(LogMaterial, Fatal, TEXT("Could not save global shader file to '%s'"), *FullPath);
	}

	return FullPath;
}

FString GetGlobalShaderMapDDCKey()
{
	return FString(GLOBALSHADERMAP_DERIVEDDATA_VER);
}

FString GetMaterialShaderMapDDCKey()
{
	return FString(MATERIALSHADERMAP_DERIVEDDATA_VER);
}

/** Creates a string key for the derived data cache entry for the global shader map. */
FString GetGlobalShaderMapKeyString(const FGlobalShaderMapId& ShaderMapId, EShaderPlatform Platform)
{
	FName Format = LegacyShaderPlatformToShaderFormat(Platform);
	FString ShaderMapKeyString = Format.ToString() + TEXT("_") + FString(FString::FromInt(GetTargetPlatformManagerRef().ShaderFormatVersion(Format))) + TEXT("_");
	ShaderMapAppendKeyString(Platform, ShaderMapKeyString);
	ShaderMapId.AppendKeyString(ShaderMapKeyString);
	return FDerivedDataCacheInterface::BuildCacheKey(TEXT("GSM"), GLOBALSHADERMAP_DERIVEDDATA_VER, *ShaderMapKeyString);
}

/** Saves the platform's shader map to the DDC. */
void SaveGlobalShaderMapToDerivedDataCache(EShaderPlatform Platform)
{
	// We've finally built the global shader map, so we can count the miss as we put it in the DDC.
	COOK_STAT(auto Timer = GlobalShaderCookStats::UsageStats.TimeSyncWork());
	TArray<uint8> SaveData;
	FMemoryWriter Ar(SaveData, true);
	SerializeGlobalShaders(Ar, GGlobalShaderMap[Platform]);

	FGlobalShaderMapId ShaderMapId(Platform);
	GetDerivedDataCacheRef().Put(*GetGlobalShaderMapKeyString(ShaderMapId, Platform), SaveData);
	COOK_STAT(Timer.AddMiss(SaveData.Num()));
}

TShaderMap<FGlobalShaderType>* GetGlobalShaderMap(EShaderPlatform Platform, bool bRefreshShaderMap)
{
	// No global shaders needed on dedicated server or clients that use NullRHI. Note that cook commandlet needs to have them, even if it is not allowed to render otherwise.
	if (FPlatformProperties::IsServerOnly() || (!IsRunningCommandlet() && !FApp::CanEverRender()))
	{
		if (!GGlobalShaderMap[Platform])
		{
			GGlobalShaderMap[Platform] = new TShaderMap<FGlobalShaderType>();
			return GGlobalShaderMap[Platform];
		}
		return nullptr;
	}

	if (bRefreshShaderMap)
	{
		// delete the current global shader map
		delete GGlobalShaderMap[Platform];
		GGlobalShaderMap[Platform] = nullptr;

		// make sure we look for updated shader source files
		FlushShaderFileCache();
	}

	// If the global shader map hasn't been created yet, create it.
	if(!GGlobalShaderMap[Platform])
	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("GetGlobalShaderMap"), STAT_GetGlobalShaderMap, STATGROUP_LoadTime);
		// GetGlobalShaderMap is called the first time during startup in the main thread.
		check(IsInGameThread());

		FScopedSlowTask SlowTask(70);

		// verify that all shader source files are intact
		SlowTask.EnterProgressFrame(20);
		VerifyShaderSourceFiles();

		GGlobalShaderMap[Platform] = new TShaderMap<FGlobalShaderType>();

		bool bLoadedFromCacheFile = false;

		// Try to load the global shaders from a local cache file if it exists
		// This method is used exclusively with cooked content, since the DDC is not present
		if (FPlatformProperties::RequiresCookedData())
		{
			SlowTask.EnterProgressFrame(50);

			TArray<uint8> GlobalShaderData;
			FString GlobalShaderCacheFilename = FPaths::GetRelativePathToRoot() / GetGlobalShaderCacheFilename(Platform);
			FPaths::MakeStandardFilename(GlobalShaderCacheFilename);
			bLoadedFromCacheFile = FFileHelper::LoadFileToArray(GlobalShaderData, *GlobalShaderCacheFilename, FILEREAD_Silent);

			if (!bLoadedFromCacheFile)
			{
				// Handle this gracefully and exit.
				FString SandboxPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*GlobalShaderCacheFilename);				
				// This can be too early to localize in some situations.
				const FText Message = FText::Format(NSLOCTEXT("Engine", "GlobalShaderCacheFileMissing", "The global shader cache file '{0}' is missing.\n\nYour application is built to load COOKED content. No COOKED content was found; This usually means you did not cook content for this build.\nIt also may indicate missing cooked data for a shader platform(e.g., OpenGL under Windows): Make sure your platform's packaging settings include this Targeted RHI.\n\nAlternatively build and run the UNCOOKED version instead."), FText::FromString( SandboxPath ) );
				if (FPlatformProperties::SupportsWindowedMode())
				{
					UE_LOG(LogMaterial, Error, TEXT("%s"), *Message.ToString());
					FMessageDialog::Open(EAppMsgType::Ok, Message);
					FPlatformMisc::RequestExit(false);
					return NULL;
				}
				else
				{
					UE_LOG(LogMaterial, Fatal, TEXT("%s"), *Message.ToString());
				}
			}

			FMemoryReader MemoryReader(GlobalShaderData);
			SerializeGlobalShaders(MemoryReader, GGlobalShaderMap[Platform]);
		}
		// Uncooked platform
		else
		{
			FGlobalShaderMapId ShaderMapId(Platform);

			TArray<uint8> CachedData;
			SlowTask.EnterProgressFrame(40);
			const FString DataKey = GetGlobalShaderMapKeyString(ShaderMapId, Platform);

			// Find the shader map in the derived data cache
			SlowTask.EnterProgressFrame(10);

			COOK_STAT(auto Timer = GlobalShaderCookStats::UsageStats.TimeSyncWork());
			if (GetDerivedDataCacheRef().GetSynchronous(*DataKey, CachedData))
			{
				COOK_STAT(Timer.AddHit(CachedData.Num()));
				FMemoryReader Ar(CachedData, true);

				// Deserialize from the cached data
				SerializeGlobalShaders(Ar, GGlobalShaderMap[Platform]);
			}
			else
			{
				// it's a miss, but we haven't built anything yet. Save the counting until we actually have it built.
				COOK_STAT(Timer.TrackCyclesOnly());
			}
		}

		// If any shaders weren't loaded, compile them now.
		VerifyGlobalShaders(Platform, bLoadedFromCacheFile);

		extern int32 GCreateShadersOnLoad;
		if (GCreateShadersOnLoad && Platform == GMaxRHIShaderPlatform)
		{
			for (auto& Pair : GGlobalShaderMap[Platform]->GetShaders())
			{
				FShader* Shader = Pair.Value;
				if (Shader)
				{
					Shader->BeginInitializeResources();
				}
			}
		}
	}
	return GGlobalShaderMap[Platform];
}

static inline bool ShouldCacheGlobalShaderTypeName(const FGlobalShaderType* GlobalShaderType, const TCHAR* TypeNameSubstring, EShaderPlatform Platform)
{
	return GlobalShaderType
		&& (TypeNameSubstring == nullptr || (FPlatformString::Strstr(GlobalShaderType->GetName(), TypeNameSubstring) != nullptr))
		&& GlobalShaderType->ShouldCache(Platform);
};


bool IsGlobalShaderMapComplete(const TCHAR* TypeNameSubstring)
{
	for (int32 i = 0; i < SP_NumPlatforms; ++i)
	{
		EShaderPlatform Platform = (EShaderPlatform)i;
		
		TShaderMap<FGlobalShaderType>* GlobalShaderMap = GGlobalShaderMap[Platform];

		if (GlobalShaderMap)
		{
			// Check if the individual shaders are complete
			for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList()); ShaderTypeIt; ShaderTypeIt.Next())
			{
				FGlobalShaderType* GlobalShaderType = ShaderTypeIt->GetGlobalShaderType();
				if (ShouldCacheGlobalShaderTypeName(GlobalShaderType, TypeNameSubstring, Platform))
				{
					if (!GlobalShaderMap->HasShader(GlobalShaderType))
					{
						return false;
					}
				}
			}

			// Then the pipelines as it may be sharing shaders
			for (TLinkedList<FShaderPipelineType*>::TIterator ShaderPipelineIt(FShaderPipelineType::GetTypeList()); ShaderPipelineIt; ShaderPipelineIt.Next())
			{
				const FShaderPipelineType* Pipeline = *ShaderPipelineIt;
				if (Pipeline->IsGlobalTypePipeline())
				{
					auto& Stages = Pipeline->GetStages();
					int32 NumStagesNeeded = 0;
					for (const FShaderType* Shader : Stages)
					{
						const FGlobalShaderType* GlobalShaderType = Shader->GetGlobalShaderType();
						if (ShouldCacheGlobalShaderTypeName(GlobalShaderType, TypeNameSubstring, Platform))
						{
							++NumStagesNeeded;
						}
						else
						{
							break;
						}
					}

					if (NumStagesNeeded == Stages.Num())
					{
						if (!GlobalShaderMap->GetShaderPipeline(Pipeline))
						{
							return false;
						}
					}
				}
			}
		}
	}
	
	return true;
}

/**
 * Forces a recompile of the global shaders.
 */
void RecompileGlobalShaders()
{
	if( !FPlatformProperties::RequiresCookedData() )
	{
		// Flush pending accesses to the existing global shaders.
		FlushRenderingCommands();

		UMaterialInterface::IterateOverActiveFeatureLevels([&](ERHIFeatureLevel::Type InFeatureLevel) {
			auto ShaderPlatform = GShaderPlatformForFeatureLevel[InFeatureLevel];
			GetGlobalShaderMap(ShaderPlatform)->Empty();
			VerifyGlobalShaders(ShaderPlatform, false);
		});

		GShaderCompilingManager->ProcessAsyncResults(false, true);

		//invalidate global bound shader states so they will be created with the new shaders the next time they are set (in SetGlobalBoundShaderState)
		for(TLinkedList<FGlobalBoundShaderStateResource*>::TIterator It(FGlobalBoundShaderStateResource::GetGlobalBoundShaderStateList());It;It.Next())
		{
			BeginUpdateResourceRHI(*It);
		}
	}
}

bool RecompileChangedShadersForPlatform( const FString& PlatformName )
{
	// figure out what shader platforms to recompile
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	ITargetPlatform* TargetPlatform = TPM->FindTargetPlatform(PlatformName);
	if (TargetPlatform == NULL)
	{
		UE_LOG(LogShaders, Display, TEXT("Failed to find target platform module for %s"), *PlatformName);
		return false;
	}

	TArray<FName> DesiredShaderFormats;
	TargetPlatform->GetAllTargetedShaderFormats(DesiredShaderFormats);



	// figure out which shaders are out of date
	TArray<FShaderType*> OutdatedShaderTypes;
	TArray<const FVertexFactoryType*> OutdatedFactoryTypes;
	TArray<const FShaderPipelineType*> OutdatedShaderPipelineTypes;

	// Pick up new changes to shader files
	FlushShaderFileCache();

	FShaderType::GetOutdatedTypes(OutdatedShaderTypes, OutdatedFactoryTypes);
	FShaderPipelineType::GetOutdatedTypes(OutdatedShaderTypes, OutdatedShaderPipelineTypes, OutdatedFactoryTypes);
	UE_LOG(LogShaders, Display, TEXT("We found %d out of date shader types, %d outdated pipeline types, and %d out of date VF types!"), OutdatedShaderTypes.Num(), OutdatedShaderPipelineTypes.Num(), OutdatedFactoryTypes.Num());

	for (int32 FormatIndex = 0; FormatIndex < DesiredShaderFormats.Num(); FormatIndex++)
	{
		// get the shader platform enum
		const EShaderPlatform ShaderPlatform = ShaderFormatToLegacyShaderPlatform(DesiredShaderFormats[FormatIndex]);

		// Only compile for the desired platform if requested
		// Kick off global shader recompiles
		BeginRecompileGlobalShaders(OutdatedShaderTypes, OutdatedShaderPipelineTypes, ShaderPlatform);
		
		// Block on global shaders
		FinishRecompileGlobalShaders();
#if WITH_EDITOR
		// we only want to actually compile mesh shaders if we have out of date ones
		if (OutdatedShaderTypes.Num() || OutdatedFactoryTypes.Num())
		{
			for (TObjectIterator<UMaterialInterface> It; It; ++It)
			{
				(*It)->ClearCachedCookedPlatformData(TargetPlatform);
			}
		}
#endif
	}

	if (OutdatedFactoryTypes.Num() || OutdatedShaderTypes.Num())
	{
		return true;
	}
	return false;
}



void RecompileShadersForRemote( 
	const FString& PlatformName,
	EShaderPlatform ShaderPlatformToCompile,
	const FString& OutputDirectory, 
	const TArray<FString>& MaterialsToLoad, 
	const TArray<uint8>& SerializedShaderResources, 
	TArray<uint8>* MeshMaterialMaps, 
	TArray<FString>* ModifiedFiles,
	bool bCompileChangedShaders )
{
	// figure out what shader platforms to recompile
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	ITargetPlatform* TargetPlatform = TPM->FindTargetPlatform(PlatformName);
	if (TargetPlatform == NULL)
	{
		UE_LOG(LogShaders, Display, TEXT("Failed to find target platform module for %s"), *PlatformName);
		return;
	}

	TArray<FName> DesiredShaderFormats;
	TargetPlatform->GetAllTargetedShaderFormats(DesiredShaderFormats);

	UE_LOG(LogShaders, Display, TEXT("Loading %d materials..."), MaterialsToLoad.Num());
	// make sure all materials the client has loaded will be processed
	TArray<UMaterialInterface*> MaterialsToCompile;

	for (int32 Index = 0; Index < MaterialsToLoad.Num(); Index++)
	{
		UE_LOG(LogShaders, Display, TEXT("   --> %s"), *MaterialsToLoad[Index]);
		MaterialsToCompile.Add(LoadObject<UMaterialInterface>(NULL, *MaterialsToLoad[Index]));
	}

	UE_LOG(LogShaders, Display, TEXT("  Done!"))

	// figure out which shaders are out of date
	TArray<FShaderType*> OutdatedShaderTypes;
	TArray<const FVertexFactoryType*> OutdatedFactoryTypes;
	TArray<const FShaderPipelineType*> OutdatedShaderPipelineTypes;

	// Pick up new changes to shader files
	FlushShaderFileCache();

	if( bCompileChangedShaders )
	{
		FShaderType::GetOutdatedTypes( OutdatedShaderTypes, OutdatedFactoryTypes );
		FShaderPipelineType::GetOutdatedTypes(OutdatedShaderTypes, OutdatedShaderPipelineTypes, OutdatedFactoryTypes);
		UE_LOG(LogShaders, Display, TEXT("We found %d out of date shader types, %d outdated pipeline types, and %d out of date VF types!"), OutdatedShaderTypes.Num(), OutdatedShaderPipelineTypes.Num(), OutdatedFactoryTypes.Num());
	}

	{
		for (int32 FormatIndex = 0; FormatIndex < DesiredShaderFormats.Num(); FormatIndex++)
		{
			// get the shader platform enum
			const EShaderPlatform ShaderPlatform = ShaderFormatToLegacyShaderPlatform(DesiredShaderFormats[FormatIndex]);

			// Only compile for the desired platform if requested
			if (ShaderPlatform == ShaderPlatformToCompile || ShaderPlatformToCompile == SP_NumPlatforms)
			{
				if( bCompileChangedShaders )
				{
					// Kick off global shader recompiles
					BeginRecompileGlobalShaders(OutdatedShaderTypes, OutdatedShaderPipelineTypes, ShaderPlatform);

					// Block on global shaders
					FinishRecompileGlobalShaders();
				}

				// we only want to actually compile mesh shaders if a client directly requested it, and there's actually some work to do
				if (MeshMaterialMaps != NULL && (OutdatedShaderTypes.Num() || OutdatedFactoryTypes.Num() || bCompileChangedShaders == false))
				{
					TMap<FString, TArray<TRefCountPtr<FMaterialShaderMap> > > CompiledShaderMaps;
					UMaterial::CompileMaterialsForRemoteRecompile(MaterialsToCompile, ShaderPlatform, CompiledShaderMaps);

					// write the shader compilation info to memory, converting fnames to strings
					FMemoryWriter MemWriter(*MeshMaterialMaps, true);
					FNameAsStringProxyArchive Ar(MemWriter);

					// pull the serialized resource ids into an array of resources
					TArray<FShaderResourceId> ClientResourceIds;
					FMemoryReader MemReader(SerializedShaderResources, true);
					MemReader << ClientResourceIds;

					// save out the shader map to the byte array
					FMaterialShaderMap::SaveForRemoteRecompile(Ar, CompiledShaderMaps, ClientResourceIds);
				}

				// save it out so the client can get it (and it's up to date next time)
				FString GlobalShaderFilename = SaveGlobalShaderFile(ShaderPlatform, OutputDirectory);

				// add this to the list of files to tell the other end about
				if (ModifiedFiles)
				{
					// need to put it in non-sandbox terms
					FString SandboxPath(GlobalShaderFilename);
					check(SandboxPath.StartsWith(OutputDirectory));
					SandboxPath.ReplaceInline(*OutputDirectory, TEXT("../../../"));
					FPaths::NormalizeFilename(SandboxPath);
					ModifiedFiles->Add(SandboxPath);
				}
			}
		}
	}
}

void BeginRecompileGlobalShaders(const TArray<FShaderType*>& OutdatedShaderTypes, const TArray<const FShaderPipelineType*>& OutdatedShaderPipelineTypes, EShaderPlatform ShaderPlatform)
{
	if( !FPlatformProperties::RequiresCookedData() )
	{
		// Flush pending accesses to the existing global shaders.
		FlushRenderingCommands();

		// Calling GetGlobalShaderMap will force starting the compile jobs if the map is empty (by calling VerifyGlobalShaders)
		TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(ShaderPlatform);

		// Now check if there is any work to be done wrt outdates types
		if (OutdatedShaderTypes.Num() > 0 || OutdatedShaderPipelineTypes.Num() > 0)
		{
			for (int32 TypeIndex = 0; TypeIndex < OutdatedShaderTypes.Num(); TypeIndex++)
			{
				FGlobalShaderType* CurrentGlobalShaderType = OutdatedShaderTypes[TypeIndex]->GetGlobalShaderType();
				if (CurrentGlobalShaderType)
				{
					UE_LOG(LogShaders, Log, TEXT("Flushing Global Shader %s"), CurrentGlobalShaderType->GetName());
					GlobalShaderMap->RemoveShaderType(CurrentGlobalShaderType);
				}
			}

			for (int32 PipelineTypeIndex = 0; PipelineTypeIndex < OutdatedShaderPipelineTypes.Num(); ++PipelineTypeIndex)
			{
				const FShaderPipelineType* ShaderPipelineType = OutdatedShaderPipelineTypes[PipelineTypeIndex];
				if (ShaderPipelineType->IsGlobalTypePipeline())
				{
					UE_LOG(LogShaders, Log, TEXT("Flushing Global Shader Pipeline %s"), ShaderPipelineType->GetName());
					GlobalShaderMap->RemoveShaderPipelineType(ShaderPipelineType);
				}
			}

			//invalidate global bound shader states so they will be created with the new shaders the next time they are set (in SetGlobalBoundShaderState)
			for (TLinkedList<FGlobalBoundShaderStateResource*>::TIterator It(FGlobalBoundShaderStateResource::GetGlobalBoundShaderStateList());It;It.Next())
			{
				BeginUpdateResourceRHI(*It);
			}

			VerifyGlobalShaders(ShaderPlatform, false);
		}
	}
}

void FinishRecompileGlobalShaders()
{
	// Block until global shaders have been compiled and processed
	GShaderCompilingManager->ProcessAsyncResults(false, true);
}

static inline FShader* ProcessCompiledJob(FShaderCompileJob* SingleJob, const FShaderPipelineType* Pipeline, TArray<EShaderPlatform>& ShaderPlatformsProcessed, TArray<const FShaderPipelineType*>& OutSharedPipelines)
{
	FGlobalShaderType* GlobalShaderType = SingleJob->ShaderType->GetGlobalShaderType();
	check(GlobalShaderType);
	FShader* Shader = GlobalShaderType->FinishCompileShader(*SingleJob, Pipeline);
	if (Shader)
	{
		// Add the new global shader instance to the global shader map if it's a shared shader
		EShaderPlatform Platform = (EShaderPlatform)SingleJob->Input.Target.Platform;
		if (!Pipeline || !Pipeline->ShouldOptimizeUnusedOutputs())
		{
			GGlobalShaderMap[Platform]->AddShader(GlobalShaderType, Shader);
			// Add this shared pipeline to the list
			if (!Pipeline)
			{
				auto* JobSharedPipelines = SingleJob->SharingPipelines.Find(nullptr);
				if (JobSharedPipelines)
				{
					for (auto* SharedPipeline : *JobSharedPipelines)
					{
						OutSharedPipelines.AddUnique(SharedPipeline);
					}
				}
			}
		}
		ShaderPlatformsProcessed.AddUnique(Platform);
	}
	else
	{
		UE_LOG(LogShaders, Fatal, TEXT("Failed to compile global shader %s %s %s.  Enable 'r.ShaderDevelopmentMode' in ConsoleVariables.ini for retries."),
			GlobalShaderType->GetName(),
			Pipeline ? TEXT("for pipeline") : TEXT(""),
			Pipeline ? Pipeline->GetName() : TEXT(""));
	}

	return Shader;
};

void ProcessCompiledGlobalShaders(const TArray<FShaderCommonCompileJob*>& CompilationResults)
{
	UE_LOG(LogShaders, Warning, TEXT("Compiled %u global shaders"), CompilationResults.Num());

	TArray<EShaderPlatform> ShaderPlatformsProcessed;
	TArray<const FShaderPipelineType*> SharedPipelines;

	for (int32 ResultIndex = 0; ResultIndex < CompilationResults.Num(); ResultIndex++)
	{
		const FShaderCommonCompileJob& CurrentJob = *CompilationResults[ResultIndex];
		FShaderCompileJob* SingleJob = nullptr;
		if ((SingleJob = (FShaderCompileJob*)CurrentJob.GetSingleShaderJob()) != nullptr)
		{
			ProcessCompiledJob(SingleJob, nullptr, ShaderPlatformsProcessed, SharedPipelines);
		}
		else
		{
			const auto* PipelineJob = CurrentJob.GetShaderPipelineJob();
			check(PipelineJob);
			TArray<FShader*> ShaderStages;
			for (int32 Index = 0; Index < PipelineJob->StageJobs.Num(); ++Index)
			{
				SingleJob = PipelineJob->StageJobs[Index]->GetSingleShaderJob();
				FShader* Shader = ProcessCompiledJob(SingleJob, PipelineJob->ShaderPipeline, ShaderPlatformsProcessed, SharedPipelines);
				ShaderStages.Add(Shader);
			}

			FShaderPipeline* ShaderPipeline = new FShaderPipeline(PipelineJob->ShaderPipeline, ShaderStages);
			if (ShaderPipeline)
			{
				EShaderPlatform Platform = (EShaderPlatform)PipelineJob->StageJobs[0]->GetSingleShaderJob()->Input.Target.Platform;
				check(ShaderPipeline && !GGlobalShaderMap[Platform]->HasShaderPipeline(ShaderPipeline->PipelineType));
				GGlobalShaderMap[Platform]->AddShaderPipeline(PipelineJob->ShaderPipeline, ShaderPipeline);
			}
		}
	}

	for (int32 PlatformIndex = 0; PlatformIndex < ShaderPlatformsProcessed.Num(); PlatformIndex++)
	{
		{
			// Process the shader pipelines that share shaders
			EShaderPlatform Platform = ShaderPlatformsProcessed[PlatformIndex];
			auto* GlobalShaderMap = GGlobalShaderMap[Platform];
			for (const FShaderPipelineType* ShaderPipelineType : SharedPipelines)
			{
				check(ShaderPipelineType->IsGlobalTypePipeline());
				if (!GlobalShaderMap->HasShaderPipeline(ShaderPipelineType))
				{
					auto& StageTypes = ShaderPipelineType->GetStages();
					TArray<FShader*> ShaderStages;
					for (int32 Index = 0; Index < StageTypes.Num(); ++Index)
					{
						FGlobalShaderType* GlobalShaderType = ((FShaderType*)(StageTypes[Index]))->GetGlobalShaderType();
						if (GlobalShaderType->ShouldCache(Platform))
						{
							FShader* Shader = GlobalShaderMap->GetShader(GlobalShaderType);
							check(Shader);
							ShaderStages.Add(Shader);
						}
						else
						{
							break;
						}
					}

					checkf(StageTypes.Num() == ShaderStages.Num(), TEXT("Internal Error adding Global ShaderPipeline %s"), ShaderPipelineType->GetName());
					FShaderPipeline* ShaderPipeline = new FShaderPipeline(ShaderPipelineType, ShaderStages);
					GlobalShaderMap->AddShaderPipeline(ShaderPipelineType, ShaderPipeline);
				}
			}
		}

		// Save the global shader map for any platforms that were recompiled
		SaveGlobalShaderMapToDerivedDataCache(ShaderPlatformsProcessed[PlatformIndex]);
	}
}

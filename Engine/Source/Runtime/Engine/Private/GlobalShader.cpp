// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

/** The global shader map. */
TShaderMap<FGlobalShaderType>* GGlobalShaderMap[SP_NumPlatforms];

IMPLEMENT_SHADER_TYPE(,FNULLPS,TEXT("NullPixelShader"),TEXT("Main"),SF_Pixel);

/** Used to identify the global shader map in compile queues. */
const int32 GlobalShaderMapId = 0;

FGlobalShaderMapId::FGlobalShaderMapId(EShaderPlatform Platform)
{
	TArray<FShaderType*> ShaderTypes;

	for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList()); ShaderTypeIt; ShaderTypeIt.Next())
	{
		FGlobalShaderType* GlobalShaderType = ShaderTypeIt->GetGlobalShaderType();

		if (GlobalShaderType && GlobalShaderType->ShouldCache(Platform))
		{
			ShaderTypes.Add(GlobalShaderType);
		}
	}

	ShaderTypes.Sort(FCompareShaderTypes());

	for (int32 TypeIndex = 0; TypeIndex < ShaderTypes.Num(); TypeIndex++)
	{
		FShaderTypeDependency Dependency;
		Dependency.ShaderType = ShaderTypes[TypeIndex];
		Dependency.SourceHash = ShaderTypes[TypeIndex]->GetSourceHash();
		ShaderTypeDependencies.Add(Dependency);
	}
}

void FGlobalShaderMapId::AppendKeyString(FString& KeyString) const
{
	TMap<const TCHAR*,FCachedUniformBufferDeclaration> ReferencedUniformBuffers;

	for (int32 ShaderIndex = 0; ShaderIndex < ShaderTypeDependencies.Num(); ShaderIndex++)
	{
		KeyString += TEXT("_");
		const FShaderTypeDependency& ShaderTypeDependency = ShaderTypeDependencies[ShaderIndex];
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

void FGlobalShaderType::BeginCompileShader(EShaderPlatform Platform, TArray<FShaderCompileJob*>& NewJobs)
{
	FShaderCompileJob* NewJob = new FShaderCompileJob(GlobalShaderMapId, NULL, this);
	FShaderCompilerEnvironment& ShaderEnvironment = NewJob->Input.Environment;

	UE_LOG(LogShaders, Verbose, TEXT("	%s"), GetName());

	// Allow the shader type to modify the compile environment.
	SetupCompileEnvironment(Platform, ShaderEnvironment);

	static FString GlobalName(TEXT("Global"));

	// Compile the shader environment passed in with the shader type's source code.
	::GlobalBeginCompileShader(
		GlobalName,
		NULL,
		this,
		GetShaderFilename(),
		GetFunctionName(),
		FShaderTarget(GetFrequency(),Platform),
		NewJob,
		NewJobs
		);
}

FShader* FGlobalShaderType::FinishCompileShader(const FShaderCompileJob& CurrentJob)
{
	if (CurrentJob.bSucceeded)
	{
		FShaderType* SpecificType = CurrentJob.ShaderType->LimitShaderResourceToThisType() ? CurrentJob.ShaderType : NULL;

		// Reuse an existing resource with the same key or create a new one based on the compile output
		// This allows FShaders to share compiled bytecode and RHI shader references
		FShaderResource* Resource = FShaderResource::FindOrCreateShaderResource(CurrentJob.Output, SpecificType);
		check(Resource);

		// Find a shader with the same key in memory
		FShader* Shader = CurrentJob.ShaderType->FindShaderById(FShaderId(GGlobalShaderMapHash, NULL, CurrentJob.ShaderType, CurrentJob.Input.Target));

		// There was no shader with the same key so create a new one with the compile output, which will bind shader parameters
		if (!Shader)
		{
			Shader = (*ConstructCompiledRef)(CompiledShaderInitializerType(this, CurrentJob.Output, Resource, GGlobalShaderMapHash, NULL));
			CurrentJob.Output.ParameterMap.VerifyBindingsAreComplete(GetName(), (EShaderFrequency)CurrentJob.Output.Target.Frequency, CurrentJob.VFType);
		}
		
		return Shader;
	}
	else
	{
		return NULL;
	}
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
	
	TArray<FShaderCompileJob*> GlobalShaderJobs;

	for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		FGlobalShaderType* GlobalShaderType = ShaderTypeIt->GetGlobalShaderType();
		if(GlobalShaderType && GlobalShaderType->ShouldCache(Platform))
		{
			if(!GlobalShaderMap->HasShader(GlobalShaderType))
			{
				bool bErrorOnMissing = bLoadedFromCacheFile;
				if (FPlatformProperties::RequiresCookedData())
				{
					// We require all shaders to exist on cooked platforms because we can't compile them.
					bErrorOnMissing = true;
				}
				if (bErrorOnMissing)
				{
					UE_LOG(LogShaders, Fatal,TEXT("Missing global shader %s, Please make sure cooking was successful."), GlobalShaderType->GetName());
				}

				if (!bEmptyMap)
				{
					UE_LOG(LogShaders, Warning, TEXT("	%s"), GlobalShaderType->GetName());
				}
	
				// Compile this global shader type.
				GlobalShaderType->BeginCompileShader(Platform, GlobalShaderJobs);
			}
		}
	}

	if (GlobalShaderJobs.Num() > 0)
	{
		GShaderCompilingManager->AddJobs(GlobalShaderJobs, true, true);

		const bool bAllowAsynchronousGlobalShaderCompiling =
			// OpenGL requires that global shader maps are compiled before attaching
			// primitives to the scene as it must be able to find FNULLPS.
			// TODO_OPENGL: Allow shaders to be compiled asynchronously.
			!IsOpenGLPlatform(GMaxRHIShaderPlatform) &&
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
void SerializeGlobalShaders(FArchive& Ar, TShaderMap<FGlobalShaderType>* GlobalShaderMap)
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
}

FString GetGlobalShaderCacheFilename(EShaderPlatform Platform)
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
	TArray<uint8> SaveData;
	FMemoryWriter Ar(SaveData, true);
	SerializeGlobalShaders(Ar, GGlobalShaderMap[Platform]);

	FGlobalShaderMapId ShaderMapId(Platform);
	GetDerivedDataCacheRef().Put(*GetGlobalShaderMapKeyString(ShaderMapId, Platform), SaveData);
}

TShaderMap<FGlobalShaderType>* GetGlobalShaderMap(EShaderPlatform Platform, bool bRefreshShaderMap)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("GetGlobalShaderMap"), STAT_GetGlobalShaderMap, STATGROUP_LoadTime);

	// No global shaders needed on dedicated server
	if (FPlatformProperties::IsServerOnly())
	{
		if (!GGlobalShaderMap[Platform])
		{
			GGlobalShaderMap[Platform] = new TShaderMap<FGlobalShaderType>();
			return GGlobalShaderMap[Platform];
		}
		return NULL;
	}

	if (bRefreshShaderMap)
	{
		// delete the current global shader map
		delete GGlobalShaderMap[Platform];
		GGlobalShaderMap[Platform] = NULL;

		// make sure we look for updated shader source files
		FlushShaderFileCache();
	}

	// If the global shader map hasn't been created yet, create it.
	if(!GGlobalShaderMap[Platform])
	{
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
				const FText Message = FText::Format( NSLOCTEXT("Engine", "GlobalShaderCacheFileMissing", "The global shader cache file '{0}' is missing.\n\nYou're running a version of the application built to load COOKED content only, however no COOKED content was found. Consider cooking content for this build, or build and run the UNCOOKED version of the application instead."), FText::FromString( SandboxPath ) );
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
			if (GetDerivedDataCacheRef().GetSynchronous(*DataKey, CachedData))
			{
				FMemoryReader Ar(CachedData, true);

				// Deserialize from the cached data
				SerializeGlobalShaders(Ar, GGlobalShaderMap[Platform]);
			}
		}

		// If any shaders weren't loaded, compile them now.
		VerifyGlobalShaders(Platform, bLoadedFromCacheFile);

		extern int32 GCreateShadersOnLoad;
		if (GCreateShadersOnLoad && Platform == GMaxRHIShaderPlatform)
		{
			for (TMap<FShaderType*, TRefCountPtr<FShader> >::TConstIterator ShaderIt(GGlobalShaderMap[Platform]->GetShaders()); ShaderIt; ++ShaderIt)
			{
				FShader* Shader = ShaderIt.Value();

				if (Shader)
				{
					Shader->BeginInitializeResources();
				}
			}
		}
	}
	return GGlobalShaderMap[Platform];
}

bool IsGlobalShaderMapComplete(const TCHAR* TypeNameSubstring)
{
	for (int32 i = 0; i < SP_NumPlatforms; ++i)
	{
		EShaderPlatform Platform = (EShaderPlatform)i;
		
		TShaderMap<FGlobalShaderType>* GlobalShaderMap = GGlobalShaderMap[Platform];

		if (GlobalShaderMap)
		{
			for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList()); ShaderTypeIt; ShaderTypeIt.Next())
			{
				FGlobalShaderType* GlobalShaderType = ShaderTypeIt->GetGlobalShaderType();

				if (GlobalShaderType
					&& (TypeNameSubstring == nullptr || (FPlatformString::Strstr(GlobalShaderType->GetName(), TypeNameSubstring) != nullptr))
					&& GlobalShaderType->ShouldCache(Platform))
				{
					if (!GlobalShaderMap->HasShader(GlobalShaderType))
					{
						return false;
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

	// Pick up new changes to shader files
	FlushShaderFileCache();

	if( bCompileChangedShaders )
	{
		FShaderType::GetOutdatedTypes( OutdatedShaderTypes, OutdatedFactoryTypes );
		UE_LOG( LogShaders, Display, TEXT( "We found %d out of date shader types, and %d out of date VF types!" ), OutdatedShaderTypes.Num(), OutdatedFactoryTypes.Num() );
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
					BeginRecompileGlobalShaders( OutdatedShaderTypes, ShaderPlatform );

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

void BeginRecompileGlobalShaders(const TArray<FShaderType*>& OutdatedShaderTypes, EShaderPlatform ShaderPlatform)
{
	if( !FPlatformProperties::RequiresCookedData() )
	{
		// Flush pending accesses to the existing global shaders.
		FlushRenderingCommands();

		TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(ShaderPlatform);

		for (int32 TypeIndex = 0; TypeIndex < OutdatedShaderTypes.Num(); TypeIndex++)
		{
			FGlobalShaderType* CurrentGlobalShaderType = OutdatedShaderTypes[TypeIndex]->GetGlobalShaderType();
			if (CurrentGlobalShaderType)
			{
				UE_LOG(LogShaders, Log, TEXT("Flushing Global Shader %s"), CurrentGlobalShaderType->GetName());
				GlobalShaderMap->RemoveShaderType(CurrentGlobalShaderType);
				
				//invalidate global bound shader states so they will be created with the new shaders the next time they are set (in SetGlobalBoundShaderState)
				for(TLinkedList<FGlobalBoundShaderStateResource*>::TIterator It(FGlobalBoundShaderStateResource::GetGlobalBoundShaderStateList());It;It.Next())
				{
					BeginUpdateResourceRHI(*It);
				}
			}
		}

		VerifyGlobalShaders(ShaderPlatform, false);
	}
}

void FinishRecompileGlobalShaders()
{
	// Block until global shaders have been compiled and processed
	GShaderCompilingManager->ProcessAsyncResults(false, true);
}

void ProcessCompiledGlobalShaders(const TArray<FShaderCompileJob*>& CompilationResults)
{
	UE_LOG(LogShaders, Warning, TEXT("Compiled %u global shaders"), CompilationResults.Num());

	TArray<EShaderPlatform> ShaderPlatformsProcessed;

	for (int32 ResultIndex = 0; ResultIndex < CompilationResults.Num(); ResultIndex++)
	{
		const FShaderCompileJob& CurrentJob = *CompilationResults[ResultIndex];
		FGlobalShaderType* GlobalShaderType = CurrentJob.ShaderType->GetGlobalShaderType();
		check(GlobalShaderType);
		FShader* Shader = GlobalShaderType->FinishCompileShader(CurrentJob);

		if (Shader)
		{
			// Add the new global shader instance to the global shader map.
			EShaderPlatform Platform = (EShaderPlatform)CurrentJob.Input.Target.Platform;
			GGlobalShaderMap[Platform]->AddShader(GlobalShaderType,Shader);
			ShaderPlatformsProcessed.AddUnique(Platform);
		}
		else
		{
			UE_LOG(LogShaders, Fatal,TEXT("Failed to compile global shader %s.  Enable 'r.ShaderDevelopmentMode' in ConsoleVariables.ini for retries."), GlobalShaderType->GetName());
		}
	}

	for (int32 PlatformIndex = 0; PlatformIndex < ShaderPlatformsProcessed.Num(); PlatformIndex++)
	{
		// Save the global shader map for any platforms that were recompiled
		SaveGlobalShaderMapToDerivedDataCache(ShaderPlatformsProcessed[PlatformIndex]);
	}
}

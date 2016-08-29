// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCache.cpp: Bound shader state cache implementation.
=============================================================================*/

#include "ShaderCorePrivatePCH.h"
#include "ShaderCore.h"
#include "ShaderCache.h"
#include "Shader.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "EngineVersion.h"

DECLARE_STATS_GROUP(TEXT("Shader Cache"),STATGROUP_ShaderCache, STATCAT_Advanced);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Shaders Cached"),STATGROUP_NumShadersCached,STATGROUP_ShaderCache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num BSS Cached"),STATGROUP_NumBSSCached,STATGROUP_ShaderCache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num New Draw-States Cached"),STATGROUP_NumDrawsCached,STATGROUP_ShaderCache);
DECLARE_DWORD_COUNTER_STAT(TEXT("Shaders Precompiled"),STATGROUP_NumPrecompiled,STATGROUP_ShaderCache);
DECLARE_DWORD_COUNTER_STAT(TEXT("Shaders Predrawn"),STATGROUP_NumPredrawn,STATGROUP_ShaderCache);
DECLARE_DWORD_COUNTER_STAT(TEXT("Draw States Predrawn"),STATGROUP_NumStatesPredrawn,STATGROUP_ShaderCache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Total Shaders Precompiled"),STATGROUP_TotalPrecompiled,STATGROUP_ShaderCache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Total Shaders Predrawn"),STATGROUP_TotalPredrawn,STATGROUP_ShaderCache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Total Draw States Predrawn"),STATGROUP_TotalStatesPredrawn,STATGROUP_ShaderCache);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num To Precompile Per Frame"),STATGROUP_NumToPrecompile,STATGROUP_ShaderCache);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Binary Cache Load Time (s)"),STATGROUP_BinaryCacheLoadTime,STATGROUP_ShaderCache);

const FGuid FShaderCacheCustomVersion::Key(0xB954F018, 0xC9624DD6, 0xA74E79B1, 0x8EA113C2);
const FGuid FShaderCacheCustomVersion::GameKey(0x03D4EB48, 0xB50B4CC3, 0xA598DE41, 0x5C6CC993);
FCustomVersionRegistration GRegisterShaderCacheVersion(FShaderCacheCustomVersion::Key, FShaderCacheCustomVersion::Latest, TEXT("ShaderCacheVersion"));
FCustomVersionRegistration GRegisterShaderCacheGameVersion(FShaderCacheCustomVersion::GameKey, 0, TEXT("ShaderCacheGameVersion"));
#if WITH_EDITOR
static TCHAR const* GShaderCacheFileName = TEXT("EditorDrawCache.ushadercache");
static TCHAR const* GShaderCodeCacheFileName = TEXT("EditorCodeCache.ushadercode");
#else
static TCHAR const* GShaderCacheFileName = TEXT("DrawCache.ushadercache");
static TCHAR const* GShaderCodeCacheFileName = TEXT("ByteCodeCache.ushadercode");
#endif

#if WITH_EDITORONLY_DATA
static TCHAR const* GCookedCodeCacheFileName = TEXT("ByteCodeCache.ushadercode");
#endif

// Only the Mac build defaults to using the shader cache for now, Editor uses a separate cache from the game to avoid ever-growing cache being propagated to the game.
int32 FShaderCache::bUseShaderCaching = 0;
FAutoConsoleVariableRef FShaderCache::CVarUseShaderCaching(
	TEXT("r.UseShaderCaching"),
	bUseShaderCaching,
	TEXT("If true, log all shaders & bound-shader-states, so that they may be instantiated in the RHI on deserialisation rather than waiting for first use."),
	ECVF_ReadOnly|ECVF_RenderThreadSafe
	);

// Predrawing takes an existing shader cache with draw log & renders each shader + draw-state combination before use to avoid in-driver recompilation
// This requires plenty of setup & is done in batches at frame-end.
int32 FShaderCache::bUseShaderPredraw = 0;
FAutoConsoleVariableRef FShaderCache::CVarUseShaderPredraw(
	TEXT("r.UseShaderPredraw"),
	bUseShaderPredraw,
	TEXT("Use an existing draw-log to predraw shaders in batches before being used to reduce hitches due to in-driver recompilation."),
	ECVF_ReadOnly|ECVF_RenderThreadSafe
	);

// The actual draw loggging is even more expensive as it has to cache all the RHI draw state & is disabled by default.
int32 FShaderCache::bUseShaderDrawLog = 0;
FAutoConsoleVariableRef FShaderCache::CVarUseShaderDrawLog(
	TEXT("r.UseShaderDrawLog"),
	bUseShaderDrawLog,
	TEXT("If true, log all the draw states used for each shader pipeline, so that they may be pre-drawn in batches (see: r.UseShaderPredraw). This can be expensive & should be used only when generating the shader cache."),
	ECVF_ReadOnly|ECVF_RenderThreadSafe
	);

// As predrawing can take significant time then batch the draws up into chunks defined by frame time
int32 FShaderCache::PredrawBatchTime = -1;
FAutoConsoleVariableRef FShaderCache::CVarPredrawBatchTime(
	TEXT("r.PredrawBatchTime"),
	PredrawBatchTime,
	TEXT("Time in ms to spend predrawing shaders each frame, or -1 to perform all predraws immediately."),
	ECVF_RenderThreadSafe
	);

// A separate cache of used shader binaries for even earlier submission - may be platform or even device specific.
int32 FShaderCache::bUseShaderBinaryCache = 0;
FAutoConsoleVariableRef FShaderCache::CVarUseShaderBinaryCache(
	TEXT("r.UseShaderBinaryCache"),
	bUseShaderBinaryCache,
	TEXT("If true generates & uses a separate cache of used shader binaries for even earlier submission - may be platform or even device specific. Defaults to false."),
	ECVF_ReadOnly|ECVF_RenderThreadSafe
	);

// Whether to try and perform shader precompilation asynchronously.
int32 FShaderCache::bUseAsyncShaderPrecompilation = 0;
FAutoConsoleVariableRef FShaderCache::CVarUseAsyncShaderPrecompilation(
	TEXT("r.UseAsyncShaderPrecompilation"),
	bUseAsyncShaderPrecompilation,
	TEXT("If true tries to perform inital shader precompilation asynchronously on a background thread. Defaults to false."),
	ECVF_ReadOnly|ECVF_RenderThreadSafe
	);

// As async precompile can take significant time specify a desired max. frame time that the cache will try to remain below while precompiling. We can't specify the time to spend directly as under GL compile operations are deferred and take no time on the user thread.
int32 FShaderCache::TargetPrecompileFrameTime = -1;
FAutoConsoleVariableRef FShaderCache::CVarTargetPrecompileFrameTime(
	TEXT("r.TargetPrecompileFrameTime"),
	TargetPrecompileFrameTime,
	TEXT("Upper limit in ms for total frame time while precompiling, allowing the shader cache to adjust how many shaders to precompile each frame. Defaults to -1 which will precompile all shaders immediately."),
	ECVF_RenderThreadSafe
	);

int32 FShaderCache::AccelPredrawBatchTime = 0;
FAutoConsoleVariableRef FShaderCache::CVarAccelPredrawBatchTime(
	TEXT("r.AccelPredrawBatchTime"),
	AccelPredrawBatchTime,
	TEXT("Override value for r.PredrawBatchTime when showing a loading-screen or similar to do more work while the player won't notice, or 0 to use r.PredrawBatchTime. Defaults to 0."),
	ECVF_RenderThreadSafe
	);

int32 FShaderCache::AccelTargetPrecompileFrameTime = 0;
FAutoConsoleVariableRef FShaderCache::CVarAccelTargetPrecompileFrameTime(
	TEXT("r.AccelTargetPrecompileFrameTime"),
	AccelTargetPrecompileFrameTime,
	TEXT("Override value for r.TargetPrecompileFrameTime when showing a loading-screen or similar to do more work while the player won't notice, or 0 to use r.TargetPrecompileFrameTime. Defaults to 0."),
	ECVF_RenderThreadSafe
	);

float FShaderCache::InitialShaderLoadTime = -1.f;
FAutoConsoleVariableRef FShaderCache::CVarInitialShaderLoadTime(
	TEXT("r.InitialShaderLoadTime"),
	InitialShaderLoadTime,
	TEXT("Time to spend loading the shader cache synchronously on startup before falling back to asynchronous precompilation/predraw. Defaults to -1 which will perform all work synchronously."),
	ECVF_RenderThreadSafe
	);

FShaderCache* FShaderCache::Cache = nullptr;
#if WITH_EDITORONLY_DATA
FShaderCache::FShaderCookCache* FShaderCache::CookCache = nullptr;
#endif
int32 FShaderCache::GameVersion = 0;
static double LoadTimeStart = 0;

static bool ShaderPlatformCanPrebindBoundShaderState(EShaderPlatform Platform)
{
	switch (Platform)
	{
		case SP_PCD3D_SM5:
		case SP_PS4:
		case SP_XBOXONE:
		case SP_PCD3D_SM4:
		case SP_PCD3D_ES2:
		case SP_METAL:
		case SP_OPENGL_SM4_MAC:
		case SP_METAL_MRT:
		case SP_METAL_SM5:
		case SP_METAL_MACES3_1:
		case SP_METAL_MACES2:
		{
			return true;
		}
		case SP_OPENGL_SM4:
		case SP_OPENGL_PCES2:
		case SP_OPENGL_SM5:
		case SP_OPENGL_ES2_ANDROID:
		case SP_OPENGL_ES2_WEBGL:
		case SP_OPENGL_ES2_IOS:
		case SP_OPENGL_ES31_EXT:
		case SP_OPENGL_ES3_1_ANDROID:
		default:
		{
			return false;
		}
	}
}

void FShaderCache::SetGameVersion(int32 InGameVersion)
{
	check(!Cache);
	GameVersion = InGameVersion;
}

void FShaderCache::InitShaderCache(uint32 Options, uint32 InMaxResources)
{
#if WITH_EDITORONLY_DATA
	check(!CookCache || (Options == SCO_Cooking && WITH_EDITORONLY_DATA));
#endif
	check(!Cache);
	checkf(!(Options & SCO_Cooking) || (Options == SCO_Cooking && WITH_EDITORONLY_DATA), TEXT("Binary shader cache cooking is only permitted on its own & within the editor/cooker."));
	
	// Set the GameVersion to the FEngineVersion::Current().GetChangelist() value if it wasn't set to ensure we don't load invalidated caches.
	if (GameVersion == 0)
	{
		GameVersion = (int32)FEngineVersion::Current().GetChangelist();
	}
	
	if(bUseShaderCaching)
	{
		if(!(Options & SCO_Cooking))
		{
			Cache = new FShaderCache(Options, InMaxResources);
		}
	}
#if WITH_EDITORONLY_DATA
	else if((Options & SCO_Cooking) && !CookCache)
	{
		CookCache = new FShaderCookCache;
	}
#endif
}

void FShaderCache::LoadBinaryCache()
{
	if(Cache && bUseShaderBinaryCache)
	{
		LoadTimeStart = FPlatformTime::Seconds();
		
		FString UserCodeCache = FPaths::GameSavedDir() / GShaderCodeCacheFileName;
		FString GameCodeCache = FPaths::GameContentDir() / GShaderCodeCacheFileName;
		
		// Try to load user cache, making sure that if we fail version test we still try game-content version.
		bool bLoadedCache = false;
		if ( IFileManager::Get().FileSize(*UserCodeCache) > 0 )
		{
			FArchive* BinaryShaderAr = IFileManager::Get().CreateFileReader(*UserCodeCache);
			
			if ( BinaryShaderAr != nullptr )
			{
				*BinaryShaderAr << Cache->CodeCache;
				
				if ( !BinaryShaderAr->IsError() && BinaryShaderAr->CustomVer(FShaderCacheCustomVersion::Key) == FShaderCacheCustomVersion::Latest && BinaryShaderAr->CustomVer(FShaderCacheCustomVersion::GameKey) == FShaderCache::GameVersion )
				{
					bLoadedCache = true;
				}
				
				delete BinaryShaderAr;
			}
		}
		
		// Fallback to game-content version.
		if ( !bLoadedCache && IFileManager::Get().FileSize(*GameCodeCache) > 0 )
		{
			FArchive* BinaryShaderAr = IFileManager::Get().CreateFileReader(*GameCodeCache);
			if ( BinaryShaderAr != nullptr )
			{
				*BinaryShaderAr << Cache->CodeCache;
				bLoadedCache = true;
				delete BinaryShaderAr;
			}
		}
		
		if ( bLoadedCache )
		{
			bool const bUseAsync = bUseAsyncShaderPrecompilation != 0;
			{
				double StartTime = FPlatformTime::Seconds();
				bUseAsyncShaderPrecompilation = false;
				
				for ( auto Entry : Cache->CodeCache.Shaders )
				{
					bool bUsable = FShaderResource::ArePlatformsCompatible(GMaxRHIShaderPlatform, Entry.Key.Platform);
					switch (Entry.Key.Frequency)
					{
						case SF_Geometry:
							bUsable &= RHISupportsGeometryShaders(GMaxRHIShaderPlatform);
							break;
							
						case SF_Hull:
						case SF_Domain:
							bUsable &= RHISupportsTessellation(GMaxRHIShaderPlatform);
							break;
							
						case SF_Compute:
							bUsable &= RHISupportsComputeShaders(GMaxRHIShaderPlatform);
							break;
							
						default:
							break;
					}
					
					if ( bUsable )
					{
						if(!bUseAsyncShaderPrecompilation)
						{
							TArray<uint8> Code;
							FArchiveLoadCompressedProxy DecompressArchive(Entry.Value, (ECompressionFlags)(COMPRESS_ZLIB));
							DecompressArchive << Code;
							
							Cache->InternalLogShader(Entry.Key.Platform, Entry.Key.Frequency, Entry.Key.SHAHash, Code);
						}
						else
						{
							Cache->ShadersToPrecompile.Emplace(Entry.Key);
						}
					}
					
					double Duration = FPlatformTime::Seconds() - StartTime;
					if(bUseAsync && InitialShaderLoadTime >= 0.0f && Duration >= InitialShaderLoadTime)
					{
						bUseAsyncShaderPrecompilation = bUseAsync;
					}
				}
			}
		}
	}
}

void FShaderCache::CacheCookedShaderPlatforms(FName PlatformName, TArray<FName> const& CachedShaderFormats)
{
#if WITH_EDITORONLY_DATA
	if(CookCache)
	{
		TSet<EShaderPlatform> Platforms;
		for(FName Format : CachedShaderFormats)
		{
			EShaderPlatform ShaderPlat = ShaderFormatToLegacyShaderPlatform(Format);
			CookCache->AllCachedPlatforms.Add(ShaderPlat);
			Platforms.Add(ShaderPlat);
		}
		CookCache->CachedPlatformShaderFormats.Add(PlatformName, Platforms);
	}
#endif
}

void FShaderCache::SaveBinaryCache(FString OutputDir, FName PlatformName)
{
	if((bUseShaderBinaryCache && Cache)
#if WITH_EDITORONLY_DATA
	   || CookCache
#endif
	   )
	{
		FShaderCodeCache Temporary;
		FShaderCodeCache* Source = nullptr;
		
		FString OutputName = GShaderCodeCacheFileName;
		if(Cache)
		{
			Source = &Cache->CodeCache;
		}
#if WITH_EDITORONLY_DATA
		else if (CookCache)
		{
			OutputName = GCookedCodeCacheFileName;
			Source = &CookCache->CodeCache;
			
			TSet<EShaderPlatform> Platforms = CookCache->CachedPlatformShaderFormats.FindRef(PlatformName);
			
			if(Platforms.Num() != 0)
			{
				// Copy only the required shaders
				for(auto Shader : Source->Shaders)
				{
					if(Platforms.Contains(Shader.Key.Platform))
					{
						Temporary.Shaders.Add(Shader.Key, Shader.Value);
					}
				}
				
				// Use temp as source
				Source = &Temporary;
			}
		}
#endif
		check(Source);
		
		if(Source->Shaders.Num() > 0)
		{
			FString BinaryShaderFile = OutputDir / OutputName;
			FArchive* BinaryShaderAr = IFileManager::Get().CreateFileWriter(*BinaryShaderFile);
			if( BinaryShaderAr != NULL )
			{
				*BinaryShaderAr << *Source;
				BinaryShaderAr->Flush();
				delete BinaryShaderAr;
			}
		}
	}
}

void FShaderCache::ShutdownShaderCache()
{
	if (Cache)
	{
		delete Cache;
		Cache = nullptr;
	}
#if WITH_EDITORONLY_DATA
	if (CookCache)
	{
		delete CookCache;
		CookCache = nullptr;
	}
#endif
}

FArchive& operator<<( FArchive& Ar, FShaderCache::FShaderCaches& Info )
{
	uint32 CacheVersion = Ar.IsLoading() ? ((uint32)~0u) : ((uint32)FShaderCacheCustomVersion::Latest);
	uint32 GameVersion = Ar.IsLoading() ? ((uint32)~0u) : ((uint32)FShaderCache::GameVersion);
	
	Ar << CacheVersion;
	if ( !Ar.IsError() && CacheVersion == FShaderCacheCustomVersion::Latest )
	{
		Ar << GameVersion;
		
		if ( !Ar.IsError() && GameVersion == FShaderCache::GameVersion )
		{
			Ar << Info.PlatformCaches;
		}
	}
	return Ar;
}

FArchive& operator<<( FArchive& Ar, FShaderCache::FShaderCodeCache& Info )
{
	uint32 CacheVersion = Ar.IsLoading() ? (uint32)~0u : ((uint32)FShaderCacheCustomVersion::Latest);
	uint32 GameVersion = Ar.IsLoading() ? (uint32)~0u : ((uint32)FShaderCache::GetGameVersion());
	
	Ar << CacheVersion;
	if ( !Ar.IsError() && CacheVersion == FShaderCacheCustomVersion::Latest )
	{
		Ar << GameVersion;
		
		if ( !Ar.IsError() && GameVersion == FShaderCache::GetGameVersion() )
		{
			Ar << Info.Shaders;
			Ar << Info.Pipelines;
		}
	}
	return Ar;
}

FShaderCache::FShaderCache(uint32 InOptions, uint32 InMaxResources)
: FTickableObjectRenderThread(!(InOptions & SCO_Cooking))
, StreamingKey(0)
, bCurrentDepthStencilTarget(false)
, CurrentNumRenderTargets(0)
, CurrentShaderState(nullptr)
, bIsPreDraw(false)
, bIsPreBind(false)
, Options(InOptions)
, MaxResources(InMaxResources)
, InvalidResourceCount(0)
, OverridePrecompileTime(0)
, OverridePredrawBatchTime(0)
, bBatchingPaused(false)
{
	check(!(Options & SCO_Cooking));
	check(InMaxResources <= FShaderDrawKey::MaxNumResources);
	
	Viewport[0] = Viewport[1] = Viewport[2] = Viewport[3] = 0;
	DepthRange[0] = DepthRange[1] = 0.0f;

	FString UserBinaryShaderFile = FPaths::GameSavedDir() / GShaderCacheFileName;
	FString GameBinaryShaderFile = FPaths::GameContentDir() / GShaderCacheFileName;

	// Try to load user cache, making sure that if we fail version test we still try game-content version.
	bool bLoadedUserCache = LoadShaderCache(UserBinaryShaderFile, &Caches);
	
	// Fallback to game-content version.
	if ( !bLoadedUserCache )
	{
		LoadShaderCache(GameBinaryShaderFile, &Caches);
	}
}

FShaderCache::~FShaderCache()
{
	FString BinaryShaderFile = FPaths::GameSavedDir() / GShaderCacheFileName;
	SaveShaderCache(BinaryShaderFile, &Caches);
	
	if(bUseShaderBinaryCache)
	{
		BinaryShaderFile = FPaths::GameSavedDir() / GShaderCodeCacheFileName;
		FArchive* BinaryShaderAr = IFileManager::Get().CreateFileWriter(*BinaryShaderFile);
		if( BinaryShaderAr != NULL )
		{
			*BinaryShaderAr << CodeCache;
			BinaryShaderAr->Flush();
			delete BinaryShaderAr;
		}
	}
}

FVertexShaderRHIRef FShaderCache::GetVertexShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code)
{
	FShaderCacheKey Key;
	Key.Platform = Platform;
	Key.Frequency = SF_Vertex;
	Key.SHAHash = Hash;
	Key.bActive = true;
	FVertexShaderRHIRef Shader = CachedVertexShaders.FindRef(Key);
	if(!IsValidRef(Shader))
	{
		FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(Platform);
		PlatformCache.Shaders.Add(Key);
		Shader = RHICreateVertexShader(Code);
		check(IsValidRef(Shader));
		Shader->SetHash(Hash);
		INC_DWORD_STAT(STATGROUP_NumShadersCached);
		CachedVertexShaders.Add(Key, Shader);
		PrebindShader(Key);
	}
	return Shader;
}

FPixelShaderRHIRef FShaderCache::GetPixelShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code)
{
	FShaderCacheKey Key;
	Key.Platform = Platform;
	Key.Frequency = SF_Pixel;
	Key.SHAHash = Hash;
	Key.bActive = true;
	FPixelShaderRHIRef Shader = CachedPixelShaders.FindRef(Key);
	if(!IsValidRef(Shader))
	{
		FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(Platform);
		PlatformCache.Shaders.Add(Key);
		Shader = RHICreatePixelShader(Code);
		check(IsValidRef(Shader));
		Shader->SetHash(Hash);
		INC_DWORD_STAT(STATGROUP_NumShadersCached);
		CachedPixelShaders.Add(Key, Shader);
		PrebindShader(Key);
	}
	return Shader;
}

FGeometryShaderRHIRef FShaderCache::GetGeometryShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code)
{
	FShaderCacheKey Key;
	Key.Platform = Platform;
	Key.Frequency = SF_Geometry;
	Key.SHAHash = Hash;
	Key.bActive = true;
	FGeometryShaderRHIRef Shader = CachedGeometryShaders.FindRef(Key);
	if(!IsValidRef(Shader))
	{
		FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(Platform);
		PlatformCache.Shaders.Add(Key);
		Shader = RHICreateGeometryShader(Code);
		check(IsValidRef(Shader));
		Shader->SetHash(Hash);
		INC_DWORD_STAT(STATGROUP_NumShadersCached);
		CachedGeometryShaders.Add(Key, Shader);
		PrebindShader(Key);
	}
	return Shader;
}

FHullShaderRHIRef FShaderCache::GetHullShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code)
{
	FShaderCacheKey Key;
	Key.Platform = Platform;
	Key.Frequency = SF_Hull;
	Key.SHAHash = Hash;
	Key.bActive = true;
	FHullShaderRHIRef Shader = CachedHullShaders.FindRef(Key);
	if(!IsValidRef(Shader))
	{
		FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(Platform);
		PlatformCache.Shaders.Add(Key);
		Shader = RHICreateHullShader(Code);
		check(IsValidRef(Shader));
		Shader->SetHash(Hash);
		INC_DWORD_STAT(STATGROUP_NumShadersCached);
		CachedHullShaders.Add(Key, Shader);
		PrebindShader(Key);
	}
	return Shader;
}

FDomainShaderRHIRef FShaderCache::GetDomainShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code)
{
	FShaderCacheKey Key;
	Key.Platform = Platform;
	Key.Frequency = SF_Domain;
	Key.SHAHash = Hash;
	Key.bActive = true;
	FDomainShaderRHIRef Shader = CachedDomainShaders.FindRef(Key);
	if(!IsValidRef(Shader))
	{
		FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(Platform);
		PlatformCache.Shaders.Add(Key);
		Shader = RHICreateDomainShader(Code);
		check(IsValidRef(Shader));
		Shader->SetHash(Hash);
		INC_DWORD_STAT(STATGROUP_NumShadersCached);
		CachedDomainShaders.Add(Key, Shader);
		PrebindShader(Key);
	}
	return Shader;
}

FComputeShaderRHIRef FShaderCache::GetComputeShader(EShaderPlatform Platform, TArray<uint8> const& Code)
{
	FShaderCacheKey Key;
	Key.Platform = Platform;
	Key.Frequency = SF_Compute;
	// @todo WARNING: RHI is responsible for hashing Compute shaders due to the way OpenGLDrv implements compute!
	FSHA1::HashBuffer(Code.GetData(), Code.Num(), Key.SHAHash.Hash);
	Key.bActive = true;
	FComputeShaderRHIRef Shader = CachedComputeShaders.FindRef(Key);
	if(!IsValidRef(Shader))
	{
		FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(Platform);
		PlatformCache.Shaders.Add(Key);
		Shader = RHICreateComputeShader(Code);
		check(IsValidRef(Shader));
		INC_DWORD_STAT(STATGROUP_NumShadersCached);
		CachedComputeShaders.Add(Key, Shader);
		PrebindShader(Key);
	}
	return Shader;
}

void FShaderCache::InternalLogStreamingKey(uint32 StreamKey, bool const bActive)
{
	// Defer to the render thread to avoid race conditions
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
	FShaderCacheInternalLogLevel,
	FShaderCache*,Cache,Cache,
	uint32,StreamKey,StreamKey,
	bool,bActive,bActive,
	{
		if(bActive)
		{
			Cache->ActiveStreamingKeys.Add(StreamKey);
		}
		else
		{
			Cache->ActiveStreamingKeys.Remove(StreamKey);
		}
		
		uint32 NewStreamingKey = 0;
		for(uint32 Key : Cache->ActiveStreamingKeys)
		{
			NewStreamingKey ^= Key;
		}
		Cache->StreamingKey = NewStreamingKey;
		
		if(!Cache->ShadersToDraw.Contains(NewStreamingKey))
		{
			FShaderPlatformCache& PlatformCache = Cache->Caches.PlatformCaches.FindOrAdd(GMaxRHIShaderPlatform);
			Cache->ShadersToDraw.Add(NewStreamingKey, PlatformCache.StreamingDrawStates.FindRef(NewStreamingKey));
		}
	});
}

void FShaderCache::InternalLogShader(EShaderPlatform Platform, EShaderFrequency Frequency, FSHAHash Hash, TArray<uint8> const& Code)
{
	bool bUsable = FShaderResource::ArePlatformsCompatible(GMaxRHIShaderPlatform, Platform);
	switch (Frequency)
	{
		case SF_Geometry:
			bUsable &= RHISupportsGeometryShaders(GMaxRHIShaderPlatform);
			break;
			
		case SF_Hull:
		case SF_Domain:
			bUsable &= RHISupportsTessellation(GMaxRHIShaderPlatform);
			break;
			
		case SF_Compute:
			bUsable &= RHISupportsComputeShaders(GMaxRHIShaderPlatform);
			break;
			
		default:
			break;
	}
	
	if ( bUsable )
	{
		FShaderCacheKey Key;
		Key.SHAHash = Hash;
		Key.Platform = Platform;
		Key.Frequency = Frequency;
		Key.bActive = true;

		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(LogShader, FShaderCache*,ShaderCache,this, FShaderCacheKey,Key,Key, TArray<uint8>,Code,Code, {
			bool bSubmit = !ShaderCache->bUseShaderBinaryCache || !ShaderCache->bUseAsyncShaderPrecompilation;
			if(ShaderCache->bUseShaderBinaryCache && !ShaderCache->CodeCache.Shaders.Contains(Key))
			{
				TArray<uint8> CompressedData;
				FArchiveSaveCompressedProxy CompressArchive(CompressedData, (ECompressionFlags)(COMPRESS_ZLIB|COMPRESS_BiasMemory));
				CompressArchive << Code;
				CompressArchive.Flush();
				
				ShaderCache->CodeCache.Shaders.Add(Key, CompressedData);
				
				bSubmit |= true;
			}
			if(!(ShaderCache->Options & SCO_NoShaderPreload) && bSubmit)
			{
				ShaderCache->SubmitShader(Key, Code);
			}
		});
	}
}

void FShaderCache::InternalLogVertexDeclaration(const FVertexDeclarationElementList& VertexElements, FVertexDeclarationRHIParamRef VertexDeclaration)
{
	VertexDeclarations.Add(VertexDeclaration, VertexElements);
}

void FShaderCache::InternalLogBoundShaderState(EShaderPlatform Platform, FVertexDeclarationRHIParamRef VertexDeclaration,
								FVertexShaderRHIParamRef VertexShader,
								FPixelShaderRHIParamRef PixelShader,
								FHullShaderRHIParamRef HullShader,
								FDomainShaderRHIParamRef DomainShader,
								FGeometryShaderRHIParamRef GeometryShader,
								FBoundShaderStateRHIParamRef BoundState)
{
	FShaderPipelineKey PipelineKey;
	FShaderCacheBoundState Info;
	if(VertexDeclaration)
	{
		Info.VertexDeclaration = VertexDeclarations.FindChecked(VertexDeclaration);
	}
	if(VertexShader)
	{
		Info.VertexShader.Platform = Platform;
		Info.VertexShader.Frequency = SF_Vertex;
		Info.VertexShader.SHAHash = VertexShader->GetHash();
		Info.VertexShader.bActive = true;
		PipelineKey.VertexShader = Info.VertexShader;
	}
	if(PixelShader)
	{
		Info.PixelShader.Platform = Platform;
		Info.PixelShader.Frequency = SF_Pixel;
		Info.PixelShader.SHAHash = PixelShader->GetHash();
		Info.PixelShader.bActive = true;
		PipelineKey.PixelShader = Info.PixelShader;
	}
	if(GeometryShader)
	{
		Info.GeometryShader.Platform = Platform;
		Info.GeometryShader.Frequency = SF_Geometry;
		Info.GeometryShader.SHAHash = GeometryShader->GetHash();
		Info.GeometryShader.bActive = true;
		PipelineKey.GeometryShader = Info.GeometryShader;
	}
	if(HullShader)
	{
		Info.HullShader.Platform = Platform;
		Info.HullShader.Frequency = SF_Hull;
		Info.HullShader.SHAHash = HullShader->GetHash();
		Info.HullShader.bActive = true;
		PipelineKey.HullShader = Info.HullShader;
	}
	if(DomainShader)
	{
		Info.DomainShader.Platform = Platform;
		Info.DomainShader.Frequency = SF_Domain;
		Info.DomainShader.SHAHash = DomainShader->GetHash();
		Info.DomainShader.bActive = true;
		PipelineKey.DomainShader = Info.DomainShader;
	}
	
	FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(Platform);
	int32 InfoId = PlatformCache.BoundShaderStates.Add(Info);
	BoundShaderStates.Add(Info, BoundState);
	
	if(VertexShader)
	{
		TSet<int32>& Set = PlatformCache.ShaderStateMembership.FindOrAdd(PlatformCache.Shaders.Find(Info.VertexShader));
		if(!Set.Find(InfoId))
		{
			Set.Add(InfoId);
		}
		if(bUseShaderBinaryCache)
		{
			TSet<FShaderPipelineKey>& Pipelines = CodeCache.Pipelines.FindOrAdd(Info.VertexShader);
			Pipelines.Add(PipelineKey);
		}
	}
	if(PixelShader)
	{
		TSet<int32>& Set = PlatformCache.ShaderStateMembership.FindOrAdd(PlatformCache.Shaders.Find(Info.PixelShader));
		if(!Set.Find(InfoId))
		{
			Set.Add(InfoId);
		}
		if(bUseShaderBinaryCache)
		{
			TSet<FShaderPipelineKey>& Pipelines = CodeCache.Pipelines.FindOrAdd(Info.PixelShader);
			Pipelines.Add(PipelineKey);
		}
	}
	if(GeometryShader)
	{
		TSet<int32>& Set = PlatformCache.ShaderStateMembership.FindOrAdd(PlatformCache.Shaders.Find(Info.GeometryShader));
		if(!Set.Find(InfoId))
		{
			Set.Add(InfoId);
		}
		if(bUseShaderBinaryCache)
		{
			TSet<FShaderPipelineKey>& Pipelines = CodeCache.Pipelines.FindOrAdd(Info.GeometryShader);
			Pipelines.Add(PipelineKey);
		}
	}
	if(HullShader)
	{
		TSet<int32>& Set = PlatformCache.ShaderStateMembership.FindOrAdd(PlatformCache.Shaders.Find(Info.HullShader));
		if(!Set.Find(InfoId))
		{
			Set.Add(InfoId);
		}
		if(bUseShaderBinaryCache)
		{
			TSet<FShaderPipelineKey>& Pipelines = CodeCache.Pipelines.FindOrAdd(Info.HullShader);
			Pipelines.Add(PipelineKey);
		}
	}
	if(DomainShader)
	{
		TSet<int32>& Set = PlatformCache.ShaderStateMembership.FindOrAdd(PlatformCache.Shaders.Find(Info.DomainShader));
		if(!Set.Find(InfoId))
		{
			Set.Add(InfoId);
		}
		if(bUseShaderBinaryCache)
		{
			TSet<FShaderPipelineKey>& Pipelines = CodeCache.Pipelines.FindOrAdd(Info.DomainShader);
			Pipelines.Add(PipelineKey);
		}
	}
	if ( bUseShaderPredraw || bUseShaderDrawLog )
	{
		INC_DWORD_STAT(STATGROUP_NumBSSCached);
		ShaderStates.Add(BoundState, Info);
	}
}
	
void FShaderCache::InternalLogBlendState(FBlendStateInitializerRHI const& Init, FBlendStateRHIParamRef State)
{
	if ( bUseShaderPredraw || bUseShaderDrawLog )
	{
		BlendStates.Add(State, Init);
	}
}

void FShaderCache::InternalLogRasterizerState(FRasterizerStateInitializerRHI const& Init, FRasterizerStateRHIParamRef State)
{
	if ( bUseShaderPredraw || bUseShaderDrawLog )
	{
		RasterizerStates.Add(State, Init);
	}
}

void FShaderCache::InternalLogDepthStencilState(FDepthStencilStateInitializerRHI const& Init, FDepthStencilStateRHIParamRef State)
{
	if ( bUseShaderPredraw || bUseShaderDrawLog )
	{
		DepthStencilStates.Add(State, Init);
	}
}

void FShaderCache::InternalLogSamplerState(FSamplerStateInitializerRHI const& Init, FSamplerStateRHIParamRef State)
{
	if ( bUseShaderPredraw || bUseShaderDrawLog )
	{
		FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(GMaxRHIShaderPlatform);
		int32 ID = PlatformCache.SamplerStates.Add(Init);
		SamplerStates.Add(State, ID);
	}
}

void FShaderCache::InternalLogTexture(FShaderTextureKey const& Init, FTextureRHIParamRef State)
{
	if ( bUseShaderPredraw || bUseShaderDrawLog )
	{
		FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(GMaxRHIShaderPlatform);
		FShaderResourceKey Key;
		Key.Tex = Init;
		Key.Format = Init.Format;
		int32 ID = PlatformCache.Resources.Add(Key);
		
		Textures.Add(State, ID);
		CachedTextures.Add(Init, State);
	}
}

void FShaderCache::InternalLogSRV(FShaderResourceViewRHIParamRef SRV, FTextureRHIParamRef Texture, uint8 StartMip, uint8 NumMips, uint8 Format)
{
	if ( bUseShaderPredraw || bUseShaderDrawLog )
	{
		FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(GMaxRHIShaderPlatform);
		
		FShaderResourceKey& TexKey = PlatformCache.Resources[Textures.FindChecked(Texture)];
		
		FShaderResourceKey Key;
		Key.Tex = TexKey.Tex;
		Key.BaseMip = StartMip;
		Key.MipLevels = NumMips;
		Key.Format = Format;
		Key.bSRV = true;
		SRVs.Add(SRV, Key);
		CachedSRVs.Add(Key, FShaderResourceViewBinding(SRV, nullptr, Texture));
		
		PlatformCache.Resources.Add(Key);
	}
}

void FShaderCache::InternalLogSRV(FShaderResourceViewRHIParamRef SRV, FVertexBufferRHIParamRef Vb, uint32 Stride, uint8 Format)
{
	if ( bUseShaderPredraw || bUseShaderDrawLog )
	{
		FShaderResourceKey Key;
		Key.Tex.Type = SCTT_Buffer;
		Key.Tex.X = Vb->GetSize();
		Key.Tex.Y = Vb->GetUsage();
		Key.Tex.Z = Stride;
		Key.Tex.Format = Format;
		Key.bSRV = true;
		SRVs.Add(SRV, Key);
		CachedSRVs.Add(Key, FShaderResourceViewBinding(SRV, Vb, nullptr));
		
		FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(GMaxRHIShaderPlatform);
		PlatformCache.Resources.Add(Key);
	}
}

void FShaderCache::InternalRemoveSRV(FShaderResourceViewRHIParamRef SRV)
{
	if ( bUseShaderPredraw || bUseShaderDrawLog )
	{
		auto Key = SRVs.FindRef(SRV);
		CachedSRVs.Remove(Key);
		SRVs.Remove(SRV);
	}
}

void FShaderCache::InternalRemoveTexture(FTextureRHIParamRef Texture)
{
	if ( bUseShaderPredraw || bUseShaderDrawLog )
	{
		FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(GMaxRHIShaderPlatform);
		
		FShaderResourceKey& TexKey = PlatformCache.Resources[Textures.FindChecked(Texture)];
		
		CachedTextures.Remove(TexKey.Tex);
		Textures.Remove(Texture);
	}
}

void FShaderCache::InternalSetBlendState(FBlendStateRHIParamRef State)
{
	if ( (bUseShaderPredraw || bUseShaderDrawLog) && !bIsPreDraw && State )
	{
		CurrentDrawKey.BlendState = BlendStates.FindChecked(State);
		CurrentDrawKey.Hash = 0;
	}
}

void FShaderCache::InternalSetRasterizerState(FRasterizerStateRHIParamRef State)
{
	if ( (bUseShaderPredraw || bUseShaderDrawLog) && !bIsPreDraw && State )
	{
		CurrentDrawKey.RasterizerState = RasterizerStates.FindChecked(State);
		CurrentDrawKey.Hash = 0;
	}
}

void FShaderCache::InternalSetDepthStencilState(FDepthStencilStateRHIParamRef State)
{
	if ( (bUseShaderPredraw || bUseShaderDrawLog) && !bIsPreDraw && State )
	{
		CurrentDrawKey.DepthStencilState = DepthStencilStates.FindChecked(State);
		CurrentDrawKey.Hash = 0;
	}
}

void FShaderCache::InternalSetRenderTargets( uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargetsRHI, const FRHIDepthRenderTargetView* NewDepthStencilTargetRHI )
{
	if ( bUseShaderDrawLog && !bIsPreDraw )
	{
		FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(GMaxRHIShaderPlatform);
		CurrentNumRenderTargets = NumSimultaneousRenderTargets;
		bCurrentDepthStencilTarget = (NewDepthStencilTargetRHI != nullptr);
		
		FMemory::Memzero(CurrentRenderTargets, sizeof(FRHIRenderTargetView) * MaxSimultaneousRenderTargets);
		FMemory::Memcpy(CurrentRenderTargets, NewRenderTargetsRHI, sizeof(FRHIRenderTargetView) * NumSimultaneousRenderTargets);
		if( NewDepthStencilTargetRHI )
		{
			CurrentDepthStencilTarget = *NewDepthStencilTargetRHI;
		}
	
		FMemory::Memset(CurrentDrawKey.RenderTargets, 255, sizeof(uint32) * MaxSimultaneousRenderTargets);
		for( int32 RenderTargetIndex = NumSimultaneousRenderTargets - 1; RenderTargetIndex >= 0; --RenderTargetIndex )
		{
			FRHIRenderTargetView const& Target = NewRenderTargetsRHI[RenderTargetIndex];
			InvalidResourceCount -= (uint32)(CurrentDrawKey.RenderTargets[RenderTargetIndex] == FShaderDrawKey::InvalidState);
			if ( Target.Texture )
			{
				int32* TexIndex = Textures.Find(Target.Texture);
				if(TexIndex)
				{
					FShaderRenderTargetKey Key;
					FShaderResourceKey& TexKey = PlatformCache.Resources[*TexIndex];
					Key.Texture = TexKey.Tex;
					check(Key.Texture.MipLevels == Target.Texture->GetNumMips());
					Key.MipLevel = Key.Texture.MipLevels > Target.MipIndex ? Target.MipIndex : 0;
					Key.ArrayIndex = Target.ArraySliceIndex;
					CurrentDrawKey.RenderTargets[RenderTargetIndex] = PlatformCache.RenderTargets.Add(Key);
				}
				else
				{
					UE_LOG(LogShaders, Warning, TEXT("Binding invalid texture %p to render target index %d, draw logging will be suspended until this is reset to a valid or null reference."), Target.Texture, RenderTargetIndex);
					CurrentDrawKey.RenderTargets[RenderTargetIndex] = FShaderDrawKey::InvalidState;
					InvalidResourceCount++;
				}
			}
			else
			{
				CurrentDrawKey.RenderTargets[RenderTargetIndex] = FShaderDrawKey::NullState;
			}
		}
		
		InvalidResourceCount -= (uint32)(CurrentDrawKey.DepthStencilTarget == FShaderDrawKey::InvalidState);
		if ( NewDepthStencilTargetRHI && NewDepthStencilTargetRHI->Texture )
		{
			int32* TexIndex = Textures.Find(NewDepthStencilTargetRHI->Texture);
			if(TexIndex)
			{
				FShaderRenderTargetKey Key;
				FShaderResourceKey& TexKey = PlatformCache.Resources[*TexIndex];
				Key.Texture = TexKey.Tex;
				CurrentDrawKey.DepthStencilTarget = PlatformCache.RenderTargets.Add(Key);
			}
			else
			{
				UE_LOG(LogShaders, Warning, TEXT("Binding invalid texture %p to denpth-stencil target, draw logging will be suspended until this is reset to a valid or null reference."), NewDepthStencilTargetRHI->Texture);
				CurrentDrawKey.DepthStencilTarget = FShaderDrawKey::InvalidState;
				InvalidResourceCount++;
			}
		}
		else
		{
			CurrentDrawKey.DepthStencilTarget = FShaderDrawKey::NullState;
		}
		CurrentDrawKey.Hash = 0;
	}
}

void FShaderCache::InternalSetSamplerState(EShaderFrequency Frequency, uint32 Index, FSamplerStateRHIParamRef State)
{
	if ( bUseShaderDrawLog && !bIsPreDraw )
	{
		checkf(Index < GetFeatureLevelMaxTextureSamplers(GMaxRHIFeatureLevel), TEXT("Attempting to bind sampler at index %u which exceeds RHI max. %d"), Index, GetFeatureLevelMaxTextureSamplers(GMaxRHIFeatureLevel));
		InvalidResourceCount -= (uint32)(CurrentDrawKey.SamplerStates[Frequency][Index] == FShaderDrawKey::InvalidState);
		if ( State )
		{
			int32* SamplerIndex = SamplerStates.Find(State);
			if(SamplerIndex)
			{
				CurrentDrawKey.SamplerStates[Frequency][Index] = *SamplerIndex;
			}
			else
			{
				UE_LOG(LogShaders, Warning, TEXT("Binding invalid sampler %p to shader stage %u index %u, draw logging will be suspended until this is reset to a valid or null reference."), State, (uint32)Frequency, Index);
				CurrentDrawKey.SamplerStates[Frequency][Index] = FShaderDrawKey::InvalidState;
				InvalidResourceCount++;
			}
		}
		else
		{
			CurrentDrawKey.SamplerStates[Frequency][Index] = FShaderDrawKey::NullState;
		}
		CurrentDrawKey.Hash = 0;
	}
}

void FShaderCache::InternalSetTexture(EShaderFrequency Frequency, uint32 Index, FTextureRHIParamRef State)
{
	if ( bUseShaderDrawLog && !bIsPreDraw )
	{
		checkf(Index < MaxResources, TEXT("Attempting to texture bind at index %u which exceeds RHI max. %d"), Index, MaxResources);
		InvalidResourceCount -= (uint32)(CurrentDrawKey.Resources[Frequency][Index] == FShaderDrawKey::InvalidState);
		if ( State )
		{
			FShaderResourceKey Key;
			
			FTextureRHIParamRef Tex = State;
			if ( State->GetTextureReference() )
			{
				Tex = State->GetTextureReference()->GetReferencedTexture();
			}
			
			FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(GMaxRHIShaderPlatform);
			int32* TexIndex = Textures.Find(Tex);
			if(TexIndex)
			{
				CurrentDrawKey.Resources[Frequency][Index] = *TexIndex;
			}
			else
			{
				UE_LOG(LogShaders, Warning, TEXT("Binding invalid texture %p to shader stage %u index %u, draw logging will be suspended until this is reset to a valid or null reference."), State, (uint32)Frequency, Index);
				CurrentDrawKey.Resources[Frequency][Index] = FShaderDrawKey::InvalidState;
				InvalidResourceCount++;
			}
		}
		else
		{
			CurrentDrawKey.Resources[Frequency][Index] = FShaderDrawKey::NullState;
		}
		CurrentDrawKey.Hash = 0;
	}
}

void FShaderCache::InternalSetSRV(EShaderFrequency Frequency, uint32 Index, FShaderResourceViewRHIParamRef SRV)
{
	if ( bUseShaderDrawLog && !bIsPreDraw )
	{
		checkf(Index < MaxResources, TEXT("Attempting to bind SRV at index %u which exceeds RHI max. %d"), Index, MaxResources);
		InvalidResourceCount -= (uint32)(CurrentDrawKey.Resources[Frequency][Index] == FShaderDrawKey::InvalidState);
		if ( SRV )
		{
			FShaderResourceKey* Key = SRVs.Find(SRV);
			if(Key)
			{
				FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(GMaxRHIShaderPlatform);
				CurrentDrawKey.Resources[Frequency][Index] = PlatformCache.Resources.Add(*Key);
			}
			else
			{
				UE_LOG(LogShaders, Warning, TEXT("Binding invalid SRV %p to shader stage %u index %u, draw logging will be suspended until this is reset to a valid or null reference."), SRV, (uint32)Frequency, Index);
				CurrentDrawKey.Resources[Frequency][Index] = FShaderDrawKey::InvalidState;
				InvalidResourceCount++;
			}
		}
		else
		{
			CurrentDrawKey.Resources[Frequency][Index] = FShaderDrawKey::NullState;
		}
		CurrentDrawKey.Hash = 0;
	}
}

void FShaderCache::InternalSetBoundShaderState(FBoundShaderStateRHIParamRef State)
{
	if ( (bUseShaderPredraw || bUseShaderDrawLog) && !bIsPreDraw )
	{
		FMemory::Memset(CurrentDrawKey.SamplerStates, 255, sizeof(CurrentDrawKey.SamplerStates));
		FMemory::Memset(CurrentDrawKey.Resources, 255, sizeof(CurrentDrawKey.Resources));
		CurrentShaderState = State;
		if ( State )
		{
			FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(GMaxRHIShaderPlatform);
			FShaderCacheBoundState* NewState = ShaderStates.Find(State);
			int32 StateIndex = NewState ? PlatformCache.BoundShaderStates.Find(*NewState) : -1;
			if(NewState && StateIndex >= 0)
			{
				BoundShaderStateIndex = StateIndex;
			}
			else
			{
				UE_LOG(LogShaders, Fatal, TEXT("Binding invalid bound-shader-state %p"), State);
				BoundShaderStateIndex = -1;
			}
		}
		else
		{
			CurrentShaderState.SafeRelease();
		}
		CurrentDrawKey.Hash = 0;
	}
}

void FShaderCache::InternalSetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ)
{
	if ( (bUseShaderPredraw || bUseShaderDrawLog) && !bIsPreDraw )
	{
		Viewport[0] = MinX;
		Viewport[1] = MinY;
		Viewport[2] = MaxX;
		Viewport[3] = MaxY;
		DepthRange[0] = MinZ;
		DepthRange[1] = MaxZ;
	}
}

void FShaderCache::InternalLogDraw(uint8 IndexType)
{
	if ( bUseShaderDrawLog && !bIsPreDraw && InvalidResourceCount == 0 )
	{
		FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(GMaxRHIShaderPlatform);
		CurrentDrawKey.IndexType = IndexType;
		int32 Id = PlatformCache.DrawStates.Add(CurrentDrawKey);
		
		FShaderPreDrawEntry Entry;
		Entry.BoundStateIndex = BoundShaderStateIndex;
		Entry.DrawKeyIndex = Id;
		int32 EntryId = PlatformCache.PreDrawEntries.Add(Entry);
		PlatformCache.PreDrawEntries[EntryId].bPredrawn = true;
		
		FShaderStreamingCache& StreamCache = PlatformCache.StreamingDrawStates.FindOrAdd(StreamingKey);
		TSet<int32>& ShaderDrawSet = StreamCache.ShaderDrawStates.FindOrAdd(BoundShaderStateIndex);
		if( !ShaderDrawSet.Contains(EntryId) )
		{
			INC_DWORD_STAT(STATGROUP_NumDrawsCached);
			ShaderDrawSet.Add(EntryId);
		}
	}
}

void FShaderCache::InternalPreDrawShaders(FRHICommandList& RHICmdList, float DeltaTime)
{
	static uint32 NumShadersToCompile = 1;
	
	static uint32 FrameNum = 0;
	
	if(FrameNum != GFrameNumberRenderThread || OverridePrecompileTime != 0 || OverridePredrawBatchTime != 0)
	{
		FrameNum = GFrameNumberRenderThread;
		uint32 NumCompiled = 0;
		int64 TimeForPredrawing = 0;
		if ( bUseShaderBinaryCache && bUseAsyncShaderPrecompilation && ShadersToPrecompile.Num() > 0 )
		{
			SET_DWORD_STAT(STATGROUP_NumToPrecompile, NumShadersToCompile);
			
			for( uint32 Index = 0; (GetTargetPrecompileFrameTime() == -1 || NumCompiled < NumShadersToCompile) && Index < (uint32)ShadersToPrecompile.Num(); ++Index )
			{
				FShaderCacheKey& Key = ShadersToPrecompile[Index];
				TArray<uint8>& CompressedCode = CodeCache.Shaders[Key];
				
				TArray<uint8> Code;
				FArchiveLoadCompressedProxy DecompressArchive(CompressedCode, (ECompressionFlags)(COMPRESS_ZLIB));
				DecompressArchive << Code;
				
				SubmitShader(Key, Code);
				INC_DWORD_STAT(STATGROUP_NumPrecompiled);
				INC_DWORD_STAT(STATGROUP_TotalPrecompiled);
				
				uint32 OldIndex = Index;
				--Index;
				ShadersToPrecompile.RemoveAt(OldIndex);
				
				++NumCompiled;
			}
			
			if(GetTargetPrecompileFrameTime() != -1)
			{
				int64 MSec = DeltaTime * 1000.0;
				if( MSec < GetTargetPrecompileFrameTime() )
				{
					NumShadersToCompile++;
				}
				else
				{
					NumShadersToCompile = FMath::Max(1u, NumShadersToCompile / 2u);
				}
				
				if(GetPredrawBatchTime() != -1)
				{
					TimeForPredrawing += FMath::Max((MSec - (int64)GetTargetPrecompileFrameTime()), (int64)0);
				}
			}
			
			double LoadTimeUpdate = FPlatformTime::Seconds();
			SET_FLOAT_STAT(STATGROUP_BinaryCacheLoadTime, LoadTimeUpdate - LoadTimeStart);
		}
		
		if ( bUseShaderPredraw && FrameNum > 1 && (GetPredrawBatchTime() == -1 || TimeForPredrawing < GetPredrawBatchTime()) && ShadersToDraw.FindRef(StreamingKey).ShaderDrawStates.Num() > 0 )
		{
			bIsPreDraw = true;
			
			if ( !IsValidRef(IndexBufferUInt16) )
			{
				FRHIResourceCreateInfo Info;
				uint32 Stride = sizeof(uint16);
				uint32 Size = sizeof(uint16) * 3;
				void* Data = nullptr;
				IndexBufferUInt16 = RHICreateAndLockIndexBuffer(Stride, Size, BUF_Static, Info, Data);			
				if ( Data )
				{
					FMemory::Memzero(Data, Size);
				}
				RHIUnlockIndexBuffer(IndexBufferUInt16);
			}
			if ( !IsValidRef(IndexBufferUInt32) )
			{
				FRHIResourceCreateInfo Info;
				uint32 Stride = sizeof(uint32);
				uint32 Size = sizeof(uint32) * 3;
				void* Data = nullptr;
				IndexBufferUInt32 = RHICreateAndLockIndexBuffer(Stride, Size, BUF_Static, Info, Data);			
				if ( Data )
				{
					FMemory::Memzero(Data, Size);
				}
				RHIUnlockIndexBuffer(IndexBufferUInt32);
			}
			
			RHICmdList.SetViewport(0, 0, FLT_MIN, 3, 3, FLT_MAX);
			
			FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(GMaxRHIShaderPlatform);
			TMap<int32, TSet<int32>>& ShaderDrawStates = ShadersToDraw.FindOrAdd(StreamingKey).ShaderDrawStates;
			for ( auto It = ShaderDrawStates.CreateIterator(); (GetPredrawBatchTime() == -1 || TimeForPredrawing < GetPredrawBatchTime()) && It; ++It )
			{
				uint32 Start = FPlatformTime::Cycles();
				
				auto Shader = *It;
				if(Shader.Key >= 0)
				{
					TSet<int32>& ShaderDrawSet = Shader.Value;
					FShaderCacheBoundState& BSS = PlatformCache.BoundShaderStates[Shader.Key];
					PreDrawShader(RHICmdList, BSS, ShaderDrawSet);
				}
				It.RemoveCurrent();
				
				uint32 End = FPlatformTime::Cycles();
				TimeForPredrawing += FPlatformTime::ToMilliseconds(End - Start);
			}
			
			// This is a bit dirty/naughty but it forces the draw commands to be flushed through on OS X
			// which means we can delete the resources without crashing MTGL.
			RHIFlushResources();
			
			RHICmdList.SetBoundShaderState(CurrentShaderState);
			
			FBlendStateRHIRef BlendState = RHICreateBlendState(CurrentDrawKey.BlendState);
			FDepthStencilStateRHIRef DepthStencil = RHICreateDepthStencilState(CurrentDrawKey.DepthStencilState);
			FRasterizerStateRHIRef Rasterizer = RHICreateRasterizerState(CurrentDrawKey.RasterizerState);
			
			RHICmdList.SetBlendState(BlendState);
			RHICmdList.SetDepthStencilState(DepthStencil);
			RHICmdList.SetRasterizerState(Rasterizer);

			RHICmdList.SetViewport(Viewport[0], Viewport[1], DepthRange[0], Viewport[2], Viewport[3], DepthRange[1]);
			
			if ( ShadersToDraw.FindOrAdd(StreamingKey).ShaderDrawStates.Num() == 0 )
			{
				PredrawRTs.Empty();
				PredrawBindings.Empty();
				PredrawVBs.Empty();
			}
			
			if ( ShadersToDraw.FindOrAdd(StreamingKey).ShaderDrawStates.Num() == 0 )
			{
				PredrawRTs.Empty();
				PredrawBindings.Empty();
				PredrawVBs.Empty();
			}
			bIsPreDraw = false;
			
			double LoadTimeUpdate = FPlatformTime::Seconds();
			SET_FLOAT_STAT(STATGROUP_BinaryCacheLoadTime, LoadTimeUpdate - LoadTimeStart);
		}
		
		if(OverridePrecompileTime == -1)
		{
			OverridePrecompileTime = 0;
		}
		
		if(OverridePredrawBatchTime == -1)
		{
			OverridePredrawBatchTime = 0;
		}
	}
}

void FShaderCache::BeginAcceleratedBatching()
{
	if ( Cache )
	{
		if(AccelTargetPrecompileFrameTime)
		{
			Cache->OverridePrecompileTime = AccelTargetPrecompileFrameTime;
		}
		if(AccelPredrawBatchTime)
		{
			Cache->OverridePredrawBatchTime = AccelPredrawBatchTime;
		}
	}
}

void FShaderCache::EndAcceleratedBatching()
{
	if ( Cache )
	{
		Cache->OverridePrecompileTime = 0;
		Cache->OverridePredrawBatchTime = 0;
	}
}

void FShaderCache::FlushOutstandingBatches()
{
	if ( Cache )
	{
		Cache->OverridePrecompileTime = -1;
		Cache->OverridePredrawBatchTime = -1;
	}
}

void FShaderCache::PauseBatching()
{
	if ( Cache )
	{
		Cache->bBatchingPaused = true;
	}
}

void FShaderCache::ResumeBatching()
{
	if ( Cache )
	{
		Cache->bBatchingPaused = false;
	}
}

uint32 FShaderCache::NumShaderPrecompilesRemaining()
{
	if ( Cache && bUseShaderBinaryCache && bUseAsyncShaderPrecompilation )
	{
		return Cache->ShadersToPrecompile.Num();
	}
	return 0;
}

void FShaderCache::CookShader(EShaderPlatform Platform, EShaderFrequency Frequency, FSHAHash Hash, TArray<uint8> const& Code)
{
#if WITH_EDITORONLY_DATA
	if ( CookCache && CookCache->AllCachedPlatforms.Contains(Platform) )
	{
		FShaderCacheKey Key;
		Key.SHAHash = Hash;
		Key.Platform = Platform;
		Key.Frequency = Frequency;
		Key.bActive = true;
		
		TArray<uint8> CompressedData;
		FArchiveSaveCompressedProxy CompressArchive(CompressedData, (ECompressionFlags)(COMPRESS_ZLIB|COMPRESS_BiasMemory));
		CompressArchive << const_cast<TArray<uint8>&>(Code);
		CompressArchive.Flush();
		
		CookCache->CodeCache.Shaders.Add(Key, CompressedData);
	}
#endif
}

void FShaderCache::CookPipeline(FShaderPipeline* Pipeline)
{
#if WITH_EDITORONLY_DATA
	if ( CookCache )
	{
		FShaderPipelineKey PipelineKey;
		FShader* VS = Pipeline->GetShader(SF_Vertex);
		bool bCachePipeline = true;
		if(VS)
		{
			PipelineKey.VertexShader.SHAHash = VS->GetOutputHash();
			PipelineKey.VertexShader.Platform = (EShaderPlatform)VS->GetTarget().Platform;
			PipelineKey.VertexShader.Frequency = (EShaderFrequency)VS->GetTarget().Frequency;
			PipelineKey.VertexShader.bActive = true;
			bCachePipeline &= CookCache->AllCachedPlatforms.Contains(PipelineKey.VertexShader.Platform);
		}
		FShader* GS = Pipeline->GetShader(SF_Geometry);
		if(GS)
		{
			PipelineKey.GeometryShader.SHAHash = GS->GetOutputHash();
			PipelineKey.GeometryShader.Platform = (EShaderPlatform)GS->GetTarget().Platform;
			PipelineKey.GeometryShader.Frequency = (EShaderFrequency)GS->GetTarget().Frequency;
			PipelineKey.GeometryShader.bActive = true;
			bCachePipeline &= CookCache->AllCachedPlatforms.Contains(PipelineKey.GeometryShader.Platform);
		}
		FShader* HS = Pipeline->GetShader(SF_Hull);
		if(HS)
		{
			PipelineKey.HullShader.SHAHash = HS->GetOutputHash();
			PipelineKey.HullShader.Platform = (EShaderPlatform)HS->GetTarget().Platform;
			PipelineKey.HullShader.Frequency = (EShaderFrequency)HS->GetTarget().Frequency;
			PipelineKey.HullShader.bActive = true;
			bCachePipeline &= CookCache->AllCachedPlatforms.Contains(PipelineKey.HullShader.Platform);
		}
		FShader* DS = Pipeline->GetShader(SF_Domain);
		if(DS)
		{
			PipelineKey.DomainShader.SHAHash = DS->GetOutputHash();
			PipelineKey.DomainShader.Platform = (EShaderPlatform)DS->GetTarget().Platform;
			PipelineKey.DomainShader.Frequency = (EShaderFrequency)DS->GetTarget().Frequency;
			PipelineKey.DomainShader.bActive = true;
			bCachePipeline &= CookCache->AllCachedPlatforms.Contains(PipelineKey.DomainShader.Platform);
		}
		FShader* PS = Pipeline->GetShader(SF_Pixel);
		if(PS)
		{
			PipelineKey.PixelShader.SHAHash = PS->GetOutputHash();
			PipelineKey.PixelShader.Platform = (EShaderPlatform)PS->GetTarget().Platform;
			PipelineKey.PixelShader.Frequency = (EShaderFrequency)PS->GetTarget().Frequency;
			PipelineKey.PixelShader.bActive = true;
			bCachePipeline &= CookCache->AllCachedPlatforms.Contains(PipelineKey.PixelShader.Platform);
		}
		
		if(bCachePipeline)
		{
			if(PipelineKey.VertexShader.bActive)
			{
				TSet<FShaderPipelineKey>& Pipelines = CookCache->CodeCache.Pipelines.FindOrAdd(PipelineKey.VertexShader);
				Pipelines.Add(PipelineKey);
			}
			if(PipelineKey.GeometryShader.bActive)
			{
				TSet<FShaderPipelineKey>& Pipelines = CookCache->CodeCache.Pipelines.FindOrAdd(PipelineKey.GeometryShader);
				Pipelines.Add(PipelineKey);
			}
			if(PipelineKey.HullShader.bActive)
			{
				TSet<FShaderPipelineKey>& Pipelines = CookCache->CodeCache.Pipelines.FindOrAdd(PipelineKey.HullShader);
				Pipelines.Add(PipelineKey);
			}
			if(PipelineKey.DomainShader.bActive)
			{
				TSet<FShaderPipelineKey>& Pipelines = CookCache->CodeCache.Pipelines.FindOrAdd(PipelineKey.DomainShader);
				Pipelines.Add(PipelineKey);
			}
			if(PipelineKey.PixelShader.bActive)
			{
				TSet<FShaderPipelineKey>& Pipelines = CookCache->CodeCache.Pipelines.FindOrAdd(PipelineKey.PixelShader);
				Pipelines.Add(PipelineKey);
			}
		}
	}
#endif
}

void FShaderCache::Tick( float DeltaTime )
{
	if ( Cache && Cache->bBatchingPaused == false )
	{
		Cache->InternalPreDrawShaders(GRHICommandList.GetImmediateCommandList(), DeltaTime);
	}
}

bool FShaderCache::IsTickable() const
{
	return !bBatchingPaused && (( bUseShaderBinaryCache && bUseAsyncShaderPrecompilation && ShadersToPrecompile.Num() > 0 ) || ( bUseShaderPredraw && ShadersToDraw.FindRef(StreamingKey).ShaderDrawStates.Num() > 0 ));
}

bool FShaderCache::NeedsRenderingResumedForRenderingThreadTick() const
{
	return true;
}

TStatId FShaderCache::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FShaderCache, STATGROUP_Tickables);
}

void FShaderCache::PrebindShader(FShaderCacheKey const& Key)
{
	FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(Key.Platform);
	bool const bCanPreBind = (ShaderPlatformCanPrebindBoundShaderState(Key.Platform) || CurrentNumRenderTargets > 0);
	if (bCanPreBind || bUseShaderPredraw)
	{
		// This only applies to OpenGL.
		if(IsOpenGLPlatform(Key.Platform))
		{
			TSet<FShaderPipelineKey> const* Pipelines = CodeCache.Pipelines.Find(Key);
			if(Pipelines && bCanPreBind)
			{
				for (FShaderPipelineKey const& Pipeline : *Pipelines)
				{
					FVertexShaderRHIRef VertexShader = Pipeline.VertexShader.bActive ? CachedVertexShaders.FindRef(Pipeline.VertexShader) : nullptr;
					FPixelShaderRHIRef PixelShader = Pipeline.PixelShader.bActive ? CachedPixelShaders.FindRef(Pipeline.PixelShader) : nullptr;
					FGeometryShaderRHIRef GeometryShader = Pipeline.GeometryShader.bActive ? CachedGeometryShaders.FindRef(Pipeline.GeometryShader) : nullptr;
					FHullShaderRHIRef HullShader = Pipeline.HullShader.bActive ? CachedHullShaders.FindRef(Pipeline.HullShader) : nullptr;
					FDomainShaderRHIRef DomainShader = Pipeline.DomainShader.bActive ? CachedDomainShaders.FindRef(Pipeline.DomainShader) : nullptr;
					
					bool bOK = true;
					bOK &= (Pipeline.VertexShader.bActive == IsValidRef(VertexShader));
					bOK &= (Pipeline.PixelShader.bActive == IsValidRef(PixelShader));
					bOK &= (Pipeline.GeometryShader.bActive == IsValidRef(GeometryShader));
					bOK &= (Pipeline.HullShader.bActive == IsValidRef(HullShader));
					bOK &= (Pipeline.DomainShader.bActive == IsValidRef(DomainShader));
					
					if (bOK)
					{
						// Will return nullptr because there's no vertex declaration - we are just forcing the LinkedProgram creation.
						bIsPreBind = true;
						RHICreateBoundShaderState(nullptr,
												  VertexShader,
												  HullShader,
												  DomainShader,
												  PixelShader,
												  GeometryShader);
						bIsPreBind = false;
					}
				}
			}
		}
		
		TSet<int32>& BoundStates = PlatformCache.ShaderStateMembership.FindOrAdd(PlatformCache.Shaders.Find(Key));
		for (int32 StateIndex : BoundStates)
		{
			FShaderCacheBoundState& State = PlatformCache.BoundShaderStates[StateIndex];
			FVertexShaderRHIRef VertexShader = State.VertexShader.bActive ? CachedVertexShaders.FindRef(State.VertexShader) : nullptr;
			FPixelShaderRHIRef PixelShader = State.PixelShader.bActive ? CachedPixelShaders.FindRef(State.PixelShader) : nullptr;
			FGeometryShaderRHIRef GeometryShader = State.GeometryShader.bActive ? CachedGeometryShaders.FindRef(State.GeometryShader) : nullptr;
			FHullShaderRHIRef HullShader = State.HullShader.bActive ? CachedHullShaders.FindRef(State.HullShader) : nullptr;
			FDomainShaderRHIRef DomainShader = State.DomainShader.bActive ? CachedDomainShaders.FindRef(State.DomainShader) : nullptr;

			bool bOK = true;
			bOK &= (State.VertexShader.bActive == IsValidRef(VertexShader));
			bOK &= (State.PixelShader.bActive == IsValidRef(PixelShader));
			bOK &= (State.GeometryShader.bActive == IsValidRef(GeometryShader));
			bOK &= (State.HullShader.bActive == IsValidRef(HullShader));
			bOK &= (State.DomainShader.bActive == IsValidRef(DomainShader));

			if (bOK)
			{
				FVertexDeclarationRHIRef VertexDeclaration = RHICreateVertexDeclaration(State.VertexDeclaration);
				bOK &= IsValidRef(VertexDeclaration);

				if (bOK)
				{
					if (bCanPreBind)
					{
						FBoundShaderStateRHIRef BoundState = RHICreateBoundShaderState(VertexDeclaration,
																					   VertexShader,
																					   HullShader,
																					   DomainShader,
																					   PixelShader,
																					   GeometryShader);
						if (IsValidRef(BoundState))
						{
							if (bUseShaderPredraw)
							{
								TSet<int32>& StreamCache = PlatformCache.StreamingDrawStates.FindOrAdd(StreamingKey).ShaderDrawStates.FindOrAdd(StateIndex);
								if(!ShadersToDraw.FindOrAdd(StreamingKey).ShaderDrawStates.Contains(StateIndex))
								{
									ShadersToDraw.FindOrAdd(StreamingKey).ShaderDrawStates.Add(StateIndex, StreamCache);
								}
							}
						}
					}
					else if (bUseShaderPredraw)
					{
						TSet<int32>& StreamCache = PlatformCache.StreamingDrawStates.FindOrAdd(StreamingKey).ShaderDrawStates.FindOrAdd(StateIndex);
						if(!ShadersToDraw.FindOrAdd(StreamingKey).ShaderDrawStates.Contains(StateIndex))
						{
							ShadersToDraw.FindOrAdd(StreamingKey).ShaderDrawStates.Add(StateIndex, StreamCache);
						}
					}
				}
			}
		}
	}
}

void FShaderCache::SubmitShader(FShaderCacheKey const& Key, TArray<uint8> const& Code)
{
	FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(Key.Platform);
	switch (Key.Frequency)
	{
		case SF_Vertex:
			if(!CachedVertexShaders.Find(Key))
			{
				FVertexShaderRHIRef Shader = RHICreateVertexShader(Code);
				if(IsValidRef(Shader))
				{
					Shader->SetHash(Key.SHAHash);
					INC_DWORD_STAT(STATGROUP_NumShadersCached);
					CachedVertexShaders.Add(Key, Shader);
					PlatformCache.Shaders.Add(Key);
					PrebindShader(Key);
				}
			}
			break;
		case SF_Pixel:
			if(!CachedPixelShaders.Find(Key))
			{
				FPixelShaderRHIRef Shader = RHICreatePixelShader(Code);
				if(IsValidRef(Shader))
				{
					Shader->SetHash(Key.SHAHash);
					INC_DWORD_STAT(STATGROUP_NumShadersCached);
					CachedPixelShaders.Add(Key, Shader);
					PlatformCache.Shaders.Add(Key);
					PrebindShader(Key);
				}
			}
			break;
		case SF_Geometry:
			if(!CachedGeometryShaders.Find(Key))
			{
				FGeometryShaderRHIRef Shader = RHICreateGeometryShader(Code);
				if(IsValidRef(Shader))
				{
					Shader->SetHash(Key.SHAHash);
					INC_DWORD_STAT(STATGROUP_NumShadersCached);
					CachedGeometryShaders.Add(Key, Shader);
					PlatformCache.Shaders.Add(Key);
					PrebindShader(Key);
				}
			}
			break;
		case SF_Hull:
			if(!CachedHullShaders.Find(Key))
			{
				FHullShaderRHIRef Shader = RHICreateHullShader(Code);
				if(IsValidRef(Shader))
				{
					Shader->SetHash(Key.SHAHash);
					INC_DWORD_STAT(STATGROUP_NumShadersCached);
					CachedHullShaders.Add(Key, Shader);
					PlatformCache.Shaders.Add(Key);
					PrebindShader(Key);
				}
			}
			break;
		case SF_Domain:
			if(!CachedDomainShaders.Find(Key))
			{
				FDomainShaderRHIRef Shader = RHICreateDomainShader(Code);
				if(IsValidRef(Shader))
				{
					Shader->SetHash(Key.SHAHash);
					INC_DWORD_STAT(STATGROUP_NumShadersCached);
					CachedDomainShaders.Add(Key, Shader);
					PlatformCache.Shaders.Add(Key);
					PrebindShader(Key);
				}
			}
			break;
		case SF_Compute:
		{
			bool const bCanPreBind = (ShaderPlatformCanPrebindBoundShaderState(Key.Platform) || CurrentNumRenderTargets > 0);
			if (!CachedComputeShaders.Find(Key) && bCanPreBind)
			{
				FComputeShaderRHIRef Shader = RHICreateComputeShader(Code);
				if (IsValidRef(Shader))
				{
					// @todo WARNING: The RHI is responsible for hashing Compute shaders, unlike other stages because of how OpenGLDrv implements compute.
					FShaderCacheKey ComputeKey = Key;
					ComputeKey.SHAHash = Shader->GetHash();
					INC_DWORD_STAT(STATGROUP_NumShadersCached);
					CachedComputeShaders.Add(ComputeKey, Shader);
					PlatformCache.Shaders.Add(ComputeKey);
					PrebindShader(ComputeKey);
				}
			}
			break;
		}
		default:
			check(false);
			break;
	}
}

FTextureRHIRef FShaderCache::CreateTexture(FShaderTextureKey const& TextureKey, bool const bCached)
{
	FTextureRHIRef Tex = bCached ? CachedTextures.FindRef(TextureKey) : nullptr;
	
	if ( !IsValidRef(Tex) )
	{
		FRHIResourceCreateInfo Info;
		
		// Remove the presentable flag if present because it will be wrong during pre-draw and will cause some RHIs (e.g. Metal) to crash because it is invalid without an attached viewport.
		uint32 Flags = (TextureKey.Flags & ~(TexCreate_Presentable));
		
		switch(TextureKey.Type)
		{
			case SCTT_Texture2D:
			{
				Tex = RHICreateTexture2D(TextureKey.X, TextureKey.Y, TextureKey.Format, TextureKey.MipLevels, TextureKey.Samples, Flags, Info);
				break;
			}
			case SCTT_Texture2DArray:
			{
				Tex = RHICreateTexture2DArray(TextureKey.X, TextureKey.Y, TextureKey.Z, TextureKey.Format, TextureKey.MipLevels, Flags, Info);
				break;
			}
			case SCTT_Texture3D:
			{
				Tex = RHICreateTexture3D(TextureKey.X, TextureKey.Y, TextureKey.Z, TextureKey.Format, TextureKey.MipLevels, Flags, Info);
				break;
			}
			case SCTT_TextureCube:
			{
				Tex = RHICreateTextureCube(TextureKey.X, TextureKey.Format, TextureKey.MipLevels, Flags, Info);
				break;
			}
			case SCTT_TextureCubeArray:
			{
				Tex = RHICreateTextureCubeArray(TextureKey.X, TextureKey.Z, TextureKey.Format, TextureKey.MipLevels, Flags, Info);
				break;
			}
			case SCTT_Buffer:
			case SCTT_Texture1D:
			case SCTT_Texture1DArray:
			default:
			{
				check(false);
				break;
			}
		}
	}
	return Tex;
}

FShaderCache::FShaderTextureBinding FShaderCache::CreateSRV(FShaderResourceKey const& ResourceKey)
{
	FShaderTextureBinding Binding = CachedSRVs.FindRef(ResourceKey);
	if ( !IsValidRef(Binding.SRV) )
	{
		FShaderTextureKey const& TextureKey = ResourceKey.Tex;
		switch(TextureKey.Type)
		{
			case SCTT_Buffer:
			{
				FRHIResourceCreateInfo Info;
				Binding.VertexBuffer = RHICreateVertexBuffer(TextureKey.X, TextureKey.Y, Info);
				Binding.SRV = RHICreateShaderResourceView(Binding.VertexBuffer, TextureKey.Z, TextureKey.Format);
				break;
			}
			case SCTT_Texture2D:
			{
				Binding.Texture = CreateTexture(TextureKey, true);
				
				if(ResourceKey.Format == PF_Unknown)
				{
					Binding.SRV = RHICreateShaderResourceView(Binding.Texture->GetTexture2D(), ResourceKey.BaseMip);
					
				}
				else
				{
					// Make sure that the mip count is valid.
					uint32 NumMips = (Binding.Texture->GetNumMips() - ResourceKey.BaseMip);
					if(ResourceKey.MipLevels > 0)
					{
						NumMips = FMath::Min(NumMips, ResourceKey.MipLevels);
					}
					
					Binding.SRV = RHICreateShaderResourceView(Binding.Texture->GetTexture2D(), ResourceKey.BaseMip, NumMips, ResourceKey.Format);
				}
				break;
			}
			default:
			{
				check(false);
				break;
			}
		}
	}
	
	return Binding;
}

FTextureRHIRef FShaderCache::CreateRenderTarget(FShaderRenderTargetKey const& TargetKey)
{
	FTextureRHIRef Texture;
	if( TargetKey.Texture.Format != PF_Unknown )
	{
		Texture = PredrawRTs.FindRef(TargetKey);
		if ( !IsValidRef(Texture) )
		{
			Texture = CreateTexture(TargetKey.Texture, false);
			PredrawRTs.Add(TargetKey, Texture);
		}
	}
	return Texture;
}

template <typename TShaderRHIRef>
void FShaderCache::SetShaderSamplerTextures( FRHICommandList& RHICmdList, FShaderDrawKey const& DrawKey, EShaderFrequency Frequency, TShaderRHIRef Shader, bool bClear )
{
	FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(GMaxRHIShaderPlatform);
	
	for ( uint32 i = 0; i < GetFeatureLevelMaxTextureSamplers(GMaxRHIFeatureLevel); i++ )
	{
		checkf(DrawKey.SamplerStates[Frequency][i] != FShaderDrawKey::InvalidState, TEXT("Resource state cannot be 'InvalidState' as that indicates a resource lifetime error in the application."));
		
		if ( DrawKey.SamplerStates[Frequency][i] != FShaderDrawKey::NullState )
		{
			FSamplerStateInitializerRHI SamplerInit = PlatformCache.SamplerStates[DrawKey.SamplerStates[Frequency][i]];
			FSamplerStateRHIRef State = RHICreateSamplerState(SamplerInit);
			RHICmdList.SetShaderSampler(Shader, i, State);
		}
	}

	for ( uint32 i = 0; i < MaxResources; i++ )
	{
		checkf(DrawKey.Resources[Frequency][i] != FShaderDrawKey::InvalidState, TEXT("Resource state cannot be 'InvalidState' as that indicates a resource lifetime error in the application."));
		
		FShaderTextureBinding Bind;
		if ( DrawKey.Resources[Frequency][i] != FShaderDrawKey::NullState )
		{
			FShaderResourceKey Resource = PlatformCache.Resources[DrawKey.Resources[Frequency][i]];
			if( Resource.bSRV == false )
			{
				if ( !bClear && Resource.Tex.Type != SCTT_Invalid )
				{
					Bind.Texture = CreateTexture(Resource.Tex, true);
					RHICmdList.SetShaderTexture(Shader, i, Bind.Texture.GetReference());
				}
				else
				{
					RHICmdList.SetShaderTexture(Shader, i, nullptr);
				}
			}
			else
			{
				if ( !bClear )
				{
					Bind = CreateSRV(Resource);
					RHICmdList.SetShaderResourceViewParameter(Shader, i, Bind.SRV.GetReference());
				}
				else
				{
					RHICmdList.SetShaderResourceViewParameter(Shader, i, nullptr);
				}
			}
		}
		else
		{
			RHICmdList.SetShaderTexture(Shader, i, nullptr);
		}
		
		if ( IsValidRef(Bind.Texture) || IsValidRef(Bind.SRV) )
		{
			PredrawBindings.Add(Bind);
		}
	}
}

void FShaderCache::PreDrawShader(FRHICommandList& RHICmdList, FShaderCacheBoundState const& Shader, TSet<int32> const& DrawStates)
{
	FBoundShaderStateRHIRef ShaderBoundState = BoundShaderStates.FindRef( Shader );
	{
		uint32 VertexBufferSize = 0;
		for( auto VertexDec : Shader.VertexDeclaration )
		{
			VertexBufferSize = VertexBufferSize > (uint32)(VertexDec.Stride + VertexDec.Offset) ? VertexBufferSize : (uint32)(VertexDec.Stride + VertexDec.Offset);
		}
		
		FRHIResourceCreateInfo Info;
		if ( VertexBufferSize > 0 && (( !IsValidRef(PredrawVB) || !IsValidRef(PredrawZVB) ) || PredrawVB->GetSize() < VertexBufferSize || PredrawZVB->GetSize() < VertexBufferSize) )
		{
			// Retain previous VBs for outstanding draws
			PredrawVBs.Add(PredrawVB);
			PredrawVBs.Add(PredrawZVB);
			
			PredrawVB = RHICreateVertexBuffer(VertexBufferSize, BUF_Static, Info);
			{
				void* Data = RHILockVertexBuffer(PredrawVB, 0, VertexBufferSize, RLM_WriteOnly);
				if ( Data )
				{
					FMemory::Memzero(Data, VertexBufferSize);
				}
				RHIUnlockVertexBuffer(PredrawVB);
			}
			PredrawZVB = RHICreateVertexBuffer(VertexBufferSize, BUF_Static|BUF_ZeroStride, Info);
			{
				void* Data = RHILockVertexBuffer(PredrawZVB, 0, VertexBufferSize, RLM_WriteOnly);
				if ( Data )
				{
					FMemory::Memzero(Data, VertexBufferSize);
				}
				RHIUnlockVertexBuffer(PredrawZVB);
			}
		}
		
		bool bWasBound = false;
		
		FShaderPlatformCache& PlatformCache = Caches.PlatformCaches.FindOrAdd(GMaxRHIShaderPlatform);
		for ( auto PreDrawKeyIdx : DrawStates )
		{
			FShaderPreDrawEntry& Entry = PlatformCache.PreDrawEntries[PreDrawKeyIdx];
			if(Entry.bPredrawn)
			{
				continue;
			}
			
			FShaderDrawKey& DrawKey = PlatformCache.DrawStates[Entry.DrawKeyIndex];
			
			FBlendStateRHIRef BlendState = RHICreateBlendState(DrawKey.BlendState);
			FDepthStencilStateRHIRef DepthStencil = RHICreateDepthStencilState(DrawKey.DepthStencilState);
			FRasterizerStateRHIRef Rasterizer = RHICreateRasterizerState(DrawKey.RasterizerState);
			
			uint32 NewNumRenderTargets = 0;
			FRHIRenderTargetView RenderTargets[MaxSimultaneousRenderTargets];
			for ( uint32 i = 0; i < MaxSimultaneousRenderTargets; i++ )
			{
				checkf(DrawKey.RenderTargets[i] != FShaderDrawKey::InvalidState, TEXT("Resource state cannot be 'InvalidState' as that indicates a resource lifetime error in the application."));
				
				if( DrawKey.RenderTargets[i] != FShaderDrawKey::NullState )
				{
					FShaderTextureBinding Bind;
					FShaderRenderTargetKey& RTKey = PlatformCache.RenderTargets[DrawKey.RenderTargets[i]];
					Bind.Texture = CreateRenderTarget(RTKey);
					RenderTargets[i].MipIndex = Bind.Texture->GetNumMips() > RTKey.MipLevel ? RTKey.MipLevel : 0;
					RenderTargets[i].ArraySliceIndex = RTKey.ArrayIndex;
					PredrawBindings.Add(Bind);
					RenderTargets[i].Texture = Bind.Texture;
					NewNumRenderTargets++;
				}
				else
				{
					break;
				}
			}
			
			bool bDepthStencilTarget = (DrawKey.DepthStencilTarget != FShaderDrawKey::NullState);
			FRHIDepthRenderTargetView DepthStencilTarget;
			if ( bDepthStencilTarget )
			{
				checkf(DrawKey.DepthStencilTarget != FShaderDrawKey::InvalidState, TEXT("Resource state cannot be 'InvalidState' as that indicates a resource lifetime error in the application."));
				
				FShaderTextureBinding Bind;
				FShaderRenderTargetKey& RTKey = PlatformCache.RenderTargets[DrawKey.DepthStencilTarget];
				Bind.Texture = CreateRenderTarget(RTKey);
				PredrawBindings.Add(Bind);
				DepthStencilTarget.Texture = Bind.Texture;
			}
			
			RHICmdList.SetRenderTargets(NewNumRenderTargets, RenderTargets, bDepthStencilTarget ? &DepthStencilTarget : nullptr, 0, nullptr);
			
			RHICmdList.SetBlendState(BlendState);
			RHICmdList.SetDepthStencilState(DepthStencil);
			RHICmdList.SetRasterizerState(Rasterizer);

			for( auto VertexDec : Shader.VertexDeclaration )
			{
				if ( VertexDec.Stride > 0 )
				{
					RHICmdList.SetStreamSource(VertexDec.StreamIndex, PredrawVB, VertexDec.Stride, VertexDec.Offset);
				}
				else
				{
					RHICmdList.SetStreamSource(VertexDec.StreamIndex, PredrawZVB, VertexDec.Stride, VertexDec.Offset);
				}
			}

			if ( !IsValidRef(ShaderBoundState) )
			{
				FVertexShaderRHIRef VertexShader = Shader.VertexShader.bActive ? CachedVertexShaders.FindRef(Shader.VertexShader) : nullptr;
				FPixelShaderRHIRef PixelShader = Shader.PixelShader.bActive ? CachedPixelShaders.FindRef(Shader.PixelShader) : nullptr;
				FGeometryShaderRHIRef GeometryShader = Shader.GeometryShader.bActive ? CachedGeometryShaders.FindRef(Shader.GeometryShader) : nullptr;
				FHullShaderRHIRef HullShader = Shader.HullShader.bActive ? CachedHullShaders.FindRef(Shader.HullShader) : nullptr;
				FDomainShaderRHIRef DomainShader = Shader.DomainShader.bActive ? CachedDomainShaders.FindRef(Shader.DomainShader) : nullptr;
				
				bool bOK = true;
				bOK &= (Shader.VertexShader.bActive == IsValidRef(VertexShader));
				bOK &= (Shader.PixelShader.bActive == IsValidRef(PixelShader));
				bOK &= (Shader.GeometryShader.bActive == IsValidRef(GeometryShader));
				bOK &= (Shader.HullShader.bActive == IsValidRef(HullShader));
				bOK &= (Shader.DomainShader.bActive == IsValidRef(DomainShader));
				
				if ( bOK )
				{
					FVertexDeclarationRHIRef VertexDeclaration = RHICreateVertexDeclaration(Shader.VertexDeclaration);
					bOK &= IsValidRef(VertexDeclaration);
					
					if ( bOK )
					{
						ShaderBoundState = RHICreateBoundShaderState( VertexDeclaration,
																	 VertexShader,
																	 HullShader,
																	 DomainShader,
																	 PixelShader,
																	 GeometryShader );
					}
				}
			}
			
			if ( IsValidRef( ShaderBoundState ) )
			{
				bWasBound = true;
				RHICmdList.SetBoundShaderState(ShaderBoundState);
			}
			else
			{
				break;
			}
			
			if ( Shader.VertexShader.bActive )
			{
				SetShaderSamplerTextures(RHICmdList, DrawKey, SF_Vertex, CachedVertexShaders.FindRef(Shader.VertexShader).GetReference());
			}
			if ( Shader.PixelShader.bActive )
			{
				SetShaderSamplerTextures(RHICmdList, DrawKey, SF_Pixel, CachedPixelShaders.FindRef(Shader.PixelShader).GetReference());
			}
			if ( Shader.GeometryShader.bActive )
			{
				SetShaderSamplerTextures(RHICmdList, DrawKey, SF_Geometry, CachedGeometryShaders.FindRef(Shader.GeometryShader).GetReference());
			}
			if ( Shader.HullShader.bActive )
			{
				SetShaderSamplerTextures(RHICmdList, DrawKey, SF_Hull, CachedHullShaders.FindRef(Shader.HullShader).GetReference());
			}
			if ( Shader.DomainShader.bActive )
			{
				SetShaderSamplerTextures(RHICmdList, DrawKey, SF_Domain, CachedDomainShaders.FindRef(Shader.DomainShader).GetReference());
			}
			
			switch ( DrawKey.IndexType )
			{
				case 0:
				{
					RHICmdList.DrawPrimitive(PT_TriangleList, 0, 1, 1);
					break;
				}
				case 2:
				{
					RHICmdList.DrawIndexedPrimitive(IndexBufferUInt16, PT_TriangleList, 0, 0, 3, 0, 1, 1);
					break;
				}
				case 4:
				{
					RHICmdList.DrawIndexedPrimitive(IndexBufferUInt32, PT_TriangleList, 0, 0, 3, 0, 1, 1);
					break;
				}
				default:
				{
					break;
				}
			}
			INC_DWORD_STAT(STATGROUP_NumStatesPredrawn);
			INC_DWORD_STAT(STATGROUP_TotalStatesPredrawn);
		}
		
		if( bWasBound && IsValidRef( ShaderBoundState ) && DrawStates.Num() )
		{
			if ( Shader.VertexShader.bActive )
			{
				SetShaderSamplerTextures(RHICmdList, CurrentDrawKey, SF_Vertex, CachedVertexShaders.FindRef(Shader.VertexShader).GetReference(), true);
			}
			if ( Shader.PixelShader.bActive )
			{
				SetShaderSamplerTextures(RHICmdList, CurrentDrawKey, SF_Pixel, CachedPixelShaders.FindRef(Shader.PixelShader).GetReference(), true);
			}
			if ( Shader.GeometryShader.bActive )
			{
				SetShaderSamplerTextures(RHICmdList, CurrentDrawKey, SF_Geometry, CachedGeometryShaders.FindRef(Shader.GeometryShader).GetReference(), true);
			}
			if ( Shader.HullShader.bActive )
			{
				SetShaderSamplerTextures(RHICmdList, CurrentDrawKey, SF_Hull, CachedHullShaders.FindRef(Shader.HullShader).GetReference(), true);
			}
			if ( Shader.DomainShader.bActive )
			{
				SetShaderSamplerTextures(RHICmdList, CurrentDrawKey, SF_Domain, CachedDomainShaders.FindRef(Shader.DomainShader).GetReference(), true);
			}
		}
		
		for( auto VertexDec : Shader.VertexDeclaration )
		{
			RHICmdList.SetStreamSource(VertexDec.StreamIndex, nullptr, 0, 0);
		}
		
		INC_DWORD_STAT(STATGROUP_NumPredrawn);
		INC_DWORD_STAT(STATGROUP_TotalPredrawn);
	}
}

static FORCEINLINE uint32 CalculateSizeOfSamplerStateInitializer()
{
	static uint32 SizeOfSamplerStateInitializer = 0;
	if (SizeOfSamplerStateInitializer == 0)
	{
		TArray<uint8> Data;
		FMemoryWriter Writer(Data);
		FSamplerStateInitializerRHI State;
		Writer << State;
		SizeOfSamplerStateInitializer = Data.Num();
	}
	return SizeOfSamplerStateInitializer;
}

bool FShaderCache::FSamplerStateInitializerRHIKeyFuncs::Matches(KeyInitType A,KeyInitType B)
{
	return FMemory::Memcmp(&A, &B, CalculateSizeOfSamplerStateInitializer()) == 0;
}

uint32 FShaderCache::FSamplerStateInitializerRHIKeyFuncs::GetKeyHash(KeyInitType Key)
{
	return FCrc::MemCrc_DEPRECATED(&Key, CalculateSizeOfSamplerStateInitializer());
}

int32 FShaderCache::GetPredrawBatchTime() const
{
	return OverridePredrawBatchTime == 0 ? PredrawBatchTime : OverridePredrawBatchTime;
}

int32 FShaderCache::GetTargetPrecompileFrameTime() const
{
	return OverridePrecompileTime == 0 ? TargetPrecompileFrameTime : OverridePrecompileTime;
}

void FShaderCache::MergePlatformCaches(FShaderPlatformCache& Target, FShaderPlatformCache const& Source)
{
	// Merge the shaders & provide a remapping
	TMap<int32, int32> ShaderRemap;
	for(int32 i = 0; i < (int32)Source.Shaders.Num(); i++)
	{
		ShaderRemap.Add(i, Target.Shaders.Add(Source.Shaders[i]));
	}
	
	// Merge the bound shader states & provide a remapping
	TMap<int32, int32> BoundShaderStatesRemap;
	for(int32 i = 0; i < (int32)Source.BoundShaderStates.Num(); i++)
	{
		BoundShaderStatesRemap.Add(i, Target.BoundShaderStates.Add(Source.BoundShaderStates[i]));
	}
	
	// Use the shader & BSS remapping to merge the map of shaders to bound shader states used to prebind
	for(auto Pair : Source.ShaderStateMembership)
	{
		int32 UnremappedShader = Pair.Key;
		check(UnremappedShader >= 0);
		int32 RemappedShader = ShaderRemap.FindChecked(Pair.Key);
		TSet<int32>& OutShaderStateMembership = Target.ShaderStateMembership.FindOrAdd(RemappedShader);
		for(int32 UnmappedBoundState : Pair.Value)
		{
			int32 RemappedBoundState = BoundShaderStatesRemap.FindChecked(UnmappedBoundState);
			OutShaderStateMembership.Add(RemappedBoundState);
		}
	}
	
	// Merge the render targets and provide a remapping
	TMap<int32, int32> RenderTargetsRemap;
	for(int32 i = 0; i < (int32)Source.RenderTargets.Num(); i++)
	{
		RenderTargetsRemap.Add(i, Target.RenderTargets.Add(Source.RenderTargets[i]));
	}
	
	// Merge the resources and provide a remapping
	TMap<int32, int32> ResourcesRemap;
	for(int32 i = 0; i < (int32)Source.Resources.Num(); i++)
	{
		ResourcesRemap.Add(i, Target.Resources.Add(Source.Resources[i]));
	}
	
	// Merge the samplers and provide a remapping
	TMap<int32, int32> SamplerStatesRemap;
	for(int32 i = 0; i < (int32)Source.SamplerStates.Num(); i++)
	{
		SamplerStatesRemap.Add(i, Target.SamplerStates.Add(Source.SamplerStates[i]));
	}
	
	// Merge the draw states, using the various remappings & provide a draw state remapping
	TMap<int32, int32> DrawStatesRemap;
	for(int32 i = 0; i < (int32)Source.DrawStates.Num(); i++)
	{
		FShaderDrawKey const& OldKey = Source.DrawStates[i];
		FShaderDrawKey NewKey;
		NewKey.BlendState = OldKey.BlendState;
		NewKey.RasterizerState = OldKey.RasterizerState;
		NewKey.DepthStencilState = OldKey.DepthStencilState;
		for(uint32 Index = 0; Index < MaxSimultaneousRenderTargets; Index++)
		{
			NewKey.RenderTargets[Index] = OldKey.RenderTargets[Index] < (uint32)FShaderDrawKey::NullState ? RenderTargetsRemap.FindChecked(OldKey.RenderTargets[Index]) : OldKey.RenderTargets[Index];
		}
		for(uint32 Freq = 0; Freq < SF_NumFrequencies; Freq++)
		{
			for(uint32 Sampler = 0; Sampler < FShaderDrawKey::MaxNumSamplers; Sampler++)
			{
				NewKey.SamplerStates[Freq][Sampler] = OldKey.SamplerStates[Freq][Sampler] < (uint32)FShaderDrawKey::NullState ? SamplerStatesRemap.FindChecked(OldKey.SamplerStates[Freq][Sampler]) : OldKey.SamplerStates[Freq][Sampler];
			}
			for(uint32 Resource = 0; Resource < FShaderDrawKey::MaxNumResources; Resource++)
			{
				NewKey.Resources[Freq][Resource] = OldKey.Resources[Freq][Resource] < (uint32)FShaderDrawKey::NullState ? ResourcesRemap.FindChecked(OldKey.Resources[Freq][Resource]) : OldKey.Resources[Freq][Resource];
			}
		}
		NewKey.DepthStencilTarget = OldKey.DepthStencilTarget < (uint32)FShaderDrawKey::NullState ? RenderTargetsRemap.FindChecked(OldKey.DepthStencilTarget) : OldKey.DepthStencilTarget;
		NewKey.IndexType = OldKey.IndexType;
		NewKey.Hash = 0;
		
		DrawStatesRemap.Add(i, Target.DrawStates.Add(NewKey));
	}
	
	// Merge the predraw states & provide a remapping
	TMap<int32, int32> PreDrawRemap;
	for(uint32 i = 0; i < Source.PreDrawEntries.Num(); i++)
	{
		FShaderPreDrawEntry const& OldEntry = Source.PreDrawEntries[i];
		FShaderPreDrawEntry NewEntry;
		NewEntry.BoundStateIndex = BoundShaderStatesRemap.FindChecked(OldEntry.BoundStateIndex);
		NewEntry.DrawKeyIndex = DrawStatesRemap.FindChecked(OldEntry.DrawKeyIndex);
		NewEntry.bPredrawn = false;
		PreDrawRemap.Add(i, Target.PreDrawEntries.Add(NewEntry));
	}
	
	// Merge the complex mapping between streaming keys and BSS/pre-draw entries using the final remapping tables.
	for(auto Pair : Source.StreamingDrawStates)
	{
		FShaderStreamingCache& OutStreamCache = Target.StreamingDrawStates.FindOrAdd(Pair.Key);
		for(auto BSSPreDrawPair : Pair.Value.ShaderDrawStates)
		{
			int32 RemappedBSSIndex = BoundShaderStatesRemap.FindChecked(BSSPreDrawPair.Key);
			TSet<int32>& OutBSSStates = OutStreamCache.ShaderDrawStates.FindOrAdd(RemappedBSSIndex);
			for(int32 UnremappedPredraw : BSSPreDrawPair.Value)
			{
				int32 RemappedPreDraw = PreDrawRemap.FindChecked(UnremappedPredraw);
				OutBSSStates.Add(RemappedPreDraw);
			}
		}
	}
}

void FShaderCache::MergeShaderCaches(FShaderCaches& Target, FShaderCaches const& Source)
{
	for(uint32 i = 0; i < SP_NumPlatforms; i++)
	{
		FShaderPlatformCache* TargetCache = Target.PlatformCaches.Find(i);
		FShaderPlatformCache const* SourceCache = Source.PlatformCaches.Find(i);
		if(TargetCache && SourceCache)
		{
			MergePlatformCaches(*TargetCache, *SourceCache);
		}
		else if(SourceCache)
		{
			Target.PlatformCaches.Add(i, *SourceCache);
		}
	}
}

bool FShaderCache::LoadShaderCache(FString Path, FShaderCaches* InCache)
{
	bool bLoadedCache = false;
	if ( IFileManager::Get().FileSize(*Path) > 0 )
	{
		FArchive* BinaryShaderAr = IFileManager::Get().CreateFileReader(*Path);
		
		if ( BinaryShaderAr != nullptr )
		{
			*BinaryShaderAr << *InCache;
			
			bool const bNoError = !BinaryShaderAr->IsError();
			bool const bMatchedCustomLatest = BinaryShaderAr->CustomVer(FShaderCacheCustomVersion::Key) == FShaderCacheCustomVersion::Latest;
			bool const bMatchedGameVersion = BinaryShaderAr->CustomVer(FShaderCacheCustomVersion::GameKey) == FShaderCache::GameVersion;
			
			bLoadedCache = ( bNoError && bMatchedCustomLatest && bMatchedGameVersion );
			
			delete BinaryShaderAr;
		}
	}
	return bLoadedCache;
}

bool FShaderCache::SaveShaderCache(FString Path, FShaderCaches* InCache)
{
	FArchive* BinaryShaderAr = IFileManager::Get().CreateFileWriter(*Path);
	if( BinaryShaderAr != NULL )
	{
		*BinaryShaderAr << *InCache;
		BinaryShaderAr->Flush();
		delete BinaryShaderAr;
		return true;
	}
	
	return false;
}

bool FShaderCache::MergeShaderCacheFiles(FString Left, FString Right, FString Output)
{
	FShaderCaches LeftCache;
	FShaderCaches RightCache;
	
	if ( LoadShaderCache(Left, &LeftCache) && LoadShaderCache(Right, &RightCache) )
	{
		MergeShaderCaches(LeftCache, RightCache);
		return SaveShaderCache(Output, &LeftCache);
	}
	return false;
}

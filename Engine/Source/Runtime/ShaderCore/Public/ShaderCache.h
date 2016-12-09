// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCache.h: Shader precompilation mechanism
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Misc/Guid.h"
#include "HAL/IConsoleManager.h"
#include "Misc/SecureHash.h"
#include "RHI.h"
#include "TickableObjectRenderThread.h"

/** Custom serialization version for FShaderCache */
struct SHADERCORE_API FShaderCacheCustomVersion
{
	static const FGuid Key;
	static const FGuid GameKey;
	enum Type {	Initial, PreDraw, CacheHashes, OptimisedHashes, StreamingKeys, AdditionalResources, SeparateBinaries, IndexedSets, PreDrawEntries, CompressedBinaries, CacheMerging, ShaderPipelines, SimpleVersioning, Latest = SimpleVersioning };
};

enum EShaderCacheOptions
{
	SCO_Default,
	SCO_NoShaderPreload = 1 << 0, /* Disable preloading of shaders for RHIs where loading all shaders is too slow (i.e. Metal online compiler). */
	SCO_Cooking = 1 << 1 /* Run the ShaderCache in a very limited mode suitable only for cooking binary shader caches. */
};

/** Texture type enum for shader cache draw keys */
enum EShaderCacheTextureType
{
	SCTT_Invalid,
	SCTT_Texture1D,
	SCTT_Texture2D,
	SCTT_Texture3D,
	SCTT_TextureCube,
	SCTT_Texture1DArray,
	SCTT_Texture2DArray,
	SCTT_TextureCubeArray,
	SCTT_Buffer
};

/** The minimum texture state required for logging shader draw states */
struct SHADERCORE_API FShaderTextureKey
{
	mutable uint32 Hash;
	uint32 X;
	uint32 Y;
	uint32 Z;
	uint32 Flags; // ETextureCreateFlags
	uint32 MipLevels;
	uint32 Samples;
	uint8 Format;
	TEnumAsByte<EShaderCacheTextureType> Type;
	
	FShaderTextureKey()
	: Hash(0)
	, X(0)
	, Y(0)
	, Z(0)
	, Flags(0)
	, MipLevels(0)
	, Samples(0)
	, Format(PF_Unknown)
	, Type(SCTT_Invalid)
	{
	}
	
	friend bool operator ==(const FShaderTextureKey& A,const FShaderTextureKey& B)
	{
		return A.X == B.X && A.Y == B.Y && A.Z == B.Z && A.Flags == B.Flags && A.MipLevels == B.MipLevels && A.Samples == B.Samples && A.Format == B.Format && A.Type == B.Type;
	}
	
	friend uint32 GetTypeHash(const FShaderTextureKey &Key)
	{
		if(!Key.Hash)
		{
			Key.Hash = Key.X * 3;
			Key.Hash ^= Key.Y * 2;
			Key.Hash ^= Key.Z;
			Key.Hash ^= Key.Flags;
			Key.Hash ^= (Key.Format << 24);
			Key.Hash ^= (Key.MipLevels << 16);
			Key.Hash ^= (Key.Samples << 8);
			Key.Hash ^= Key.Type;
		}
		return Key.Hash;
	}
	
	friend FArchive& operator<<( FArchive& Ar, FShaderTextureKey& Info )
	{
		return Ar << Info.Format << Info.Type << Info.Samples << Info.MipLevels << Info.Flags << Info.X << Info.Y << Info.Z << Info.Hash;
	}
};

/** SRV state tracked by the shader-cache to properly predraw shaders */
struct SHADERCORE_API FShaderResourceKey
{
	FShaderTextureKey Tex;
	mutable uint32 Hash;
	uint32 BaseMip;
	uint32 MipLevels;
	uint8 Format;
	bool bSRV;
	
	FShaderResourceKey()
	: Hash(0)
	, BaseMip(0)
	, MipLevels(0)
	, Format(0)
	, bSRV(false)
	{
	}
	
	friend bool operator ==(const FShaderResourceKey& A,const FShaderResourceKey& B)
	{
		return A.BaseMip == B.BaseMip && A.MipLevels == B.MipLevels && A.Format == B.Format && A.bSRV == B.bSRV && A.Tex == B.Tex;
	}
	
	friend uint32 GetTypeHash(const FShaderResourceKey &Key)
	{
		if(!Key.Hash)
		{
			Key.Hash = GetTypeHash(Key.Tex);
			Key.Hash ^= (Key.BaseMip << 24);
			Key.Hash ^= (Key.MipLevels << 16);
			Key.Hash ^= (Key.Format << 8);
			Key.Hash ^= (uint32)Key.bSRV;
		}
		return Key.Hash;
	}
	
	friend FArchive& operator<<( FArchive& Ar, FShaderResourceKey& Info )
	{
		return Ar << Info.Tex << Info.BaseMip << Info.MipLevels << Info.Format << Info.bSRV << Info.Hash;
	}
};

/** Render target state tracked for predraw */
struct SHADERCORE_API FShaderRenderTargetKey
{
	FShaderTextureKey Texture;
	mutable uint32 Hash;
	uint32 MipLevel;
	uint32 ArrayIndex;
	
	FShaderRenderTargetKey()
	: Hash(0)
	, MipLevel(0)
	, ArrayIndex(0)
	{
		
	}
	
	friend bool operator ==(const FShaderRenderTargetKey& A,const FShaderRenderTargetKey& B)
	{
		return A.MipLevel == B.MipLevel && A.ArrayIndex == B.ArrayIndex && A.Texture == B.Texture;
	}
	
	friend uint32 GetTypeHash(const FShaderRenderTargetKey &Key)
	{
		if(!Key.Hash)
		{
			Key.Hash = GetTypeHash(Key.Texture);
			Key.Hash ^= (Key.MipLevel << 8);
			Key.Hash ^= Key.ArrayIndex;
		}
		return Key.Hash;
	}
	
	friend FArchive& operator<<( FArchive& Ar, FShaderRenderTargetKey& Info )
	{
		return Ar << Info.Texture << Info.MipLevel << Info.ArrayIndex << Info.Hash;
	}
};

/**
 * The FShaderCache provides mechanisms for reducing shader hitching in-game:
 *	- Early submission during shader deserialisation rather than on-demand (r.UseShaderCaching 0/1).
 *	- Tracking of bound-shader-states so that they may be pre-bound during early submission (r.UseShaderCaching 0/1).
 *	- Tracking of RHI draw states so that each bound-shader-state can be predrawn (r.UseShaderDrawLog 0/1).
 *	- Predrawing of tracked RHI draw states to eliminate first-use hitches (r.UseShaderPredraw 0/1).
 *	- Control over time spent predrawing each frame to distribute over many frames if required (r.PredrawBatchTime Time (ms), -1 for all).
 *  - Accumulation of all shader byte code into a single cache file (r.UseShaderBinaryCache 0/1).
 *  - Asynchronous precompilation of shader code during gameplay (r.UseAsyncShaderPrecompilation 0/1).
 *  - The target maximum frame time to maintain when r.UseAsyncShaderPrecompilation is enabled (r.TargetPrecompileFrameTime Time (ms), -1 to precompile all shaders at once).
 *  - An option to temporarily accelerate predrawing when in a non-interactive mode such as a load screen (r.AccelPredrawBatchTime Time (ms), 0 to use r.PredrawBatchTime).
 *  - An option to accelerate asynchronous precompilation when in a non-interactive mode such as a load screen (r.AccelTargetPrecompileFrameTime Time (ms), 0 to use r.TargetPrecompileFrameTime).
 *  - A maximum amount of time to spend loading the shaders at launch before moving on to asynchrous precompilation (r.InitialShaderLoadTime Time (ms), -1 to load synchronously).
 *
 * The cache should be populated by enabling r.UseShaderCaching & r.UseShaderDrawLog on a development machine. 
 * Users/players should then consume the cache by enabling r.UseShaderCaching & r.UseShaderPredraw. 
 * Draw logging (r.UseShaderDrawLog) adds noticeable fixed overhead so should be avoided if possible. 
 * The binary shader cache can either be accumulated during play through or (currently Mac targets only) during cooking by specifying the shader platforms to cache in the CachedShaderFormats array within the /Script/MacTargetPlatform.MacTargetSettings settings group of Engine.ini. 
 * For OpenGL the binary cache contains enough data about shader pipelines to construct fully linked GL programs or GL program pipelines (depending on availability of GL_ARB_separate_shader_objects) but not enough for pipeline construction on any other RHI. 
 * This can help reduce the amount of hitching on OpenGL without first playing through the game, though this is still advisable for maximum effect.
 * Since the caching is done via shader hashes it is also advisable to only use this as a final optimisation tool 
 * when content is largely complete as changes to shader hashes will result in unusued entries accumulating in the cache, 
 * increasing cache size without reducing hitches.
 * 
 * Cache locations:
 *  - While populating: <Game>/Saved/DrawCache.ushadercache, <Game>/Saved/ByteCodeCache.ushadercode
 *  - For distribution: <Game>/Content/DrawCache.ushadercache, <Game>/Content/ByteCodeCache.ushadercode
 * The code will first try and load the writeable cache, then fallback to the distribution cache if needed.
 * 
 * To Integrate Into A Project;
 *  - Enable r.UseShaderCaching & r.UseShaderPredraw in the project configuration for all users.
 *  - If possible enable r.UseShaderDrawLog only for internal builds & ensure that shader draw states are recorded during final QA for each release. When not feasible (e.g. an extremely large and/or streaming game) enable for all users.
 *  - Test whether this setup is sufficient to reduce hitching to an acceptable level - the additional optimisations require more work and have significant side-effects.
 *  - If the above is insufficient then also enable r.UseShaderBinaryCache and configure the CachedShaderFormats to ensure population of the binary cache (currently Mac only).
 *  - All shader code will now be loaded at startup and all predraw operations will occur on the first frame, so if the loading times are too extreme also set r.PredrawBatchTime to a value greater than 0 in ms to spend predrawing each frame.
 *  - You may also wish to specify a larger value for r.AccelPredrawBatchTime that can be applied when showing loading screens or other non-interactive content. 
 *  - To inform the shader cache that it may accelerate predrawing at the expense of game frame rate call FShaderCache::BeginAcceleratedBatching and when it is necessary to return to the less expensive predraw batching call FShaderCache::EndAcceleratedBatching.
 *  - A call to FShaderCache::FlushOutstandingBatches will cause all remaining shaders to be processed in the next frame.
 *  - If the project still takes too long to load initially then enable r.UseAsyncShaderPrecompilation and set r.InitialShaderLoadTime to a value > 0, the longer this is the less work that must be done while the game is running.
 *  - You can tweak the desired target frame time while asynchronously precompiling shaders by modifying the value of r.TargetPrecompileFrameTime.
 *  - As with predraw batching a more aggressive value can be specified for precompilation by setting r.AccelTargetPrecompileFrameTime to a larger value and then calling FShaderCache::BeginAcceleratedBatching/FShaderCache::EndAcceleratedBatching.
 *  - With r.UseAsyncShaderPrecompilation enabled a call to FShaderCache::FlushOutstandingBatches will also flush any outstanding compilation requests.
 *
 * Handling Updates/Invalidation:
 * When the cache needs to be updated & writable caches invalidated the game should specify a new GameVersion.
 * Call FShaderCache::SetGameVersion before initialisating the RHI (which initialises the cache), which will cause 
 * the contents of caches from previous version to be ignored. At present you cannot carry over cache entries from a previous version.
 *
 * Region/Stream Batching:
 * For streaming games, or where the cache becomes very large, calls to FShaderCache::LogStreamingKey should be added with unique values for
 * the currently relevant game regions/streaming levels (as required). Logged draw states will be linked to the active streaming key.
 * This limits predrawing to only those draw states required by the active streaming key on subsequent runs.
 * 
 * Limitations:
 * The shader cache consumes considerable memory, especially when binary shader caching is enabled. It is not recommended that binary caching be enabled for memory constrained platforms.
 * At present only Mac support has received significant testing, other platforms may work but there may be unanticipated issues in the current version.
 * The binary shader cache can only be generated during cooking for Mac target platforms at present.
 *
 * Supported RHIs:
 * - OpenGLDrv
 * - MetalRHI
 */
class SHADERCORE_API FShaderCache : public FTickableObjectRenderThread
{
	struct SHADERCORE_API FShaderCacheKey
	{
		FShaderCacheKey() : Platform(SP_NumPlatforms), Frequency(SF_NumFrequencies), Hash(0), bActive(false) {}
		
		FSHAHash SHAHash;
		EShaderPlatform Platform;
		EShaderFrequency Frequency;
		mutable uint32 Hash;
		bool bActive;
		
		friend bool operator ==(const FShaderCacheKey& A,const FShaderCacheKey& B)
		{
			return A.SHAHash == B.SHAHash && A.Platform == B.Platform && A.Frequency == B.Frequency && A.bActive == B.bActive;
		}
		
		friend uint32 GetTypeHash(const FShaderCacheKey &Key)
		{
			if(!Key.Hash)
			{
				uint32 TargetFrequency = Key.Frequency;
				uint32 TargetPlatform = Key.Platform;
				Key.Hash = FCrc::MemCrc_DEPRECATED((const void*)&Key.SHAHash, sizeof(Key.SHAHash)) ^ GetTypeHash(TargetPlatform) ^ (GetTypeHash(TargetFrequency) << 16) ^ GetTypeHash(Key.bActive);
			}
			return Key.Hash;
		}
		
		friend FArchive& operator<<( FArchive& Ar, FShaderCacheKey& Info )
		{
			uint32 TargetFrequency = Info.Frequency;
			uint32 TargetPlatform = Info.Platform;
			Ar << TargetFrequency << TargetPlatform;
			Info.Frequency = (EShaderFrequency)TargetFrequency;
			Info.Platform = (EShaderPlatform)TargetPlatform;
			return Ar << Info.SHAHash << Info.bActive << Info.Hash;
		}
	};
	
	struct SHADERCORE_API FShaderDrawKey
	{
		enum
		{
			MaxNumSamplers = 16,
			MaxNumResources = 128,
			NullState = ~(0u),
			InvalidState = ~(1u)
		};
		
		FShaderDrawKey()
		: DepthStencilTarget(NullState)
		, Hash(0)
		, IndexType(0)
		{
			FMemory::Memzero(&BlendState, sizeof(BlendState));
			FMemory::Memzero(&RasterizerState, sizeof(RasterizerState));
			FMemory::Memzero(&DepthStencilState, sizeof(DepthStencilState));
			FMemory::Memset(RenderTargets, 255, sizeof(RenderTargets));
			FMemory::Memset(SamplerStates, 255, sizeof(SamplerStates));
			FMemory::Memset(Resources, 255, sizeof(Resources));
			check(GetMaxTextureSamplers() <= MaxNumSamplers);
		}
		
		struct FShaderRasterizerState
		{
			FShaderRasterizerState() { FMemory::Memzero(*this); }
			FShaderRasterizerState(FRasterizerStateInitializerRHI const& Other) { operator=(Other); }
			
			float DepthBias;
			float SlopeScaleDepthBias;
			TEnumAsByte<ERasterizerFillMode> FillMode;
			TEnumAsByte<ERasterizerCullMode> CullMode;
			bool bAllowMSAA;
			bool bEnableLineAA;
			
			FShaderRasterizerState& operator=(FRasterizerStateInitializerRHI const& Other)
			{
				DepthBias = Other.DepthBias;
				SlopeScaleDepthBias = Other.SlopeScaleDepthBias;
				FillMode = Other.FillMode;
				CullMode = Other.CullMode;
				bAllowMSAA = Other.bAllowMSAA;
				bEnableLineAA = Other.bEnableLineAA;
				return *this;
			}
			
			operator FRasterizerStateInitializerRHI () const
			{
				FRasterizerStateInitializerRHI Initializer = {FillMode, CullMode, DepthBias, SlopeScaleDepthBias, bAllowMSAA, bEnableLineAA};
				return Initializer;
			}
			
			friend FArchive& operator<<(FArchive& Ar,FShaderRasterizerState& RasterizerStateInitializer)
			{
				Ar << RasterizerStateInitializer.DepthBias;
				Ar << RasterizerStateInitializer.SlopeScaleDepthBias;
				Ar << RasterizerStateInitializer.FillMode;
				Ar << RasterizerStateInitializer.CullMode;
				Ar << RasterizerStateInitializer.bAllowMSAA;
				Ar << RasterizerStateInitializer.bEnableLineAA;
				return Ar;
			}
			
			friend uint32 GetTypeHash(const FShaderRasterizerState &Key)
			{
				uint32 KeyHash = (*((uint32*)&Key.DepthBias) ^ *((uint32*)&Key.SlopeScaleDepthBias));
				KeyHash ^= (Key.FillMode << 8);
				KeyHash ^= Key.CullMode;
				KeyHash ^= Key.bAllowMSAA ? 2 : 0;
				KeyHash ^= Key.bEnableLineAA ? 1 : 0;
				return KeyHash;
			}
		};
		
		FBlendStateInitializerRHI BlendState;
		FShaderRasterizerState RasterizerState;
		FDepthStencilStateInitializerRHI DepthStencilState;
		uint32 RenderTargets[MaxSimultaneousRenderTargets];
		uint32 SamplerStates[SF_NumFrequencies][MaxNumSamplers];
		uint32 Resources[SF_NumFrequencies][MaxNumResources];
		uint32 DepthStencilTarget;
		mutable uint32 Hash;
		uint8 IndexType;
		
		friend bool operator ==(const FShaderDrawKey& A,const FShaderDrawKey& B)
		{
			bool Compare = A.IndexType == B.IndexType &&
			A.DepthStencilTarget == B.DepthStencilTarget &&
			FMemory::Memcmp(&A.Resources, &B.Resources, sizeof(A.Resources)) == 0 &&
			FMemory::Memcmp(&A.SamplerStates, &B.SamplerStates, sizeof(A.SamplerStates)) == 0 &&
			FMemory::Memcmp(&A.BlendState, &B.BlendState, sizeof(FBlendStateInitializerRHI)) == 0 &&
			FMemory::Memcmp(&A.RasterizerState, &B.RasterizerState, sizeof(FShaderRasterizerState)) == 0 &&
			FMemory::Memcmp(&A.DepthStencilState, &B.DepthStencilState, sizeof(FDepthStencilStateInitializerRHI)) == 0 &&
			FMemory::Memcmp(&A.RenderTargets, &B.RenderTargets, sizeof(A.RenderTargets)) == 0;
			return Compare;
		}
		
		friend uint32 GetTypeHash(const FShaderDrawKey &Key)
		{
			if(!Key.Hash)
			{
				Key.Hash ^= (Key.BlendState.bUseIndependentRenderTargetBlendStates ? (1 << 31) : 0);
				for( uint32 i = 0; i < MaxSimultaneousRenderTargets; i++ )
				{
					Key.Hash ^= (Key.BlendState.RenderTargets[i].ColorBlendOp << 24);
					Key.Hash ^= (Key.BlendState.RenderTargets[i].ColorSrcBlend << 16);
					Key.Hash ^= (Key.BlendState.RenderTargets[i].ColorDestBlend << 8);
					Key.Hash ^= (Key.BlendState.RenderTargets[i].ColorWriteMask << 0);
					Key.Hash ^= (Key.BlendState.RenderTargets[i].AlphaBlendOp << 24);
					Key.Hash ^= (Key.BlendState.RenderTargets[i].AlphaSrcBlend << 16);
					Key.Hash ^= (Key.BlendState.RenderTargets[i].AlphaDestBlend << 8);
					Key.Hash ^= Key.RenderTargets[i];
				}
				
				for( uint32 i = 0; i < SF_NumFrequencies; i++ )
				{
					for( uint32 j = 0; j < MaxNumSamplers; j++ )
					{
						Key.Hash ^= Key.SamplerStates[i][j];
					}
					for( uint32 j = 0; j < MaxNumResources; j++ )
					{
						Key.Hash ^= Key.Resources[i][j];
					}
				}
				
				Key.Hash ^= (Key.DepthStencilState.bEnableDepthWrite ? (1 << 31) : 0);
				Key.Hash ^= (Key.DepthStencilState.DepthTest << 24);
				Key.Hash ^= (Key.DepthStencilState.bEnableFrontFaceStencil ? (1 << 23) : 0);
				Key.Hash ^= (Key.DepthStencilState.FrontFaceStencilTest << 24);
				Key.Hash ^= (Key.DepthStencilState.FrontFaceStencilFailStencilOp << 16);
				Key.Hash ^= (Key.DepthStencilState.FrontFaceDepthFailStencilOp << 8);
				Key.Hash ^= (Key.DepthStencilState.FrontFacePassStencilOp);
				Key.Hash ^= (Key.DepthStencilState.bEnableBackFaceStencil ? (1 << 15) : 0);
				Key.Hash ^= (Key.DepthStencilState.BackFaceStencilTest << 24);
				Key.Hash ^= (Key.DepthStencilState.BackFaceStencilFailStencilOp << 16);
				Key.Hash ^= (Key.DepthStencilState.BackFaceDepthFailStencilOp << 8);
				Key.Hash ^= (Key.DepthStencilState.BackFacePassStencilOp);
				Key.Hash ^= (Key.DepthStencilState.StencilReadMask << 8);
				Key.Hash ^= (Key.DepthStencilState.StencilWriteMask);
				
				Key.Hash ^= GetTypeHash(Key.DepthStencilTarget);
				Key.Hash ^= GetTypeHash(Key.IndexType);
				
				Key.Hash ^= GetTypeHash(Key.RasterizerState);
			}
			return Key.Hash;
		}
		
		friend FArchive& operator<<( FArchive& Ar, FShaderDrawKey& Info )
		{
			for ( uint32 i = 0; i < SF_NumFrequencies; i++ )
			{
				for ( uint32 j = 0; j < MaxNumSamplers; j++ )
				{
					Ar << Info.SamplerStates[i][j];
				}
				for ( uint32 j = 0; j < MaxNumResources; j++ )
				{
					Ar << Info.Resources[i][j];
				}
			}
			for ( uint32 i = 0; i < MaxSimultaneousRenderTargets; i++ )
			{
				Ar << Info.RenderTargets[i];
			}
			return Ar << Info.BlendState << Info.RasterizerState << Info.DepthStencilState << Info.DepthStencilTarget << Info.IndexType << Info.Hash;
		}
	};
	
public:
	FShaderCache(uint32 Options, uint32 InMaxResources);
	virtual ~FShaderCache();
	
    /** Called by the game to set the game specific shader cache version, only caches of this version will be loaded. Must be called before RHI initialisation, as InitShaderCache will load any existing cache. Defaults to FEngineVersion::Current().GetChangelist() if never called. */
	static void SetGameVersion(int32 InGameVersion);
	static int32 GetGameVersion() { return GameVersion; }
	
	/** Shader cache initialisation, called only by the RHI. */
	static void InitShaderCache(uint32 Options, uint32 InMaxResources = FShaderDrawKey::MaxNumResources);
	/** Loads any existing cache of shader binaries, called by the RHI after full initialisation. */
	static void LoadBinaryCache();
	/** Adds the shader formats to the set to be cached during cook for the given platform. */
	static void CacheCookedShaderPlatforms(FName PlatformName, TArray<FName> const& CachedShaderFormats);
	/** Save binary cache immediately to the given output dir for the given platform. */
	static void SaveBinaryCache(FString OutputDir, FName PlatformName);
	/** Merge the shader draw state cache files at Left & Right into a new file at Output */
	static bool MergeShaderCacheFiles(FString Left, FString Right, FString Output);
	/** Shader cache shutdown, called only by the RHI. */
	static void ShutdownShaderCache();
	
	/** Get the global shader cache if it exists or nullptr otherwise. */
	static FORCEINLINE FShaderCache* GetShaderCache()
	{
		return bUseShaderCaching ? Cache : nullptr;
	}
	
	/** Instantiate or retrieve a vertex shader from the cache for the provided code & hash. */
	FVertexShaderRHIRef GetVertexShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	/** Instantiate or retrieve a pixel shader from the cache for the provided code & hash. */
	FPixelShaderRHIRef GetPixelShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	/** Instantiate or retrieve a geometry shader from the cache for the provided code & hash. */
	FGeometryShaderRHIRef GetGeometryShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	/** Instantiate or retrieve a hull shader from the cache for the provided code & hash. */
	FHullShaderRHIRef GetHullShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	/** Instantiate or retrieve a domain shader from the cache for the provided code & hash. */
	FDomainShaderRHIRef GetDomainShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	/** Instantiate or retrieve a compute shader from the cache for the provided code & hash. */
	FComputeShaderRHIRef GetComputeShader(EShaderPlatform Platform, TArray<uint8> const& Code);
	/** Instantiate or retrieve a vertex declaration from the cache for the provided layout. */
	FVertexDeclarationRHIParamRef GetVertexDeclaration(FVertexDeclarationElementList& VertexElements);
	
	/** Called by the game. Logs whether a user-defined streaming key is active or disabled, which is used to batch logged draw-states so that they are only predrawn when the same combination of streaming keys are active. This is important for streaming games, or those with very large numbers of shaders, as it will reduce the number of predraw requests performed at any given time. */
	static FORCEINLINE void LogStreamingKey(uint32 StreamingKey, bool const bActive)
	{
		if ( Cache )
		{
			Cache->InternalLogStreamingKey(StreamingKey, bActive);
		}
	}
	
	/** Called by the Engine/RHI. Logs the use of a given shader & will ensure it is instantiated if not already. */
	static FORCEINLINE void LogShader(EShaderPlatform Platform, EShaderFrequency Frequency, FSHAHash Hash, TArray<uint8> const& Code)
	{
		if ( Cache )
		{
			Cache->InternalLogShader(Platform, Frequency, Hash, Code);
		}
	}
	
	/** Called by the RHI. Logs the user of a vertex declaration so that the cache can find it for recording shader & draw states. */
	static FORCEINLINE void LogVertexDeclaration(const FVertexDeclarationElementList& VertexElements, FVertexDeclarationRHIParamRef VertexDeclaration)
	{
		if ( Cache )
		{
			Cache->InternalLogVertexDeclaration(VertexElements, VertexDeclaration);
		}
	}
	
	/** Called by the RHI. Logs the construction of a bound shader state & will record it for prebinding on subsequent runs. */
	static FORCEINLINE void LogBoundShaderState(EShaderPlatform Platform, FVertexDeclarationRHIParamRef VertexDeclaration,
												FVertexShaderRHIParamRef VertexShader,
												FPixelShaderRHIParamRef PixelShader,
												FHullShaderRHIParamRef HullShader,
												FDomainShaderRHIParamRef DomainShader,
												FGeometryShaderRHIParamRef GeometryShader,
												FBoundShaderStateRHIParamRef BoundState)
	{
		if ( Cache )
		{
			Cache->InternalLogBoundShaderState(Platform, VertexDeclaration, VertexShader, PixelShader, HullShader, DomainShader, GeometryShader, BoundState);
		}
	}
	
	/** Called by the RHI. Logs the construction of a blend state & caches it so that draw states can be properly recorded. No-op when r.UseShaderDrawLog & r.UseShaderPredraw are disabled. */
	static FORCEINLINE void LogBlendState(FBlendStateInitializerRHI const& Init, FBlendStateRHIParamRef State)
	{
		if ( Cache )
		{
			Cache->InternalLogBlendState(Init, State);
		}
	}
	
	/** Called by the RHI. Logs the construction of a rasterize state & caches it so that draw states can be properly recorded. No-op when r.UseShaderDrawLog & r.UseShaderPredraw are disabled. */
	static FORCEINLINE void LogRasterizerState(FRasterizerStateInitializerRHI const& Init, FRasterizerStateRHIParamRef State)
	{
		if ( Cache )
		{
			Cache->InternalLogRasterizerState(Init, State);
		}
	}
	
	/** Called by the RHI. Logs the construction of a stencil state & caches it so that draw states can be properly recorded. No-op when r.UseShaderDrawLog & r.UseShaderPredraw are disabled. */
	static FORCEINLINE void LogDepthStencilState(FDepthStencilStateInitializerRHI const& Init, FDepthStencilStateRHIParamRef State)
	{
		if ( Cache )
		{
			Cache->InternalLogDepthStencilState(Init, State);
		}
	}
	
	/** Called by the RHI. Logs the construction of a sampelr state & caches it so that draw states can be properly recorded. No-op when r.UseShaderDrawLog & r.UseShaderPredraw are disabled. */
	static FORCEINLINE void LogSamplerState(FSamplerStateInitializerRHI const& Init, FSamplerStateRHIParamRef State)
	{
		if ( Cache )
		{
			Cache->InternalLogSamplerState(Init, State);
		}
	}
	
	/** Called by the RHI. Logs the construction of a texture & caches it so that draw states can be properly recorded. No-op when r.UseShaderDrawLog & r.UseShaderPredraw are disabled. */
	static FORCEINLINE void LogTexture(FShaderTextureKey const& Init, FTextureRHIParamRef State)
	{
		if ( Cache )
		{
			Cache->InternalLogTexture(Init, State);
		}
	}
	
	/** Called by the RHI. Logs the construction of a SRV & caches it so that draw states can be properly recorded. No-op when r.UseShaderDrawLog & r.UseShaderPredraw are disabled. */
	static FORCEINLINE void LogSRV(FShaderResourceViewRHIParamRef SRV, FTextureRHIParamRef Texture, uint8 StartMip, uint8 NumMips, uint8 Format)
	{
		if ( Cache )
		{
			Cache->InternalLogSRV(SRV, Texture, StartMip, NumMips, Format);
		}
	}
	
	/** Called by the RHI. Logs the construction of a SRV & caches it so that draw states can be properly recorded. No-op when r.UseShaderDrawLog & r.UseShaderPredraw are disabled. */
	static FORCEINLINE void LogSRV(FShaderResourceViewRHIParamRef SRV, FVertexBufferRHIParamRef Vb, uint32 Stride, uint8 Format)
	{
		if ( Cache )
		{
			Cache->InternalLogSRV(SRV, Vb, Stride, Format);
		}
	}
	
	/** Called by the RHI. Removes the cached SRV pointer on destruction to prevent further use. No-op when r.UseShaderDrawLog & r.UseShaderPredraw are disabled. */
	static FORCEINLINE void RemoveSRV(FShaderResourceViewRHIParamRef SRV)
	{
		if ( Cache )
		{
			Cache->InternalRemoveSRV(SRV);
		}
	}
	
	/** Called by the RHI. Removes the cached texture pointer on destruction to prevent further use. No-op when r.UseShaderDrawLog & r.UseShaderPredraw are disabled. */
	static FORCEINLINE void RemoveTexture(FTextureRHIParamRef Texture)
	{
		if ( Cache )
		{
			Cache->InternalRemoveTexture(Texture);
		}
	}
	
	/** Called by the RHI. Records the current blend state when r.UseShaderDrawLog or r.UseShaderPredraw are enabled. */
	static FORCEINLINE void SetBlendState(FBlendStateRHIParamRef State)
	{
		if ( Cache )
		{
			Cache->InternalSetBlendState(State);
		}
	}
	
	/** Called by the RHI. Records the current rasterizer state when r.UseShaderDrawLog or r.UseShaderPredraw are enabled. */
	static FORCEINLINE void SetRasterizerState(FRasterizerStateRHIParamRef State)
	{
		if ( Cache )
		{
			Cache->InternalSetRasterizerState(State);
		}
	}
	
	/** Called by the RHI. Records the current depth stencil state when r.UseShaderDrawLog or r.UseShaderPredraw are enabled. */
	static FORCEINLINE void SetDepthStencilState(FDepthStencilStateRHIParamRef State)
	{
		if ( Cache )
		{
			Cache->InternalSetDepthStencilState(State);
		}
	}
	
	/** Called by the RHI. Records the current render-targets when r.UseShaderDrawLog or r.UseShaderPredraw are enabled. */
	static FORCEINLINE void SetRenderTargets( uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargetsRHI, const FRHIDepthRenderTargetView* NewDepthStencilTargetRHI )
	{
		if ( Cache )
		{
			Cache->InternalSetRenderTargets(NumSimultaneousRenderTargets, NewRenderTargetsRHI, NewDepthStencilTargetRHI);
		}
	}
	
	/** Called by the RHI. Records the current sampler state when r.UseShaderDrawLog or r.UseShaderPredraw are enabled. */
	static FORCEINLINE void SetSamplerState(EShaderFrequency Frequency, uint32 Index, FSamplerStateRHIParamRef State)
	{
		if ( Cache )
		{
			Cache->InternalSetSamplerState(Frequency, Index, State);
		}
	}
	
	/** Called by the RHI. Records the current texture binding when r.UseShaderDrawLog or r.UseShaderPredraw are enabled. */
	static FORCEINLINE void SetTexture(EShaderFrequency Frequency, uint32 Index, FTextureRHIParamRef State)
	{
		if ( Cache )
		{
			Cache->InternalSetTexture(Frequency, Index, State);
		}
	}
	
	/** Called by the RHI. Records the current SRV binding when r.UseShaderDrawLog or r.UseShaderPredraw are enabled. */
	static FORCEINLINE void SetSRV(EShaderFrequency Frequency, uint32 Index, FShaderResourceViewRHIParamRef SRV)
	{
		if ( Cache )
		{
			Cache->InternalSetSRV(Frequency, Index, SRV);
		}
	}
	
	/** Called by the RHI. Records the current bound shader state when r.UseShaderDrawLog or r.UseShaderPredraw are enabled. */
	static FORCEINLINE void SetBoundShaderState(FBoundShaderStateRHIParamRef State)
	{
		if ( Cache )
		{
			Cache->InternalSetBoundShaderState(State);
		}
	}
	
	/** Called by the RHI. Records the current vieport when r.UseShaderDrawLog or r.UseShaderPredraw are enabled. */
	static FORCEINLINE void SetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ)
	{
		if ( Cache )
		{
			Cache->InternalSetViewport(MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
		}
	}
	
	/** Called by the RHI. Records the current draw state using the information captured from the other Log/Set* calls if and only if r.UseShaderCaching & r.UseShaderDrawLog are enabled. */
	static FORCEINLINE void LogDraw(uint8 IndexType)
	{
		if ( Cache )
		{
			Cache->InternalLogDraw(IndexType);
		}
	}
	
	/** Called by the RHI. Returns whether the current draw call is a predraw call for shader variant submission in the underlying driver rather than a real UE4 draw call. */
	static FORCEINLINE bool IsPredrawCall()
	{
		FShaderCache* ShaderCache = GetShaderCache();
		if ( Cache )
		{
			return Cache->bIsPreDraw;
		}
		return false;
	}
	
	/** Called by the RHI. Returns whether the current CreateBSS is a prebind call for shader  submission to the underlying driver rather than a real UE4 CreateBSS call. */
	static FORCEINLINE bool IsPrebindCall()
	{
		FShaderCache* ShaderCache = GetShaderCache();
		if ( Cache )
		{
			return Cache->bIsPreBind;
		}
		return false;
	}
	
	/** Use the accelerated target precompile frame time (in ms) and predraw batch time (in ms) during a loading screen or other scenario where frame time can be much slower than normal. */
	static void BeginAcceleratedBatching();
	
	/** Stops applying the overrides applied with BeginAcceleratedBatching but doesn't flush outstanding work. */
	static void EndAcceleratedBatching();
	
	/** Flushes all outstanding precompilation/predraw work & then ends accelerated batching as per EndAcceleratedBatching. */
	static void FlushOutstandingBatches();
	
	/** Pauses precompilation & predraw batching. */
	static void PauseBatching();
	
	/** Resumes precompilation & predraw batching. */
	static void ResumeBatching();
	
	/** Returns the number of shaders waiting for precompilation */
	static uint32 NumShaderPrecompilesRemaining();
	
	static void CookShader(EShaderPlatform Platform, EShaderFrequency Frequency, FSHAHash Hash, TArray<uint8> const& Code);
	
	static void CookPipeline(class FShaderPipeline* Pipeline);

	struct SHADERCORE_API FShaderPipelineKey
	{
		FShaderCacheKey VertexShader;
		FShaderCacheKey PixelShader;
		FShaderCacheKey GeometryShader;
		FShaderCacheKey HullShader;
		FShaderCacheKey DomainShader;
		mutable uint32 Hash;
		
		FShaderPipelineKey() : Hash(0) {}
		
		friend bool operator ==(const FShaderPipelineKey& A,const FShaderPipelineKey& B)
		{
			return A.VertexShader == B.VertexShader && A.PixelShader == B.PixelShader && A.GeometryShader == B.GeometryShader && A.HullShader == B.HullShader && A.DomainShader == B.DomainShader;
		}
		
		friend uint32 GetTypeHash(const FShaderPipelineKey &Key)
		{
			if(!Key.Hash)
			{
				Key.Hash ^= GetTypeHash(Key.VertexShader) ^ GetTypeHash(Key.PixelShader) ^ GetTypeHash(Key.GeometryShader) ^ GetTypeHash(Key.HullShader) ^ GetTypeHash(Key.DomainShader);
			}
			return Key.Hash;
		}
		
		friend FArchive& operator<<( FArchive& Ar, FShaderPipelineKey& Info )
		{
			return Ar << Info.VertexShader << Info.PixelShader << Info.GeometryShader << Info.HullShader << Info.DomainShader << Info.Hash;
		}
	};
	
	struct SHADERCORE_API FShaderCodeCache
	{
		friend FArchive& operator<<( FArchive& Ar, FShaderCodeCache& Info );
		
		TMap<FShaderCacheKey, TArray<uint8>> Shaders;
		TMap<FShaderCacheKey, TSet<FShaderPipelineKey>> Pipelines;
	};
	
public: // From FTickableObjectRenderThread
	virtual void Tick( float DeltaTime ) final override;
	
	virtual bool IsTickable() const final override;
	
	virtual bool NeedsRenderingResumedForRenderingThreadTick() const final override;
	
	virtual TStatId GetStatId() const final override;
	
private:
	struct FShaderCacheBoundState
	{
		FVertexDeclarationElementList VertexDeclaration;
		FShaderCacheKey VertexShader;
		FShaderCacheKey PixelShader;
		FShaderCacheKey GeometryShader;
		FShaderCacheKey HullShader;
		FShaderCacheKey DomainShader;
		mutable uint32 Hash;
		
		FShaderCacheBoundState() : Hash(0) {}
		
		friend bool operator ==(const FShaderCacheBoundState& A,const FShaderCacheBoundState& B)
		{
			if(A.VertexDeclaration.Num() == B.VertexDeclaration.Num())
			{
				for(int32 i = 0; i < A.VertexDeclaration.Num(); i++)
				{
					if(FMemory::Memcmp(&A.VertexDeclaration[i], &B.VertexDeclaration[i], sizeof(FVertexElement)))
					{
						return false;
					}
				}
				return A.VertexShader == B.VertexShader && A.PixelShader == B.PixelShader && A.GeometryShader == B.GeometryShader && A.HullShader == B.HullShader && A.DomainShader == B.DomainShader;
			}
			return false;
		}
		
		friend uint32 GetTypeHash(const FShaderCacheBoundState &Key)
		{
			if(!Key.Hash)
			{
				for(auto Element : Key.VertexDeclaration)
				{
					Key.Hash ^= FCrc::MemCrc_DEPRECATED(&Element, sizeof(FVertexElement));
				}
				
				Key.Hash ^= GetTypeHash(Key.VertexShader) ^ GetTypeHash(Key.PixelShader) ^ GetTypeHash(Key.GeometryShader) ^ GetTypeHash(Key.HullShader) ^ GetTypeHash(Key.DomainShader);
			}
			return Key.Hash;
		}
		
		friend FArchive& operator<<( FArchive& Ar, FShaderCacheBoundState& Info )
		{
			return Ar << Info.VertexDeclaration << Info.VertexShader << Info.PixelShader << Info.GeometryShader << Info.HullShader << Info.DomainShader << Info.Hash;
		}
	};
	
	struct FSamplerStateInitializerRHIKeyFuncs : TDefaultMapKeyFuncs<FSamplerStateInitializerRHI,int32,false>
	{
		/**
		 * @return True if the keys match.
		 */
		static bool Matches(KeyInitType A,KeyInitType B);
		
		/** Calculates a hash index for a key. */
		static uint32 GetKeyHash(KeyInitType Key);
	};
	
	struct FShaderStreamingCache
	{
		TMap<int32, TSet<int32>> ShaderDrawStates;
		
		friend FArchive& operator<<( FArchive& Ar, FShaderStreamingCache& Info )
		{
			return Ar << Info.ShaderDrawStates;
		}
	};
	
	template <class Type, typename KeyFuncs = TDefaultMapKeyFuncs<Type,int32,false>>
	class TIndexedSet
	{
		TMap<Type, int32, FDefaultSetAllocator, KeyFuncs> Map;
		TArray<Type> Data;
		
	public:
#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
		TIndexedSet() = default;
		TIndexedSet(TIndexedSet&&) = default;
		TIndexedSet(const TIndexedSet&) = default;
		TIndexedSet& operator=(TIndexedSet&&) = default;
		TIndexedSet& operator=(const TIndexedSet&) = default;
#else
		FORCEINLINE TIndexedSet() {}
		FORCEINLINE TIndexedSet(      TIndexedSet&& Other) : Map(MoveTemp(Other.Map)), Data(MoveTemp(Other.Data)) {}
		FORCEINLINE TIndexedSet(const TIndexedSet&  Other) : Map( Other.Map ), Data( Other.Data ) {}
		FORCEINLINE TIndexedSet& operator=(      TIndexedSet&& Other) { Map = MoveTemp(Other.Map); Data = MoveTemp(Other.Data); return *this; }
		FORCEINLINE TIndexedSet& operator=(const TIndexedSet&  Other) { Map = Other.Map; Data = Other.Data; return *this; }
#endif
		
		int32 Add(Type const& Object)
		{
			int32* Index = Map.Find(Object);
			if(Index)
			{
				return *Index;
			}
			else
			{
				int32 NewIndex = Data.Num();
				Data.Push(Object);
				Map.Add(Object, NewIndex);
				return NewIndex;
			}
		}
		
		int32 Find(Type const& Object)
		{
			int32* Index = Map.Find(Object);
			if(Index)
			{
				return *Index;
			}
			else
			{
				check(false);
				return -1;
			}
		}
		
		int32 FindChecked(Type const& Object)
		{
			return Map.FindChecked(Object);
		}
		
		Type& operator[](int32 Index)
		{
			return Data[Index];
		}
		
		Type const& operator[](int32 Index) const
		{
			return Data[Index];
		}
		
		uint32 Num() const
		{
			return Data.Num();
		}
		
		friend FORCEINLINE FArchive& operator<<( FArchive& Ar, TIndexedSet& Set )
		{
			return Ar << Set.Map << Set.Data;
		}
	};
	
	struct FShaderPreDrawEntry
	{
		int32 BoundStateIndex;
		int32 DrawKeyIndex;
		bool bPredrawn;
		
		FShaderPreDrawEntry() : BoundStateIndex(-1), DrawKeyIndex(-1), bPredrawn(false) {}
		
		friend bool operator ==(const FShaderPreDrawEntry& A,const FShaderPreDrawEntry& B)
		{
			return (A.BoundStateIndex == B.BoundStateIndex && A.DrawKeyIndex == B.DrawKeyIndex);
		}
		
		friend uint32 GetTypeHash(const FShaderPreDrawEntry &Key)
		{
			return (Key.BoundStateIndex ^ Key.DrawKeyIndex);
		}
		
		friend FArchive& operator<<( FArchive& Ar, FShaderPreDrawEntry& Info )
		{
			if(Ar.IsLoading())
			{
				Info.bPredrawn = false;
			}
			return Ar << Info.BoundStateIndex << Info.DrawKeyIndex;
		}
	};
	
	struct FShaderPlatformCache
	{
		TIndexedSet<FShaderCacheKey> Shaders;
		TIndexedSet<FShaderCacheBoundState> BoundShaderStates;
		TIndexedSet<FShaderDrawKey> DrawStates;
		TIndexedSet<FShaderRenderTargetKey> RenderTargets;
		TIndexedSet<FShaderResourceKey> Resources;
		TIndexedSet<FSamplerStateInitializerRHI, FSamplerStateInitializerRHIKeyFuncs> SamplerStates;
		TIndexedSet<FShaderPreDrawEntry> PreDrawEntries;

		TMap<int32, TSet<int32>> ShaderStateMembership;
		TMap<uint32, FShaderStreamingCache> StreamingDrawStates;
		
		friend FArchive& operator<<( FArchive& Ar, FShaderPlatformCache& Info )
		{
			return Ar << Info.Shaders << Info.BoundShaderStates << Info.DrawStates << Info.RenderTargets << Info.Resources << Info.SamplerStates << Info.PreDrawEntries << Info.ShaderStateMembership << Info.StreamingDrawStates;
		}
	};
	
	struct FShaderCaches
	{
		TMap<uint32, FShaderPlatformCache> PlatformCaches;
	};
	
	/** Archive serialisation of the cache data. */
	friend FArchive& operator<<( FArchive& Ar, FShaderCaches& Info );
	
	static void MergePlatformCaches(FShaderPlatformCache& Target, FShaderPlatformCache const& Source);
	static void MergeShaderCaches(FShaderCaches& Target, FShaderCaches const& Source);
	static bool LoadShaderCache(FString Path, FShaderCaches* Cache);
	static bool SaveShaderCache(FString Path, FShaderCaches* Cache);
	
	struct FShaderResourceViewBinding
	{
		FShaderResourceViewBinding()
		: SRV(nullptr)
		, VertexBuffer(nullptr)
		, Texture(nullptr)
		{}
		
		FShaderResourceViewBinding(FShaderResourceViewRHIParamRef InSrv, FVertexBufferRHIParamRef InVb, FTextureRHIParamRef InTexture)
		: SRV(nullptr)
		, VertexBuffer(nullptr)
		, Texture(nullptr)
		{}
		
		FShaderResourceViewRHIParamRef SRV;
		FVertexBufferRHIParamRef VertexBuffer;
		FTextureRHIParamRef Texture;
	};
	
	struct FShaderTextureBinding
	{
		FShaderTextureBinding() {}
		FShaderTextureBinding(FShaderResourceViewBinding const& Other)
		{
			operator=(Other);
		}
		
		FShaderTextureBinding& operator=(FShaderResourceViewBinding const& Other)
		{
			SRV = Other.SRV;
			VertexBuffer = Other.VertexBuffer;
			Texture = Other.Texture;
			return *this;
		}
		
		friend bool operator ==(const FShaderTextureBinding& A,const FShaderTextureBinding& B)
		{
			return A.SRV == B.SRV && A.VertexBuffer == B.VertexBuffer && A.Texture == B.Texture;
		}
		
		friend uint32 GetTypeHash(const FShaderTextureBinding &Key)
		{
			uint32 Hash = GetTypeHash(Key.SRV);
			Hash ^= GetTypeHash(Key.VertexBuffer);
			Hash ^= GetTypeHash(Key.Texture);
			return Hash;
		}
		
		FShaderResourceViewRHIRef SRV;
		FVertexBufferRHIRef VertexBuffer;
		FTextureRHIRef Texture;
	};
	
private:
	void InternalLogStreamingKey(uint32 StreamKey, bool const bActive);
	void InternalLogShader(EShaderPlatform Platform, EShaderFrequency Frequency, FSHAHash Hash, TArray<uint8> const& Code);
	
	void InternalLogVertexDeclaration(const FVertexDeclarationElementList& VertexElements, FVertexDeclarationRHIParamRef VertexDeclaration);
	
	void InternalLogBoundShaderState(EShaderPlatform Platform, FVertexDeclarationRHIParamRef VertexDeclaration,
									 FVertexShaderRHIParamRef VertexShader,
									 FPixelShaderRHIParamRef PixelShader,
									 FHullShaderRHIParamRef HullShader,
									 FDomainShaderRHIParamRef DomainShader,
									 FGeometryShaderRHIParamRef GeometryShader,
									 FBoundShaderStateRHIParamRef BoundState);
	
	void InternalLogBlendState(FBlendStateInitializerRHI const& Init, FBlendStateRHIParamRef State);
	void InternalLogRasterizerState(FRasterizerStateInitializerRHI const& Init, FRasterizerStateRHIParamRef State);
	void InternalLogDepthStencilState(FDepthStencilStateInitializerRHI const& Init, FDepthStencilStateRHIParamRef State);
	void InternalLogSamplerState(FSamplerStateInitializerRHI const& Init, FSamplerStateRHIParamRef State);
	void InternalLogTexture(FShaderTextureKey const& Init, FTextureRHIParamRef State);
	void InternalLogSRV(FShaderResourceViewRHIParamRef SRV, FTextureRHIParamRef Texture, uint8 StartMip, uint8 NumMips, uint8 Format);
	void InternalLogSRV(FShaderResourceViewRHIParamRef SRV, FVertexBufferRHIParamRef Vb, uint32 Stride, uint8 Format);
	
	void InternalRemoveSRV(FShaderResourceViewRHIParamRef SRV);
	void InternalRemoveTexture(FTextureRHIParamRef Texture);
	
	void InternalSetBlendState(FBlendStateRHIParamRef State);
	void InternalSetRasterizerState(FRasterizerStateRHIParamRef State);
	void InternalSetDepthStencilState(FDepthStencilStateRHIParamRef State);
	void InternalSetRenderTargets( uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargetsRHI, const FRHIDepthRenderTargetView* NewDepthStencilTargetRHI );
	void InternalSetSamplerState(EShaderFrequency Frequency, uint32 Index, FSamplerStateRHIParamRef State);
	void InternalSetTexture(EShaderFrequency Frequency, uint32 Index, FTextureRHIParamRef State);
	void InternalSetSRV(EShaderFrequency Frequency, uint32 Index, FShaderResourceViewRHIParamRef SRV);
	void InternalSetBoundShaderState(FBoundShaderStateRHIParamRef State);
	void InternalSetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ);
	
	void InternalLogDraw(uint8 IndexType);
	void InternalPreDrawShaders(FRHICommandList& RHICmdList, float DeltaTime);
	
	void PrebindShader(FShaderCacheKey const& Key);
	void SubmitShader(FShaderCacheKey const& Key, TArray<uint8> const& Code);
	void PreDrawShader(FRHICommandList& RHICmdList, FShaderCacheBoundState const& Shader, TSet<int32> const& DrawStates);
	template <typename TShaderRHIRef>
	void SetShaderSamplerTextures( FRHICommandList& RHICmdList, FShaderDrawKey const& DrawKey, EShaderFrequency Frequency, TShaderRHIRef Shader, bool bClear = false );
	FTextureRHIRef CreateTexture(FShaderTextureKey const& TextureKey, bool const bCached);
	FShaderTextureBinding CreateSRV(FShaderResourceKey const& ResourceKey);
	FTextureRHIRef CreateRenderTarget(FShaderRenderTargetKey const& TargetKey);
	
	int32 GetPredrawBatchTime() const;
	int32 GetTargetPrecompileFrameTime() const;
	
private:
	// Serialised
	FShaderCaches Caches;
	
	// Optional, separate code cache
	FShaderCodeCache CodeCache;
	
	// Transient non-invasive tracking of RHI resources for shader logging
	TMap<FShaderCacheKey, FVertexShaderRHIRef> CachedVertexShaders;
	TMap<FShaderCacheKey, FPixelShaderRHIRef> CachedPixelShaders;
	TMap<FShaderCacheKey, FGeometryShaderRHIRef> CachedGeometryShaders;
	TMap<FShaderCacheKey, FHullShaderRHIRef> CachedHullShaders;
	TMap<FShaderCacheKey, FDomainShaderRHIRef> CachedDomainShaders;
	TMap<FShaderCacheKey, FComputeShaderRHIRef> CachedComputeShaders;
	TMap<FVertexDeclarationRHIParamRef, FVertexDeclarationElementList> VertexDeclarations;
	TMap<FShaderCacheBoundState, FBoundShaderStateRHIRef> BoundShaderStates;
	
	// Transient non-invasive tracking of RHI resources for shader predrawing
	TMap<FBoundShaderStateRHIParamRef, FShaderCacheBoundState> ShaderStates;
	TMap<FBlendStateRHIParamRef, FBlendStateInitializerRHI> BlendStates;
	TMap<FRasterizerStateRHIParamRef, FRasterizerStateInitializerRHI> RasterizerStates;
	TMap<FDepthStencilStateRHIParamRef, FDepthStencilStateInitializerRHI> DepthStencilStates;
	TMap<FSamplerStateRHIParamRef, int32> SamplerStates;
	TMap<FTextureRHIParamRef, int32> Textures;
	TMap<FShaderResourceViewRHIParamRef, FShaderResourceKey> SRVs;
	
	// Active streaming keys
	TSet<uint32> ActiveStreamingKeys;
	
	// Current combination of streaming keys that define the current streaming environment.
	// Logged draws will only be predrawn when this key becomes active.
	uint32 StreamingKey;
	
	// Shaders to precompile
	TArray<FShaderCacheKey> ShadersToPrecompile;
	
	// Shaders we need to predraw
	TMap<uint32, FShaderStreamingCache> ShadersToDraw;
	
	// Caches to track application & predraw created textures/SRVs so that we minimise temporary resource creation
	TMap<FShaderTextureKey, FTextureRHIParamRef> CachedTextures;
	TMap<FShaderResourceKey, FShaderResourceViewBinding> CachedSRVs;
	
	// Temporary shader resources for pre-draw
	// Cleared after each round of pre-drawing is complete
	TSet<FShaderTextureBinding> PredrawBindings;
	TMap<FShaderRenderTargetKey, FTextureRHIParamRef> PredrawRTs;
	TSet<FVertexBufferRHIRef> PredrawVBs;
	
	// Permanent shader pre-draw resources
	FIndexBufferRHIRef IndexBufferUInt16;
	FIndexBufferRHIRef IndexBufferUInt32;
	
	// Growable pre-draw resources
	FVertexBufferRHIRef PredrawVB; // Standard VB
	FVertexBufferRHIRef PredrawZVB; // Zero-stride VB
	
	// Current render state for draw state tracking
	bool bCurrentDepthStencilTarget;
	uint32 CurrentNumRenderTargets;
	FRHIDepthRenderTargetView CurrentDepthStencilTarget;
	FRHIRenderTargetView CurrentRenderTargets[MaxSimultaneousRenderTargets];
	FShaderDrawKey CurrentDrawKey;
	FBoundShaderStateRHIRef CurrentShaderState;
	int32 BoundShaderStateIndex;
	uint32 Viewport[4];
	float DepthRange[2];
	bool bIsPreDraw;
	bool bIsPreBind;
	uint32 Options;
	uint8 MaxResources;
	
	// When the invalid resource count is greater than 0 no draw keys will be stored to prevent corrupting the shader cache.
	// Warnings are emitted to indicate that the shader cache has encountered a resource lifetime error.
	uint32 InvalidResourceCount;
	
	// Overrides for shader warmup times to use when loading or to force a flush.
	int32 OverridePrecompileTime;
	int32 OverridePredrawBatchTime;
	bool bBatchingPaused;
	
	static FShaderCache* Cache;
#if WITH_EDITORONLY_DATA
	struct FShaderCookCache
	{
		TMap<FName, TSet<EShaderPlatform>> CachedPlatformShaderFormats;
		TSet<EShaderPlatform> AllCachedPlatforms;
		FShaderCodeCache CodeCache;
	};
	static FShaderCookCache* CookCache;
#endif
	static int32 GameVersion;
	static int32 bUseShaderCaching;
	static int32 bUseShaderPredraw;
	static int32 bUseShaderDrawLog;
	static int32 PredrawBatchTime;
	static int32 bUseShaderBinaryCache;
	static int32 bUseAsyncShaderPrecompilation;
	static int32 TargetPrecompileFrameTime;
	static int32 AccelPredrawBatchTime;
	static int32 AccelTargetPrecompileFrameTime;
	static float InitialShaderLoadTime;
	static FAutoConsoleVariableRef CVarUseShaderCaching;
	static FAutoConsoleVariableRef CVarUseShaderPredraw;
	static FAutoConsoleVariableRef CVarUseShaderDrawLog;
	static FAutoConsoleVariableRef CVarPredrawBatchTime;
	static FAutoConsoleVariableRef CVarUseShaderBinaryCache;
	static FAutoConsoleVariableRef CVarUseAsyncShaderPrecompilation;
	static FAutoConsoleVariableRef CVarTargetPrecompileFrameTime;
	static FAutoConsoleVariableRef CVarAccelPredrawBatchTime;
	static FAutoConsoleVariableRef CVarAccelTargetPrecompileFrameTime;
	static FAutoConsoleVariableRef CVarInitialShaderLoadTime;
};

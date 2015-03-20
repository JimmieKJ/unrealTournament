// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCache.h: Shader precompilation mechanism
=============================================================================*/

#pragma once

#include "ShaderCore.h"
#include "RHI.h"
#include "BoundShaderStateCache.h"

/** Custom serialization version for FShaderCache */
struct SHADERCORE_API FShaderCacheCustomVersion
{
	static const FGuid Key;
	static const FGuid GameKey;
	enum Type {	Initial, PreDraw, Latest = PreDraw };
};

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

struct SHADERCORE_API FShaderTextureKey
{
	uint32 X;
	uint32 Y;
	uint32 Z;
	uint32 Flags; // ETextureCreateFlags
	uint32 MipLevels;
	uint32 Samples;
	uint8 Format;
	TEnumAsByte<EShaderCacheTextureType> Type;
	
	FShaderTextureKey()
	: X(0)
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
		uint32 Hash = Key.X;
		Hash ^= Key.Y;
		Hash ^= Key.Z;
		Hash ^= Key.Flags;
		Hash ^= Key.MipLevels;
		Hash ^= Key.Samples;
		Hash ^= Key.Format;
		Hash ^= Key.Type;
		return Hash;
	}
		
	friend FArchive& operator<<( FArchive& Ar, FShaderTextureKey& Info )
	{
		return Ar << Info.Format << Info.Type << Info.Samples << Info.MipLevels << Info.Flags << Info.X << Info.Y << Info.Z;
	}
};

struct SHADERCORE_API FShaderResourceKey
{
	FShaderTextureKey Tex;
	uint32 BaseMip;
	uint32 MipLevels;
	uint8 Format;
	bool bSRV;
	
	FShaderResourceKey()
	: BaseMip(0)
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
		uint32 Hash = GetTypeHash(Key.Tex);
		Hash ^= Key.BaseMip;
		Hash ^= Key.MipLevels;
		Hash ^= Key.Format;
		Hash ^= (uint32)Key.bSRV;
		return Hash;
	}
	
	friend FArchive& operator<<( FArchive& Ar, FShaderResourceKey& Info )
	{
		return Ar << Info.Tex << Info.BaseMip << Info.MipLevels << Info.Format << Info.bSRV;
	}
};

struct SHADERCORE_API FShaderRenderTargetKey
{
	FShaderTextureKey Texture;
	uint32 MipLevel;
	uint32 ArrayIndex;
	
	FShaderRenderTargetKey()
	: MipLevel(0)
	, ArrayIndex(0)
	{
		
	}
	
	friend bool operator ==(const FShaderRenderTargetKey& A,const FShaderRenderTargetKey& B)
	{
		return A.MipLevel == B.MipLevel && A.ArrayIndex == B.ArrayIndex && A.Texture == B.Texture;
	}
	
	friend uint32 GetTypeHash(const FShaderRenderTargetKey &Key)
	{
		uint32 Hash = GetTypeHash(Key.Texture);
		Hash ^= Key.MipLevel;
		Hash ^= Key.ArrayIndex;
		return Hash;
	}
	
	friend FArchive& operator<<( FArchive& Ar, FShaderRenderTargetKey& Info )
	{
		return Ar << Info.Texture << Info.MipLevel << Info.ArrayIndex;
	}
};

class SHADERCORE_API FShaderCache
{
	struct SHADERCORE_API FShaderCacheKey
	{
		FShaderCacheKey() : Platform(SP_NumPlatforms), Frequency(SF_NumFrequencies), bActive(false) {}
	
		FSHAHash Hash;
		EShaderPlatform Platform;
		EShaderFrequency Frequency;
		bool bActive;
		
		friend bool operator ==(const FShaderCacheKey& A,const FShaderCacheKey& B)
		{
			return A.Hash == B.Hash && A.Platform == B.Platform && A.Frequency == B.Frequency && A.bActive == B.bActive;
		}

		friend uint32 GetTypeHash(const FShaderCacheKey &Key)
		{
			uint32 TargetFrequency = Key.Frequency;
			uint32 TargetPlatform = Key.Platform;
			return FCrc::MemCrc_DEPRECATED((const void*)&Key.Hash, sizeof(Key.Hash)) ^ GetTypeHash(TargetPlatform) ^ GetTypeHash(TargetFrequency) ^ GetTypeHash(Key.bActive);
		}
		
		friend FArchive& operator<<( FArchive& Ar, FShaderCacheKey& Info )
		{
			uint32 TargetFrequency = Info.Frequency;
			uint32 TargetPlatform = Info.Platform;
			Ar << TargetFrequency << TargetPlatform;
			Info.Frequency = (EShaderFrequency)TargetFrequency;
			Info.Platform = (EShaderPlatform)TargetPlatform;
			return Ar << Info.Hash << Info.bActive;
		}
	};
	
	struct SHADERCORE_API FShaderDrawKey
	{
		enum
		{
			MaxTextureSamplers = 16,
			NullState = ~0u
		};
		
		FShaderDrawKey()
		: DepthStencilTarget(NullState)
		, IndexType(0)
		{
			FMemory::Memzero(&BlendState, sizeof(BlendState));
			FMemory::Memzero(&RasterizerState, sizeof(RasterizerState));
			FMemory::Memzero(&DepthStencilState, sizeof(DepthStencilState));
			FMemory::Memset(RenderTargets, 255, sizeof(RenderTargets));
			FMemory::Memset(SamplerStates, 255, sizeof(SamplerStates));
			FMemory::Memset(Resources, 255, sizeof(Resources));
			check(GetFeatureLevelMaxTextureSamplers(GMaxRHIFeatureLevel) <= MaxTextureSamplers);
		}
		
		struct FShaderRasterizerState
		{
			FShaderRasterizerState() { FMemory::MemZero(*this); }
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
		};
	
		FBlendStateInitializerRHI BlendState;
		FShaderRasterizerState RasterizerState;
		FDepthStencilStateInitializerRHI DepthStencilState;
		uint32 RenderTargets[MaxSimultaneousRenderTargets];
		uint32 SamplerStates[SF_NumFrequencies][MaxTextureSamplers];
		uint32 Resources[SF_NumFrequencies][MaxTextureSamplers];
		uint32 DepthStencilTarget;
		uint8 IndexType;
		
		friend bool operator ==(const FShaderDrawKey& A,const FShaderDrawKey& B)
		{
			bool Compare = A.IndexType == B.IndexType &&
							A.DepthStencilTarget == B.DepthStencilTarget &&
							FMemory::Memcmp(&A.BlendState, &B.BlendState, sizeof(FBlendStateInitializerRHI)) == 0 &&
							FMemory::Memcmp(&A.RasterizerState, &B.RasterizerState, sizeof(FShaderRasterizerState)) == 0 &&
							FMemory::Memcmp(&A.DepthStencilState, &B.DepthStencilState, sizeof(FDepthStencilStateInitializerRHI)) == 0 &&
							FMemory::Memcmp(&A.RenderTargets, &B.RenderTargets, sizeof(A.RenderTargets)) == 0 &&
							FMemory::Memcmp(&A.SamplerStates, &B.SamplerStates, sizeof(A.SamplerStates)) == 0 &&
							FMemory::Memcmp(&A.Resources, &B.Resources, sizeof(A.Resources)) == 0;
			return Compare;
		}
		
		friend uint32 GetTypeHash(const FShaderDrawKey &Key)
		{
			uint32 CRC = FCrc::MemCrc32(&Key.BlendState, sizeof(FBlendStateInitializerRHI));
			CRC ^= FCrc::MemCrc32(&Key.RasterizerState, sizeof(FShaderRasterizerState));
			CRC ^= FCrc::MemCrc32(&Key.DepthStencilState, sizeof(FDepthStencilStateInitializerRHI));
			CRC ^= FCrc::MemCrc32(&Key.RenderTargets, sizeof(Key.RenderTargets));
			CRC ^= FCrc::MemCrc32(&Key.SamplerStates, sizeof(Key.SamplerStates));
			CRC ^= FCrc::MemCrc32(&Key.Resources, sizeof(Key.Resources));
			CRC ^= GetTypeHash(Key.DepthStencilTarget);
			CRC ^= GetTypeHash(Key.IndexType);
			return CRC;
		}
		
		friend FArchive& operator<<( FArchive& Ar, FShaderDrawKey& Info )
		{
			for ( uint32 i = 0; i < SF_NumFrequencies; i++ )
			{
				for ( uint32 j = 0; j < MaxTextureSamplers; j++ )
				{
					Ar << Info.SamplerStates[i][j];
					Ar << Info.Resources[i][j];
				}
			}
			for ( uint32 i = 0; i < MaxSimultaneousRenderTargets; i++ )
			{
				Ar << Info.RenderTargets[i];
			}
			return Ar << Info.BlendState << Info.RasterizerState << Info.DepthStencilState << Info.DepthStencilTarget << Info.IndexType;
		}
	};
		
public:
	FShaderCache();
	~FShaderCache();

	// Called by the game to set the game specific shader cache version, only caches of this version will be loaded.
	// Must be called before RHI initialisation, as InitShaderCache will load any existing cache.
	static void SetGameVersion(int32 InGameVersion);

	// Called by the RHI implementation, not to be called by the game.
	static void InitShaderCache();
	static void ShutdownShaderCache();
	
	static FORCEINLINE FShaderCache* GetShaderCache()
	{
		return bUseShaderCaching ? Cache : nullptr;
	}

	FVertexShaderRHIRef GetVertexShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	FPixelShaderRHIRef GetPixelShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	FGeometryShaderRHIRef GetGeometryShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	FHullShaderRHIRef GetHullShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	FDomainShaderRHIRef GetDomainShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	FComputeShaderRHIRef GetComputeShader(EShaderPlatform Platform, TArray<uint8> const& Code);
	FVertexDeclarationRHIParamRef GetVertexDeclaration(FVertexDeclarationElementList& VertexElements);
	
	static FORCEINLINE void LogShader(EShaderPlatform Platform, EShaderFrequency Frequency, FSHAHash Hash, TArray<uint8> const& Code)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalLogShader(Platform, Frequency, Hash, Code);
		}
	}
	
	static FORCEINLINE void LogVertexDeclaration(const FVertexDeclarationElementList& VertexElements, FVertexDeclarationRHIParamRef VertexDeclaration)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalLogVertexDeclaration(VertexElements, VertexDeclaration);
		}
	}
	
	static FORCEINLINE void LogBoundShaderState(EShaderPlatform Platform, FVertexDeclarationRHIParamRef VertexDeclaration,
												FVertexShaderRHIParamRef VertexShader,
												FPixelShaderRHIParamRef PixelShader,
												FHullShaderRHIParamRef HullShader,
												FDomainShaderRHIParamRef DomainShader,
												FGeometryShaderRHIParamRef GeometryShader,
												FBoundShaderStateRHIParamRef BoundState)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalLogBoundShaderState(Platform, VertexDeclaration, VertexShader, PixelShader, HullShader, DomainShader, GeometryShader, BoundState);
		}
	}
	
	static FORCEINLINE void LogBlendState(FBlendStateInitializerRHI const& Init, FBlendStateRHIParamRef State)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalLogBlendState(Init, State);
		}
	}
	static FORCEINLINE void LogRasterizerState(FRasterizerStateInitializerRHI const& Init, FRasterizerStateRHIParamRef State)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalLogRasterizerState(Init, State);
		}
	}
	static FORCEINLINE void LogDepthStencilState(FDepthStencilStateInitializerRHI const& Init, FDepthStencilStateRHIParamRef State)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalLogDepthStencilState(Init, State);
		}
	}
	static FORCEINLINE void LogSamplerState(FSamplerStateInitializerRHI const& Init, FSamplerStateRHIParamRef State)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalLogSamplerState(Init, State);
		}
	}
	static FORCEINLINE void LogTexture(FShaderTextureKey const& Init, FTextureRHIParamRef State)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalLogTexture(Init, State);
		}
	}
	static FORCEINLINE void LogSRV(FShaderResourceViewRHIParamRef SRV, FTextureRHIParamRef Texture, uint8 StartMip, uint8 NumMips, uint8 Format)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalLogSRV(SRV, Texture, StartMip, NumMips, Format);
		}
	}
	static FORCEINLINE void LogSRV(FShaderResourceViewRHIParamRef SRV, FVertexBufferRHIParamRef Vb, uint32 Stride, uint8 Format)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalLogSRV(SRV, Vb, Stride, Format);
		}
	}
	static FORCEINLINE void RemoveSRV(FShaderResourceViewRHIParamRef SRV)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalRemoveSRV(SRV);
		}
	}
	static FORCEINLINE void RemoveTexture(FTextureRHIParamRef Texture)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalRemoveTexture(Texture);
		}
	}
	static FORCEINLINE void SetBlendState(FBlendStateRHIParamRef State)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalSetBlendState(State);
		}
	}
	static FORCEINLINE void SetRasterizerState(FRasterizerStateRHIParamRef State)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalSetRasterizerState(State);
		}
	}
	static FORCEINLINE void SetDepthStencilState(FDepthStencilStateRHIParamRef State)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalSetDepthStencilState(State);
		}
	}
	static FORCEINLINE void SetRenderTargets( uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargetsRHI, const FRHIDepthRenderTargetView* NewDepthStencilTargetRHI )
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalSetRenderTargets(NumSimultaneousRenderTargets, NewRenderTargetsRHI, NewDepthStencilTargetRHI);
		}
	}
	static FORCEINLINE void SetSamplerState(EShaderFrequency Frequency, uint32 Index, FSamplerStateRHIParamRef State)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalSetSamplerState(Frequency, Index, State);
		}
	}
	static FORCEINLINE void SetTexture(EShaderFrequency Frequency, uint32 Index, FTextureRHIParamRef State)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalSetTexture(Frequency, Index, State);
		}
	}
	static FORCEINLINE void SetSRV(EShaderFrequency Frequency, uint32 Index, FShaderResourceViewRHIParamRef SRV)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalSetSRV(Frequency, Index, SRV);
		}
	}
	static FORCEINLINE void SetBoundShaderState(FBoundShaderStateRHIParamRef State)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalSetBoundShaderState(State);
		}
	}
	static FORCEINLINE void SetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalSetViewport(MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
		}
	}
	static FORCEINLINE void LogDraw(uint8 IndexType)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalLogDraw(IndexType);
		}
	}
	static FORCEINLINE void PreDrawShaders(FRHICommandList& RHICmdList)
	{
		FShaderCache* Cache = GetShaderCache();
		if ( Cache )
		{
			Cache->InternalPreDrawShaders(RHICmdList);
		}
	}
		
	friend FArchive& operator<<( FArchive& Ar, FShaderCache& Info );
	
private:
	struct FShaderCacheBoundState
	{
		FVertexDeclarationElementList VertexDeclaration;
		FShaderCacheKey VertexShader;
		FShaderCacheKey PixelShader;
		FShaderCacheKey GeometryShader;
		FShaderCacheKey HullShader;
		FShaderCacheKey DomainShader;
		
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
			uint32 Hash = 0;
			
			for(auto Element : Key.VertexDeclaration)
			{
				Hash ^= FCrc::MemCrc_DEPRECATED(&Element, sizeof(FVertexElement));
			}
			
			return Hash ^ GetTypeHash(Key.VertexShader) ^ GetTypeHash(Key.PixelShader) ^ GetTypeHash(Key.GeometryShader) ^ GetTypeHash(Key.HullShader) ^ GetTypeHash(Key.DomainShader);
		}
		
		friend FArchive& operator<<( FArchive& Ar, FShaderCacheBoundState& Info )
		{
			return Ar << Info.VertexDeclaration << Info.VertexShader << Info.PixelShader << Info.GeometryShader << Info.HullShader << Info.DomainShader;
		}
	};
	
	struct FSamplerStateInitializerRHIKeyFuncs : BaseKeyFuncs<FSamplerStateInitializerRHI,FSamplerStateInitializerRHI,false>
	{
		typedef TCallTraits<FSamplerStateInitializerRHI>::ParamType KeyInitType;
		typedef TCallTraits<FSamplerStateInitializerRHI>::ParamType ElementInitType;
		
		/**
		 * @return The key used to index the given element.
		 */
		static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element)
		{
			return Element;
		}
		
		/**
		 * @return True if the keys match.
		 */
		static bool Matches(KeyInitType A,KeyInitType B);
		
		/** Calculates a hash index for a key. */
		static uint32 GetKeyHash(KeyInitType Key);
	};

	struct FShaderPlatformCache
	{
		TSet<FShaderCacheKey> Shaders;
		TSet<FShaderCacheBoundState> BoundShaderStates;
		TSet<FShaderDrawKey> DrawStates;
		TSet<FShaderRenderTargetKey> RenderTargets;
		TSet<FShaderResourceKey> Resources;
		TSet<FSamplerStateInitializerRHI, FSamplerStateInitializerRHIKeyFuncs> SamplerStates;
		TMap<FShaderCacheKey, TSet<FShaderCacheBoundState>> ShaderStateMembership;
		TMap<FShaderCacheBoundState, TSet<int32>> ShaderDrawStates;
		
		friend FArchive& operator<<( FArchive& Ar, FShaderPlatformCache& Info )
		{
			return Ar << Info.Shaders << Info.BoundShaderStates << Info.DrawStates << Info.RenderTargets << Info.Resources << Info.SamplerStates << Info.ShaderStateMembership << Info.ShaderDrawStates;
		}
	};

	typedef TMap<uint32, FShaderPlatformCache> FShaderCaches;

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
	void InternalPreDrawShaders(FRHICommandList& RHICmdList);
	
	void PrebindShader(FShaderCacheKey const& Key);
	void SubmitShader(FShaderCacheKey const& Key, TArray<uint8> const& Code);
	void PreDrawShader(FRHICommandList& RHICmdList, FShaderCacheBoundState const& Shader, TSet<int32> const& DrawStates);
	template <typename TShaderRHIRef>
	void SetShaderSamplerTextures( FRHICommandList& RHICmdList, FShaderDrawKey const& DrawKey, EShaderFrequency Frequency, TShaderRHIRef Shader, bool bClear = false );
	FTextureRHIRef CreateTexture(FShaderTextureKey const& TextureKey, bool const bCached);
	FShaderTextureBinding CreateSRV(FShaderResourceKey const& ResourceKey);
	FTextureRHIRef CreateRenderTarget(FShaderRenderTargetKey const& TargetKey);
	
private:
	// Serialised
	FShaderCaches Caches;
	
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
	TMap<FSamplerStateRHIParamRef, FSamplerStateInitializerRHI> SamplerStates;
	TMap<FTextureRHIParamRef, FShaderTextureKey> Textures;
	TMap<FShaderResourceViewRHIParamRef, FShaderResourceKey> SRVs;
	
	// Shaders we need to predraw
	TMap<FShaderCacheBoundState, TSet<int32>> ShadersToDraw;
	
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
	FBoundShaderStateRHIParamRef CurrentShaderState;
	FShaderCacheBoundState BoundShaderState;
	uint32 Viewport[4];
	float DepthRange[2];
	bool bIsPreDraw;
	
	static FShaderCache* Cache;
	static int32 GameVersion;
	static int32 bUseShaderCaching;
	static int32 bUseShaderPredraw;
	static int32 bUseShaderDrawLog;
	static int32 PredrawBatchTime;
	static FAutoConsoleVariableRef CVarUseShaderCaching;
	static FAutoConsoleVariableRef CVarUseShaderPredraw;
	static FAutoConsoleVariableRef CVarUseShaderDrawLog;
	static FAutoConsoleVariableRef CVarPredrawBatchTime;
};

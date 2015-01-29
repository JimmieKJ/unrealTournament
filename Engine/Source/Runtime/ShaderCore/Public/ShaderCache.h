// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCache.h: Shader precompilation mechanism
=============================================================================*/

#pragma once

#include "ShaderCore.h"
#include "RHI.h"
#include "BoundShaderStateCache.h"

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
		
public:
	FShaderCache();
	~FShaderCache();

	static void InitShaderCache();
	static FShaderCache* GetShaderCache();
	static void ShutdownShaderCache();

	FVertexShaderRHIRef GetVertexShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	FPixelShaderRHIRef GetPixelShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	FGeometryShaderRHIRef GetGeometryShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	FHullShaderRHIRef GetHullShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	FDomainShaderRHIRef GetDomainShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	FComputeShaderRHIRef GetComputeShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	FVertexDeclarationRHIParamRef GetVertexDeclaration(FVertexDeclarationElementList& VertexElements);
	
	void LogShader(EShaderPlatform Platform, EShaderFrequency Frequency, FSHAHash Hash, TArray<uint8> const& Code);
	
	void LogVertexDeclaration(const FVertexDeclarationElementList& VertexElements, FVertexDeclarationRHIParamRef VertexDeclaration);
	
	void LogBoundShaderState(EShaderPlatform Platform, FVertexDeclarationRHIParamRef VertexDeclaration,
								FVertexShaderRHIParamRef VertexShader,
								FPixelShaderRHIParamRef PixelShader,
								FHullShaderRHIParamRef HullShader,
								FDomainShaderRHIParamRef DomainShader,
								FGeometryShaderRHIParamRef GeometryShader);
		
	friend FArchive& operator<<( FArchive& Ar, FShaderCache& Info )
	{
		return Ar << Info.Caches;
	}
	
private:
	void PrebindShader(FShaderCacheKey Key);
	void SubmitShader(FShaderCacheKey Key, TArray<uint8> const& Code);
	
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

	struct FShaderPlatformCache
	{
		TSet<FShaderCacheKey> Shaders;
		TSet<FShaderCacheBoundState> BoundShaderStates;
		TMap<FShaderCacheKey, TSet<FShaderCacheBoundState>> ShaderStateMembership;
		
		friend FArchive& operator<<( FArchive& Ar, FShaderPlatformCache& Info )
		{
			return Ar << Info.Shaders << Info.BoundShaderStates << Info.ShaderStateMembership;
		}
	};

	typedef TMap<uint32, FShaderPlatformCache> FShaderCaches;

	// Serialised
	FShaderCaches Caches;
	
	// Transient
	TMap<FShaderCacheKey, FVertexShaderRHIRef> CachedVertexShaders;
	TMap<FShaderCacheKey, FPixelShaderRHIRef> CachedPixelShaders;
	TMap<FShaderCacheKey, FGeometryShaderRHIRef> CachedGeometryShaders;
	TMap<FShaderCacheKey, FHullShaderRHIRef> CachedHullShaders;
	TMap<FShaderCacheKey, FDomainShaderRHIRef> CachedDomainShaders;
	TMap<FShaderCacheKey, FComputeShaderRHIRef> CachedComputeShaders;
	TMap<FVertexDeclarationRHIParamRef, FVertexDeclarationElementList> VertexDeclarations;
	TMap<FShaderCacheBoundState, FBoundShaderStateRHIRef> BoundShaderStates;
	
	static FShaderCache* Cache;
};

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCache.cpp: Bound shader state cache implementation.
=============================================================================*/

#include "ShaderCore.h"
#include "ShaderCache.h"
#include "RHI.h"
#include "RenderingThread.h"

int32 bUseShaderCaching = PLATFORM_MAC ? 1 : 0;
static FAutoConsoleVariableRef CVarUseShaderCaching(
	TEXT("r.UseShaderCaching"),
	bUseShaderCaching,
	TEXT("If true, log all shaders & bound-shader-states, so that they may be instantiated in the RHI on deserialisation rather than waiting for first use.")
	);

FShaderCache* FShaderCache::Cache = nullptr;

void FShaderCache::InitShaderCache()
{
	check(!Cache);
	if(bUseShaderCaching)
	{
		Cache = new FShaderCache;
	}
}

FShaderCache* FShaderCache::GetShaderCache()
{
	return bUseShaderCaching ? Cache : nullptr;
}

void FShaderCache::ShutdownShaderCache()
{
	if (Cache)
	{
		delete Cache;
		Cache = nullptr;
	}
}

FShaderCache::FShaderCache()
{
	FString UserBinaryShaderFile = FPaths::GameSavedDir() / TEXT("ShaderCache.ushaderprecache");
	FString GameBinaryShaderFile = FPaths::GameContentDir() / TEXT("ShaderCache.ushaderprecache");

	FArchive* BinaryShaderAr = nullptr;
	if ( IFileManager::Get().FileSize(*UserBinaryShaderFile) > 0 )
	{
		BinaryShaderAr = IFileManager::Get().CreateFileReader(*UserBinaryShaderFile);
	}
	else
	{
		BinaryShaderAr = IFileManager::Get().CreateFileReader(*GameBinaryShaderFile);
	}

	if ( BinaryShaderAr != nullptr )
	{
		*BinaryShaderAr << *this;
		delete BinaryShaderAr;
	}
}

FShaderCache::~FShaderCache()
{
	FString BinaryShaderFile = FPaths::GameSavedDir() / TEXT("ShaderCache.ushaderprecache");
	FArchive* BinaryShaderAr = IFileManager::Get().CreateFileWriter(*BinaryShaderFile);
	if( BinaryShaderAr != NULL )
	{
		*BinaryShaderAr << *this;
		delete BinaryShaderAr;
	}
}

FVertexShaderRHIRef FShaderCache::GetVertexShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code)
{
	FShaderCacheKey Key;
	Key.Platform = Platform;
	Key.Frequency = SF_Vertex;
	Key.Hash = Hash;
	Key.bActive = true;
	FVertexShaderRHIRef Shader = CachedVertexShaders.FindRef(Key);
	if(!IsValidRef(Shader))
	{
		FShaderPlatformCache& PlatformCache = Caches.FindOrAdd(Platform);
		PlatformCache.Shaders.Add(Key);
		Shader = RHICreateVertexShader(Code);
		check(IsValidRef(Shader));
		Shader->SetHash(Hash);
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
	Key.Hash = Hash;
	Key.bActive = true;
	FPixelShaderRHIRef Shader = CachedPixelShaders.FindRef(Key);
	if(!IsValidRef(Shader))
	{
		FShaderPlatformCache& PlatformCache = Caches.FindOrAdd(Platform);
		PlatformCache.Shaders.Add(Key);
		Shader = RHICreatePixelShader(Code);
		check(IsValidRef(Shader));
		Shader->SetHash(Hash);
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
	Key.Hash = Hash;
	Key.bActive = true;
	FGeometryShaderRHIRef Shader = CachedGeometryShaders.FindRef(Key);
	if(!IsValidRef(Shader))
	{
		FShaderPlatformCache& PlatformCache = Caches.FindOrAdd(Platform);
		PlatformCache.Shaders.Add(Key);
		Shader = RHICreateGeometryShader(Code);
		check(IsValidRef(Shader));
		Shader->SetHash(Hash);
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
	Key.Hash = Hash;
	Key.bActive = true;
	FHullShaderRHIRef Shader = CachedHullShaders.FindRef(Key);
	if(!IsValidRef(Shader))
	{
		FShaderPlatformCache& PlatformCache = Caches.FindOrAdd(Platform);
		PlatformCache.Shaders.Add(Key);
		Shader = RHICreateHullShader(Code);
		check(IsValidRef(Shader));
		Shader->SetHash(Hash);
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
	Key.Hash = Hash;
	Key.bActive = true;
	FDomainShaderRHIRef Shader = CachedDomainShaders.FindRef(Key);
	if(!IsValidRef(Shader))
	{
		FShaderPlatformCache& PlatformCache = Caches.FindOrAdd(Platform);
		PlatformCache.Shaders.Add(Key);
		Shader = RHICreateDomainShader(Code);
		check(IsValidRef(Shader));
		Shader->SetHash(Hash);
		CachedDomainShaders.Add(Key, Shader);
		PrebindShader(Key);
	}
	return Shader;
}

FComputeShaderRHIRef FShaderCache::GetComputeShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code)
{
	FShaderCacheKey Key;
	Key.Platform = Platform;
	Key.Frequency = SF_Compute;
	Key.Hash = Hash;
	Key.bActive = true;
	FComputeShaderRHIRef Shader = CachedComputeShaders.FindRef(Key);
	if(!IsValidRef(Shader))
	{
		FShaderPlatformCache& PlatformCache = Caches.FindOrAdd(Platform);
		PlatformCache.Shaders.Add(Key);
		Shader = RHICreateComputeShader(Code);
		check(IsValidRef(Shader));
		Shader->SetHash(Hash);
		CachedComputeShaders.Add(Key, Shader);
		PrebindShader(Key);
	}
	return Shader;
}

void FShaderCache::LogShader(EShaderPlatform Platform, EShaderFrequency Frequency, FSHAHash Hash, TArray<uint8> const& Code)
{
	FShaderCacheKey Key;
	Key.Hash = Hash;
	Key.Platform = Platform;
	Key.Frequency = Frequency;
	Key.bActive = true;
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(LogShader, FShaderCacheKey,Key,Key, TArray<uint8>,Code,Code, {
		FShaderCache::GetShaderCache()->SubmitShader(Key, Code);
	});
}

void FShaderCache::LogVertexDeclaration(const FVertexDeclarationElementList& VertexElements, FVertexDeclarationRHIParamRef VertexDeclaration)
{
	VertexDeclarations.Add(VertexDeclaration, VertexElements);
}

void FShaderCache::LogBoundShaderState(EShaderPlatform Platform, FVertexDeclarationRHIParamRef VertexDeclaration,
								FVertexShaderRHIParamRef VertexShader,
								FPixelShaderRHIParamRef PixelShader,
								FHullShaderRHIParamRef HullShader,
								FDomainShaderRHIParamRef DomainShader,
								FGeometryShaderRHIParamRef GeometryShader)
{
	FShaderCacheBoundState Info;
	if(VertexDeclaration)
	{
		Info.VertexDeclaration = VertexDeclarations.FindChecked(VertexDeclaration);
	}
	if(VertexShader)
	{
		Info.VertexShader.Platform = Platform;
		Info.VertexShader.Frequency = SF_Vertex;
		Info.VertexShader.Hash = VertexShader->GetHash();
		Info.VertexShader.bActive = true;
	}
	if(PixelShader)
	{
		Info.PixelShader.Platform = Platform;
		Info.PixelShader.Frequency = SF_Pixel;
		Info.PixelShader.Hash = PixelShader->GetHash();
		Info.PixelShader.bActive = true;
	}
	if(GeometryShader)
	{
		Info.GeometryShader.Platform = Platform;
		Info.GeometryShader.Frequency = SF_Geometry;
		Info.GeometryShader.Hash = GeometryShader->GetHash();
		Info.GeometryShader.bActive = true;
	}
	if(HullShader)
	{
		Info.HullShader.Platform = Platform;
		Info.HullShader.Frequency = SF_Hull;
		Info.HullShader.Hash = HullShader->GetHash();
		Info.HullShader.bActive = true;
	}
	if(DomainShader)
	{
		Info.DomainShader.Platform = Platform;
		Info.DomainShader.Frequency = SF_Domain;
		Info.DomainShader.Hash = DomainShader->GetHash();
		Info.DomainShader.bActive = true;
	}
	
	FShaderPlatformCache& PlatformCache = Caches.FindOrAdd(Platform);
	PlatformCache.BoundShaderStates.Add(Info);
	
	if(VertexShader)
	{
		TSet<FShaderCacheBoundState>& Set = PlatformCache.ShaderStateMembership.FindOrAdd(Info.VertexShader);
		if(!Set.Find(Info))
		{
			Set.Add(Info);
		}
	}
	if(PixelShader)
	{
		TSet<FShaderCacheBoundState>& Set = PlatformCache.ShaderStateMembership.FindOrAdd(Info.PixelShader);
		if(!Set.Find(Info))
		{
			Set.Add(Info);
		}
	}
	if(GeometryShader)
	{
		TSet<FShaderCacheBoundState>& Set = PlatformCache.ShaderStateMembership.FindOrAdd(Info.GeometryShader);
		if(!Set.Find(Info))
		{
			Set.Add(Info);
		}
	}
	if(HullShader)
	{
		TSet<FShaderCacheBoundState>& Set = PlatformCache.ShaderStateMembership.FindOrAdd(Info.HullShader);
		if(!Set.Find(Info))
		{
			Set.Add(Info);
		}
	}
	if(DomainShader)
	{
		TSet<FShaderCacheBoundState>& Set = PlatformCache.ShaderStateMembership.FindOrAdd(Info.DomainShader);
		if(!Set.Find(Info))
		{
			Set.Add(Info);
		}
	}
}

void FShaderCache::PrebindShader(FShaderCacheKey Key)
{
	FShaderPlatformCache& PlatformCache = Caches.FindOrAdd(Key.Platform);
	TSet<FShaderCacheBoundState>& BoundStates = PlatformCache.ShaderStateMembership.FindOrAdd(Key);
	for (FShaderCacheBoundState State : BoundStates)
	{
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
		
		if(bOK)
		{
			FVertexDeclarationRHIRef VertexDeclaration = RHICreateVertexDeclaration(State.VertexDeclaration);
			bOK &= IsValidRef(VertexDeclaration);
			
			if(bOK)
			{
				FBoundShaderStateRHIRef BoundState = RHICreateBoundShaderState(
					VertexDeclaration,
					VertexShader,
					HullShader,
					DomainShader,
					PixelShader,
					GeometryShader
				);
				if(IsValidRef(BoundState))
				{
					BoundShaderStates.Add(State, BoundState);
				}
			}
		}
	}
}

void FShaderCache::SubmitShader(FShaderCacheKey Key, TArray<uint8> const& Code)
{
	FShaderPlatformCache& PlatformCache = Caches.FindOrAdd(Key.Platform);
	switch (Key.Frequency)
	{
		case SF_Vertex:
			if(!CachedVertexShaders.Find(Key))
			{
				FVertexShaderRHIRef Shader = RHICreateVertexShader(Code);
				if(IsValidRef(Shader))
				{
					Shader->SetHash(Key.Hash);
					CachedVertexShaders.Add(Key, Shader);
					PrebindShader(Key);
					PlatformCache.Shaders.Add(Key);
				}
			}
			break;
		case SF_Pixel:
			if(!CachedPixelShaders.Find(Key))
			{
				FPixelShaderRHIRef Shader = RHICreatePixelShader(Code);
				if(IsValidRef(Shader))
				{
					Shader->SetHash(Key.Hash);
					CachedPixelShaders.Add(Key, Shader);
					PrebindShader(Key);
					PlatformCache.Shaders.Add(Key);
				}
			}
			break;
		case SF_Geometry:
			if(!CachedGeometryShaders.Find(Key))
			{
				FGeometryShaderRHIRef Shader = RHICreateGeometryShader(Code);
				if(IsValidRef(Shader))
				{
					Shader->SetHash(Key.Hash);
					CachedGeometryShaders.Add(Key, Shader);
					PrebindShader(Key);
					PlatformCache.Shaders.Add(Key);
				}
			}
			break;
		case SF_Hull:
			if(!CachedHullShaders.Find(Key))
			{
				FHullShaderRHIRef Shader = RHICreateHullShader(Code);
				if(IsValidRef(Shader))
				{
					Shader->SetHash(Key.Hash);
					CachedHullShaders.Add(Key, Shader);
					PrebindShader(Key);
					PlatformCache.Shaders.Add(Key);
				}
			}
			break;
		case SF_Domain:
			if(!CachedDomainShaders.Find(Key))
			{
				FDomainShaderRHIRef Shader = RHICreateDomainShader(Code);
				if(IsValidRef(Shader))
				{
					Shader->SetHash(Key.Hash);
					CachedDomainShaders.Add(Key, Shader);
					PrebindShader(Key);
					PlatformCache.Shaders.Add(Key);
				}
			}
			break;
		case SF_Compute:
			if(!CachedComputeShaders.Find(Key))
			{
				FComputeShaderRHIRef Shader = RHICreateComputeShader(Code);
				if(IsValidRef(Shader))
				{
					Shader->SetHash(Key.Hash);
					CachedComputeShaders.Add(Key, Shader);
					PrebindShader(Key);
					PlatformCache.Shaders.Add(Key);
				}
			}
			break;
		default:
			check(false);
			break;
	}
}

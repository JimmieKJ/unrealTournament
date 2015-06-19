// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BoundShaderStateCache.cpp: Bound shader state cache implementation.
=============================================================================*/

#include "RHI.h"
#include "BoundShaderStateCache.h"


typedef TMap<FBoundShaderStateKey,FCachedBoundShaderStateLink*> FBoundShaderStateCache;
typedef TMap<FBoundShaderStateKey,FCachedBoundShaderStateLink_Threadsafe*> FBoundShaderStateCache_Threadsafe;

/** Lazily initialized bound shader state cache singleton. */
static FBoundShaderStateCache& GetBoundShaderStateCache()
{
	static FBoundShaderStateCache BoundShaderStateCache;
	return BoundShaderStateCache;
}

/** Lazily initialized bound shader state cache singleton. */
static FBoundShaderStateCache_Threadsafe& GetBoundShaderStateCache_Threadsafe()
{
	static FBoundShaderStateCache_Threadsafe BoundShaderStateCache;
	return BoundShaderStateCache;
}

static FCriticalSection BoundShaderStateCacheLock;


FCachedBoundShaderStateLink::FCachedBoundShaderStateLink(
	FVertexDeclarationRHIParamRef VertexDeclaration,
	FVertexShaderRHIParamRef VertexShader,
	FPixelShaderRHIParamRef PixelShader,
	FHullShaderRHIParamRef HullShader,
	FDomainShaderRHIParamRef DomainShader,
	FGeometryShaderRHIParamRef GeometryShader,
	FBoundShaderStateRHIParamRef InBoundShaderState,
	bool bAddToSingleThreadedCache
	):
	BoundShaderState(InBoundShaderState),
	Key(VertexDeclaration,VertexShader,PixelShader,HullShader,DomainShader,GeometryShader),
	bAddedToSingleThreadedCache(bAddToSingleThreadedCache)
{
	if (bAddToSingleThreadedCache)
	{
		GetBoundShaderStateCache().Add(Key,this);
	}
}

FCachedBoundShaderStateLink::FCachedBoundShaderStateLink(
	FVertexDeclarationRHIParamRef VertexDeclaration,
	FVertexShaderRHIParamRef VertexShader,
	FPixelShaderRHIParamRef PixelShader,
	FBoundShaderStateRHIParamRef InBoundShaderState,
	bool bAddToSingleThreadedCache
	):
	BoundShaderState(InBoundShaderState),
	Key(VertexDeclaration,VertexShader,PixelShader),
	bAddedToSingleThreadedCache(bAddToSingleThreadedCache)
{
	if (bAddToSingleThreadedCache)
	{
		GetBoundShaderStateCache().Add(Key,this);
	}
}

FCachedBoundShaderStateLink::~FCachedBoundShaderStateLink()
{
	if (bAddedToSingleThreadedCache)
	{
		GetBoundShaderStateCache().Remove(Key);
		bAddedToSingleThreadedCache = false;
	}
}

FCachedBoundShaderStateLink* GetCachedBoundShaderState(
	FVertexDeclarationRHIParamRef VertexDeclaration,
	FVertexShaderRHIParamRef VertexShader,
	FPixelShaderRHIParamRef PixelShader,
	FHullShaderRHIParamRef HullShader,
	FDomainShaderRHIParamRef DomainShader,
	FGeometryShaderRHIParamRef GeometryShader
	)
{
	// Find the existing bound shader state in the cache.
	return GetBoundShaderStateCache().FindRef(
		FBoundShaderStateKey(VertexDeclaration,VertexShader,PixelShader,HullShader,DomainShader,GeometryShader)
		);
}


void FCachedBoundShaderStateLink_Threadsafe::AddToCache()
{
	FScopeLock Lock(&BoundShaderStateCacheLock);
	GetBoundShaderStateCache_Threadsafe().Add(Key,this);
}
void FCachedBoundShaderStateLink_Threadsafe::RemoveFromCache()
{
	FScopeLock Lock(&BoundShaderStateCacheLock);
	GetBoundShaderStateCache_Threadsafe().Remove(Key);
}


FBoundShaderStateRHIRef GetCachedBoundShaderState_Threadsafe(
	FVertexDeclarationRHIParamRef VertexDeclaration,
	FVertexShaderRHIParamRef VertexShader,
	FPixelShaderRHIParamRef PixelShader,
	FHullShaderRHIParamRef HullShader,
	FDomainShaderRHIParamRef DomainShader,
	FGeometryShaderRHIParamRef GeometryShader
	)
{
	FScopeLock Lock(&BoundShaderStateCacheLock);
	// Find the existing bound shader state in the cache.
	FCachedBoundShaderStateLink_Threadsafe* CachedBoundShaderStateLink = GetBoundShaderStateCache_Threadsafe().FindRef(
		FBoundShaderStateKey(VertexDeclaration,VertexShader,PixelShader,HullShader,DomainShader,GeometryShader)
		);
	if(CachedBoundShaderStateLink)
	{
		// If we've already created a bound shader state with these parameters, reuse it.
		return CachedBoundShaderStateLink->BoundShaderState;
	}
	return FBoundShaderStateRHIRef();
}


// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BoundShaderStateCache.cpp: Bound shader state cache implementation.
=============================================================================*/

#include "RHI.h"
#include "BoundShaderStateCache.h"

#if !defined(HAS_THREADSAFE_CreateBoundShaderState)
#error "HAS_THREADSAFE_CreateBoundShaderState must be defined"
#endif


typedef TMap<FBoundShaderStateKey,FCachedBoundShaderStateLink*> FBoundShaderStateCache;

/** Lazily initialized bound shader state cache singleton. */
static FBoundShaderStateCache& GetBoundShaderStateCache()
{
	static FBoundShaderStateCache BoundShaderStateCache;
	return BoundShaderStateCache;
}

#if HAS_THREADSAFE_CreateBoundShaderState
static FCriticalSection BoundShaderStateCacheLock;
#endif


FCachedBoundShaderStateLink::FCachedBoundShaderStateLink(
	FVertexDeclarationRHIParamRef VertexDeclaration,
	FVertexShaderRHIParamRef VertexShader,
	FPixelShaderRHIParamRef PixelShader,
	FHullShaderRHIParamRef HullShader,
	FDomainShaderRHIParamRef DomainShader,
	FGeometryShaderRHIParamRef GeometryShader,
	FBoundShaderStateRHIParamRef InBoundShaderState
	):
	BoundShaderState(InBoundShaderState),
	Key(VertexDeclaration,VertexShader,PixelShader,HullShader,DomainShader,GeometryShader)
{
	// Add this bound shader state to the cache.
#if !HAS_THREADSAFE_CreateBoundShaderState
	GetBoundShaderStateCache().Add(Key,this);
#endif
}

FCachedBoundShaderStateLink::FCachedBoundShaderStateLink(
	FVertexDeclarationRHIParamRef VertexDeclaration,
	FVertexShaderRHIParamRef VertexShader,
	FPixelShaderRHIParamRef PixelShader,
	FBoundShaderStateRHIParamRef InBoundShaderState
	):
	BoundShaderState(InBoundShaderState),
	Key(VertexDeclaration,VertexShader,PixelShader)
{
	// Add this bound shader state to the cache.
#if !HAS_THREADSAFE_CreateBoundShaderState
	GetBoundShaderStateCache().Add(Key,this);
#endif
}

FCachedBoundShaderStateLink::~FCachedBoundShaderStateLink()
{
#if !HAS_THREADSAFE_CreateBoundShaderState
	GetBoundShaderStateCache().Remove(Key);
#endif
}

#if HAS_THREADSAFE_CreateBoundShaderState
void FCachedBoundShaderStateLink::AddToCache()
{
	FScopeLock Lock(&BoundShaderStateCacheLock);
	GetBoundShaderStateCache().Add(Key,this);
}
void FCachedBoundShaderStateLink::RemoveFromCache()
{
	FScopeLock Lock(&BoundShaderStateCacheLock);
	GetBoundShaderStateCache().Remove(Key);
}
#endif


#if HAS_THREADSAFE_CreateBoundShaderState
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
	FCachedBoundShaderStateLink* CachedBoundShaderStateLink = GetBoundShaderStateCache().FindRef(
		FBoundShaderStateKey(VertexDeclaration,VertexShader,PixelShader,HullShader,DomainShader,GeometryShader)
		);
	if(CachedBoundShaderStateLink)
	{
		// If we've already created a bound shader state with these parameters, reuse it.
		return CachedBoundShaderStateLink->BoundShaderState;
	}
	return FBoundShaderStateRHIRef();
}

#else

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

#endif

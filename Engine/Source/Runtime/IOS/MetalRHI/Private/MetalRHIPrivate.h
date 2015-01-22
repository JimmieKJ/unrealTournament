// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalRHIPrivate.h: Private Metal RHI definitions.
=============================================================================*/

#pragma once

#include "Engine.h"

// UE4 has a Max of 8 RTs, but we can spend less time looping with 6
const uint32 MaxMetalRenderTargets = 6;

// How many possible vertex streams are allowed
const uint32 MaxMetalStreams = 16;

// Requirement for vertex buffer offset field
const uint32 BufferOffsetAlignment = 256;

//#define BUFFER_CACHE_MODE MTLResourceOptionCPUCacheModeDefault
#define BUFFER_CACHE_MODE MTLResourceOptionCPUCacheModeWriteCombined

#define SHOULD_TRACK_OBJECTS 0 // (UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)

#define UNREAL_TO_METAL_BUFFER_INDEX(Index) (15 - Index)

// Dependencies
#include "MetalRHI.h"
#include "MetalGlobalUniformBuffer.h"
#include "RHI.h"
#include "MetalManager.h"


// Access the underlying surface object from any kind of texture
FMetalSurface* GetMetalSurfaceFromRHITexture(FRHITexture* Texture);

#define NOT_SUPPORTED(Func) UE_LOG(LogMetal, Fatal, TEXT("'%s' is not supported"), L##Func);

#if SHOULD_TRACK_OBJECTS
extern TMap<id, int32> ClassCounts;
#define TRACK_OBJECT(Obj) ClassCounts.FindOrAdd([Obj class])++;
#define UNTRACK_OBJECT(Obj) ClassCounts.FindOrAdd([Obj class])--;
#else
#define TRACK_OBJECT(Obj)
#define UNTRACK_OBJECT(Obj)
#endif



FORCEINLINE int32 GetMetalCubeFace(ECubeFace Face)
{
	switch (Face)
	{
	case CubeFace_NegX:	return 0;
	case CubeFace_NegY:	return 1;
	case CubeFace_NegZ:	return 2;
	case CubeFace_PosX:	return 3;
	case CubeFace_PosY:	return 4;
	case CubeFace_PosZ:	return 5;
	default:			return -1;
	}
}

FORCEINLINE MTLLoadAction GetMetalRTLoadAction(ERenderTargetLoadAction LoadAction)
{
	switch(LoadAction)
	{
		case ERenderTargetLoadAction::ENoAction: return MTLLoadActionDontCare;
		case ERenderTargetLoadAction::ELoad: return MTLLoadActionLoad;
		case ERenderTargetLoadAction::EClear: return MTLLoadActionClear;
		default: return MTLLoadActionDontCare;
	}
}

FORCEINLINE MTLStoreAction GetMetalRTStoreAction(ERenderTargetStoreAction StoreAction)
{
	switch(StoreAction)
	{
		case ERenderTargetStoreAction::ENoAction: return MTLStoreActionDontCare;
		case ERenderTargetStoreAction::EStore: return MTLStoreActionStore;
		case ERenderTargetStoreAction::EMultisampleResolve: return MTLStoreActionMultisampleResolve;
		default: return MTLStoreActionDontCare;
	}
}
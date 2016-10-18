// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalRHIPrivate.h: Private Metal RHI definitions.
=============================================================================*/

#pragma once

#include "Engine.h"

// UE4 has a Max of 8 RTs, but we can spend less time looping with 6
const uint32 MaxMetalRenderTargets = 6;

// Requirement for vertex buffer offset field
const uint32 BufferOffsetAlignment = 256;

#define METAL_API_1_1 (__IPHONE_9_0 || __MAC_10_11)
#define METAL_API_1_2 ((__IPHONE_10_0 && __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0) || (__MAC_10_12 && __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_12))

#if METAL_API_1_1
#define BUFFER_CACHE_MODE MTLResourceCPUCacheModeWriteCombined
#else
#define BUFFER_CACHE_MODE MTLResourceOptionCPUCacheModeWriteCombined
#endif

#if PLATFORM_MAC
#define BUFFER_MANAGED_MEM MTLResourceStorageModeManaged
#define BUFFER_STORAGE_MODE MTLStorageModeManaged
#define BUFFER_RESOURCE_STORAGE_MANAGED MTLResourceStorageModeManaged
#define BUFFER_DYNAMIC_REALLOC BUF_AnyDynamic
// How many possible vertex streams are allowed
const uint32 MaxMetalStreams = 31;
#else
#define BUFFER_MANAGED_MEM 0
#define BUFFER_STORAGE_MODE MTLStorageModeShared
#define BUFFER_RESOURCE_STORAGE_MANAGED MTLResourceStorageModeShared
#define BUFFER_DYNAMIC_REALLOC BUF_Volatile
// How many possible vertex streams are allowed
const uint32 MaxMetalStreams = 30;
#endif

#ifndef METAL_STATISTICS
#define METAL_STATISTICS 0
#endif

#define SHOULD_TRACK_OBJECTS (UE_BUILD_DEBUG)

#define UNREAL_TO_METAL_BUFFER_INDEX(Index) ((MaxMetalStreams - 1) - Index)

// Dependencies
#include "MetalRHI.h"
#include "RHI.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#if !METAL_API_1_1
#define MTLVisibilityResultModeCounting ((MTLVisibilityResultMode)2)
#endif

#if !METAL_API_1_2
#define MTLFeatureSet_iOS_GPUFamily3_v1 ((MTLFeatureSet)4)
#define MTLFeatureSet_iOS_GPUFamily1_v3 ((MTLFeatureSet)5)
#define MTLFeatureSet_iOS_GPUFamily2_v3 ((MTLFeatureSet)6)
#define MTLFeatureSet_iOS_GPUFamily3_v2 ((MTLFeatureSet)7)
#define MTLFeatureSet_tvOS_GPUFamily1_v2 ((MTLFeatureSet)30001)
#define MTLFeatureSet_OSX_GPUFamily1_v2 ((MTLFeatureSet)10001)
#define MTLPixelFormatDepth16Unorm ((MTLPixelFormat)250)
#define MTLPixelFormatX24_Stencil8 ((MTLPixelFormat)262)
#define MTLPixelFormatX32_Stencil8 ((MTLPixelFormat)261)
#endif

// Access the internal context for the device-owning DynamicRHI object
FMetalDeviceContext& GetMetalDeviceContext();

// Safely release a metal resource, correctly handling the case where the RHI has been destructed first
void SafeReleaseMetalResource(id Object);

// Safely release a pooled buffer, correctly handling the case where the RHI has been destructed first
void SafeReleasePooledBuffer(id<MTLBuffer> Buffer);

// Access the underlying surface object from any kind of texture
FMetalSurface* GetMetalSurfaceFromRHITexture(FRHITexture* Texture);

#define NOT_SUPPORTED(Func) UE_LOG(LogMetal, Fatal, TEXT("'%s' is not supported"), L##Func);

#if SHOULD_TRACK_OBJECTS
void TrackMetalObject(NSObject* Object);
void UntrackMetalObject(NSObject* Object);
#define TRACK_OBJECT(Stat, Obj) INC_DWORD_STAT(Stat); TrackMetalObject(Obj)
#define UNTRACK_OBJECT(Stat, Obj) DEC_DWORD_STAT(Stat); UntrackMetalObject(Obj)
#else
#define TRACK_OBJECT(Stat, Obj) INC_DWORD_STAT(Stat)
#define UNTRACK_OBJECT(Stat, Obj) DEC_DWORD_STAT(Stat)
#endif

FORCEINLINE int32 GetMetalCubeFace(ECubeFace Face)
{
	// According to Metal docs these should match now: https://developer.apple.com/library/prerelease/ios/documentation/Metal/Reference/MTLTexture_Ref/index.html#//apple_ref/c/tdef/MTLTextureType
	switch (Face)
	{
		case CubeFace_PosX:;
		default:			return 0;
		case CubeFace_NegX:	return 1;
		case CubeFace_PosY:	return 2;
		case CubeFace_NegY:	return 3;
		case CubeFace_PosZ:	return 4;
		case CubeFace_NegZ:	return 5;
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

#include "MetalStateCache.h"
#include "MetalContext.h"

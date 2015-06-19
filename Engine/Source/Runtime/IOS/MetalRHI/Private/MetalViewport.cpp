// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalViewport.cpp: Metal viewport RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"


FMetalViewport::FMetalViewport(void* WindowHandle, uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen)
{

}

FMetalViewport::~FMetalViewport()
{
}

void FMetalViewport::Resize(uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen)
{
	FMetalManager::Get()->ResizeBackBuffer(InSizeX, InSizeY);
}

/*=============================================================================
 *	The following RHI functions must be called from the main thread.
 *=============================================================================*/
FViewportRHIRef FMetalDynamicRHI::RHICreateViewport(void* WindowHandle,uint32 SizeX,uint32 SizeY,bool bIsFullscreen,EPixelFormat PreferredPixelFormat /* ignored */)
{
	check( IsInGameThread() );
	return new FMetalViewport(WindowHandle, SizeX, SizeY, bIsFullscreen);
}

void FMetalDynamicRHI::RHIResizeViewport(FViewportRHIParamRef ViewportRHI,uint32 SizeX,uint32 SizeY,bool bIsFullscreen)
{
	check( IsInGameThread() );

	FMetalViewport* Viewport = ResourceCast(ViewportRHI);
	Viewport->Resize(SizeX,SizeY,bIsFullscreen);
}

void FMetalDynamicRHI::RHITick( float DeltaTime )
{
	check( IsInGameThread() );

}

/*=============================================================================
 *	Viewport functions.
 *=============================================================================*/

void FMetalDynamicRHI::RHIBeginDrawingViewport(FViewportRHIParamRef ViewportRHI, FTextureRHIParamRef RenderTargetRHI)
{
	FMetalManager::Get()->BeginFrame();
}

void FMetalDynamicRHI::RHIEndDrawingViewport(FViewportRHIParamRef ViewportRHI,bool bPresent,bool bLockToVsync)
{
	FMetalManager::Get()->EndFrame(bPresent);
}

bool FMetalDynamicRHI::RHIIsDrawingViewport()
{
	return true;
}

FTexture2DRHIRef FMetalDynamicRHI::RHIGetViewportBackBuffer(FViewportRHIParamRef ViewportRHI)
{
	return FMetalManager::Get()->GetBackBuffer();
}

void FMetalDynamicRHI::RHIAdvanceFrameForGetViewportBackBuffer()
{
}

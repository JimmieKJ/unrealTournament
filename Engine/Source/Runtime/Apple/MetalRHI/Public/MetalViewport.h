// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalViewport.h: Metal viewport RHI definitions.
=============================================================================*/

#pragma once

#if PLATFORM_MAC
#include "CocoaTextView.h"
@interface FMetalView : FCocoaTextView
@end
#endif

enum EMetalViewportAccessFlag
{
	EMetalViewportAccessRHI,
	EMetalViewportAccessRenderer,
	EMetalViewportAccessGame
};

class FMetalViewport : public FRHIViewport
{
public:
	FMetalViewport(void* WindowHandle, uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen);
	~FMetalViewport();

	void BeginDrawingViewport();
	void Resize(uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen);
	
	FMetalTexture2D* GetBackBuffer(EMetalViewportAccessFlag Accessor) const;
	id<MTLDrawable> GetDrawable(EMetalViewportAccessFlag Accessor);
	id<MTLTexture> GetDrawableTexture(EMetalViewportAccessFlag Accessor);
	void ReleaseDrawable(void);
	
#if PLATFORM_MAC
	NSWindow* GetWindow() const;
#endif
	
private:
	uint32 GetViewportIndex(EMetalViewportAccessFlag Accessor) const;

private:
	id<MTLDrawable> Drawable;
	TRefCountPtr<FMetalTexture2D> BackBuffer[2];
	TArray<TRefCountPtr<FMetalTexture2D>> BackBuffersQueue;
	FCriticalSection Mutex;

#if PLATFORM_MAC
	FMetalView* View;
#endif
};

template<>
struct TMetalResourceTraits<FRHIViewport>
{
	typedef FMetalViewport TConcreteType;
};

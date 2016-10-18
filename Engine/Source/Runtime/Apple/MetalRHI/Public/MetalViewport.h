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

class FMetalViewport : public FRHIViewport
{
public:
	FMetalViewport(void* WindowHandle, uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen);
	~FMetalViewport();

	void Resize(uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen);
	
	FMetalTexture2D* GetBackBuffer() const { return BackBuffer; }
	id<MTLDrawable> GetDrawable();
	id<MTLTexture> GetDrawableTexture();
	void ReleaseDrawable();
	
#if PLATFORM_MAC
	NSWindow* GetWindow() const;
#endif

private:
	id<MTLDrawable> Drawable;
	TRefCountPtr<FMetalTexture2D> BackBuffer;

#if PLATFORM_MAC
	FMetalView* View;
#endif
};

template<>
struct TMetalResourceTraits<FRHIViewport>
{
	typedef FMetalViewport TConcreteType;
};

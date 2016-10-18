// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalViewport.cpp: Metal viewport RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#if PLATFORM_MAC
#include "CocoaWindow.h"
#include "CocoaThread.h"
#else
#include "IOSAppDelegate.h"
#endif

#if PLATFORM_MAC

@implementation FMetalView

- (id)initWithFrame:(NSRect)frameRect
{
	self = [super initWithFrame:frameRect];
	if (self)
	{
	}
	return self;
}

- (BOOL)isOpaque
{
	return YES;
}

- (BOOL)mouseDownCanMoveWindow
{
	return YES;
}

@end

#endif

FMetalViewport::FMetalViewport(void* WindowHandle, uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen)
: Drawable(nil)
{
#if PLATFORM_MAC
	FCocoaWindow* Window = (FCocoaWindow*)WindowHandle;
	const NSRect ContentRect = NSMakeRect(0, 0, InSizeX, InSizeY);
	View = [[FMetalView alloc] initWithFrame:ContentRect];
	[View setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
	[View setWantsLayer:YES];

	CAMetalLayer* Layer = [CAMetalLayer new];

	CGFloat bgColor[] = { 0.0, 0.0, 0.0, 0.0 };
	Layer.edgeAntialiasingMask = 0;
	Layer.masksToBounds = YES;
	Layer.backgroundColor = CGColorCreate(CGColorSpaceCreateDeviceRGB(), bgColor);
	Layer.presentsWithTransaction = NO;
	Layer.anchorPoint = CGPointMake(0.5, 0.5);
	Layer.frame = ContentRect;
	Layer.magnificationFilter = kCAFilterNearest;
	Layer.minificationFilter = kCAFilterNearest;
//	Layer.drawsAsynchronously = YES; // @todo zebra: do we need and/or want this on Mac?

	[Layer setDevice:GetMetalDeviceContext().GetDevice()];
	[Layer setPixelFormat:MTLPixelFormatBGRA8Unorm];
	[Layer setFramebufferOnly:NO];
	[Layer removeAllAnimations];

	[View setLayer:Layer];

	[Window setContentView:View];
	[[Window standardWindowButton:NSWindowCloseButton] setAction:@selector(performClose:)];

	Resize(InSizeX, InSizeY, bInIsFullscreen);
#else
	Resize(InSizeX, InSizeY, bInIsFullscreen);
#endif
}

FMetalViewport::~FMetalViewport()
{
	BackBuffer.SafeRelease();	// when the rest of the engine releases it, its framebuffers will be released too (those the engine knows about)
	check(!IsValidRef(BackBuffer));
}

void FMetalViewport::Resize(uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen)
{
	FRHIResourceCreateInfo CreateInfo;
	BackBuffer.SafeRelease();	// when the rest of the engine releases it, its framebuffers will be released too (those the engine knows about)
	
#if PLATFORM_MAC // @todo zebra: ios
	BackBuffer = (FMetalTexture2D*)(FTexture2DRHIParamRef)GDynamicRHI->RHICreateTexture2D(InSizeX, InSizeY, PF_B8G8R8A8, 1, 1, TexCreate_RenderTargetable, CreateInfo);
	((CAMetalLayer*)View.layer).drawableSize = CGSizeMake(InSizeX, InSizeY);
#else
	BackBuffer = (FMetalTexture2D*)(FTexture2DRHIParamRef)GDynamicRHI->RHICreateTexture2D(InSizeX, InSizeY, PF_B8G8R8A8, 1, 1, TexCreate_RenderTargetable | TexCreate_Presentable, CreateInfo);
	IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
	FIOSView* GLView = AppDelegate.IOSView;
	[GLView UpdateRenderWidth:InSizeX andHeight:InSizeY];
	
	// check the size of the window
	float ScalingFactor = [[IOSAppDelegate GetDelegate].IOSView contentScaleFactor];
	CGRect ViewFrame = [[IOSAppDelegate GetDelegate].IOSView frame];
	check(FMath::TruncToInt(ScalingFactor * ViewFrame.size.width) == InSizeX &&
		  FMath::TruncToInt(ScalingFactor * ViewFrame.size.height) == InSizeY);
#endif
	BackBuffer->Surface.Viewport = this;
}

id<MTLDrawable> FMetalViewport::GetDrawable()
{
	if (!Drawable)
	{
		@autoreleasepool
		{
			uint32 IdleStart = FPlatformTime::Cycles();
			
	#if PLATFORM_MAC
			CAMetalLayer* CurrentLayer = (CAMetalLayer*)[View layer];
			Drawable = CurrentLayer ? [[CurrentLayer nextDrawable] retain] : nil;
	#else
			Drawable = [[[IOSAppDelegate GetDelegate].IOSView MakeDrawable] retain];
	#endif
			
			GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUPresent] += FPlatformTime::Cycles() - IdleStart;
			GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUPresent]++;
		}
	}
	return Drawable;
}

id<MTLTexture> FMetalViewport::GetDrawableTexture()
{
	id<CAMetalDrawable> CurrentDrawable = (id<CAMetalDrawable>)GetDrawable();
	return CurrentDrawable.texture;
}

void FMetalViewport::ReleaseDrawable()
{
	if (Drawable)
	{
		[Drawable release];
		Drawable = nil;
	}
#if !PLATFORM_MAC
	BackBuffer->Surface.Texture = nil;
#endif
}

#if PLATFORM_MAC
NSWindow* FMetalViewport::GetWindow() const
{
	return [View window];
}
#endif

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
	Viewport->Resize(SizeX, SizeY, bIsFullscreen);
}

void FMetalDynamicRHI::RHITick( float DeltaTime )
{
	check( IsInGameThread() );
}

/*=============================================================================
 *	Viewport functions.
 *=============================================================================*/

void FMetalRHICommandContext::RHIBeginDrawingViewport(FViewportRHIParamRef ViewportRHI, FTextureRHIParamRef RenderTargetRHI)
{
	check(false);
}

void FMetalDynamicRHI::RHIBeginDrawingViewport(FViewportRHIParamRef ViewportRHI, FTextureRHIParamRef RenderTargetRHI)
{
	FMetalViewport* Viewport = ResourceCast(ViewportRHI);
	
	check(Viewport);

	check(Viewport);
	
	((FMetalDeviceContext*)Context)->BeginDrawingViewport(Viewport);

	// Set the render target and viewport.
	if (RenderTargetRHI)
	{
		FRHIRenderTargetView RTV(RenderTargetRHI);
		RHISetRenderTargets(1, &RTV, nullptr, 0, NULL);
	}
	else
	{
		FRHIRenderTargetView RTV(Viewport->GetBackBuffer());
		RHISetRenderTargets(1, &RTV, nullptr, 0, NULL);
	}
}

void FMetalRHICommandContext::RHIEndDrawingViewport(FViewportRHIParamRef ViewportRHI,bool bPresent,bool bLockToVsync)
{
	check(false);
}

void FMetalDynamicRHI::RHIEndDrawingViewport(FViewportRHIParamRef ViewportRHI,bool bPresent,bool bLockToVsync)
{
	FMetalViewport* Viewport = ResourceCast(ViewportRHI);
	((FMetalDeviceContext*)Context)->EndDrawingViewport(Viewport, bPresent);
#if PLATFORM_MAC
	FCocoaWindow* Window = (FCocoaWindow*)Viewport->GetWindow();
	MainThreadCall(^{
		[Window startRendering];
	}, NSDefaultRunLoopMode, false);
#endif
}

FTexture2DRHIRef FMetalDynamicRHI::RHIGetViewportBackBuffer(FViewportRHIParamRef ViewportRHI)
{
	FMetalViewport* Viewport = ResourceCast(ViewportRHI);
	return Viewport->GetBackBuffer();
}

void FMetalDynamicRHI::RHIAdvanceFrameForGetViewportBackBuffer()
{
}

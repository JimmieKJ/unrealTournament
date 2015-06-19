// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OpenGLDrvPrivate.h"

#include "MacOpenGLContext.h"
#include "MacOpenGLQuery.h"

#include "MacWindow.h"
#include "MacTextInputMethodSystem.h"
#include "CocoaTextView.h"
#include "CocoaThread.h"
#include "CocoaWindow.h"
#include <IOKit/IOKitLib.h>
#include <mach-o/dyld.h>

/*------------------------------------------------------------------------------
 OpenGL static variables.
 ------------------------------------------------------------------------------*/

// @todo: remove once Apple fixes radr://16754329 AMD Cards don't always perform FRAMEBUFFER_SRGB if the draw FBO has mixed sRGB & non-SRGB colour attachments
static TAutoConsoleVariable<int32> CVarMacUseFrameBufferSRGB(
	TEXT("r.Mac.UseFrameBufferSRGB"),
	0,
	TEXT("Flag to toggle use of GL_FRAMEBUFFER_SRGB for better color accuracy.\n"),
	ECVF_RenderThreadSafe
	);

// @todo: remove once Apple fixes radr://15553950, TTP# 315197
static int32 GMacFlushTexStorage = true;
static int32 GMacMustFlushTexStorage = false;
static FAutoConsoleVariableRef CVarMacFlushTexStorage(
	TEXT("r.Mac.FlushTexStorage"),
	GMacFlushTexStorage,
	TEXT("When true, for GPUs that require it the OpenGL command stream will be flushed after glTexStorage* to avoid driver errors, when false no flush will be inserted which is faster but may cause errors on some drivers. (Default: True)"),
	ECVF_RenderThreadSafe
	);

int32 GMacUseMTGL = 1;
static FAutoConsoleVariableRef CVarMacUseMTGL(
	TEXT("r.Mac.UseMTGL"),
	GMacUseMTGL,
	TEXT("If true use Apple's multi-threaded OpenGL which parallelises the OpenGL driver with the rendering thread for improved performance, use false to disable. (Default: True)"),
	ECVF_RenderThreadSafe
	);

static int32 GMacMinTexturesToDeletePerFrame = 8;
static FAutoConsoleVariableRef CVarMacMinTexturesToDeletePerFrame(
	TEXT("r.Mac.MinTexturesToDeletePerFrame"),
	GMacMinTexturesToDeletePerFrame,
	TEXT("Specifies how many textures are required to be waiting before a call to glDeleteTextures is made in order to amortize the cost. (Default: 8)"),
	ECVF_RenderThreadSafe
	);

static int32 GMacMaxTexturesToDeletePerFrame = INT_MAX;
static FAutoConsoleVariableRef CVarMacMaxTexturesToDeletePerFrame(
	TEXT("r.Mac.MaxTexturesToDeletePerFrame"),
	GMacMaxTexturesToDeletePerFrame,
	TEXT("Specifies the maximum number of textures that can be deleted in the single call to glDeleteTextures each frame. (Default: INT_MAX)"),
	ECVF_RenderThreadSafe
	);

static const uint32 GMacTexturePoolNum = 3;
static TArray<GLuint> GMacTexturesToDelete[GMacTexturePoolNum];

/*------------------------------------------------------------------------------
 OpenGL context management.
 ------------------------------------------------------------------------------*/

class FScopeContext
{
public:
	FScopeContext( NSOpenGLContext* Context )
	{
		SCOPED_AUTORELEASE_POOL;

		check(Context);
		PreviousContext = [NSOpenGLContext currentContext];
		bSameContext = (PreviousContext == Context);
		if (!bSameContext)
		{
			if (PreviousContext)
			{
				glFlushRenderAPPLE();
			}
			[Context makeCurrentContext];
			FPlatformOpenGLContext::VerifyCurrentContext();
		}
	}

	~FScopeContext( void )
	{
		if (!bSameContext)
		{
			SCOPED_AUTORELEASE_POOL;

			glFlushRenderAPPLE();
			if (PreviousContext)
			{
				[PreviousContext makeCurrentContext];
				FPlatformOpenGLContext::VerifyCurrentContext();
			}
			else
			{
				[NSOpenGLContext clearCurrentContext];
			}
		}
	}

private:
	NSOpenGLContext*	PreviousContext;
	bool				bSameContext;
};

static void DeleteQueriesForCurrentContext( NSOpenGLContext* Context );
static void DrawOpenGLViewport(FPlatformOpenGLContext* const Context, uint32 Width, uint32 Height);

static void LockGLContext(NSOpenGLContext* Context)
{
	if (FPlatformMisc::IsRunningOnMavericks())
	{
		CGLLockContext([Context CGLContextObj]);
	}
	else
	{
		[Context lock];
	}
}

static void UnlockGLContext(NSOpenGLContext* Context)
{
	if (FPlatformMisc::IsRunningOnMavericks())
	{
		CGLUnlockContext([Context CGLContextObj]);
	}
	else
	{
		[Context unlock];
	}
}

/*------------------------------------------------------------------------------
 OpenGL view.
 ------------------------------------------------------------------------------*/

@interface FSlateOpenGLLayer : NSOpenGLLayer
@property (assign) NSOpenGLContext* Context;
@property (assign) NSOpenGLPixelFormat* PixelFormat;
@end

@implementation FSlateOpenGLLayer

- (NSOpenGLPixelFormat *)openGLPixelFormatForDisplayMask:(uint32)Mask
{
	return self.PixelFormat;
}

- (NSOpenGLContext *)openGLContextForPixelFormat:(NSOpenGLPixelFormat *)PixelFormat
{
	return self.Context;
}

- (id)initWithContext:(NSOpenGLContext*)context andPixelFormat:(NSOpenGLPixelFormat*)pixelFormat
{
	self = [super init];
	if (self)
	{
		self.Context = context;
		self.PixelFormat = pixelFormat;
	}
	return self;
}

- (BOOL)canDrawInOpenGLContext:(NSOpenGLContext *)context pixelFormat:(NSOpenGLPixelFormat *)pixelFormat forLayerTime:(CFTimeInterval)timeInterval displayTime:(const CVTimeStamp *)timeStamp
{
	BOOL bOK = [super canDrawInOpenGLContext:context pixelFormat:pixelFormat forLayerTime:timeInterval displayTime:timeStamp];
	if ( bOK && context && (self.Context == context) )
	{
		LockGLContext(context);
	}
	return bOK;
}

@end

/**
 * Custom view class
 */
@interface FCocoaOpenGLView : FCocoaTextView
@property (assign) FPlatformOpenGLContext* Context;
@property (atomic) bool bNeedsUpdate;
@end

@implementation FCocoaOpenGLView

- (CALayer*)makeBackingLayer
{
	return [[FSlateOpenGLLayer alloc] initWithContext:self.Context->OpenGLContext andPixelFormat:self.Context->OpenGLPixelFormat];
}

- (id)initWithFrame:(NSRect)frameRect context:(FPlatformOpenGLContext*)context
{
	self = [super initWithFrame:frameRect];
	if (self)
	{
		self.Context = context;
		self.bNeedsUpdate = true;
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_surfaceNeedsUpdate:) name:NSViewGlobalFrameDidChangeNotification object:self];
	}
	return self;
}

- (void)dealloc
{
	[[NSNotificationCenter defaultCenter] removeObserver:self name:NSViewGlobalFrameDidChangeNotification object:self];
	[super dealloc];
}

- (void)_surfaceNeedsUpdate:(NSNotification*)notification
{
	self.bNeedsUpdate = true;
}

- (void)drawRect:(NSRect)DirtyRect
{
	DrawOpenGLViewport(self.Context, self.frame.size.width, self.frame.size.height);
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

extern void OnQueryInvalidation( void );

struct FPlatformOpenGLDevice
{
	FPlatformOpenGLContext	SharedContext;

	FPlatformOpenGLContext	RenderingContext;

	TArray<FPlatformOpenGLContext*>	FreeOpenGLContexts;
	int32						NumUsedContexts;

	/** Guards against operating on viewport contexts from more than one thread at the same time. */
	FCriticalSection*			ContextUsageGuard;

	FPlatformOpenGLDevice()
		: NumUsedContexts(0)
	{
		SCOPED_AUTORELEASE_POOL;
		
		FPlatformOpenGLContext::RegisterGraphicsSwitchingCallback();
		
		ContextUsageGuard = new FCriticalSection;
		SharedContext.Initialise(NULL);
		
		RenderingContext.Initialise(SharedContext.OpenGLContext);
		
		SharedContext.MakeCurrent();
	}

	~FPlatformOpenGLDevice()
	{
		SCOPED_AUTORELEASE_POOL;

		check(NumUsedContexts==0);

		// Release all used OpenGL contexts
		{
			FScopeLock ScopeLock(ContextUsageGuard);
			for (int32 ContextIndex = 0; ContextIndex < FreeOpenGLContexts.Num(); ++ContextIndex)
			{
				delete FreeOpenGLContexts[ContextIndex];
			}
			FreeOpenGLContexts.Empty();
		}

		OnQueryInvalidation();
		
		{
			FScopeContext Context(SharedContext.OpenGLContext);

			for(uint32 i = 0; i < GMacTexturePoolNum; i++)
			{
				if(GMacTexturesToDelete[i].Num())
				{
					glDeleteTextures(GMacTexturesToDelete[i].Num(), GMacTexturesToDelete[i].GetData());
					GMacTexturesToDelete[i].Reset();
				}
			}
		}
		
		FPlatformOpenGLContext::UnregisterGraphicsSwitchingCallback();
		
		delete ContextUsageGuard;
	}
};

FPlatformOpenGLDevice* PlatformCreateOpenGLDevice()
{
	return new FPlatformOpenGLDevice;
}

void PlatformDestroyOpenGLDevice(FPlatformOpenGLDevice* Device)
{
	delete Device;
}

FPlatformOpenGLContext* PlatformCreateOpenGLContext(FPlatformOpenGLDevice* Device, void* InWindowHandle)
{
	SCOPED_AUTORELEASE_POOL;

	check(InWindowHandle);

	FPlatformOpenGLContext* Context = nullptr;
	{
		FScopeLock ScopeLock(Device->ContextUsageGuard);
		if( Device->FreeOpenGLContexts.Num() )
		{
			Context = Device->FreeOpenGLContexts.Pop();
		}
		else
		{
			Context = new FPlatformOpenGLContext;
		}
	}
	check(Context);

	{
		NSOpenGLContext* PreviousContext = [NSOpenGLContext currentContext];

		Context->Initialise(Device->SharedContext.OpenGLContext);
		Context->ViewportFramebuffer = 0;	// will be created on demand
		Context->SyncInterval = 0;
		Context->WindowHandle = (NSWindow*)InWindowHandle;

		NSRect ContentRect = NSMakeRect(0, 0, Context->WindowHandle.frame.size.width, Context->WindowHandle.frame.size.height);
		Context->OpenGLView = [[FCocoaOpenGLView alloc] initWithFrame:ContentRect context:Context];
		[Context->OpenGLView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

		glFlushRenderAPPLE();

		if (PreviousContext)
		{
			[PreviousContext makeCurrentContext];
			FPlatformOpenGLContext::VerifyCurrentContext();
		}
		else
		{
			[NSOpenGLContext clearCurrentContext];
		}
	}

	++Device->NumUsedContexts;

	[Context->OpenGLView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

	MainThreadCall(^{
		if (FPlatformMisc::IsRunningOnMavericks() && ([Context->WindowHandle styleMask] & NSTexturedBackgroundWindowMask))
		{
			NSView* SuperView = [[Context->WindowHandle contentView] superview];
			[SuperView addSubview:Context->OpenGLView];
			[SuperView setWantsLayer:YES];
			[SuperView addSubview:[Context->WindowHandle standardWindowButton:NSWindowCloseButton]];
			[SuperView addSubview:[Context->WindowHandle standardWindowButton:NSWindowMiniaturizeButton]];
			[SuperView addSubview:[Context->WindowHandle standardWindowButton:NSWindowZoomButton]];
		}
		else
		{
			[Context->OpenGLView setWantsLayer:YES];
			[Context->WindowHandle setContentView:Context->OpenGLView];
		}

		[[Context->WindowHandle standardWindowButton:NSWindowCloseButton] setAction:@selector(performClose:)];

		Context->OpenGLView.layer.magnificationFilter = kCAFilterNearest;
		Context->OpenGLView.layer.minificationFilter = kCAFilterNearest;
	}, NSDefaultRunLoopMode, true);

	return Context;
}

void PlatformReleaseOpenGLContext(FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context)
{
	SCOPED_AUTORELEASE_POOL;

	check(Context && Context->OpenGLView && Context->OpenGLContext);
	{
		FScopeLock ScopeLock(Device->ContextUsageGuard);
		
		Context->Reset();
		Device->FreeOpenGLContexts.Add(Context);

		--Device->NumUsedContexts;
	}
}

void PlatformDestroyOpenGLContext(FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context)
{
	PlatformReleaseOpenGLContext(Device, Context);
}

void* PlatformGetWindow(FPlatformOpenGLContext* Context, void** AddParam)
{
	check(Context && Context->WindowHandle);

	return Context->WindowHandle;
}

bool PlatformBlitToViewport(FPlatformOpenGLDevice* Device, const FOpenGLViewport& Viewport, uint32 BackbufferSizeX, uint32 BackbufferSizeY, bool bPresent,bool bLockToVsync, int32 SyncInterval)
{
	FPlatformOpenGLContext* const Context = Viewport.GetGLContext();
	check(Context && Context->OpenGLView);

	FScopeLock ScopeLock(Device->ContextUsageGuard);
	LockGLContext(Context->OpenGLContext);
	{
		FScopeContext ScopeContext(Context->OpenGLContext);

		if(Context->OpenGLView.bNeedsUpdate)
		{
			Context->OpenGLView.bNeedsUpdate = false;
			[Context->OpenGLContext update];
		}

		if (bPresent)
		{
			int32 RealSyncInterval = bLockToVsync ? SyncInterval : 0;

			if (Context->SyncInterval != RealSyncInterval)
			{
				[Context->OpenGLContext setValues: &RealSyncInterval forParameter: NSOpenGLCPSwapInterval];
				Context->SyncInterval = RealSyncInterval;
			}

			[(FCocoaWindow*)Context->WindowHandle startRendering];
			
			int32 CurrentReadFramebuffer = 0;
			glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &CurrentReadFramebuffer);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, Context->ViewportFramebuffer);
			if ( Context->ViewportSize[0] != BackbufferSizeX || Context->ViewportSize[1] != BackbufferSizeY )
			{
				glBindRenderbuffer(GL_RENDERBUFFER, Context->ViewportRenderbuffer);
				glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, BackbufferSizeX, BackbufferSizeY);
				glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, Context->ViewportRenderbuffer);
				glBindRenderbuffer(GL_RENDERBUFFER, 0);
			}
			glDrawBuffer(GL_COLOR_ATTACHMENT1);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, Context->ViewportFramebuffer);
			glReadBuffer(GL_COLOR_ATTACHMENT0);
			glDisable(GL_FRAMEBUFFER_SRGB);
			glBlitFramebuffer(0, 0, BackbufferSizeX, BackbufferSizeY, 0, 0, BackbufferSizeX, BackbufferSizeY, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glEnable(GL_FRAMEBUFFER_SRGB);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, CurrentReadFramebuffer);
			Context->ViewportSize[0] = BackbufferSizeX;
			Context->ViewportSize[1] = BackbufferSizeY;

			MainThreadCall(^{ [Context->OpenGLView setNeedsDisplay:YES]; }, NSDefaultRunLoopMode, false);

			TArray<GLuint>& TexturesToDelete = GMacTexturesToDelete[(GFrameNumberRenderThread - (GMacTexturePoolNum - 1)) % GMacTexturePoolNum];
			if(TexturesToDelete.Num() && TexturesToDelete.Num() > GMacMinTexturesToDeletePerFrame)
			{
				uint32 Num = FMath::Min(TexturesToDelete.Num(), GMacMaxTexturesToDeletePerFrame);
				glDeleteTextures(Num, TexturesToDelete.GetData());
				TexturesToDelete.RemoveAt(0, Num, false);
			}
		}
	}
	UnlockGLContext(Context->OpenGLContext);

	return !Viewport.GetCustomPresent();
}

void DrawOpenGLViewport(FPlatformOpenGLContext* const Context, uint32 Width, uint32 Height)
{
	FCocoaWindow* Window = (FCocoaWindow*)Context->WindowHandle;
	if ([Window isRenderInitialized] && Context->ViewportSize[0] && Context->ViewportSize[1] && Context->ViewportFramebuffer && Context->ViewportRenderbuffer && ([Window styleMask] & (NSTexturedBackgroundWindowMask|NSFullSizeContentViewWindowMask) || !Window.inLiveResize))
	{
		int32 CurrentReadFramebuffer = 0;
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &CurrentReadFramebuffer);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, Context->ViewportFramebuffer);
		glReadBuffer(GL_COLOR_ATTACHMENT1);
		glDisable(GL_FRAMEBUFFER_SRGB);
		glBlitFramebuffer(0, 0, Context->ViewportSize[0], Context->ViewportSize[1], 0, Height, Width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		glEnable(GL_FRAMEBUFFER_SRGB);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, CurrentReadFramebuffer);
	}
	else
	{
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	[Context->OpenGLContext flushBuffer];
	UnlockGLContext(Context->OpenGLContext);
}

void PlatformRenderingContextSetup(FPlatformOpenGLDevice* Device)
{
	check(Device);
	Device->RenderingContext.MakeCurrent();
}

void PlatformSharedContextSetup(FPlatformOpenGLDevice* Device)
{
	check(Device);
	Device->SharedContext.MakeCurrent();
}

void PlatformNULLContextSetup()
{
	SCOPED_AUTORELEASE_POOL;
	if ([NSOpenGLContext currentContext])
	{
		glFlushRenderAPPLE();
	}
	[NSOpenGLContext clearCurrentContext];
}

EOpenGLCurrentContext PlatformOpenGLCurrentContext(FPlatformOpenGLDevice* Device)
{
	SCOPED_AUTORELEASE_POOL;

	NSOpenGLContext* Context = [NSOpenGLContext currentContext];

	if (Context == Device->RenderingContext.OpenGLContext)	// most common case
	{
		return CONTEXT_Rendering;
	}
	else if (Context == Device->SharedContext.OpenGLContext)
	{
		return CONTEXT_Shared;
	}
	else if (Context)
	{
		return CONTEXT_Other;
	}
	else
	{
		return CONTEXT_Invalid;
	}
}

void PlatformFlushIfNeeded()
{
	if([NSOpenGLContext currentContext])
	{
		FMacOpenGL::Flush();
	}
}

void PlatformRebindResources(FPlatformOpenGLDevice* Device)
{
	// @todo: Figure out if we need to rebind frame & renderbuffers after switching contexts
}

void PlatformResizeGLContext( FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context, uint32 SizeX, uint32 SizeY, bool bFullscreen, bool bWasFullscreen, GLenum BackBufferTarget, GLuint BackBufferResource)
{
	FScopeLock ScopeLock(Device->ContextUsageGuard);
	LockGLContext(Context->OpenGLContext);
	{
		FScopeContext ScopeContext(Context->OpenGLContext);

		SCOPED_AUTORELEASE_POOL;
		
		// Cache & clear the drawable view before resizing the context
		// otherwise backstore size changes won't be respected.
		NSView* View = [Context->OpenGLContext view];
		[Context->OpenGLContext clearDrawable];
		
		// Resize the context to the desired resolution directly rather than
		// trusting the OS. We must do this because of our subversion of AppKit
		// message handling. If we don't do this ourselves then the OS will not
		// resize the backing store when the window size changes to adjust the resolution
		// when exiting fullscreen. This fails despite the transition from fullscreen
		// having completed according to the notifications.
		GLint dim[2];
		dim[0] = SizeX;
		dim[1] = SizeY;
		CGLError Err = CGLSetParameter((CGLContextObj)[Context->OpenGLContext CGLContextObj], kCGLCPSurfaceBackingSize, &dim[0]);
		Err = CGLEnable((CGLContextObj)[Context->OpenGLContext CGLContextObj], kCGLCESurfaceBackingSize);
		
		// Restore the drawable view to make it display again.
		[Context->OpenGLContext setView: View];
		
		[Context->OpenGLContext update];

		if (Context->ViewportFramebuffer == 0)
		{
			glGenFramebuffers(1, &Context->ViewportFramebuffer);
			check(Context->ViewportFramebuffer);
		}
		
		glBindFramebuffer(GL_FRAMEBUFFER, Context->ViewportFramebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, BackBufferTarget, BackBufferResource, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, 0, 0);
		glViewport(0, 0, SizeX, SizeY);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glClearColor(0.f, 0.f, 0.f, 0.f);
		glClear(GL_COLOR_BUFFER_BIT);
		
		if (Context->ViewportRenderbuffer != 0)
		{
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, Context->ViewportRenderbuffer);
		}
		else
		{
			glGenRenderbuffers(1, &Context->ViewportRenderbuffer);
			check(Context->ViewportRenderbuffer);
		}
#if UE_BUILD_DEBUG
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		GLenum CompleteResult = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (CompleteResult != GL_FRAMEBUFFER_COMPLETE)
		{
			UE_LOG(LogRHI, Fatal,TEXT("Framebuffer not complete. Status = 0x%x"), CompleteResult);
		}
#endif
	}
	UnlockGLContext(Context->OpenGLContext);
}

void PlatformGetSupportedResolution(uint32 &Width, uint32 &Height)
{
	uint32 InitializedMode = false;
	uint32 BestWidth = 0;
	uint32 BestHeight = 0;

	CFArrayRef AllModes = CGDisplayCopyAllDisplayModes(kCGDirectMainDisplay, NULL);
	if (AllModes)
	{
		int32 NumModes = CFArrayGetCount(AllModes);
		for (int32 Index = 0; Index < NumModes; Index++)
		{
			CGDisplayModeRef Mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(AllModes, Index);
			int32 ModeWidth = (int32)CGDisplayModeGetWidth(Mode);
			int32 ModeHeight = (int32)CGDisplayModeGetHeight(Mode);

			bool IsEqualOrBetterWidth = FMath::Abs((int32)ModeWidth - (int32)Width) <= FMath::Abs((int32)BestWidth - (int32)Width);
			bool IsEqualOrBetterHeight = FMath::Abs((int32)ModeHeight - (int32)Height) <= FMath::Abs((int32)BestHeight - (int32)Height);
			if(!InitializedMode || (IsEqualOrBetterWidth && IsEqualOrBetterHeight))
			{
				BestWidth = ModeWidth;
				BestHeight = ModeHeight;
				InitializedMode = true;
			}
		}
		CFRelease(AllModes);
	}
	check(InitializedMode);
	Width = BestWidth;
	Height = BestHeight;
}

bool PlatformGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{
	SCOPED_AUTORELEASE_POOL;

	NSArray* AllScreens = [NSScreen screens];
	NSScreen* PrimaryScreen = (NSScreen*)[AllScreens objectAtIndex: 0];

	SIZE_T Scale = (SIZE_T)[PrimaryScreen backingScaleFactor];

	int32 MinAllowableResolutionX = 0;
	int32 MinAllowableResolutionY = 0;
	int32 MaxAllowableResolutionX = 10480;
	int32 MaxAllowableResolutionY = 10480;
	int32 MinAllowableRefreshRate = 0;
	int32 MaxAllowableRefreshRate = 10480;

	if (MaxAllowableResolutionX == 0)
	{
		MaxAllowableResolutionX = 10480;
	}
	if (MaxAllowableResolutionY == 0)
	{
		MaxAllowableResolutionY = 10480;
	}
	if (MaxAllowableRefreshRate == 0)
	{
		MaxAllowableRefreshRate = 10480;
	}

	CFArrayRef AllModes = CGDisplayCopyAllDisplayModes(kCGDirectMainDisplay, NULL);
	if (AllModes)
	{
		int32 NumModes = CFArrayGetCount(AllModes);
		for (int32 Index = 0; Index < NumModes; Index++)
		{
			CGDisplayModeRef Mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(AllModes, Index);
			SIZE_T Width = CGDisplayModeGetWidth(Mode) / Scale;
			SIZE_T Height = CGDisplayModeGetHeight(Mode) / Scale;
			int32 RefreshRate = (int32)CGDisplayModeGetRefreshRate(Mode);

			if (((int32)Width >= MinAllowableResolutionX) &&
				((int32)Width <= MaxAllowableResolutionX) &&
				((int32)Height >= MinAllowableResolutionY) &&
				((int32)Height <= MaxAllowableResolutionY)
				)
			{
				bool bAddIt = true;
				if (bIgnoreRefreshRate == false)
				{
					if (RefreshRate < MinAllowableRefreshRate || RefreshRate > MaxAllowableRefreshRate)
					{
						continue;
					}
				}
				else
				{
					// See if it is in the list already
					for (int32 CheckIndex = 0; CheckIndex < Resolutions.Num(); CheckIndex++)
					{
						FScreenResolutionRHI& CheckResolution = Resolutions[CheckIndex];
						if ((CheckResolution.Width == Width) &&
							(CheckResolution.Height == Height))
						{
							// Already in the list...
							bAddIt = false;
							break;
						}
					}
				}

				if (bAddIt)
				{
					// Add the mode to the list
					int32 Temp2Index = Resolutions.AddZeroed();
					FScreenResolutionRHI& ScreenResolution = Resolutions[Temp2Index];

					ScreenResolution.Width = Width;
					ScreenResolution.Height = Height;
					ScreenResolution.RefreshRate = RefreshRate;
				}
			}
		}

		CFRelease(AllModes);
	}

	return true;
}

static CGDisplayModeRef GDesktopDisplayMode = NULL;

void PlatformRestoreDesktopDisplayMode()
{
	if (GDesktopDisplayMode)
	{
		CGDisplaySetDisplayMode(kCGDirectMainDisplay, GDesktopDisplayMode, NULL);
		CGDisplayModeRelease(GDesktopDisplayMode);
		GDesktopDisplayMode = NULL;
	}
}

bool PlatformInitOpenGL()
{
	if (!GDesktopDisplayMode)
	{
		GDesktopDisplayMode = CGDisplayCopyDisplayMode(kCGDirectMainDisplay);
	}
	return true;
}

bool PlatformOpenGLContextValid()
{
	return( [NSOpenGLContext currentContext] != NULL );
}

int32 PlatformGlGetError()
{
	return glGetError();
}

void PlatformGetBackbufferDimensions( uint32& OutWidth, uint32& OutHeight )
{
	SCOPED_AUTORELEASE_POOL;

	OutWidth = OutHeight = 0;
	NSOpenGLContext* Context = [NSOpenGLContext currentContext];
	if( Context )
	{
		NSView* View = [Context view];
		if( View )
		{
			// I assume here that kCGLCESurfaceBackingSize is not used
			NSRect Frame = [View frame];
			OutWidth = (uint32)Frame.size.width;
			OutHeight = (uint32)Frame.size.height;
		}
	}
}

// =============================================================

struct FOpenGLReleasedQuery
{
	NSOpenGLContext*	Context;
	GLuint				Query;
};

static TArray<FOpenGLReleasedQuery>	ReleasedQueries;
static FCriticalSection*			ReleasedQueriesGuard;

void PlatformGetNewRenderQuery( GLuint* OutQuery, uint64* OutQueryContext )
{
	if( !ReleasedQueriesGuard )
	{
		ReleasedQueriesGuard = new FCriticalSection;
	}

	{
		FScopeLock Lock(ReleasedQueriesGuard);

#ifdef UE_BUILD_DEBUG
		check( OutQuery && OutQueryContext );
#endif

		SCOPED_AUTORELEASE_POOL;

		NSOpenGLContext* Context = [NSOpenGLContext currentContext];
		check( Context );

		GLuint NewQuery = 0;
		
		if(GIsEmulatingTimestamp)
		{
			CGLContextObj Current = CGLGetCurrentContext();
			FPlatformOpenGLContext* PlatformContext = nullptr;
			if(Current && (CGLGetParameter(Current, kCGLCPClientStorage, (GLint *)&PlatformContext) == kCGLNoError) && PlatformContext)
			{
				for( int32 Index = 0; Index < ReleasedQueries.Num(); ++Index )
				{
					if( ReleasedQueries[Index].Context == Context )
					{
						PlatformContext->EmulatedQueries->DeleteQuery(ReleasedQueries[Index].Query);
						ReleasedQueries.RemoveAt(Index);
						Index--;
					}
				}
				
				NewQuery = PlatformContext->EmulatedQueries->CreateQuery();
			}
		}
		else
		{
			// Check for possible query reuse
			const int32 ArraySize = ReleasedQueries.Num();
			for( int32 Index = 0; Index < ArraySize; ++Index )
			{
				if( ReleasedQueries[Index].Context == Context )
				{
					NewQuery = ReleasedQueries[Index].Query;
					ReleasedQueries.RemoveAtSwap(Index);
					break;
				}
			}

			if( !NewQuery )
			{
				glGenQueries( 1, &NewQuery );
			}
		}

		*OutQuery = NewQuery;
		*OutQueryContext = (uint64)Context;
	}
}

void PlatformReleaseRenderQuery( GLuint Query, uint64 QueryContext )
{
	SCOPED_AUTORELEASE_POOL;

	NSOpenGLContext* Context = [NSOpenGLContext currentContext];
	if( (uint64)Context == QueryContext )
	{
		if(GIsEmulatingTimestamp)
		{
			CGLContextObj Current = CGLGetCurrentContext();
			FPlatformOpenGLContext* PlatformContext = nullptr;
			if(Current && (CGLGetParameter(Current, kCGLCPClientStorage, (GLint *)&PlatformContext) == kCGLNoError) && PlatformContext)
			{
				PlatformContext->EmulatedQueries->DeleteQuery(Query);
			}
		}
		else
		{
			glDeleteQueries(1, &Query );
		}
	}
	else
	{
		FScopeLock Lock(ReleasedQueriesGuard);
#ifdef UE_BUILD_DEBUG
		check( Query && QueryContext && ReleasedQueriesGuard );
#endif
		FOpenGLReleasedQuery ReleasedQuery;
		ReleasedQuery.Context = (NSOpenGLContext*)QueryContext;
		ReleasedQuery.Query = Query;
		ReleasedQueries.Add(ReleasedQuery);
	}
}

void DeleteQueriesForCurrentContext( NSOpenGLContext* Context )
{
	if( !ReleasedQueriesGuard )
	{
		ReleasedQueriesGuard = new FCriticalSection;
	}
	
	if(GIsEmulatingTimestamp)
	{
		CGLContextObj Current = (CGLContextObj)[Context CGLContextObj];
		FPlatformOpenGLContext* PlatformContext = nullptr;
		if(Current && (CGLGetParameter(Current, kCGLCPClientStorage, (GLint *)&PlatformContext) == kCGLNoError) && PlatformContext)
		{
			FScopeLock Lock(ReleasedQueriesGuard);
			for( int32 Index = 0; Index < ReleasedQueries.Num(); ++Index )
			{
				if( ReleasedQueries[Index].Context == Context )
				{
					PlatformContext->EmulatedQueries->DeleteQuery(ReleasedQueries[Index].Query);
					ReleasedQueries.RemoveAtSwap(Index);
					--Index;
				}
			}
		}
	}
	else
	{
		FScopeLock Lock(ReleasedQueriesGuard);
		for( int32 Index = 0; Index < ReleasedQueries.Num(); ++Index )
		{
			if( ReleasedQueries[Index].Context == Context )
			{
				glDeleteQueries(1,&ReleasedQueries[Index].Query);
				ReleasedQueries.RemoveAtSwap(Index);
				--Index;
			}
		}
	}
}

bool PlatformContextIsCurrent( uint64 QueryContext )
{
	SCOPED_AUTORELEASE_POOL;
	return (uint64)[NSOpenGLContext currentContext] == QueryContext;
}

FRHITexture* PlatformCreateBuiltinBackBuffer(FOpenGLDynamicRHI* OpenGLRHI, uint32 SizeX, uint32 SizeY)
{
	return NULL;
}

bool FMacOpenGL::bUsingApitrace = false;
bool FMacOpenGL::bSupportsTextureCubeMapArray = false;
PFNGLTEXSTORAGE2DPROC FMacOpenGL::glTexStorage2D = NULL;
PFNGLTEXSTORAGE3DPROC FMacOpenGL::glTexStorage3D = NULL;
bool FMacOpenGL::bSupportsDrawBuffersBlend = false;
PFNGLBLENDFUNCSEPARATEIARBPROC FMacOpenGL::glBlendFuncSeparatei = NULL;
PFNGLBLENDEQUATIONSEPARATEIARBPROC FMacOpenGL::glBlendEquationSeparatei = NULL;
PFNGLBLENDFUNCIARBPROC FMacOpenGL::glBlendFunci = NULL;
PFNGLBLENDEQUATIONIARBPROC FMacOpenGL::glBlendEquationi = NULL;
PFNGLLABELOBJECTEXTPROC FMacOpenGL::glLabelObjectEXT = NULL;
PFNGLPUSHGROUPMARKEREXTPROC FMacOpenGL::glPushGroupMarkerEXT = NULL;
PFNGLPOPGROUPMARKEREXTPROC FMacOpenGL::glPopGroupMarkerEXT = NULL;
PFNGLPATCHPARAMETERIPROC FMacOpenGL::glPatchParameteri = NULL;
bool FMacOpenGL::bSupportsDrawIndirect = false;
PFNGLDRAWARRAYSINDIRECTPROC FMacOpenGL::glDrawArraysIndirect = nullptr;
PFNGLDRAWELEMENTSINDIRECTPROC FMacOpenGL::glDrawElementsIndirect = nullptr;

uint64 FMacOpenGL::GetVideoMemorySize()
{
	uint64 VideoMemory = 0;
	NSOpenGLContext* NSContext = [NSOpenGLContext currentContext];
	CGLContextObj Context = NSContext ? (CGLContextObj)[NSContext CGLContextObj] : NULL;
	if(Context)
	{
		GLint VirtualScreen = [NSContext currentVirtualScreen];
		GLint RendererID = 0;
		GLint DisplayMask = 0;
		
		CGLPixelFormatObj ContextPixelFormat = CGLGetPixelFormat(Context);
		
		if(ContextPixelFormat && CGLDescribePixelFormat(ContextPixelFormat, VirtualScreen, kCGLPFADisplayMask, &DisplayMask) == kCGLNoError
		   && CGLGetParameter(Context, kCGLCPCurrentRendererID, &RendererID) == kCGLNoError)
		{
			// Get renderer info for all renderers that match the display mask.
			GLint Num = 0;
			CGLRendererInfoObj Renderer;
			
			CGLQueryRendererInfo((GLuint)DisplayMask, &Renderer, &Num);
			
			for (GLint i = 0; i < Num; i++)
			{
				GLint ThisRendererID = 0;
				CGLDescribeRenderer(Renderer, i, kCGLRPRendererID, &ThisRendererID);
				
				// See if this is the one we want
				if (ThisRendererID == RendererID)
				{
					GLint MemoryMB = 0;
					CGLDescribeRenderer(Renderer, i, kCGLRPVideoMemoryMegabytes, (GLint*)&MemoryMB);
					VideoMemory = (uint64)MemoryMB * 1024llu * 1024llu;
					break;
				}
			}
			
			CGLDestroyRendererInfo(Renderer);
		}
	}
	return VideoMemory;
}

void FMacOpenGL::ProcessQueryGLInt()
{
	GET_GL_INT(GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS, 0, MaxHullUniformComponents);
	GET_GL_INT(GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS, 0, MaxDomainUniformComponents);
}

void FMacOpenGL::ProcessExtensions(const FString& ExtensionsString)
{
	// Get the Vendor ID by parsing the renderer string
	FString VendorName( ANSI_TO_TCHAR((const ANSICHAR*)glGetString(GL_VENDOR)));
	if (VendorName.Contains(TEXT("Intel")))
	{
		GRHIVendorId = 0x8086;
	}
	else if (VendorName.Contains(TEXT("NVIDIA")))
	{
		GRHIVendorId = 0x10DE;
	}
	else if (VendorName.Contains(TEXT("ATi")) || VendorName.Contains(TEXT("AMD")))
	{
		GRHIVendorId = 0x1002;
	}
	
	if(GRHIVendorId == 0)
	{
		// Get the current renderer ID
		CGLContextObj Current = CGLGetCurrentContext();
		GLint RendererID = 0;
		CGLError Error = CGLGetParameter(Current, kCGLCPCurrentRendererID, &RendererID);
		if(Error == kCGLNoError)
		{
			switch(RendererID & 0x000ff000)
			{
				case 0x00021000:
				{
					GRHIVendorId = 0x1002;
					break;
				}
				case 0x00022000:
				{
					GRHIVendorId = 0x10DE;
					break;
				}
				case 0x00024000:
				{
					GRHIVendorId = 0x8086;
					break;
				}
				default:
				{
					// Unknown GPU vendor - assuming Intel!
					GRHIVendorId = 0x8086;
					break;
				}
			}
		}
	}
	check(GRHIVendorId != 0);
	
	ProcessQueryGLInt();
	FOpenGL3::ProcessExtensions(ExtensionsString);
	
	// Check for Apitrace, which doesn't understand some Apple extensions.
	uint32 ModuleCount = _dyld_image_count();
	for(uint32 Index = 0; Index < ModuleCount; Index++)
	{
		ANSICHAR const* ModulePath = (ANSICHAR const*)_dyld_get_image_name(Index);
		if(FCStringAnsi::Strstr(ModulePath, "OpenGL.framework/Versions/A/OpenGL") && FCStringAnsi::Strcmp(ModulePath, "/System/Library/Frameworks/OpenGL.framework/Versions/A/OpenGL"))
		{
			bUsingApitrace = true;
			break;
		}
	}
	
	if(GIsEmulatingTimestamp)
	{
		TimestampQueryBits = 64;
	}
	
	// Not all GPUs support the new 4.1 Core Profile required for GL_ARB_texture_storage
	// Those that are stuck with 3.2 don't have this extension.
	if(ExtensionsString.Contains(TEXT("GL_ARB_texture_storage")))
	{
		glTexStorage2D = (PFNGLTEXSTORAGE2DPROC)dlsym(RTLD_SELF, "glTexStorage2D");
		glTexStorage3D = (PFNGLTEXSTORAGE3DPROC)dlsym(RTLD_SELF, "glTexStorage3D");
	}
	
	bSupportsTextureCubeMapArray = ExtensionsString.Contains(TEXT("GL_ARB_texture_cube_map_array"));
	
	// Not all GPUs support the new 4.1 Core Profile required for GL_ARB_draw_buffers_blend
	// Those that are stuck with 3.2 don't have this extension.
	if(ExtensionsString.Contains(TEXT("GL_ARB_draw_buffers_blend")))
	{
		bSupportsDrawBuffersBlend = true;
		glBlendFuncSeparatei = (PFNGLBLENDFUNCSEPARATEIARBPROC)dlsym(RTLD_SELF, "glBlendFuncSeparatei");
		glBlendEquationSeparatei = (PFNGLBLENDEQUATIONSEPARATEIARBPROC)dlsym(RTLD_SELF, "glBlendEquationSeparatei");
		glBlendFunci = (PFNGLBLENDFUNCIARBPROC)dlsym(RTLD_SELF, "glBlendFunci");
		glBlendEquationi = (PFNGLBLENDEQUATIONIARBPROC)dlsym(RTLD_SELF, "glBlendEquationi");
	}
	
	// Don't label objects with MTGL, it causes synchronisation of the MTGL thread.
	if(ExtensionsString.Contains(TEXT("GL_EXT_debug_label")) && !GMacUseMTGL)
	{
		glLabelObjectEXT = (PFNGLLABELOBJECTEXTPROC)dlsym(RTLD_SELF, "glLabelObjectEXT");
	}
	
	if(ExtensionsString.Contains(TEXT("GL_EXT_debug_marker")))
	{
		glPushGroupMarkerEXT = (PFNGLPUSHGROUPMARKEREXTPROC)dlsym(RTLD_SELF, "glPushGroupMarkerEXT");
		glPopGroupMarkerEXT = (PFNGLPOPGROUPMARKEREXTPROC)dlsym(RTLD_SELF, "glPopGroupMarkerEXT");
#if !UE_BUILD_SHIPPING // For debuggable builds emit draw events when the extension is GL_EXT_debug_marker present.
        GEmitDrawEvents = !FParse::Param(FCommandLine::Get(), TEXT("DisableMacDrawEvents"));
#endif
	}
	
	if(ExtensionsString.Contains(TEXT("GL_ARB_tessellation_shader")))
	{
		glPatchParameteri = (PFNGLPATCHPARAMETERIPROC)dlsym(RTLD_SELF, "glPatchParameteri");
	}
	
	if(ExtensionsString.Contains(TEXT("GL_ARB_draw_indirect")))
	{
		bSupportsDrawIndirect = true;
		glDrawArraysIndirect = (PFNGLDRAWARRAYSINDIRECTPROC)dlsym(RTLD_SELF, "glDrawArraysIndirect");
		glDrawElementsIndirect = (PFNGLDRAWELEMENTSINDIRECTPROC)dlsym(RTLD_SELF, "glDrawElementsIndirect");
	}
	
	if(FMacPlatformMisc::MacOSXVersionCompare(10,10,1) < 0)
	{
		GMacMustFlushTexStorage = ((FPlatformMisc::IsRunningOnMavericks() && IsRHIDeviceNVIDIA()) || GMacUseMTGL);
	}
	
	// SSOs require structs in geometry shaders - which only work in 10.10.0 and later
	bSupportsSeparateShaderObjects &= (FMacPlatformMisc::MacOSXVersionCompare(10,10,0) >= 0);
}

void FMacOpenGL::MacQueryTimestampCounter(GLuint QueryID)
{
	if(GIsEmulatingTimestamp)
	{
		CGLContextObj Current = CGLGetCurrentContext();
		FPlatformOpenGLContext* PlatformContext = nullptr;
		if(Current && (CGLGetParameter(Current, kCGLCPClientStorage, (GLint *)&PlatformContext) == kCGLNoError) && PlatformContext)
		{
			TSharedPtr<FMacOpenGLQuery> Query = PlatformContext->EmulatedQueries->GetQuery(QueryID);
			if(Query.IsValid())
			{
				Query->Begin(GL_TIMESTAMP);
				Query->End();
			}
		}
	}
	else
	{
		FOpenGL3::QueryTimestampCounter(QueryID);
	}
}

void FMacOpenGL::MacBeginQuery(GLenum QueryType, GLuint QueryId)
{
	if(GIsEmulatingTimestamp)
	{
		CGLContextObj Current = CGLGetCurrentContext();
		FPlatformOpenGLContext* PlatformContext = nullptr;
		if(Current && (CGLGetParameter(Current, kCGLCPClientStorage, (GLint *)&PlatformContext) == kCGLNoError) && PlatformContext)
		{
			PlatformContext->EmulatedQueries->BeginQuery(QueryType, QueryId);
		}
	}
	else
	{
		FOpenGL3::BeginQuery(QueryType, QueryId);
	}
}

void FMacOpenGL::MacEndQuery(GLenum QueryType)
{
	if(GIsEmulatingTimestamp)
	{
		CGLContextObj Current = CGLGetCurrentContext();
		FPlatformOpenGLContext* PlatformContext = nullptr;
		if(Current && (CGLGetParameter(Current, kCGLCPClientStorage, (GLint *)&PlatformContext) == kCGLNoError) && PlatformContext)
		{
			PlatformContext->EmulatedQueries->EndQuery(QueryType);
		}
	}
	else
	{
		FOpenGL3::EndQuery(QueryType);
	}
}

void FMacOpenGL::MacGetQueryObject(GLuint QueryId, EQueryMode QueryMode, uint64 *OutResult)
{
	if(GIsEmulatingTimestamp)
	{
		CGLContextObj Current = CGLGetCurrentContext();
		FPlatformOpenGLContext* PlatformContext = nullptr;
		if(Current && (CGLGetParameter(Current, kCGLCPClientStorage, (GLint *)&PlatformContext) == kCGLNoError) && PlatformContext)
		{
			TSharedPtr<FMacOpenGLQuery> Query = PlatformContext->EmulatedQueries->GetQuery(QueryId);
			if(Query.IsValid())
			{
				if(QueryMode == QM_Result)
				{
					*OutResult = Query->GetResult();
				}
				else
				{
					*OutResult = Query->GetResultAvailable();
				}
			}
		}
	}
	else
	{
		FOpenGL3::GetQueryObject(QueryId, QueryMode, OutResult);
	}
}

void FMacOpenGL::MacGetQueryObject(GLuint QueryId, EQueryMode QueryMode, GLuint *OutResult)
{
	if(GIsEmulatingTimestamp)
	{
		CGLContextObj Current = CGLGetCurrentContext();
		FPlatformOpenGLContext* PlatformContext = nullptr;
		if(Current && (CGLGetParameter(Current, kCGLCPClientStorage, (GLint *)&PlatformContext) == kCGLNoError) && PlatformContext)
		{
			TSharedPtr<FMacOpenGLQuery> Query = PlatformContext->EmulatedQueries->GetQuery(QueryId);
			if(Query.IsValid())
			{
				if(QueryMode == QM_Result)
				{
					*OutResult = (GLuint)Query->GetResult();
				}
				else
				{
					*OutResult = (GLuint)Query->GetResultAvailable();
				}
			}
		}
	}
	else
	{
		FOpenGL3::GetQueryObject(QueryId, QueryMode, OutResult);
	}
}

bool FMacOpenGL::MustFlushTexStorage(void)
{
	// @todo There is a bug in Apple's GL with TexStorage calls when using MTGL that can see the texture never be created, which then subsequently causes crashes
	// @todo This bug also affects Nvidia cards under Mavericks without MTGL.
	// @todo Fixed in 10.10.1.
	FPlatformOpenGLContext::VerifyCurrentContext();
	return GMacFlushTexStorage || GMacMustFlushTexStorage;
}

/** Is the current renderer the Intel HD3000? */
bool FMacOpenGL::IsIntelHD3000()
{
	// Get the current renderer ID
	GLint RendererID = 0;
	CGLContextObj Current = CGLGetCurrentContext();
	if (Current)
	{
		CGLError Error = CGLGetParameter(Current, kCGLCPCurrentRendererID, &RendererID);
		check(Error == kCGLNoError && RendererID != 0);
	}
	return (RendererID & kCGLRendererIDMatchingMask) == kCGLRendererIntelHDID;
}

void FMacOpenGL::DeleteTextures(GLsizei Number, const GLuint* Textures)
{
	TArray<GLuint>& TexturesToDelete = GMacTexturesToDelete[GFrameNumberRenderThread % GMacTexturePoolNum];
	TexturesToDelete.Append(Textures, Number);
}

void FMacOpenGL::BufferSubData(GLenum Target, GLintptr Offset, GLsizeiptr Size, const GLvoid* Data)
{
	// @todo The crash that caused UE-4772 is no longer occuring for me on 10.10.1, so use glBufferSubData from that revision onward.
	static bool const bNeedsMTLGMapBuffer = (FMacPlatformMisc::MacOSXVersionCompare(10,10,1) < 0);
	if(GMacUseMTGL && bNeedsMTLGMapBuffer)
	{
		void* Dest = MapBufferRange(Target, Offset, Size, FOpenGLBase::RLM_WriteOnly);
		FMemory::Memcpy(Dest, Data, Size);
		UnmapBuffer(Target);
	}
	else
	{
		glBufferSubData(Target, Offset, Size, Data);
	}
}

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "StandaloneRendererPrivate.h"
#include "OpenGL/SlateOpenGLRenderer.h"

#include "SlateOpenGLMac.h"

#include "MacWindow.h"
#include "MacTextInputMethodSystem.h"
#include "CocoaTextView.h"
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>

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

@end

@implementation FSlateCocoaView

- (CALayer*)makeBackingLayer
{
	return [[FSlateOpenGLLayer alloc] initWithContext:self.Context andPixelFormat:self.PixelFormat];
}

- (id)initWithFrame:(NSRect)frameRect context:(NSOpenGLContext*)context pixelFormat:(NSOpenGLPixelFormat*)pixelFormat
{
	self = [super initWithFrame:frameRect];
	if (self)
	{
		self.Context = context;
		self.PixelFormat = pixelFormat;
		Framebuffer = 0;
		Renderbuffer = 0;
	}
	return self;
}

-(void)dealloc
{
	[super dealloc];
}

- (void)drawRect:(NSRect)dirtyRect
{
	if (Framebuffer && [(FCocoaWindow*)[self window] isRenderInitialized] && ViewportRect.IsValid())
	{
		int32 CurrentReadFramebuffer = 0;
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &CurrentReadFramebuffer);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, Framebuffer);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glBlitFramebuffer(0, 0, ViewportRect.Right, ViewportRect.Bottom, 0, 0, self.frame.size.width, self.frame.size.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		CHECK_GL_ERRORS;
		glBindFramebuffer(GL_READ_FRAMEBUFFER, CurrentReadFramebuffer);
	}
	else
	{
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}
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

static void MacOpenGLContextReconfigurationCallBack(CGDirectDisplayID Display, CGDisplayChangeSummaryFlags Flags, void* UserInfo)
{
	FSlateOpenGLContext* Context = (FSlateOpenGLContext*)UserInfo;
	if (Context)
	{
		Context->bNeedsUpdate = true;
	}
}

FSlateOpenGLContext::FSlateOpenGLContext()
:	View(NULL)
,	PixelFormat(NULL)
,	Context(NULL)
,	bNeedsUpdate(false)
{
}

FSlateOpenGLContext::~FSlateOpenGLContext()
{
	Destroy();
}

void FSlateOpenGLContext::Initialize(void* InWindow, const FSlateOpenGLContext* SharedContext)
{
	NSOpenGLPixelFormatAttribute Attributes[] =
	{
		kCGLPFAAccelerated,
		kCGLPFANoRecovery,
		kCGLPFASupportsAutomaticGraphicsSwitching,
		kCGLPFADoubleBuffer,
		kCGLPFAColorSize,
		32,
		0
	};

	PixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:Attributes];
	Context = [[NSOpenGLContext alloc] initWithFormat:PixelFormat shareContext:SharedContext ? SharedContext->Context : NULL];

	NSWindow* Window = (NSWindow*)InWindow;
	if (Window)
	{
		const NSRect ViewRect = NSMakeRect(0, 0, Window.frame.size.width, Window.frame.size.height);
		View = [[FSlateCocoaView alloc] initWithFrame:ViewRect context:Context pixelFormat:PixelFormat];
		[View setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

		if (FPlatformMisc::IsRunningOnMavericks() && ([Window styleMask] & NSTexturedBackgroundWindowMask))
		{
			NSView* SuperView = [[Window contentView] superview];
			[SuperView addSubview:View];
			[SuperView setWantsLayer:YES];
			[SuperView addSubview:[Window standardWindowButton:NSWindowCloseButton]];
			[SuperView addSubview:[Window standardWindowButton:NSWindowMiniaturizeButton]];
			[SuperView addSubview:[Window standardWindowButton:NSWindowZoomButton]];
		}
		else
		{
			[View setWantsLayer:YES];
			[Window setContentView:View];
		}

		[[Window standardWindowButton:NSWindowCloseButton] setAction:@selector(performClose:)];

		View.layer.magnificationFilter = kCAFilterNearest;
		View.layer.minificationFilter = kCAFilterNearest;
	}

	[Context update];
	MakeCurrent();

	CGDisplayRegisterReconfigurationCallback(&MacOpenGLContextReconfigurationCallBack, this);
}

void FSlateOpenGLContext::Destroy()
{
	if (View)
	{
		NSWindow* Window = [View window];
		if (Window)
		{
			[Window setContentView:NULL];
		}
		
		NSOpenGLContext* Current = [NSOpenGLContext currentContext];
		[Context makeCurrentContext];
		FSlateCocoaView* SlateView = ((FSlateCocoaView*)View);
		if (SlateView->Framebuffer)
		{
			glDeleteFramebuffers(1, &SlateView->Framebuffer);
			SlateView->Framebuffer = 0;
		}
		if (SlateView->Renderbuffer)
		{
			glDeleteRenderbuffers(1, &SlateView->Renderbuffer);
			SlateView->Renderbuffer = 0;
		}
		[Current makeCurrentContext];
		
		[View release];
		View = NULL;

		// PixelFormat and Context are released by View
		CGDisplayRemoveReconfigurationCallback(&MacOpenGLContextReconfigurationCallBack, this);

		[PixelFormat release];
		[Context clearDrawable];
		[Context release];
		PixelFormat = NULL;
		Context = NULL;
	}
}

void FSlateOpenGLContext::MakeCurrent()
{
	if (bNeedsUpdate)
	{
		[Context update];
		bNeedsUpdate = false;
	}
	[Context makeCurrentContext];
}

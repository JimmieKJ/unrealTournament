// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "StandaloneRendererPrivate.h"

#include <OpenGL/gl.h>
#include <OpenGL/glext.h>

#include "OpenGL/SlateOpenGLRenderer.h"

#include "MacWindow.h"
#include "MacTextInputMethodSystem.h"
#include "CocoaTextView.h"

char const* const CompositedBlitVertexShader = "#version 120\n"
"#extension GL_EXT_gpu_shader4 : require\n"
"const int VertexCount = 6;\n"
"uniform int TextureDirection;\n"
"attribute vec2 InPosition;\n"
"attribute vec2 InTexCoord;\n"
"varying vec2 TexCoord;\n"
"void main()\n"
"{\n"
"	TexCoord = InTexCoord;\n"
"	if(TextureDirection == 1){ TexCoord.x = (TextureDirection - InTexCoord.x); }\n"
"	gl_Position = vec4(InPosition, 0.0, 1.0);\n"
"}\n";

char const* const CompositedBlitFragmentShader = "#version 120\n"
"uniform sampler2D WindowTexture;\n"
"varying vec2 TexCoord;\n"
"void main()\n"
"{\n"
"	vec4 WindowColor = texture2D(WindowTexture, TexCoord);\n"
"	gl_FragColor = vec4(WindowColor.x, WindowColor.y, WindowColor.z, WindowColor.x);\n"
"}\n";

@interface NSView(NSThemeFramePrivate)
- (float)roundedCornerRadius;
@end

/**
 * Custom view class used to provide window OpenGL-enabled content view.
 */
@interface FSlateCocoaView : FCocoaTextView
{
	@private

	NSOpenGLContext		*Context;
	NSOpenGLPixelFormat	*PixelFormat;
}

/** Creates a pixel format and OpenGL context */
- (id)initWithFrame:(NSRect)FrameRect context:(NSOpenGLContext *)SharedContext;
/** Destructor */
- (void)dealloc;
/** Accessors */
- (NSOpenGLContext *)openGLContext;
- (NSOpenGLPixelFormat*)pixelFormat;

@end

/**
 * Custom view class used to provide window OpenGL-enabled content view.
 */
@implementation FSlateCocoaView

- (id)initWithFrame:(NSRect)FrameRect context:(NSOpenGLContext *)SharedContext
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

	PixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes: Attributes];
	if (PixelFormat)
	{
		Context = [[NSOpenGLContext alloc] initWithFormat: PixelFormat shareContext: SharedContext];

		if (Context)
		{
			// Setup Opacity - can't change it dynamically later!
			int32 SurfaceOpacity = 0;
			[Context setValues: &SurfaceOpacity forParameter: NSOpenGLCPSurfaceOpacity];
			
			self = [super initWithFrame: FrameRect];
		}
	}
	
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_surfaceNeedsUpdate:) name:NSViewGlobalFrameDidChangeNotification object:self];

	return self;
}

- (void)dealloc
{
	[[NSNotificationCenter defaultCenter] removeObserver:self name:NSViewGlobalFrameDidChangeNotification object:self];
	
	if (Context)
	{
		[Context release];
	}

	if (PixelFormat)
	{
		[PixelFormat release];
	}

	[super dealloc];
}

- (NSOpenGLContext *)openGLContext
{
	return Context;
}

- (NSOpenGLPixelFormat*)pixelFormat
{
	return PixelFormat;
}

- (void)renewGState
{
	extern bool GMacEnableCocoaScreenUpdates;
	if(GMacEnableCocoaScreenUpdates)
	{
		GMacEnableCocoaScreenUpdates = false;
		NSDisableScreenUpdates();
	}
	
	[super renewGState];
}

- (void) _surfaceNeedsUpdate:(NSNotification*)notification
{
	if(Context)
	{
		[Context update];
	}
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
,	CompositeVertexShader(0)
,	CompositeFragmentShader(0)
,	CompositeProgram(0)
,	WindowTextureUniform(0)
,	TextureDirectionUniform(0)
,	CompositeTexture(0)
,	CompositeVAO(0)
,	bNeedsUpdate(false)
{
}

FSlateOpenGLContext::~FSlateOpenGLContext()
{
	Destroy();
}

void FSlateOpenGLContext::Initialize( void* InWindow, const FSlateOpenGLContext* SharedContext )
{
	NSWindow* Window = (NSWindow*)InWindow;

	const int32 Width = Window ? [Window frame].size.width : 10;
	const int32 Height = Window ? [Window frame].size.height : 10;
	const NSRect ViewRect = NSMakeRect(0, 0, Width, Height);

	FSlateCocoaView *View = [[FSlateCocoaView alloc] initWithFrame: ViewRect context: SharedContext ? SharedContext->Context : NULL];

	// Attach the view to the window
	if(Window)
	{
		// Slate windows may require a view that fills the entire window & border frame, not just content
		if([Window styleMask] & (NSTexturedBackgroundWindowMask))
		{
			// For windows where we want to hide the titlebar add the view as the uppermost child of the
			// window's superview.
			NSView* SuperView = [[Window contentView] superview];
			
			[View setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
			
			[SuperView addSubview:View];
		}
		else
		{
			// Otherwise, set as the content view to see the title bar.
			[Window setContentView: View];
		}
	}

	Context = [View openGLContext];
	[Context setView: View];
	[Context update];

	MakeCurrent();
	
	if([Window styleMask] & (NSTexturedBackgroundWindowMask))
	{
		// For windows where we want to hide the titlebar add the view as the uppermost child of the
		// window's superview.
		NSView* SuperView = [[Window contentView] superview];
		CompositeVertexShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(CompositeVertexShader, 1, &CompositedBlitVertexShader, NULL);
		glCompileShader(CompositeVertexShader);
		check(glGetError() == GL_NO_ERROR);
		
		CompositeFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(CompositeFragmentShader, 1, &CompositedBlitFragmentShader, NULL);
		glCompileShader(CompositeFragmentShader);
		check(glGetError() == GL_NO_ERROR);
		
		CompositeProgram = glCreateProgram();
		glAttachShader(CompositeProgram, CompositeVertexShader);
		glAttachShader(CompositeProgram, CompositeFragmentShader);
		glLinkProgram(CompositeProgram);
		check(glGetError() == GL_NO_ERROR);
		glValidateProgram(CompositeProgram);
		check(glGetError() == GL_NO_ERROR);
		WindowTextureUniform = glGetUniformLocation(CompositeProgram, "WindowTexture");
		check(glGetError() == GL_NO_ERROR);
		TextureDirectionUniform = glGetUniformLocation(CompositeProgram, "TextureDirection");
		check(glGetError() == GL_NO_ERROR);
		
		glGenVertexArraysAPPLE(1, &CompositeVAO);
		
		glGenTextures(1, &CompositeTexture);
		{
			glActiveTexture(GL_TEXTURE0);
			glBindTexture (GL_TEXTURE_2D, CompositeTexture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			uint32 Size = 32;
			NSImage* MaskImage = [[[NSImage alloc] initWithSize:NSMakeSize(Size*2, Size*2)] autorelease];
			{
				[MaskImage lockFocus];
				{
					NSGraphicsContext* CurrentContext = [NSGraphicsContext currentContext];
					[CurrentContext saveGraphicsState];
					[CurrentContext setShouldAntialias: NO];
					
					[[NSColor clearColor] set];
					[[NSBezierPath bezierPathWithRect:NSMakeRect(0.f, 0.f, Size*2, Size*2)] fill];
					
					[[NSColor colorWithDeviceRed:0.0f green:0.0f blue:0.0f alpha:1.0f] set];
					float Radius = [SuperView roundedCornerRadius] * 1.6f;
					[[NSBezierPath bezierPathWithRoundedRect:NSMakeRect(0.f, 0.f, Size*2, Size*2) xRadius: Radius yRadius: Radius] fill];
					
					[CurrentContext restoreGraphicsState];
				}
				[MaskImage unlockFocus];
			}
			
			NSRect SrcRect = NSMakeRect(0, Size, Size, Size);
			NSRect DestRect = NSMakeRect(0, 0, Size, Size);
			NSImage* CornerImage = [[[NSImage alloc] initWithSize:DestRect.size] autorelease];
			{
				[CornerImage lockFocus];
				{
					NSGraphicsContext* CurrentContext = [NSGraphicsContext currentContext];
					[CurrentContext saveGraphicsState];
					
					[[NSColor clearColor] set];
					[[NSBezierPath bezierPathWithRect:DestRect] fill];
					
					[MaskImage drawInRect:DestRect fromRect:SrcRect operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
					
					[CurrentContext restoreGraphicsState];
				}
				[CornerImage unlockFocus];
			}
			
			CGImageRef CGImage = [CornerImage CGImageForProposedRect:nil context:nil hints:nil];
			check(CGImage);
			NSBitmapImageRep* ImageRep = [[[NSBitmapImageRep alloc] initWithCGImage:CGImage] autorelease];
			check(ImageRep);
			
			GLenum format = [ImageRep hasAlpha] ? GL_RGBA : GL_RGB;
			glTexImage2D (GL_TEXTURE_2D, 0, format, [ImageRep size].width, [ImageRep size].height, 0, format, GL_UNSIGNED_BYTE, [ImageRep bitmapData]);

			glBindTexture (GL_TEXTURE_2D, 0);
		}
	}

	CGDisplayRegisterReconfigurationCallback(&MacOpenGLContextReconfigurationCallBack, this);
}

void FSlateOpenGLContext::Destroy()
{
	if( View )
	{
		NSWindow* Window = [View window];
		if( Window )
		{
			[Window setContentView: NULL];
			
			if([Window styleMask] & (NSTexturedBackgroundWindowMask))
			{
				MakeCurrent();
				
				glDeleteTextures(1, &CompositeTexture);
				glDeleteVertexArraysAPPLE(1, &CompositeVAO);
				glDeleteProgram(CompositeProgram);
				glDeleteShader(CompositeVertexShader);
				glDeleteShader(CompositeFragmentShader);
				
				CompositeVertexShader = 0;
				CompositeFragmentShader = 0;
				CompositeProgram = 0;
				WindowTextureUniform = 0;
				TextureDirectionUniform = 0;
				CompositeTexture = 0;
				CompositeVAO = 0;
			}
		}
		[View release];
		View = NULL;

		CGDisplayRemoveReconfigurationCallback(&MacOpenGLContextReconfigurationCallBack, this);

		// PixelFormat and Context are released by View
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

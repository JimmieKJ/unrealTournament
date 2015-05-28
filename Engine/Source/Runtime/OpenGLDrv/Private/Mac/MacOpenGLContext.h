// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

@class FCocoaOpenGLView;
class FMacOpenGLQueryEmu;
@class NSOpenGLContext;
@class NSWindow;

struct FPlatformOpenGLContext
{
	FPlatformOpenGLContext();
	~FPlatformOpenGLContext();

	void Initialise(NSOpenGLContext* const SharedContext);
	void Reset();
	void MakeCurrent();

	static void VerifyCurrentContext();
	static void RegisterGraphicsSwitchingCallback();
	static void UnregisterGraphicsSwitchingCallback();

	/** Underlying OpenGL context and its pixel format. */
	NSOpenGLContext*		OpenGLContext;
	NSOpenGLPixelFormat*	OpenGLPixelFormat;

	// Window Management
	/** Currently bound Cocoa view - nil for offscreen only contexts */
	FCocoaOpenGLView*		OpenGLView;
	/** Owning window - nil for offscreen only contexts */
	NSWindow*				WindowHandle;

	// Mac OpenGL Timestamp Query emulation
	/** Emulated queries */
	FMacOpenGLQueryEmu*		EmulatedQueries;

	// OpenGL Resources
	/** Required vertex array object. */
	GLuint					VertexArrayObject;
	/** Output Framebuffer */
	GLuint					ViewportFramebuffer;
	/** Output render buffer */
	GLuint					ViewportRenderbuffer;
	/** Cached viewport render buffer dimensions */
	int32					ViewportSize[2];

	// Context Settings
	/** Flush Synchronisation Interval (V-Sync) */
	int32					SyncInterval;
	/** Virtual Screen */
	GLint					VirtualScreen;

	// Context Renderer Details
	/** Device Vendor ID */
	uint32					VendorID;
	/** Renderer ID */
	GLint					RendererID;
	/** Whether the renderer supports depth-fetch during depth-test */
	bool					SupportsDepthFetchDuringDepthTest;
};

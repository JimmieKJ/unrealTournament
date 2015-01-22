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
	
	void Initialise( NSOpenGLContext* const SharedContext );
	void Reset(void);
	void MakeCurrent(void);
	
	static void VerifyCurrentContext(void);
	static void RegisterGraphicsSwitchingCallback(void);
	static void UnregisterGraphicsSwitchingCallback(void);
	
	/** Underlying OpenGL context. */
	NSOpenGLContext*	OpenGLContext;

	// Window Management
	/** Currently bound Cocoa view - nil for offscreen only contexts */
	FCocoaOpenGLView*	OpenGLView;
	/** Owning window - nil for offscreen only contexts */
	NSWindow*			WindowHandle;
	
	// Mac OpenGL Timestamp Query emulation
	/** Emulated queries */
	FMacOpenGLQueryEmu*	EmulatedQueries;
	
	// OpenGL Resources
	/** Required vertex array object. */
	GLuint				VertexArrayObject;
	/** Output Framebuffer */
	GLuint				ViewportFramebuffer;
	
	// Context Settings
	/** Flush Synchronisation Interval (V-Sync) */
	int32				SyncInterval;
	/** Virtual Screen */
	GLint				VirtualScreen;
	
	// Context Renderer Details
	/** Device Vendor ID */
	uint32				VendorID;
	/** Renderer ID */
	GLint				RendererID;
	/** Whether the renderer supports depth-fetch during depth-test */
	bool				SupportsDepthFetchDuringDepthTest;
};

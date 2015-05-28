/************************************************************************************

Filename    :   DirectRender.h
Content     :   Handles multi window front buffer setup
Created     :   October 1, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_DirectRender_h
#define OVR_DirectRender_h

// A key aspect of our performance is the ability to render directly
// to a buffer being scanned out for display, instead of going through
// the standard Android swapbuffers path, which adds two or three frames
// of latency to the output.

#include <jni.h>
#include "Android/GlUtils.h"

namespace OVR
{

class VrSurfaceManager
{
public:
			VrSurfaceManager();
			~VrSurfaceManager();

	void	Init( JNIEnv * jni );
	void	Shutdown();

	bool	SetFrontBuffer( const EGLSurface surface, const bool set );
	void *	GetFrontBufferAddress( const EGLSurface surface );

	void *	GetSurfaceBufferAddress( const EGLSurface surface, int attribs[], const int attr_size, const int pitch );
	void *	GetClientBufferAddress( const EGLSurface surface );

private:

	JNIEnv *	env;

	jclass		surfaceClass;
	jmethodID	setFrontBufferID;
	jmethodID	getFrontBufferAddressID;
	jmethodID	getSurfaceBufferAddressID;
	jmethodID	getClientBufferAddressID;
};

class DirectRender
{
public:
			DirectRender();

	// Makes the currently active EGL window surface into a front buffer if possible.
	// Queries all information from EGL so it will work inside Unity as well
	// as our native code.
	// Must be called by the same thread that will be rendering to the surfaces.
	void	InitForCurrentSurface( JNIEnv * jni, bool wantFrontBuffer, int buildVersionSDK );

	// Go back to a normal swapped window.
	void	Shutdown();

	// Conventionally swapped rendering will return false.
	bool	IsFrontBuffer() const;

	// Returns the full resolution as if the screen is in landscape orientation,
	// (1920x1080, etc) even if it is in portrait mode.
	void	GetScreenResolution( int & width, int & height ) const;

	// This starts direct rendering to the front buffer, hopefully using vendor
	// specific extensions to only update half of the window without
	// incurring a tiled renderer pre-read and post-resolve of the unrendered
	// parts.
	// This implicitly sets the scissor rect, which should not be re-set
	// until after EndDirectRendering(), since drivers may use scissor
	// to infer areas to tile.
	void	BeginDirectRendering( int x, int y, int width, int height );

	// This will automatically perform whatever flush is necessary to
	// get the rendering going before returning.
	void	EndDirectRendering() const;

	// We still do a swapbuffers even when rendering to the front buffer,
	// because most tools expect swapbuffer delimited frames.
	// This has no effect when in front buffer mode.
	void	SwapBuffers() const;

	EGLSurface 		windowSurface;		// swapbuffers will be called on this

private:
	bool				wantFrontBuffer;

	VrSurfaceManager	surfaceMgr;

	EGLDisplay			display;
	EGLContext			context;

	// From eglQuerySurface
	int					width;
	int					height;

	bool				gvrFrontbufferExtension;
};


}	// namespace OVR

#endif	// OVR_Direct_render


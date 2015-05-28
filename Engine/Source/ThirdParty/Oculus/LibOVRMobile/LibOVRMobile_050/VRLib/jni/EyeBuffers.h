/************************************************************************************

Filename    :   EyeBuffers.h
Content     :   Handling of different eye buffer formats
Created     :   March 7, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_EyeBuffers_h
#define OVR_EyeBuffers_h

#include <jni.h>

#include "Android/GlUtils.h"		// GLuint
#include "Android/LogUtils.h"

namespace OVR {


enum colorFormat_t
{
	COLOR_565,
	COLOR_5551,		// single bit alpha useful for overlay planes
	COLOR_4444,
	COLOR_8888,
	COLOR_8888_sRGB
	// We may eventually want high dynamic range and MRT formats.

	// Should we include an option for getting a full mip
	// chain on the frame buffer object color buffer instead of
	// just a single level, allowing the application to call
	// glGenerateMipmap for distortion effects or use supersampling
	// for the final presentation?
	// This would not be possible for ANativeWindow based surfaces.
};

enum depthFormat_t
{
	DEPTH_0,		// useful for overlay planes
	DEPTH_16,
	DEPTH_24,
	DEPTH_24_STENCIL_8

	// For dimshadow rendering, we would need an option to
	// make the depth buffer a texture instead of a renderBufferStorage,
	// which requires an extension on Gl ES 2.0, but would be fine on 3.0.
	// It would also require a flag to allow the resolve of depth instead
	// of invalidating it.
};

enum textureFilter_t
{
	TEXTURE_FILTER_NEAREST,		// Causes significant aliasing, only for performance testing.
	TEXTURE_FILTER_BILINEAR,	// This should be used under almost all circumstances.
	TEXTURE_FILTER_ANISO_2,		// Anisotropic filtering can in some cases reduce aliasing.
	TEXTURE_FILTER_ANISO_4
};

struct EyeParms
{
		EyeParms() :
			resolution( 1024 ),
			WidthScale( 1 ),
			multisamples( 2 ),
			colorFormat( COLOR_8888 ),
			depthFormat( DEPTH_24 ),
			textureFilter( TEXTURE_FILTER_BILINEAR )
		{
		}

	// Setting the resolution higher than necessary will cause aliasing
	// when presented to the screen, since we do not currently generate
	// mipmaps for the eye buffers, but lowering the resolution can
	// significantly improve the application frame rate.
	int					resolution;

	// For double wide UE4
	int					WidthScale;

	// Multisample anti-aliasing is almost always desirable for VR, even
	// if it requires a drop in resolution.
	int					multisamples;

	// 16 bit color eye buffers can improve performance noticeably, but any
	// dithering effects will be distorted by the warp to screen.
	//
	// Defaults to FMT_8888.
	colorFormat_t		colorFormat;

	// Adreno and Tegra benefit from 16 bit depth buffers
	depthFormat_t		depthFormat;

	// Determines how the time warp samples the eye buffers.
	// Defaults to TEXTURE_FILTER_BILINEAR.
	textureFilter_t		textureFilter;
};

enum multisample_t
{
	MSAA_OFF,
	MSAA_RENDER_TO_TEXTURE,	// GL_multisampled_render_to_texture_IMG / EXT
	MSAA_BLIT				// GL ES 3.0 explicit resolve
};


// Things are a bit more convenient with a separate
// target for each eye, but if we want to use GPU primitive
// amplification to render both eyes at once, they will
// need to be packed into a single buffer and use independent
// viewports.  This is not going to be available soon on mobile GPUs.
struct EyeBuffer
{
		EyeBuffer() :
			Texture( 0 ),
			DepthBuffer( 0 ),
			MultisampleColorBuffer( 0 ),
			RenderFrameBuffer( 0 ),
			ResolveFrameBuffer( 0 )
		{
		}
		~EyeBuffer()
		{
			Delete();
		}

	// Free all resources.
	// Any background time warping from the buffers must be already stopped!
	void				Delete();

	void				Allocate( const EyeParms & bufferParms,
									multisample_t multisampleMode );

	GLuint				Texture;

	// This may be a normal or multisample buffer
	GLuint				DepthBuffer;

	// This is not used for single sample rendering or glFramebufferTexture2DMultisampleEXT
	GLuint				MultisampleColorBuffer;

	// For non-MSAA or glFramebufferTexture2DMultisampleEXT,
	// Texture will be attached to the color buffer.
	GLuint				RenderFrameBuffer;

	// These framebuffers are the target of the resolve blit
	// for MSAA without glFramebufferTexture2DMultisampleEXT.
	// The only attachment will be textures[] to the color
	// buffer.
	GLuint				ResolveFrameBuffer;
};

struct EyePairs
{
	EyePairs() : MultisampleMode( MSAA_OFF ) {}

	EyeParms			BufferParms;
	multisample_t		MultisampleMode;
	EyeBuffer			Eyes[2];
};


enum EyeIndex
{
	EYE_LEFT = 0,
	EYE_RIGHT = 1
};


// This is handed off to TimeWarp
struct CompletedEyes
{
	// Needed to determine sRGB conversions.
	colorFormat_t	ColorFormat;

	// For GPU time warp
	// This will be the MSAA resolved buffer if a blit was done.
	GLuint			Textures[2];
};


class EyeBuffers
{
public:
	EyeBuffers();

	// Note the pose information for this frame and
	// Possibly reconfigure the buffer.
	void		BeginFrame( const EyeParms & 	bufferParms_ );

	// Handles binding the FBO or making the surface current,
	// and setting up the viewport.
	//
	// We might need a "return to eye" call if eye rendering
	// ever needs to change to a different FBO and come back,
	// but that is a bad idea on tiled GPUs anyway.
	void		BeginRenderingEye( const int eyeNum );

	// Handles resolving multisample if necessary, and adding
	// a fence object for tracking completion.
	void		EndRenderingEye( const int eyeNum );

	// Thread safe call that returns the most recent completed
	// eye buffer set for TimeWarp to use.
	CompletedEyes	GetCompletedEyes();

	// Create a screenshot and a thumbnail from the undistorted left eye view
	void 		ScreenShot();

	// GPU time queries around eye scene rendering.
	LogGpuTime<2>	LogEyeSceneGpuTime;

	// SGX wants a clear, Adreno wants a discard, not sure what Mali wants.
	bool			DiscardInsteadOfClear;

	// Current settings
	// If this just changed, not all eye buffers will
	// necessarily have been reallocated yet.
	EyeParms 		BufferParms;

	// For asynchronous time warp, we need to
	// triple buffer the eye pairs:
	// Commands are being written for one set
	// GPU is rendering a second set
	// Time Warp is using a third set
	//
	// If we knew the driver wasn't going to do any interlocks,
	// we could get by with two.
	//
	// We should consider sharing the depth buffers.
	static const int MAX_EYE_SETS = 3;
	long 			SwapCount;		// continuously increasing
	EyePairs		BufferData[MAX_EYE_SETS];
};

}	// namespace OVR

#endif // OVR_EyeBuffers_h

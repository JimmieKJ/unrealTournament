/************************************************************************************

Filename    :   EyeBuffers.cpp
Content     :   Handling of different eye buffer formats
Created     :   March 8, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "EyeBuffers.h"

#include <math.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>						// usleep, etc
#include <stdio.h>
#include <stdlib.h>

#include "Android/LogUtils.h"

#include "3rdParty/stb/stb_image_write.h"
#include "GlTexture.h"
#include "ImageData.h"

using namespace OVR;

EyeBuffers::EyeBuffers() :
	LogEyeSceneGpuTime(),
	DiscardInsteadOfClear( true ),
	SwapCount( 0 )
{
}

void EyeBuffer::Delete()
{
	if ( Texture )
	{
		glDeleteTextures( 1, &Texture );
		Texture = 0;
	}
	if ( DepthBuffer )
	{
		glDeleteRenderbuffers( 1, &DepthBuffer );
		DepthBuffer = 0;
	}
	if ( MultisampleColorBuffer )
	{
		glDeleteRenderbuffers( 1, &MultisampleColorBuffer );
		MultisampleColorBuffer = 0;
	}
	if ( RenderFrameBuffer )
	{
		glDeleteFramebuffers( 1, &RenderFrameBuffer );
		RenderFrameBuffer = 0;
	}
	if ( ResolveFrameBuffer )
	{
		glDeleteFramebuffers( 1, &ResolveFrameBuffer );
		ResolveFrameBuffer = 0;
	}
}

void EyeBuffer::Allocate( const EyeParms & bufferParms, multisample_t multisampleMode )
{
	Delete();

	// GL_DEPTH_COMPONENT16 is the only strictly legal thing in unextended GL ES 2.0
	// The GL_OES_depth24 extension allows GL_DEPTH_COMPONENT24_OES.
	// The GL_OES_packed_depth_stencil extension allows GL_DEPTH24_STENCIL8_OES.
	GLenum depthFormat;
	switch ( bufferParms.depthFormat )
	{
	case DEPTH_24: 				depthFormat = GL_DEPTH_COMPONENT24_OES; break;
	case DEPTH_24_STENCIL_8:	depthFormat = GL_DEPTH24_STENCIL8_OES; break;
	default: 					depthFormat = GL_DEPTH_COMPONENT16; break;
	}

	GLenum colorFormat;
	switch ( bufferParms.colorFormat )
	{
	case COLOR_565: 			colorFormat = GL_RGB565; break;
	case COLOR_5551:			colorFormat = GL_RGB5_A1; break;
	case COLOR_4444:			colorFormat = GL_RGBA4; break;
	default: 					colorFormat = GL_RGBA8; break;
	}



	// Allocate new color texture with uninitialized data.
	glGenTextures( 1, &Texture );
	glBindTexture( GL_TEXTURE_2D, Texture );

	if ( bufferParms.colorFormat == COLOR_565 )
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, bufferParms.WidthScale*bufferParms.resolution, bufferParms.resolution, 0,
				GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL );
	}
	else if ( bufferParms.colorFormat == COLOR_5551 )
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB5_A1, bufferParms.WidthScale*bufferParms.resolution, bufferParms.resolution, 0,
				GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, NULL );
	}
	else if ( bufferParms.colorFormat == COLOR_8888_sRGB )
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, bufferParms.WidthScale*bufferParms.resolution, bufferParms.resolution, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, NULL );
	}
	else
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, bufferParms.WidthScale*bufferParms.resolution, bufferParms.resolution, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, NULL );
	}

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	switch ( bufferParms.textureFilter )
	{
		case TEXTURE_FILTER_NEAREST:
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			LOG( "textureFilter = TEXTURE_FILTER_NEAREST" );
			break;
		case TEXTURE_FILTER_BILINEAR:
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
			LOG( "textureFilter = TEXTURE_FILTER_BILINEAR" );
			break;
		case TEXTURE_FILTER_ANISO_2:
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		   	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 2 );
			LOG( "textureFilter = TEXTURE_FILTER_ANISO_2" );
			break;
		case TEXTURE_FILTER_ANISO_4:
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		   	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 4 );
			LOG( "textureFilter = TEXTURE_FILTER_ANISO_4" );
			break;
		default:
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
			LOG( "textureFilter = TEXTURE_FILTER_BILINEAR" );
			break;
	}

	if ( multisampleMode == MSAA_RENDER_TO_TEXTURE )
	{
		// Imagination Technologies can automatically resolve a multisample rendering on a tile-by-tile
		// basis, without needing to draw to a full size multisample buffer, then blit resolve to a
		// normal texture.
		LOG( "Making a %i sample buffer with glFramebufferTexture2DMultisample", bufferParms.multisamples );

		if ( bufferParms.depthFormat != DEPTH_0 )
		{
			glGenRenderbuffers( 1, &DepthBuffer );
			glBindRenderbuffer( GL_RENDERBUFFER, DepthBuffer );
			glRenderbufferStorageMultisampleIMG_( GL_RENDERBUFFER,bufferParms. multisamples,
					depthFormat, bufferParms.WidthScale*bufferParms.resolution, bufferParms.resolution );

			glBindRenderbuffer( GL_RENDERBUFFER, 0 );
		}

		// Allocate a new frame buffer and attach the two buffers.
		glGenFramebuffers( 1, &RenderFrameBuffer );
		glBindFramebuffer( GL_FRAMEBUFFER, RenderFrameBuffer );

		glFramebufferTexture2DMultisampleIMG_( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D, Texture, 0, bufferParms.multisamples );

		if ( bufferParms.depthFormat != DEPTH_0 )
		{
			glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
					DepthBuffer );
		}
		GL_CheckErrors( "glRenderbufferStorageMultisampleIMG MSAA");
	}
	else if ( multisampleMode == MSAA_BLIT )
	{
		// standard OpenGL ES 3 path for Adreno
		if ( bufferParms.depthFormat != DEPTH_0 )
		{
			LOG( "Making a %i sample %i res depth buffer with GL ES 3", bufferParms.multisamples, bufferParms.resolution );
			glGenRenderbuffers( 1, &DepthBuffer );
			glBindRenderbuffer( GL_RENDERBUFFER, DepthBuffer );
			glRenderbufferStorageMultisample_( GL_RENDERBUFFER, bufferParms.multisamples, depthFormat, bufferParms.resolution, bufferParms.resolution );
		}

		// We also need to make a multisample color buffer here
		glGenRenderbuffers( 1, &MultisampleColorBuffer );
		glBindRenderbuffer( GL_RENDERBUFFER, MultisampleColorBuffer );
		glRenderbufferStorageMultisample_( GL_RENDERBUFFER, bufferParms.multisamples, colorFormat, bufferParms.resolution, bufferParms.resolution );

		glBindRenderbuffer( GL_RENDERBUFFER, 0 );

		// Allocate a new frame buffer and attach the two buffers.
		glGenFramebuffers( 1, &RenderFrameBuffer );
		glBindFramebuffer( GL_FRAMEBUFFER, RenderFrameBuffer );

		glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
				MultisampleColorBuffer );
		if ( bufferParms.depthFormat != DEPTH_0 )
		{
			glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
					DepthBuffer );
		}
		GL_CheckErrors( "ES 3 MSAA");
	}
	else
	{
		// No MSAA, use ES 2 render targets
		LOG( "Making a single sample buffer" );

		if ( bufferParms.depthFormat != DEPTH_0 )
		{
			glGenRenderbuffers( 1, &DepthBuffer );
			glBindRenderbuffer( GL_RENDERBUFFER, DepthBuffer );
			glRenderbufferStorage( GL_RENDERBUFFER, depthFormat, bufferParms.resolution, bufferParms.resolution );

			glBindRenderbuffer( GL_RENDERBUFFER, 0 );
		}

		// Allocate a new frame buffer and attach the two buffers.
		glGenFramebuffers( 1, &RenderFrameBuffer );
		glBindFramebuffer( GL_FRAMEBUFFER, RenderFrameBuffer );
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
				Texture, 0 );

		if ( bufferParms.depthFormat != DEPTH_0 )
		{
			glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
				DepthBuffer );
		}

		GL_CheckErrors( "NO MSAA");
	}

	GLenum status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
	if (status != GL_FRAMEBUFFER_COMPLETE )
	{
		FAIL( "render FBO %i is not complete: 0x%x", RenderFrameBuffer, status );	// TODO: fall back to something else
	}

	// Explicitly clear the color buffer to a color we would notice
	glScissor( 0, 0, bufferParms.WidthScale*bufferParms.resolution, bufferParms.resolution );
	glViewport( 0, 0, bufferParms.WidthScale*bufferParms.resolution, bufferParms.resolution );
	glClearColor( 0, 1, 0, 1 );
	glClear( GL_COLOR_BUFFER_BIT );

	// Blit style MSAA needs to make a second FBO
	if ( multisampleMode == MSAA_BLIT )
	{
		glGenFramebuffers( 1, &ResolveFrameBuffer );
		glBindFramebuffer( GL_FRAMEBUFFER, ResolveFrameBuffer );
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
				Texture, 0 );
		GLenum status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
		if (status != GL_FRAMEBUFFER_COMPLETE )
		{
			FAIL( "resolve FBO %i is not complete: 0x%x", ResolveFrameBuffer, status );	// TODO: fall back to something else
		}
	}

	// Back to the default framebuffer, which should never be
	// used by a time warp client.
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
}

// Call with a %i in the fmt string: "/sdcard/Oculus/screenshot%i.bmp"
static int FindUnusedFilename( const char * fmt, int max )
{
	for ( int i = 0 ; i <= max ; i++ )
	{
		char	buf[1024];
		sprintf( buf, fmt, i );
		FILE * f = fopen( buf, "r" );
		if ( !f )
		{
			return i;
		}
		fclose( f );
	}
	return max;
}

static void ScreenShotTexture( const int eyeResolution, const GLuint texId )
{
	GLuint	fbo;
	glGenFramebuffers( 1, &fbo );
	glBindFramebuffer( GL_FRAMEBUFFER, fbo );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0 );

	unsigned char * buf = (unsigned char *)malloc( eyeResolution * eyeResolution * 8 );
	glReadPixels( 0, 0, eyeResolution, eyeResolution, GL_RGBA, GL_UNSIGNED_BYTE, (void *)buf );
	glDeleteFramebuffers( 1, &fbo );

	for ( int y = 0 ; y < eyeResolution ; y++ )
	{
		const int iy = eyeResolution - 1 - y;
		unsigned char * src = buf + y * eyeResolution * 4;
		unsigned char * dest = buf + (eyeResolution + iy ) * eyeResolution * 4;
		memcpy( dest, src, eyeResolution*4 );
		for ( int x = 0 ; x < eyeResolution ; x++ )
		{
			dest[x*4+3] = 255;
		}
	}


	const char * fmt = "/sdcard/Oculus/screenshot%03i.bmp";
	const int v = FindUnusedFilename( fmt, 999 );
	char	filename[1024];
	sprintf( filename, fmt, v );

	const unsigned char * flipped = (buf + eyeResolution*eyeResolution*4);
	stbi_write_bmp( filename, eyeResolution, eyeResolution, 4, (void *)flipped );

	// make a quarter size version for launcher thumbnails
	unsigned char * shrunk1 = QuarterImageSize( flipped, eyeResolution, eyeResolution, true );
	unsigned char * shrunk2 = QuarterImageSize( shrunk1, eyeResolution>>1, eyeResolution>>1, true );
	char	filename2[1024];
	sprintf( filename2, "/sdcard/Oculus/thumbnail%03i.pvr", v );
	Write32BitPvrTexture( filename2, shrunk2, eyeResolution>>2, eyeResolution>>2 );

	free( buf );
	free( shrunk1 );
	free( shrunk2 );
}

void EyeBuffers::BeginFrame( const EyeParms & bufferParms_ )
{
	SwapCount++;

	EyePairs & buffers = BufferData[ SwapCount % MAX_EYE_SETS ];

	// Save the current buffer parms
	BufferParms = bufferParms_;

	// Update the buffers if parameters have changed
	if ( buffers.Eyes[0].Texture == 0
			|| buffers.BufferParms.resolution != bufferParms_.resolution
			|| buffers.BufferParms.multisamples != bufferParms_.multisamples
			|| buffers.BufferParms.colorFormat != bufferParms_.colorFormat
			|| buffers.BufferParms.depthFormat != bufferParms_.depthFormat
			)
	{
		/*
		 * Changes a buffer set from one (possibly uninitialized) state to a new
		 * resolution / multisample / color depth state.
		 *
		 * This is normally only done at startup, but the resolutions and color depths can be
		 * freely changed without interrupting the background time warp.
		 *
		 * Various GL binding points are modified, as well as the clear color and scissor rect.
		 *
		 * Note that in OpenGL 3.0 and ARB_framebuffer_object, frame buffer objects are NOT
		 * shared across context share groups, so this work must be done in the rendering thread,
		 * not the timewarp thread.  The texture that is bound to the FBO is properly shared.
		 *
		 * TODO: fall back to simpler cases on failure and mark a flag in bufferData?
		 */
		LOG( "Reallocating buffers" );

		// Note the requested parameters, we don't want to allocate again
		// the following frame if we had to fall back for some reason.
		buffers.BufferParms = bufferParms_;

		LOG( "Allocate FBO: res=%i color=%i depth=%i", bufferParms_.resolution,
				bufferParms_.colorFormat, bufferParms_.depthFormat );
		if ( glFramebufferTexture2DMultisampleIMG_ && bufferParms_.multisamples > 1 ) {
			buffers.MultisampleMode = MSAA_RENDER_TO_TEXTURE;
		} else if ( bufferParms_.multisamples > 1 ) {
			buffers.MultisampleMode = MSAA_BLIT;
		} else {
			buffers.MultisampleMode = MSAA_OFF;
		}
		GL_CheckErrors( "Before framebuffer creation");
		for ( int eye = 0; eye < 2; eye++ ) {
			buffers.Eyes[eye].Allocate( bufferParms_, buffers.MultisampleMode );
		}

		GL_CheckErrors( "after framebuffer creation" );
	}
}

void EyeBuffers::BeginRenderingEye( const int eyeNum )
{
	const int resolution = BufferParms.resolution;
	EyePairs & pair = BufferData[ SwapCount % MAX_EYE_SETS ];
	EyeBuffer & eye = pair.Eyes[eyeNum];

	LogEyeSceneGpuTime.Begin( eyeNum );
	LogEyeSceneGpuTime.PrintTime( eyeNum, "GPU time for eye render" );

	glBindFramebuffer( GL_FRAMEBUFFER, eye.RenderFrameBuffer );
	glViewport( 0, 0, resolution, resolution );
	glScissor( 0, 0, resolution, resolution );
	glDepthMask( GL_TRUE );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );

	if ( DiscardInsteadOfClear )
	{
		GL_InvalidateFramebuffer( INV_FBO, true, true );
		glClear( GL_DEPTH_BUFFER_BIT );
	}
	else
	{
		glClearColor( 0, 0, 0, 1 );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}
}

void EyeBuffers::EndRenderingEye( const int eyeNum )
{
	const int resolution = BufferParms.resolution;
	EyePairs & pair = BufferData[ SwapCount % MAX_EYE_SETS ];
	EyeBuffer & eye = pair.Eyes[eyeNum];

	// Discard the depth buffer, so the tiler won't need to write it back out to memory
	GL_InvalidateFramebuffer( INV_FBO, false, true );

	// Do a blit-MSAA-resolve if necessary.
	if ( eye.ResolveFrameBuffer )
	{
		glBindFramebuffer( GL_READ_FRAMEBUFFER, eye.RenderFrameBuffer );
		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, eye.ResolveFrameBuffer );
		glBlitFramebuffer_( 0, 0, resolution, resolution,
				0, 0, resolution, resolution,
				GL_COLOR_BUFFER_BIT, GL_NEAREST );
		// Discard the multisample color buffer after we have resolved it,
		// so the tiler won't need to write it back out to memory
		GL_InvalidateFramebuffer( INV_FBO, true, false );
	}

	LogEyeSceneGpuTime.End( eyeNum );

	// Left to themselves, tiled GPU drivers will avoid starting rendering
	// until they are absolutely forced to by a swap or read of a buffer,
	// because poorly implemented applications may switch away from an
	// FBO (say, to render a shadow buffer), then switch back to it and
	// continue rendering, which would cause a wasted resolve and unresolve
	// from real memory if drawing started automatically on leaving an FBO.
	//
	// We are not going to do any such thing, and we want the drawing of the
	// first eye to overlap with the command generation for the second eye
	// if the GPU has idled, so explicitly flush the pipeline.
	//
	// Adreno and Mali Do The Right Thing on glFlush, but PVR is
	// notorious for ignoring flushes to optimize naive apps.  The preferred
	// solution there is to use EGL_SYNC_FLUSH_COMMANDS_BIT_KHR on a
	// KHR sync object, but several versions of the Adreno driver have
	// had broken implementations of this that performed a full finish
	// instead of just a flush.
	glFlush();
}

CompletedEyes EyeBuffers::GetCompletedEyes()
{
	CompletedEyes	cmp = {};
	// The GPU commands are flushed for BufferData[ SwapCount % MAX_EYE_SETS ]
	EyePairs & currentBuffers = BufferData[ SwapCount % MAX_EYE_SETS ];

	EyePairs * buffers = &currentBuffers;

	for ( int e = 0 ; e < 2 ; e++ )
	{
		cmp.Textures[e] = buffers->Eyes[e].Texture;
	}
	cmp.ColorFormat = buffers->BufferParms.colorFormat;

	return cmp;
}

void EyeBuffers::ScreenShot()
{
	ScreenShotTexture( BufferParms.resolution, BufferData[0].Eyes[0].Texture );
}

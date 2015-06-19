/************************************************************************************

Filename    :   DirectRender.cpp
Content     :   Handles vendor specific extensions for direct front buffer rendering
Created     :   October 1, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "DirectRender.h"

#include <unistd.h>						// sleep, etc
#include <stdlib.h>
#include <assert.h>

#include "Android/GlUtils.h"
#include "Android/LogUtils.h"

PFNEGLLOCKSURFACEKHRPROC	eglLockSurfaceKHR_;
PFNEGLUNLOCKSURFACEKHRPROC	eglUnlockSurfaceKHR_;

namespace OVR
{

// JDC: we NEED an egl extension string for this
static bool FrontBufferExtensionPresent()
{
	return true;	// will crash on some unsupported systems...
//	return false;
}

VrSurfaceManager::VrSurfaceManager() :
	env( NULL ),
	surfaceClass( NULL ),
	setFrontBufferID( NULL ),
	getFrontBufferAddressID( NULL ),
	getSurfaceBufferAddressID( NULL ),
	getClientBufferAddressID( NULL )
{
}

VrSurfaceManager::~VrSurfaceManager()
{
	Shutdown();
}

void VrSurfaceManager::Init( JNIEnv * jni_ )
{
	if ( jni_ == NULL )
	{
		LOG( "VrSurfaceManager::Init - Invalid jni" );
		return;
	}

	env = jni_;

	// Determine if the Java Front Buffer IF exists. If not, fall back
	// to using the egl extensions.
	jclass lc = env->FindClass( "android/app/VRSurfaceManager" );
	if ( lc != NULL )
	{
		surfaceClass = (jclass)env->NewGlobalRef( lc );
		LOG( "Found VrSurfaceManager API: %p", surfaceClass );
		env->DeleteLocalRef( lc );
	}

	// Clear NoClassDefFoundError, if thrown
	if ( env->ExceptionOccurred() )
	{
		env->ExceptionClear();
		LOG( "Clearing JNI Exceptions" );
	}

	// Look up the Java Front Buffer IF method IDs
	if ( surfaceClass != NULL )
	{
		setFrontBufferID = env->GetStaticMethodID( surfaceClass, "setFrontBuffer", "(IZ)V" );
		getFrontBufferAddressID = env->GetStaticMethodID( surfaceClass, "getFrontBufferAddress", "(I)I" );
		getSurfaceBufferAddressID = env->GetStaticMethodID( surfaceClass, "getSurfaceBufferAddress", "(I[II)I" );
		getClientBufferAddressID = env->GetStaticMethodID( surfaceClass, "getClientBufferAddress", "(I)I" );
	}
}

void VrSurfaceManager::Shutdown()
{
	LOG( "VrSurfaceManager::Shutdown" );

	if ( surfaceClass != NULL )
	{
		env->DeleteGlobalRef( surfaceClass );
		surfaceClass = NULL;
	}

	env = NULL;
}

bool VrSurfaceManager::SetFrontBuffer( const EGLSurface surface_, const bool set )
{
	bool gvrFrontbuffer = false;

	if ( FrontBufferExtensionPresent() )
	{
		// Test the Java IF first (present on Note 4)
		if ( setFrontBufferID != NULL )
		{
			LOG( "Calling java method");
			// Use the Java Front Buffer IF
			env->CallStaticVoidMethod( surfaceClass, setFrontBufferID, (int)surface_, set );

			// test address - currently crashes if executed here
			/*
			void * addr = (void *)env->CallStaticIntMethod( surfaceClass, getFrontBufferAddressID, (int)surface_ );
			if ( addr ) {
				LOG( "getFrontBufferAddress succeeded %i", addr );
				gvrFrontbufferExtension = true;
			} else {
				LOG( "getFrontBufferAddress failed %i", addr );
				gvrFrontbufferExtension = false;
			}
			*/

			gvrFrontbuffer = true;

			// Catch Permission denied exception
			if ( env->ExceptionOccurred() )
			{
				WARN( "Exception: egl_GVR_FrontBuffer failed" );
				env->ExceptionClear();
				gvrFrontbuffer = false;
			}
		}

		// If the Java IF is not present or permission was denied, 
		// fallback to the egl extension found on the S5s.
		if ( set && ( setFrontBufferID == NULL || gvrFrontbuffer == false ) )
		{
			// Fall back to using the EGL Front Buffer extension
			typedef void * (*PFN_GVR_FrontBuffer) (EGLSurface surface);
			PFN_GVR_FrontBuffer egl_GVR_FrontBuffer = NULL;

			// look for the extension
			egl_GVR_FrontBuffer = (PFN_GVR_FrontBuffer)eglGetProcAddress( "egl_GVR_FrontBuffer" );
			if ( egl_GVR_FrontBuffer == NULL )
			{
				LOG( "Not found: egl_GVR_FrontBuffer" );
				gvrFrontbuffer = false;
			}
			else
			{
				LOG( "Found: egl_GVR_FrontBuffer" );
				void * ret = egl_GVR_FrontBuffer( surface_ );
				if ( ret )
				{
					LOG( "egl_GVR_FrontBuffer succeeded" );
					gvrFrontbuffer = true;
				}
				else
				{
					WARN( "egl_GVR_FrontBuffer failed" );
					gvrFrontbuffer = false;
				}
			}
		}
	}
	else
	{
		// ignore it
		LOG( "Not found: egl_GVR_FrontBuffer" );
		gvrFrontbuffer = false;
	}

	return gvrFrontbuffer;
}

void * VrSurfaceManager::GetFrontBufferAddress( const EGLSurface surface )
{
	if ( getFrontBufferAddressID != NULL )
	{
		return (void *)env->CallStaticIntMethod( surfaceClass, getFrontBufferAddressID, (int)surface );
	}

	LOG( "getFrontBufferAddress not found" );
	return NULL;
}

void * VrSurfaceManager::GetSurfaceBufferAddress( const EGLSurface surface, int attribs[], const int attr_size, const int pitch )
{
	if ( getSurfaceBufferAddressID != NULL )
	{
		jintArray attribs_array = env->NewIntArray( attr_size );
		env->SetIntArrayRegion( attribs_array, 0, attr_size, attribs );
		return (void *)env->CallStaticIntMethod( surfaceClass, getSurfaceBufferAddressID, (int)surface, attribs_array, pitch );
	}

	LOG( "getSurfaceBufferAddress not found" );
	return NULL;
}

void * VrSurfaceManager::GetClientBufferAddress( const EGLSurface surface )
{
	if ( getClientBufferAddressID != NULL )
	{
		// Use the Java Front Buffer IF
		void * ret = (void *)env->CallStaticIntMethod( surfaceClass, getClientBufferAddressID, (int)surface );
		LOG( "getClientBufferAddress(%p) = %p", eglGetCurrentSurface( EGL_DRAW ), ret );
		return ret;
	}
	else
	{
		// Fall back to using the EGL Front Buffer extension
		typedef EGLClientBuffer (*PFN_EGL_SEC_getClientBufferForFrontBuffer) (EGLSurface surface);
		PFN_EGL_SEC_getClientBufferForFrontBuffer EGL_SEC_getClientBufferForFrontBuffer =
				(PFN_EGL_SEC_getClientBufferForFrontBuffer)eglGetProcAddress( "EGL_SEC_getClientBufferForFrontBuffer" );
		if ( EGL_SEC_getClientBufferForFrontBuffer == NULL )
		{
			LOG( "Not found: EGL_SEC_getClientBufferForFrontBuffer" );
			return NULL;
		}
		void * ret = EGL_SEC_getClientBufferForFrontBuffer( eglGetCurrentSurface( EGL_DRAW ) );
		LOG( "EGL_SEC_getClientBufferForFrontBuffer(%p) = %p", eglGetCurrentSurface( EGL_DRAW ), ret );
		return ret;
	}

	return NULL;
}

// For Adreno, we can render half-screens three different ways.
enum tilerControl_t
{
	FB_TILED_RENDERING,			// works properly, but re-issues geometry for each tile
	FB_BINNING_CONTROL,			// doesn't work on 330, but does on 420
	FB_WRITEONLY_RENDERING,		// blended vignettes don't work
	FB_MALI
};

tilerControl_t tilerControl;

DirectRender::DirectRender() :
	windowSurface( 0 ),
	wantFrontBuffer( false ),
	display( 0 ),
	context( 0 ),
	width( 0 ),
	height( 0 ),
	gvrFrontbufferExtension( false )
{
}

#if 0 // unused?
static void ExerciseFrontBuffer()
{
	glEnable( GL_WRITEONLY_RENDERING_QCOM );

	for ( int i = 0 ; i < 1000 ; i++ )
	{
		glClearColor( i&1, (i>>1)&1, (i>>2)&1, 1 );
		glClear( GL_COLOR_BUFFER_BIT );
		glFinish();
	}

	glDisable( GL_WRITEONLY_RENDERING_QCOM );
}
#endif

void DirectRender::InitForCurrentSurface( JNIEnv * jni, bool wantFrontBuffer_, int buildVersionSDK_ )
{
	LOG( "%p DirectRender::InitForCurrentSurface(%s)", this, wantFrontBuffer_ ? "true" : "false" );

	wantFrontBuffer = wantFrontBuffer_;

	display = eglGetDisplay( EGL_DEFAULT_DISPLAY );
	context = eglGetCurrentContext();
	windowSurface = eglGetCurrentSurface( EGL_DRAW );

	// NOTE: On Mali as well as under Android-L, we need to perform
	// an initial swapbuffers in order for the front-buffer extension
	// to work.
	// NOTE: On Adreno KitKat, we cannot apply the initial swapbuffers
	// as it will result in poor performance.
	static const int KITKAT_WATCH = 20;
	const GpuType gpuType = EglGetGpuType();
	if ( ( buildVersionSDK_ > KITKAT_WATCH ) ||	// if the SDK is Lollipop or higher
		 ( gpuType & GPU_TYPE_MALI ) != 0 )		// or the GPU is Mali
	{
		LOG( "Performing an initial swapbuffers for Mali and/or Android-L" );
		// When we use the protected Trust Zone framebuffer there is trash in the
		// swap chain, so clear it out.
		glClearColor( 0, 0, 0, 0 );
		glClear( GL_COLOR_BUFFER_BIT );
		eglSwapBuffers( display, windowSurface );	// swap buffer will operate que/deque related process internally
													// now ready to set usage to proper surface
	}

	// Get the surface size.
	eglQuerySurface( display, windowSurface, EGL_WIDTH, &width );
	eglQuerySurface( display, windowSurface, EGL_HEIGHT, &height );
	LOG( "surface size: %i x %i", width, height );

	if ( !wantFrontBuffer_ )
	{
		LOG( "Running without front buffer");
	}
	else
	{
		surfaceMgr.Init( jni );

		gvrFrontbufferExtension = surfaceMgr.SetFrontBuffer( windowSurface, true );
		LOG ( "gvrFrontbufferExtension = %s", ( gvrFrontbufferExtension ) ? "TRUE" : "FALSE" );

		if ( ( gpuType & GPU_TYPE_MALI ) != 0 )
		{
			LOG( "Mali GPU" );
			tilerControl = FB_MALI;
		}
		else if ( ( gpuType & GPU_TYPE_ADRENO ) != 0 )
		{
			// Query the number of samples on the display
			EGLint configID;
			if ( !eglQueryContext( display, context, EGL_CONFIG_ID, &configID ) )
			{
				FAIL( "eglQueryContext EGL_CONFIG_ID failed" );
			}
			EGLConfig eglConfig = EglConfigForConfigID( display, configID );
			if ( eglConfig == NULL )
			{
				FAIL( "EglConfigForConfigID failed" );
			}
			EGLint samples = 0;
			eglGetConfigAttrib( display, eglConfig, EGL_SAMPLES, &samples );

			if ( gpuType == GPU_TYPE_ADRENO_330 )
			{
				LOG( "Adreno 330 GPU" );
				tilerControl = FB_TILED_RENDERING;
			}
			else
			{
				LOG( "Adreno GPU" );

				// NOTE: On KitKat, only tiled render mode will continue to work
				// with multisamples set on the frame buffer (at a performance
				// loss). On Lollipop, having multisamples set on the frame buffer
				// is an error for all render modes and will result in a black screen.
				if ( samples != 0 )
				{
					// TODO: We may want to make this a FATAL ERROR.

					WARN( "**********************************************" );
					WARN( "ERROR: frame buffer uses MSAA - turn off MSAA!" );
					WARN( "**********************************************" );
					tilerControl = FB_TILED_RENDERING;
				}
				else
				{
					// NOTE: Currently (2014-11-19) the memory controller
					// clock is not fixed when running with fixed CPU/GPU levels.
					// For direct render mode, the fluctuation may cause significant
					// performance issues.

					// FIXME: Enable tiled render mode for now until we are able
					// to run with fixed memory clock.
	#if 0
					tilerControl = FB_BINNING_CONTROL;

					// 2014-09-28: Qualcomm is moving to a new extension with
					// the next driver. In order for the binning control to
					// work for both the current and next driver, we add the
					// following call which should happen before any calls to
					// glHint( GL_* ).
					// This causes a gl error on current drivers, but will be
					// needed for the new driver.
					GL_CheckErrors( "Before enabling Binning Control" );
					LOG( "Enable GL_BINNING_CONTROL_HINT_QCOM - may cause a GL_ERROR on current driver" );
					glEnable( GL_BINNING_CONTROL_HINT_QCOM );
					GL_CheckErrors( "Expected on current driver" );
	#else
					tilerControl = FB_TILED_RENDERING;
	#endif
				}
			}
		}

		// draw stuff to the screen without swapping to see if it is working
		// ExerciseFrontBuffer();
	}
}

void DirectRender::Shutdown()
{
	LOG( "%p DirectRender::Shutdown", this );
	if ( wantFrontBuffer )
	{
		if ( windowSurface != EGL_NO_SURFACE )
		{
			surfaceMgr.SetFrontBuffer( windowSurface, false );
			windowSurface = EGL_NO_SURFACE;
		}

		surfaceMgr.Shutdown();
	}
}

// Conventionally swapped rendering will return false.
bool DirectRender::IsFrontBuffer() const
{
	return wantFrontBuffer && gvrFrontbufferExtension;
}

void DirectRender::BeginDirectRendering( int x, int y, int width, int height )
{
	switch( tilerControl )
	{
		case FB_TILED_RENDERING:
		{
			if ( QCOM_tiled_rendering )
			{
				glStartTilingQCOM_( x, y, width, height, 0 );
			}
			glScissor( x, y, width, height );
			break;
		}
		case FB_BINNING_CONTROL:
		{
			glHint( GL_BINNING_CONTROL_HINT_QCOM, GL_RENDER_DIRECT_TO_FRAMEBUFFER_QCOM );
			glScissor( x, y, width, height );
			break;
		}
		case FB_WRITEONLY_RENDERING:
		{
			glEnable( GL_WRITEONLY_RENDERING_QCOM );
			glScissor( x, y, width, height );
			break;
		}
		case FB_MALI:
		{
			const GLenum attachments[3] = { GL_COLOR, GL_DEPTH, GL_STENCIL };
			glInvalidateFramebuffer_( GL_FRAMEBUFFER, 3, attachments );
			glScissor( x, y, width, height );
			// This clear is not absolutely necessarily but ARM prefers an explicit glClear call to avoid ambiguity.
			//glClearColor( 0, 0, 0, 1 );
			//glClear( GL_COLOR_BUFFER_BIT );
			break;
		}
		default:
		{
			glScissor( x, y, width, height );
			break;
		}
	}
}

void DirectRender::EndDirectRendering() const
{
	switch( tilerControl )
	{
		case FB_TILED_RENDERING:
		{
			// This has an implicit flush
			if ( QCOM_tiled_rendering )
			{
				glEndTilingQCOM_( GL_COLOR_BUFFER_BIT0_QCOM );
			}
			break;
		}
		case FB_BINNING_CONTROL:
		{
			glHint( GL_BINNING_CONTROL_HINT_QCOM, GL_DONT_CARE );
			// Flush explicitly
			glFlush();		// GL_Flush() with KHR_sync seems to be synchronous
			break;
		}
		case FB_WRITEONLY_RENDERING:
		{
			glDisable( GL_WRITEONLY_RENDERING_QCOM );
			// Flush explicitly
			glFlush();		// GL_Flush() with KHR_sync seems to be synchronous
			break;
		}
		case FB_MALI:
		{
			const GLenum attachments[2] = { GL_DEPTH, GL_STENCIL };
			glInvalidateFramebuffer_( GL_FRAMEBUFFER, 2, attachments );
			// Flush explicitly
			glFlush();		// GL_Flush() with KHR_sync seems to be synchronous
			break;
		}
		default:
		{
			glFlush();		// GL_Flush() with KHR_sync seems to be synchronous
			break;
		}
	}
}

void DirectRender::SwapBuffers() const
{
	eglSwapBuffers( display, windowSurface );
}

void DirectRender::GetScreenResolution( int & width_, int & height_ ) const
{
	height_ = height;
	width_ = width;
}

}	// namespace OVR

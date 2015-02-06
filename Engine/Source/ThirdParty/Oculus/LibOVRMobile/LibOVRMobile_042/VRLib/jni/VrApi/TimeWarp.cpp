/************************************************************************************

Filename    :   TimeWarp.h
Content     :   Background thread continuous frame warping
Created     :   August 12, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

/*
 * This file should be as self-contained as possible, so it can be used in
 * both Unity and stand alone code.
 *
 * Should eye buffers be submitted to TimeWarp independently?
 * Definitely NOT -- If the left eye was picked up and used by time warp, but
 * the right eye wasn't ready in time, then the right eye would have to use an
 * older buffer, which could result in things briefly animating backwards.
 *
 * Should TimeWarp check for newly completed buffers before each eye, or
 * just at vsync?
 * Would reduce buffer latency on the right eye when an application is running
 * slower than vsync, but if an application ever got fast enough to complete
 * rendering in < 1/2 of a vsync, the right eye could stutter between 0 and 16
 * ms of latency with visible animation stutters.
 *
 * Should applications fetch latest HMD position before rendering each eye?
 * Probably.  No real downside other than needing to pass a sensor state for
 * each eye for TimeWarp.
 *
 * Should applications be allowed to generate more buffers than the vsync rate?
 * Probably not.  Reduces average latency at the expense of variability and
 * significant power waste.
 *
 */

#include "TimeWarp.h"

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>			// for usleep

#include <android/sensor.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "JniUtils.h"
#include "TimeWarpLocal.h"
#include "Vsync.h"
#include "VrCommon.h"
#include "MemBuffer.h"
#include "Distortion.h"
#include "PathUtils.h"
#include "../PackageFiles.h"

#include "embedded/oculus_loading_indicator.h"

int rand_r (unsigned *seed)
{
	unsigned int next = *seed;
	int result;

	next *= 1103515245;
	next += 12345;
	result = (unsigned) (next / 65536) & 2047;

	next *= 1103515245;
	next += 12345;
	result <<= 10;
	result ^= (unsigned) (next / 65536) & 1023;

	next *= 1103515245;
	next += 12345;
	result <<= 10;
	result ^= (unsigned) (next / 65536) & 1023;

	*seed = next;

	return result;
}

// Convert a standard projection matrix into a TanAngle matrix for
// the primary time warp surface.
ovrMatrix4f TanAngleMatrixFromProjection( const ovrMatrix4f & projection )
{
	// A projection matrix goes from a view point to NDC, or -1 to 1 space.
	// Scale and bias to convert that to a 0 to 1 space.

	OVR::Matrix4f proj( projection );
	for ( int i = 2; i <= 3; i++ )
	{
		proj.M[i][0] = 0.0f;
		proj.M[i][1] = 0.0f;
		proj.M[i][2] = -1.0f;
		proj.M[i][3] = 0.0f;
	}
	return ( OVR::Matrix4f::Translation( 0.5f, 0.5f, 0.0f) *
			OVR::Matrix4f::Scaling( 0.5f, 0.5f, 1.0f ) * proj );
}

// Trivial version of TanAngleMatrixFromProjection() for a symmetric field of view.
ovrMatrix4f TanAngleMatrixFromFov( const float fovDegrees )
{
	const OVR::Matrix4f proj = OVR::Matrix4f::PerspectiveRH( OVR::DegreeToRad(fovDegrees), 1.0f, 1, 100);
	return TanAngleMatrixFromProjection( proj );
}


// Convert a modelview matrix that would normally draw a -1 to 1 unit
// square to the view into a TanAngle matrix for an overlay surface.
//
// The resulting z value should be straight ahead distance to the plane.
// The x and y values will be pre-multiplied by z for projective texturing.
ovrMatrix4f TanAngleMatrixFromUnitSquare( const ovrMatrix4f & modelView )
{
	const OVR::Matrix4f inv = OVR::Matrix4f( modelView ).Inverted();
	const OVR::Vector4f org = inv.Transform( OVR::Vector4f( 0.0f, 0.0f, 0.0f, 1.0f ) );
	OVR::Matrix4f m;

	m.M[3][0] = inv.M[2][0];
	m.M[3][1] = inv.M[2][1];
	m.M[3][2] = inv.M[2][2];
	m.M[3][3] = 0.0f;

	for ( int j = 0; j < 3; j++ )
	{
		for ( int i = 0; i < 3; i++ )
		{
			m.M[j][i] = org[2] * inv.M[j][i];
			m.M[j][i] -= org[j] * m.M[3][i];
		}
	}

	m.M[2][0] = m.M[3][0];
	m.M[2][1] = m.M[3][1];
	m.M[2][2] = m.M[3][2];
	m.M[2][3] = m.M[3][3];

	m = OVR::Matrix4f::Scaling( -0.5f, 0.5f, 1.0f ) * m;
	m = OVR::Matrix4f::Translation( 0.5f, 0.5f, 0.0f ) * m;
	return m;
}

ovrMatrix4f	CalculateExternalVelocity( const ovrMatrix4f & viewMatrix, const float yawRadiansPerSecond )
{
	if ( yawRadiansPerSecond == 0.0f )
	{
		return OVR::Matrix4f::Identity();
	}

	// Yaw is always going to be around the world Y axis
	const OVR::Vector4f worldY = OVR::Matrix4f( viewMatrix ).Transform( OVR::Vector4f( 0.0f, 1.0f, 0.0f, 0.0f ) );
	const float		angle = yawRadiansPerSecond * -1.0f / 60.0f;
	const float   sinHalfAngle = sin(angle * 0.5f);

	ovrQuatf rot;
	rot.w = cos(angle * 0.5f);
	rot.x = worldY.x * sinHalfAngle;
	rot.y = worldY.y * sinHalfAngle;
	rot.z = worldY.z * sinHalfAngle;

	return OVR::Matrix4f( rot );
}


//=================================================================================
namespace OVR {


static const int WARP_TESSELATION = 32;	// This is probably a bit too low, I can see some wiggle.

// async	drawn by an independent thread
// sync		drawn by the same thread that draws the eye buffers
//
// frontBuffer	drawn directly to the front buffer with danger of tearing
// swappedBuffer drawn to a swapped buffer with danger of missing a flip
//
// portrait		display scans the left eye completely before scanning the right
// landscape	display scans both eyes simultaneously (DK1, not any of the mobile displays)
//
// Note that the OpenGL buffer may be rotated by hardware, and does not
// necessarily match the scanning orientation -- a landscape Android app is still
// displayed on a portrait scanned display.

// We will need additional swapPrograms for global shutter low-persistence displays
// with the same prediction value used for all points.

// The values reported by the latency tester will tend to be
// 8 milliseconds longer than the prediction values used here,
// because the tester event pulse will happen at some random point
// during the frame previous to the sensor sampling to render
// an eye.  The IMU updates 500 or 1000 times a second, so there
// is only a millisecond or so of jitter.

// The target vsync will always go up by at least one after each
// warp, which should prevent the swapped versions from ever falling into
// triple buffering and getting an additional frame of latency.
//
// It may need to go up by more than one if warping is falling behind
// the video rate, otherwise front buffer rendering could happen
// prematurely, or swapped rendering could go into triple buffering.
//
// On entry to WarpToScreen()
// nextVsync = floor( currentVsync ) + 1.0

// If we have reliable GPU scheduling and front buffer rendering,
// try to warp each eye exactly half a frame ahead.
swapProgram_t	spAsyncFrontBufferPortrait = {
	false,	false, { 0.5, 1.0},	{ {1.0, 1.5}, {1.5, 2.0} }
};

// If we have reliable GPU scheduling, but don't have front
// buffer rendering, the warp thread should still wait until
// mid frame before rendering the second eye, reducing latency.
swapProgram_t	spAsyncSwappedBufferPortrait = {
	false,	false, { 0.0, 0.5},	{ {1.0, 1.5}, {1.5, 2.0} }
};

// If a single thread of control is doing the warping as well
// as the eye rendering, we will usually already be in the second half
// of the scanout, but we may still need to wait for it if the eye
// rendering was unusually quick.  We will then need to wait for
// vsync to start the right eye rendering.
swapProgram_t	spSyncFrontBufferPortrait = {
	true,	false, { 0.5, 1.0},	{ {1.0, 1.5}, {1.5, 2.0} }
};

// If we are drawing to a swapped buffer, we don't want to wait at all,
// for fear of missing the swap point and dropping the frame.
// The true prediction timings for android would be 3,3.5,3.5,4, but
// that is way too much prediction.
swapProgram_t	spSyncSwappedBufferPortrait = {
	true,	false, { 0.0, 0.0},	{ {2.0, 2.5}, {2.5, 3.0} }
};

// Landscape scanned displays, like Rift DK1, will
// always have identical top and bottom predictions.
// For front buffer landscape rendering, We may want to split each eye
// into a pair of renderings with a wait in the middle to avoid
// tight timing near the bottom of the frame and assumptions about
// the speed and raster order of warping.

static const char * DefaultDistortionFile = "/Oculus/defaultDistortion.bin";

static const float LOADING_ICON_ROTATION		= 1.0f;		// in radians per second
static const float LOADING_ICON_SCALE			= 16.0f;	// factor smaller than fullscreen

//=========================================================================================

// If lensCentered, the coordinates will be square and extend past the left and
// right edges for a viewport.
//
// Otherwise, it will cover only the pixels in the correct half of the screen
// for a scissor rect.

// If the window is wider than it is tall, ie 1920x1080
void EyeRectLandscape( const hmdInfoInternal_t & hmd, const ScreenEye eye,
		const WhichEyeRect rect, int &x, int &y, int &width, int &height )
{
	if ( rect == RECT_SCREEN ) {
		// always scissor exactly to half the screen
		int scissorX = ( eye == 0 ) ? 0 : hmd.widthPixels / 2;
		int scissorY = 0;
		int scissorWidth = hmd.widthPixels / 2;
		int scissorHeight = hmd.heightPixels;
		x = scissorX;
		y = scissorY;
		width = scissorWidth;
		height = scissorHeight;
		return;
	}

	const float	metersToPixels = hmd.widthPixels / hmd.widthMeters;

	// Even though the lens center is shifted outwards slightly,
	// the height is still larger than the largest horizontal value.
	// TODO: check for sure on other HMD
	const int	pixelRadius = hmd.heightPixels / 2;
	const int	pixelDiameter = pixelRadius * 2;
	const float	horizontalShiftMeters =  ( hmd.lensSeparation / 2 ) - ( hmd.widthMeters / 4 );
	const float	horizontalShiftPixels = horizontalShiftMeters * metersToPixels;

	// Make a viewport that is symetric, extending off the sides of the screen and into the other half.
	x = hmd.widthPixels/4 - pixelRadius + ( ( eye == 0 ) ? - horizontalShiftPixels : hmd.widthPixels/2 + horizontalShiftPixels );
	y = 0;
	width = pixelDiameter;
	height = pixelDiameter;
}

void EyeRect( const hmdInfoInternal_t & hmd, const ScreenEye eye, const WhichEyeRect rect,
		int &x, int &y, int &width, int &height )
{
	int	lx, ly, lWidth, lHeight;
	EyeRectLandscape( hmd, eye, rect, lx, ly, lWidth, lHeight );

	x = lx;
	y = ly;
	width = lWidth;
	height = lHeight;
}

Matrix4f CalculateTimeWarpMatrix2( const Quatf &inFrom, const Quatf &inTo )
{
	// FIXME: this is a horrible hack to fix a zero quaternion that's passed in
	// the night before a demo. This is coming from the sensor pose and needs to
	// be tracked down further.
	Quatf from = inFrom;
	Quatf to = inTo;

	bool fromValid = from.LengthSq() > 0.95f;
	bool toValid = to.LengthSq() > 0.95f;
	if ( !fromValid )
	{  
		if ( toValid )
		{
			from = to;
		}
		else
		{
			from = Quatf( 0.0f, 0.0f, 0.0f, 1.0f ); // just force identity
		}
	}
	if ( !toValid )
	{  
		if ( fromValid )
		{
			to = from;
		}
		else
		{
			to = Quatf( 0.0f, 0.0f, 0.0f, 1.0f ); // just force identity
		}
	}

	Matrix4f		lastSensorMatrix = Matrix4f( to );
	Matrix4f		lastViewMatrix = Matrix4f( from );

	return ( lastSensorMatrix.Inverted() * lastViewMatrix ).Inverted();
}

static GlGeometry LoadMeshFromMemory( const MemBuffer & buf,
		const int NUM_SLICES_PER_EYE, const float fovScale )
{
	GlGeometry	geometry;

	if ( buf.Length < 12 )
	{
		LOG( "bad buf.length %i", buf.Length );
		return geometry;
	}

	const int magic = ((int *)buf.Buffer)[0];
	const int tesselationsX = ((int *)buf.Buffer)[1];
	const int tesselationsY = ((int *)buf.Buffer)[2];

	const int vertexBytes = 12 + 2 * (tesselationsX+1) * (tesselationsY+1) * 6 * sizeof( float );
	if ( buf.Length != vertexBytes )
	{
		LOG( "buf.length %i != %i", buf.Length, vertexBytes );
		return geometry;
	}
	if ( magic != DISTORTION_BUFFER_MAGIC || tesselationsX < 1 || tesselationsY < 1 )
	{
		LOG( "Bad distortion header" );
		return geometry;
	}
	const float * bufferVerts = &((float *)buf.Buffer)[3];

	// build a VertexArrayObject
	glGenVertexArraysOES_( 1, &geometry.vertexArrayObject );
	glBindVertexArrayOES_( geometry.vertexArrayObject );

	const int attribCount = 10;
	const int sliceTess = tesselationsX / NUM_SLICES_PER_EYE;

	const int vertexCount = 2*NUM_SLICES_PER_EYE*(sliceTess+1)*(tesselationsY+1);
	const int floatCount = vertexCount * attribCount;
	float * tessVertices = new float[floatCount];

	const int indexCount = 2*tesselationsX*tesselationsY*6;
	unsigned short * tessIndices = new unsigned short[indexCount];
	const int indexBytes = indexCount * sizeof( *tessIndices );

	int	index = 0;
	int	verts = 0;

	for ( int eye = 0; eye < 2; eye++ )
	{
		for ( int slice = 0; slice < NUM_SLICES_PER_EYE; slice++ )
		{
			const int vertBase = verts;

			for ( int y = 0; y <= tesselationsY; y++ )
			{
				const float	yf = (float)y / (float)tesselationsY;
				for ( int x = 0; x <= sliceTess; x++ )
				{
					const int sx = slice * sliceTess + x;
					const float	xf = (float)sx / (float)tesselationsX;
					float * v = &tessVertices[attribCount * ( vertBase + y * (sliceTess+1) + x ) ];
					v[0] = -1.0 + eye + xf;
					v[1] = yf*2.0f - 1.0f;

					// Copy the offsets from the file
					for ( int i = 0 ; i < 6; i++ )
					{
						v[2+i] = fovScale * bufferVerts
								[(y*(tesselationsX+1)*2+sx + eye * (tesselationsX+1))*6+i];
					}

					v[8] = (float)x / sliceTess;
					// Enable this to allow fading at the edges.
					// Samsung recommends not doing this, because it could cause
					// visible differences in pixel wear on the screen over long
					// periods of time.
					if ( 0 && ( y == 0 || y == tesselationsY || sx == 0 || sx == tesselationsX ) )
					{
						v[9] = 0.0f;	// fade to black at edge
					}
					else
					{
						v[9] = 1.0f;
					}
				}
			}
			verts += (tesselationsY+1)*(sliceTess+1);

			// The order of triangles doesn't matter for tiled rendering,
			// but when we can do direct rendering to the screen, we want the
			// order to follow the raster order to minimize the chance
			// of tear lines.
			//
			// This can be checked by quartering the number of indexes, and
			// making sure that the drawn pixels are the first pixels that
			// the raster will scan.
			for ( int x = 0; x < sliceTess; x++ )
			{
				for ( int y = 0; y < tesselationsY; y++ )
				{
					// flip the triangulation in opposite corners
					if ( (slice*sliceTess+x <  tesselationsX/2) ^ (y < (tesselationsY/2)) )
					{
						tessIndices[index+0] = vertBase + y * (sliceTess+1) + x;
						tessIndices[index+1] = vertBase + y * (sliceTess+1) + x + 1;
						tessIndices[index+2] = vertBase + (y+1) * (sliceTess+1) + x + 1;

						tessIndices[index+3] = vertBase + y * (sliceTess+1) + x;
						tessIndices[index+4] = vertBase + (y+1) * (sliceTess+1) + x + 1;
						tessIndices[index+5] = vertBase + (y+1) * (sliceTess+1) + x;
					}
					else
					{
						tessIndices[index+0] = vertBase + y * (sliceTess+1) + x;
						tessIndices[index+1] = vertBase + y * (sliceTess+1) + x + 1;
						tessIndices[index+2] = vertBase + (y+1) * (sliceTess+1) + x;

						tessIndices[index+3] = vertBase + (y+1) * (sliceTess+1) + x;
						tessIndices[index+4] = vertBase + y * (sliceTess+1) + x + 1;
						tessIndices[index+5] = vertBase + (y+1) * (sliceTess+1) + x + 1;
					}
					index += 6;
				}
			}
		}
	}

	glGenBuffers( 1, &geometry.vertexBuffer );
	glBindBuffer( GL_ARRAY_BUFFER, geometry.vertexBuffer );
	glBufferData( GL_ARRAY_BUFFER, floatCount * sizeof(*tessVertices), (void *)tessVertices, GL_STATIC_DRAW );
	delete[] tessVertices;

	geometry.indexCount = index;

	glGenBuffers( 1, &geometry.indexBuffer );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, geometry.indexBuffer );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, indexBytes, (void *)tessIndices, GL_STATIC_DRAW );
	delete[] tessIndices;

	glEnableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_POSITION );
	glVertexAttribPointer( VERTEX_ATTRIBUTE_LOCATION_POSITION, 2, GL_FLOAT, false, attribCount * sizeof( float ), (void *)( 0 * sizeof( float ) ) );

	glEnableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_NORMAL );
	glVertexAttribPointer( VERTEX_ATTRIBUTE_LOCATION_NORMAL, 2, GL_FLOAT, false, attribCount * sizeof( float ), (void *)( 2 * sizeof( float ) ) );

	glEnableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_UV0 );
	glVertexAttribPointer( VERTEX_ATTRIBUTE_LOCATION_UV0, 2, GL_FLOAT, false, attribCount * sizeof( float ), (void *)( 4 * sizeof( float ) ) );

	glEnableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_TANGENT );
	glVertexAttribPointer( VERTEX_ATTRIBUTE_LOCATION_TANGENT, 2, GL_FLOAT, false, attribCount * sizeof( float ), (void *)( 6 * sizeof( float ) ) );

	glEnableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_UV1 );
	glVertexAttribPointer( VERTEX_ATTRIBUTE_LOCATION_UV1, 2, GL_FLOAT, false, attribCount * sizeof( float ), (void *)( 8 * sizeof( float ) ) );

	glBindVertexArrayOES_( 0 );

	return geometry;
}

//=========================================================================================


// The abstract class still needs a destructor to get the vtable generated.
TimeWarp::~TimeWarp()
{
}

// Shim to call a C++ object from a posix thread start.
void *TimeWarpLocal::ThreadStarter( void * parm )
{
	TimeWarpLocal & tw = *(TimeWarpLocal *)parm;
	tw.WarpThread();
	return NULL;
}

void TimeWarpLocal::WarpThread()
{
	WarpThreadInit();

	// Signal the main thread to wake up and return.
	pthread_mutex_lock( &swapMutex );
	pthread_cond_signal( &swapIsLatched );
	pthread_mutex_unlock( &swapMutex );

	LOG( "WarpThreadLoop()" );

	bool removedSchedFifo = false;

	// Loop until we get a shutdown request
	for ( double vsync = 0; ; vsync++ )
	{
		const double current = ceil( GetFractionalVsync() );
		if ( abs( current - vsync ) > 2.0 )
		{
			LOG( "Changing vsync from %f to %f", vsync, current );
			vsync = current;
		}
		if ( ShutdownRequest.GetState() )
		{
			LOG( "ShutdownRequest received" );
			break;
		}

		// The time warp thread functions as a watch dog for the calling thread.
		// If the calling thread does not call WarpSwap() in a long time then SCHED_FIFO is removed
		// from the calling thread here to keep the Android watch dog from rebooting the device.
		// SCHED_FIFO is set back as soon as the calling thread calls WarpSwap() again.
		if ( SetSchedFifoMethodId != 0 )
		{
			const double currentTime = ovr_GetTimeInSeconds();
			const double lastWarpTime = LastWarpSwapTimeInSeconds.GetState();
			if ( removedSchedFifo )
			{
				if ( lastWarpTime > currentTime - 0.1 )
				{
					LOG( "TimeWarp Watchdog: restored SCHED_FIFO on tid %d", StartupTid );
					Jni->CallStaticIntMethod( InitParms.VrLibClass, SetSchedFifoMethodId, InitParms.ActivityObject, StartupTid, SCHED_FIFO_PRIORITY_VRTHREAD );
					if ( InitParms.GameThreadTid )
					{
						LOG( "TimeWarp Watchdog: restored SCHED_FIFO on tid %d", InitParms.GameThreadTid );
						Jni->CallStaticIntMethod( InitParms.VrLibClass, SetSchedFifoMethodId, InitParms.ActivityObject, InitParms.GameThreadTid, SCHED_FIFO_PRIORITY_VRTHREAD );
					}
					removedSchedFifo = false;
				}
			}
			else
			{
				if ( lastWarpTime < currentTime - 1.0 )
				{
					LOG( "TimeWarp Watchdog: removed SCHED_FIFO from tid %d", StartupTid );
					Jni->CallStaticIntMethod( InitParms.VrLibClass, SetSchedFifoMethodId, InitParms.ActivityObject, StartupTid, SCHED_FIFO_PRIORITY_NONE );
					if ( InitParms.GameThreadTid )
					{
						LOG( "TimeWarp Watchdog: removed SCHED_FIFO from tid %d", InitParms.GameThreadTid );
						Jni->CallStaticIntMethod( InitParms.VrLibClass, SetSchedFifoMethodId, InitParms.ActivityObject, InitParms.GameThreadTid, SCHED_FIFO_PRIORITY_NONE );
					}
					removedSchedFifo = true;
				}
			}
		}

		WarpToScreen( vsync,
				Screen.IsFrontBuffer() ? spAsyncFrontBufferPortrait
						: spAsyncSwappedBufferPortrait );
	}

	WarpThreadShutdown();

	LOG( "Exiting WarpThreadLoop()" );
}

/*
 * Shutdown()
 */
TimeWarpLocal::~TimeWarpLocal()
{
	LOG( "---------------- ~TimeWarpLocal() Start ----------------" );
	if ( warpThread != 0 )
	{
		// Get the background thread to kill itself.
		ShutdownRequest.SetState( true );

		LOG( "pthread_join() called");
		void * data;
		pthread_join( warpThread, &data );

		LOG( "pthread_join() returned");

		warpThread = 0;

		if ( eglGetCurrentSurface(EGL_DRAW) != eglPbufferSurface )
		{
			LOG( "eglGetCurrentSurface(EGL_DRAW) != eglPbufferSurface" );
		}

		// Attach the windowSurface to the calling context again.
		if ( eglMakeCurrent( eglDisplay, eglMainThreadSurface,
				eglMainThreadSurface, eglShareContext ) == EGL_FALSE)
		{
			FAIL( "eglMakeCurrent to window failed: %s", EglErrorString() );
		}

		// Destroy the pbuffer surface that was attached to the calling context.
		if ( EGL_FALSE == eglDestroySurface( eglDisplay, eglPbufferSurface ) )
		{
			LOG( "Failed to destroy pbuffer." );
		}
		else
		{
			LOG( "Destroyed pbuffer." );
		}
	}
	else
	{
		// Shutdown the front buffer rendering
		Screen.Shutdown();

		// Vertex array objects can only be destroyed on the context they were created on
		// If there is no warp thread then InitParms.AsynchronousTimeWarp was false and
		// CreateFrameworkGraphics() was called from the TimeWarpLocal constructor.
		DestroyFrameworkGraphics();
	}

	// Delete the image server after TimeWarp has shut down
	delete NetImageServer;
	NetImageServer = NULL;

	LOG( "---------------- ~TimeWarpLocal() End ----------------" );
}


// Not in extension string, unfortunately.
static bool IsContextPriorityExtensionPresent()
{
	EGLint currentPriorityLevel = -1;
	if ( !eglQueryContext( eglGetCurrentDisplay(), eglGetCurrentContext(), EGL_CONTEXT_PRIORITY_LEVEL_IMG, &currentPriorityLevel )
			|| currentPriorityLevel == -1 )
	{
		// If we can't report the priority, assume the extension isn't there
		return false;
	}
	return true;
}

TimeWarp * TimeWarp::Factory( TimeWarpInitParms initParms )
{
	return new TimeWarpLocal( initParms );
}

/*
 * Startup()
 */
TimeWarpLocal::TimeWarpLocal( const TimeWarpInitParms initParms ) :
	untexturedMvpProgram(),
	debugLineProgram(),
	warpPrograms(),
	blackTexture( 0 ),
	loadingTexture( 0 ),
	HasEXT_sRGB_write_control( false ),
	NetImageServer( NULL ),
	StartupTid( 0 ),
	Jni( NULL ),
	SetSchedFifoMethodId( 0 ),
	eglDisplay( 0 ),
	eglPbufferSurface( 0 ),
	eglMainThreadSurface( 0 ),
	eglConfig( 0 ),
	eglClientVersion( 0 ),
	eglShareContext( 0 ),
	eglWarpContext( 0 ),
	contextPriority( 0 ),
	eyeLog(),
	lastEyeLog( 0 ),
	LogEyeWarpGpuTime(),
	warpThread( 0 ),
	warpThreadTid( 0 ),
	LastSwapVsyncCount( 0 )
{
	// Code which auto-disable chromatic aberration expects
	// the warpProgram list to be symmetric.
	// See disableChromaticCorrection.
	OVR_COMPILER_ASSERT( ( WP_PROGRAM_MAX & 1 ) == 0 );
	OVR_COMPILER_ASSERT( ( WP_CHROMATIC - WP_SIMPLE ) ==
						 ( WP_PROGRAM_MAX - WP_CHROMATIC ) );

	ShutdownRequest.SetState( false );
	EyeBufferCount.SetState( 0 );
	memset( WarpSources, 0, sizeof( WarpSources ) );
	memset( warpPrograms, 0, sizeof( warpPrograms ) );

	// set up our synchronization primitives
	pthread_mutex_init( &swapMutex, NULL /* default attributes */ );
	pthread_cond_init( &swapIsLatched, NULL /* default attributes */ );

	LOG("-------------------- Startup() --------------------");

	// Only allow WarpSwap() to be called from this thread
	StartupTid = gettid();

	// Keep track of the last time WarpSwap() was called.
	LastWarpSwapTimeInSeconds.SetState( ovr_GetTimeInSeconds() );

	// If this isn't set, Shutdown() doesn't need to kill the thread
	warpThread = 0;

	// Save our startup parms
	InitParms = initParms;

	// No buffers have been submitted yet
	EyeBufferCount.SetState( 0 );

	//---------------------------------------------------------
	// OpenGL initialization that can be done on the main thread
	//---------------------------------------------------------

	// Get values for the current OpenGL context
	eglDisplay = eglGetCurrentDisplay();
	if ( eglDisplay == EGL_NO_DISPLAY )
	{
		FAIL( "EGL_NO_DISPLAY" );
	}
	eglMainThreadSurface = eglGetCurrentSurface(EGL_DRAW);
	if ( eglMainThreadSurface == EGL_NO_SURFACE )
	{
		FAIL( "EGL_NO_SURFACE" );
	}
	eglShareContext = eglGetCurrentContext();
	if ( eglShareContext == EGL_NO_CONTEXT )
	{
		FAIL( "EGL_NO_CONTEXT" );
	}
	EGLint configID;
	if ( !eglQueryContext( eglDisplay, eglShareContext, EGL_CONFIG_ID, &configID ) )
	{
    	FAIL( "eglQueryContext EGL_CONFIG_ID failed" );
	}
	eglConfig = EglConfigForConfigID( eglDisplay, configID );
	if ( eglConfig == NULL )
	{
    	FAIL( "EglConfigForConfigID failed" );
	} 
	if ( !eglQueryContext( eglDisplay, eglShareContext, EGL_CONTEXT_CLIENT_VERSION, (EGLint *)&eglClientVersion ) )
	{
    	FAIL( "eglQueryContext EGL_CONTEXT_CLIENT_VERSION failed" );
	}
	LOG( "Current EGL_CONTEXT_CLIENT_VERSION:%i", eglClientVersion );

	// It is wasteful for the main config to be anything but a color buffer.
	EGLint depthSize = 0;
	eglGetConfigAttrib( eglDisplay, eglConfig, EGL_DEPTH_SIZE, &depthSize );
	if ( depthSize != 0 )
	{
		LOG( "Share context eglConfig has %i depth bits -- should be 0", depthSize );
	}

	EGLint samples = 0;
	eglGetConfigAttrib( eglDisplay, eglConfig, EGL_SAMPLES, &samples );
	if ( samples != 0 )
	{
		LOG( "Share context eglConfig has %i samples -- should be 0", samples );
	}

	// See if we have sRGB_write_control extension
	HasEXT_sRGB_write_control = ExtensionStringPresent( "GL_EXT_sRGB_write_control",
			(const char *)glGetString( GL_EXTENSIONS ) );

	// Start up the network image server if requested
	if ( InitParms.EnableImageServer )
	{
		NetImageServer = new ImageServer();
	}
	else
	{
		NetImageServer = NULL;
		LOG( "Skipping ImageServer setup because !useImageServer" );
	}

	// Skip thread initialization if we are running synchronously
	if ( !InitParms.AsynchronousTimeWarp )
	{
		// The current thread is presumably already attached, so this
		// should just return a valid environment.
		const jint rtn = InitParms.JavaVm->AttachCurrentThread( &Jni, 0 );
		if ( rtn != JNI_OK )
		{
			FAIL( "AttachCurrentThread() returned %i", rtn );
		}

		Screen.InitForCurrentSurface( Jni, InitParms.FrontBuffer, InitParms.BuildVersionSDK );

		if ( Screen.windowSurface == EGL_NO_SURFACE )
		{
			FAIL( "Screen.windowSurface == EGL_NO_SURFACE" );
		}


		// create the framework graphics on this thread
		CreateFrameworkGraphics();
		LOG( "Skipping thread setup because !AsynchronousTimeWarp" );
	}
	else
	{

		//---------------------------------------------------------
		// Thread initialization
		//---------------------------------------------------------

		// Make GL current on the pbuffer, because the window will be
		// used by the background thread.

		if ( IsContextPriorityExtensionPresent() )
		{
			LOG( "Requesting EGL_CONTEXT_PRIORITY_HIGH_IMG" );
			contextPriority = EGL_CONTEXT_PRIORITY_HIGH_IMG;
		}
		else
		{
			// If we can't report the priority, assume the extension isn't there
			LOG( "IMG_Context_Priority doesn't seem to be present." );
			contextPriority = EGL_CONTEXT_PRIORITY_MEDIUM_IMG;
		}

		// Detach the windowSurface from the current context, because the
		// windowSurface will be used by the context on the background thread.
		//
		// Because EGL_KHR_surfaceless_context is not widespread (Only on Tegra as of
		// September 2013), we need to create and attach a tiny pbuffer surface to the
		// current context.
		//
		// It is necessary to use a config with the same characteristics that the
		// context was created with, plus the pbuffer flag, or we will get an
		// EGL_BAD_MATCH error on the eglMakeCurrent() call.
		const EGLint attrib_list[] =
		{
				EGL_WIDTH, 16,
				EGL_HEIGHT, 16,
				EGL_NONE
		};
		eglPbufferSurface = eglCreatePbufferSurface( eglDisplay, eglConfig, attrib_list );
		if ( eglPbufferSurface == EGL_NO_SURFACE )
		{
			FAIL( "eglCreatePbufferSurface failed: %s", EglErrorString() );
		}

		if ( eglMakeCurrent( eglDisplay, eglPbufferSurface, eglPbufferSurface,
				eglShareContext ) == EGL_FALSE )
		{
			FAIL( "eglMakeCurrent: eglMakeCurrent pbuffer failed" );
		}

		// The thread will exit when this is set true.
		ShutdownRequest.SetState( false );

		// Grab the mutex before spawning the warp thread, so it won't be
		// able to exit until we do the pthread_cond_wait
		pthread_mutex_lock( &swapMutex );

		// spawn the warp thread
		const int createErr = pthread_create( &warpThread, NULL /* default attributes */, &ThreadStarter, this );
		if ( createErr != 0 )
		{
			FAIL( "pthread_create returned %i", createErr );
		}

		// Atomically unlock the mutex and block until the warp thread
		// has completed the initialization and either failed or went
		// into WarpThreadLoop()
		pthread_cond_wait( &swapIsLatched, &swapMutex );

		// Pthread_cond_wait re-locks the mutex before exit.
		pthread_mutex_unlock( &swapMutex );
	}

	LOG("----------------- Startup() End -----------------");
}

/*
 * WarpThreadInit()
 */
void TimeWarpLocal::WarpThreadInit()
{
	LOG( "WarpThreadInit()" );

	pthread_setname_np( pthread_self(), "OVR::TimeWarp" );

	//SetCurrentThreadAffinityMask( 0xF0 );

	//---------------------------------------------------------
	// OpenGl initiailization
	//
	// On windows, GL context creation must be done on the thread that is
	// going to use the context, or random failures will occur.  This may
	// not be necessary on other platforms, but do it anyway.
	//---------------------------------------------------------

	// Create a new GL context on this thread, sharing it with the main thread context
	// so the render targets can be passed back and forth.

	EGLint contextAttribs[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, eglClientVersion,
		EGL_NONE, EGL_NONE,
		EGL_NONE
	};
	// Don't set EGL_CONTEXT_PRIORITY_LEVEL_IMG at all if set to EGL_CONTEXT_PRIORITY_MEDIUM_IMG,
	// It is the caller's responsibility to use that if the driver doesn't support it.
	if ( contextPriority != EGL_CONTEXT_PRIORITY_MEDIUM_IMG )
	{
		contextAttribs[2] = EGL_CONTEXT_PRIORITY_LEVEL_IMG;
		contextAttribs[3] = contextPriority;
	}

	eglWarpContext = eglCreateContext( eglDisplay, eglConfig, eglShareContext, contextAttribs );
	if ( eglWarpContext == EGL_NO_CONTEXT )
	{
		FAIL( "eglCreateContext failed: %s", EglErrorString() );
	}
	LOG( "eglWarpContext: %p", eglWarpContext );
	if ( contextPriority != EGL_CONTEXT_PRIORITY_MEDIUM_IMG )
	{
		// See what context priority we actually got
		EGLint actualPriorityLevel;
		eglQueryContext( eglDisplay, eglWarpContext, EGL_CONTEXT_PRIORITY_LEVEL_IMG, &actualPriorityLevel );
		switch ( actualPriorityLevel )
		{
		case EGL_CONTEXT_PRIORITY_HIGH_IMG: LOG( "Context is EGL_CONTEXT_PRIORITY_HIGH_IMG" ); break;
		case EGL_CONTEXT_PRIORITY_MEDIUM_IMG: LOG( "Context is EGL_CONTEXT_PRIORITY_MEDIUM_IMG" ); break;
		case EGL_CONTEXT_PRIORITY_LOW_IMG: LOG( "Context is EGL_CONTEXT_PRIORITY_LOW_IMG" ); break;
		default: LOG( "Context has unknown priority level" ); break;
		}
	}

	// Make the context current on the window, so no more makeCurrent calls will be needed
	LOG( "eglMakeCurrent on %p", eglMainThreadSurface );
	if ( eglMakeCurrent( eglDisplay, eglMainThreadSurface,
			eglMainThreadSurface, eglWarpContext ) == EGL_FALSE )
	{
		FAIL( "eglMakeCurrent failed: %s", EglErrorString() );
	}

	// Get a jni environment for front buffer setting and changing SCHED_FIFO
	const jint rtn = InitParms.JavaVm->AttachCurrentThread( &Jni, 0 );
	if ( rtn != JNI_OK )
	{
		FAIL( "AttachCurrentThread() returned %i", rtn );
	}
	SetSchedFifoMethodId = ovr_GetStaticMethodID( Jni, InitParms.VrLibClass, "setSchedFifoStatic", "(Landroid/app/Activity;II)I" );

	// Make the current window into a front-buffer
	// Must be called after a context is current on the window.
	Screen.InitForCurrentSurface( Jni, InitParms.FrontBuffer, InitParms.BuildVersionSDK );

	// create the framework graphics on this thread
	CreateFrameworkGraphics();

	// Get the linux tid so App can set SCHED_FIFO on it
	warpThreadTid = gettid();
}

/*
 * WarpThreadShutdown()
 */
void TimeWarpLocal::WarpThreadShutdown()
{
	// Vertex array objects can only be destroyed on the context they were created on
	DestroyFrameworkGraphics();

	// release the window so it can be made current by another thread
	if ( eglMakeCurrent( eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE,
			EGL_NO_CONTEXT ) == EGL_FALSE )
	{
		FAIL( "eglMakeCurrent: shutdown failed" );
	}

	if ( eglDestroyContext( eglDisplay, eglWarpContext ) == EGL_FALSE )
	{
		FAIL( "eglDestroyContext: shutdown failed" );
	}
	eglWarpContext = 0;

	// Shutdown the front buffer rendering
	Screen.Shutdown();

	const jint rtn = InitParms.JavaVm->DetachCurrentThread();
	if ( rtn != JNI_OK )
	{
		FAIL( "DetachCurrentThread() returned %i", rtn );
	}
}

const WarpProg & TimeWarpLocal::ProgramForParms( const TimeWarpParms & parms, const bool disableChromaticCorrection ) const
{
	int program = Alg::Clamp( (int)parms.WarpProgram, (int)WP_SIMPLE, (int)WP_PROGRAM_MAX - 1 );

	if ( disableChromaticCorrection && program >= WP_CHROMATIC )
	{
		program -= ( WP_CHROMATIC - WP_SIMPLE );
	}
	return warpPrograms[program];
}

void TimeWarpLocal::SetWarpState( const warpSource_t & currentWarpSource ) const
{
	glDepthMask( GL_FALSE );	// don't write to depth, even if Unity has depth on window
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_CULL_FACE );
	glDisable( GL_BLEND );
	glEnable( GL_SCISSOR_TEST );

	// sRGB conversion on write will only happen if BOTH the window
	// surface was created with EGL_GL_COLORSPACE_KHR,  EGL_GL_COLORSPACE_SRGB_KHR
	// and GL_FRAMEBUFFER_SRGB_EXT is enabled.
	if ( HasEXT_sRGB_write_control )
	{
		if ( currentWarpSource.WarpParms.SwapOptions & SWAP_INHIBIT_SRGB_FRAMEBUFFER )
		{
			glDisable( GL_FRAMEBUFFER_SRGB_EXT );
		}
		else
		{
			glEnable( GL_FRAMEBUFFER_SRGB_EXT );
		}
	}
	GL_CheckErrors( "SetWarpState" );
}

void TimeWarpLocal::BindWarpProgram( const warpSource_t & currentWarpSource, const Matrix4f timeWarps[2][2], const int eye, const double vsyncBase ) const
{
	// TODO: bake this into the buffer objects
	const Matrix4f landscapeOrientationMatrix(
	    		1.0f, 0.0f, 0.0f, 0.0f,
	            0.0f, 1.0f, 0.0f, 0.0f,
	            0.0f, 0.0f, 0.0f, 0.0f,
	            0.0f, 0.0f, 0.0f, 1.0f );

	// Select the warp program.
	const WarpProg & warpProg = ProgramForParms( currentWarpSource.WarpParms, currentWarpSource.disableChromaticCorrection );
	const GlProgram & prog = warpProg.Prog;
	glUseProgram( prog.program );

	// Set the shader parameters.
	glUniform1f( prog.uColor, currentWarpSource.WarpParms.ProgramParms[0] );

	glUniformMatrix4fv( prog.uMvp, 1, GL_FALSE, landscapeOrientationMatrix.Transposed().M[0] );
	glUniformMatrix4fv( prog.uTexm, 1, GL_FALSE, timeWarps[0][0].Transposed().M[0] );
	glUniformMatrix4fv( prog.uTexm2, 1, GL_FALSE, timeWarps[0][1].Transposed().M[0] );
	if ( warpProg.Texm3 > 0 )
	{
		glUniformMatrix4fv( warpProg.Texm3, 1, GL_FALSE, timeWarps[1][0].Transposed().M[0] );
		glUniformMatrix4fv( warpProg.Texm4, 1, GL_FALSE, timeWarps[1][1].Transposed().M[0] );
	}
	if ( warpProg.TexClamp > 0 )
	{// split screen clamping for UE4
		const Vector2f clamp( eye * 0.5f, (eye+1)* 0.5f );
		glUniform2fv( warpProg.TexClamp, 1, &clamp.x );
	}
	if ( warpProg.RotateScale > 0 )
	{
		const float angle = FramePointTimeInSeconds( vsyncBase ) * M_PI * LOADING_ICON_ROTATION;
		const Vector4f RotateScale( sinf( angle ), cosf( angle ), LOADING_ICON_SCALE, 1.0f );
		glUniform4fv( warpProg.RotateScale, 1, &RotateScale[0] );
	}
}

static void BindEyeTextures( const warpSource_t & currentWarpSource, const ScreenEye eye, const GlTexture loadingTexture )
{
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, currentWarpSource.WarpParms.Images[eye][0].TexId );
	if ( HasEXT_sRGB_texture_decode )
	{
		if ( currentWarpSource.WarpParms.SwapOptions & SWAP_INHIBIT_SRGB_FRAMEBUFFER )
		{
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SRGB_DECODE_EXT, GL_SKIP_DECODE_EXT );
		}
		else
		{
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SRGB_DECODE_EXT, GL_DECODE_EXT );
		}
	}

	if ( currentWarpSource.WarpParms.WarpProgram == WP_MASKED_PLANE
		|| currentWarpSource.WarpParms.WarpProgram == WP_CHROMATIC_MASKED_PLANE
		|| currentWarpSource.WarpParms.WarpProgram == WP_OVERLAY_PLANE
		|| currentWarpSource.WarpParms.WarpProgram == WP_CHROMATIC_OVERLAY_PLANE
		|| currentWarpSource.WarpParms.WarpProgram == WP_OVERLAY_PLANE_SHOW_LOD
		|| currentWarpSource.WarpParms.WarpProgram == WP_CHROMATIC_OVERLAY_PLANE_SHOW_LOD
			)
	{
		glActiveTexture( GL_TEXTURE1 );
		glBindTexture( GL_TEXTURE_2D, currentWarpSource.WarpParms.Images[eye][1].TexId );
		if ( HasEXT_sRGB_texture_decode )
		{
			if ( currentWarpSource.WarpParms.SwapOptions & SWAP_INHIBIT_SRGB_FRAMEBUFFER )
			{
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SRGB_DECODE_EXT, GL_SKIP_DECODE_EXT );
			}
			else
			{
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SRGB_DECODE_EXT, GL_DECODE_EXT );
			}
		}
	}
	if ( currentWarpSource.WarpParms.WarpProgram == WP_MASKED_PLANE_EXTERNAL || currentWarpSource.WarpParms.WarpProgram == WP_CHROMATIC_MASKED_PLANE_EXTERNAL )
	{
		glActiveTexture( GL_TEXTURE1 );
		glBindTexture( GL_TEXTURE_EXTERNAL_OES, currentWarpSource.WarpParms.Images[eye][1].TexId );
		if ( HasEXT_sRGB_texture_decode )
		{
			if ( currentWarpSource.WarpParms.SwapOptions & SWAP_INHIBIT_SRGB_FRAMEBUFFER )
			{
				glTexParameteri( GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_SRGB_DECODE_EXT, GL_SKIP_DECODE_EXT );
			}
			else
			{
				glTexParameteri( GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_SRGB_DECODE_EXT, GL_DECODE_EXT );
			}
		}
	}
	if ( currentWarpSource.WarpParms.WarpProgram == WP_MASKED_CUBE || currentWarpSource.WarpParms.WarpProgram == WP_CHROMATIC_MASKED_CUBE )
	{
		glActiveTexture( GL_TEXTURE1 );
		glBindTexture( GL_TEXTURE_CUBE_MAP, currentWarpSource.WarpParms.Images[eye][1].TexId );
		if ( HasEXT_sRGB_texture_decode )
		{
			if ( currentWarpSource.WarpParms.SwapOptions & SWAP_INHIBIT_SRGB_FRAMEBUFFER )
			{
				glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_SRGB_DECODE_EXT, GL_SKIP_DECODE_EXT );
			}
			else
			{
				glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_SRGB_DECODE_EXT, GL_DECODE_EXT );
			}
		}
	}

	if ( currentWarpSource.WarpParms.WarpProgram == WP_CUBE || currentWarpSource.WarpParms.WarpProgram == WP_CHROMATIC_CUBE )
	{
		for ( int i = 0; i < 3; i++ )
		{
			glActiveTexture( GL_TEXTURE1 + i );
			glBindTexture( GL_TEXTURE_CUBE_MAP, currentWarpSource.WarpParms.Images[eye][1].PlanarTexId[i] );
			if ( HasEXT_sRGB_texture_decode )
			{
				if ( currentWarpSource.WarpParms.SwapOptions & SWAP_INHIBIT_SRGB_FRAMEBUFFER )
				{
					glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_SRGB_DECODE_EXT, GL_SKIP_DECODE_EXT );
				}
				else
				{
					glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_SRGB_DECODE_EXT, GL_DECODE_EXT );
				}
			}
		}
	}

	if ( currentWarpSource.WarpParms.WarpProgram == WP_LOADING_ICON || currentWarpSource.WarpParms.WarpProgram == WP_CHROMATIC_LOADING_ICON )
	{
		glActiveTexture( GL_TEXTURE1 );
		glBindTexture( GL_TEXTURE_2D, loadingTexture );
		if ( HasEXT_sRGB_texture_decode )
		{
			if ( currentWarpSource.WarpParms.SwapOptions & SWAP_INHIBIT_SRGB_FRAMEBUFFER )
			{
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SRGB_DECODE_EXT, GL_SKIP_DECODE_EXT );
			}
			else
			{
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SRGB_DECODE_EXT, GL_DECODE_EXT );
			}
		}
	}
}

static void UnbindEyeTextures()
{
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, 0 );

	glActiveTexture( GL_TEXTURE1 );
	glBindTexture( GL_TEXTURE_2D, 0 );
	glBindTexture( GL_TEXTURE_EXTERNAL_OES, 0 );
	glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );

	for ( int i = 1; i < 3; i++ )
	{
		glActiveTexture( GL_TEXTURE1 + i );
		glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );
	}
}

/*
 * WarpToScreen
 *
 * Can be called by either the dedicated warp thread, or by the
 * application thread at swap time if running synchronously.
 *
 * Wait for sync point, read sensor, Warp both eyes and returns the next vsyncBase
 *
 * Calls GetFractionalVsync() multiple times, but this only calls kernel time functions, not java
 * Calls SleepUntilTimePoint() for each eye.
 * May write to the log
 * Writes eyeLog[]
 * Reads warpSources
 * Reads eyeBufferCount
 * May lock and unlock swapMutex
 * May signal swapIsLatched
 * Writes LastSwap
 *
 */
void TimeWarpLocal::WarpToScreen(
		const double 			vsyncBase_,
		const swapProgram_t &	swap )
{
	static double lastReportTime = 0;
	const double timeNow = floor( ovr_GetTimeInSeconds() );
	if ( timeNow > lastReportTime )
	{
		LOG( "Warp GPU time: %3.1f ms", LogEyeWarpGpuTime.GetTotalTime() );
		lastReportTime = timeNow;
	}

	const warpSource_t & latestWarpSource = WarpSources[EyeBufferCount.GetState()%MAX_WARP_SOURCES];

	// switch to sliced rendering
	if ( latestWarpSource.WarpParms.SwapOptions & SWAP_OPTION_USE_SLICED_WARP )
	{
		WarpToScreenSliced( vsyncBase_ );
		return;
	}

	const double vsyncBase = vsyncBase_;

	// Build new line verts if timing graph is enabled
	UpdateTimingGraphVerts( latestWarpSource.WarpParms.DebugGraphMode, latestWarpSource.WarpParms.DebugGraphValue );

	// This will only be updated in SCREENEYE_LEFT
	warpSource_t currentWarpSource = {};

	// The mesh covers the full screen, but we only draw part of it at a time
	int screenWidth, screenHeight;
	Screen.GetScreenResolution( screenWidth, screenHeight );
	glViewport( 0, 0, screenWidth, screenHeight );
	glScissor( 0, 0, screenWidth, screenHeight );

	// Warp each eye to the display surface
	for ( ScreenEye eye = SCREENEYE_LEFT; eye <= SCREENEYE_RIGHT; eye = (ScreenEye)((int)eye+1) )
	{
		//LOG( "Eye %i: now=%f  sleepTo=%f", eye, GetFractionalVsync(), vsyncBase + swap.deltaVsync[eye] );

		// Sleep until we are in the correct half of the screen for
		// rendering this eye.  If we are running single threaded,
		// the first eye will probably already be past the sleep point,
		// so only the second eye will be at a dependable time.
		const double sleepTargetVsync = vsyncBase + swap.deltaVsync[eye];
		const double sleepTargetTime = FramePointTimeInSeconds( sleepTargetVsync );
		const float secondsToSleep = SleepUntilTimePoint( sleepTargetTime, false );
		const double preFinish = TimeInSeconds();

		//LOG( "Vsync %f:%i sleep %f", vsyncBase, eye, secondsToSleep );

		// Check for availability of updated eye renderings
		// now that we are about to render.
		long long thisEyeBufferNum = 0;
		int	back;

		if ( eye == SCREENEYE_LEFT )
		{
			const long long latestEyeBufferNum = EyeBufferCount.GetState();
			for ( back = 0; back < MAX_WARP_SOURCES - 1; back++ )
			{
				thisEyeBufferNum = latestEyeBufferNum - back;
				if ( thisEyeBufferNum <= 0 )
				{	// just starting, and we don't have any eye buffers to use
					break;
				}
				warpSource_t & testWarpSource = WarpSources[thisEyeBufferNum % MAX_WARP_SOURCES];
				if ( testWarpSource.MinimumVsync > vsyncBase )
				{	// a full frame got completed in less time than a single eye; don't use it to avoid stuttering
					continue;
				}
				if ( testWarpSource.GpuSync == 0 )
				{
					LOG( "thisEyeBufferNum %lli had 0 sync", thisEyeBufferNum );
					break;
				}

				if ( Quatf( testWarpSource.WarpParms.Images[eye][0].Pose.Pose.Orientation ).LengthSq() < 1e-18f )
				{
					LOG( "Bad Pose.Orientation in bufferNum %lli!", thisEyeBufferNum );
					break;
				}

				const EGLint wait = eglClientWaitSyncKHR_( eglDisplay, testWarpSource.GpuSync,
						EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, 0 );
				if ( wait == EGL_TIMEOUT_EXPIRED_KHR )
				{
					continue;
				}
				if ( wait == EGL_FALSE )
				{
					LOG( "eglClientWaitSyncKHR returned EGL_FALSE" );
				}

				// This buffer set is good to use
				if ( testWarpSource.FirstDisplayedVsync[eye] == 0 )
				{
					testWarpSource.FirstDisplayedVsync[eye] = (long long)vsyncBase;
				}
				currentWarpSource = testWarpSource;
				break;
			}

			// Save this sensor state for the next application rendering frame.
			// It is important that this always be done, even if we wind up
			// not rendering anything because there are no current eye buffers.
			{
				SwapState	state;
				state.VsyncCount = (long long)vsyncBase;
				state.EyeBufferCount = thisEyeBufferNum;
				SwapVsync.SetState( state );
				// Wake the VR thread up if it is blocked on us.
				// If the other thread happened to be scheduled out right
				// after locking the mutex, but before waiting on the condition,
				// we would rather it sleep for another frame than potentially
				// miss a raster point here in the time warp thread, so use
				// a trylock() instead of a lock().
				if ( !pthread_mutex_trylock( &swapMutex ) )
				{
					pthread_cond_signal( &swapIsLatched );
					pthread_mutex_unlock( &swapMutex );
				}
			}

			if ( Screen.windowSurface == EGL_NO_SURFACE )
			{
				static int logCount = 0;
				if ( ( logCount++ & 31 ) == 0 )
				{
					LOG( "WarpToScreen: no valid window surface" );
				}
				return;
			}

			if ( currentWarpSource.WarpParms.Images[eye][0].TexId == 0 )
			{
				// We don't have anything valid to draw, so just sleep until
				// the next time point and check again.
				LOG( "No valid eyeTexture %i", eye );
				SleepUntilTimePoint( FramePointTimeInSeconds( sleepTargetVsync + 1.0f ), false );
				break;
			}
		}

		// Build up the external velocity transform
		Matrix4f velocity;
		const int velocitySteps = OVR::Alg::Min( 3, (int)((long long)vsyncBase - currentWarpSource.MinimumVsync) );
		for ( int i = 0; i < velocitySteps; i++ )
		{
			velocity = velocity * currentWarpSource.WarpParms.ExternalVelocity;
		}

		// If we have a second image layer, we will need to calculate
		// a second set of time warps and use a different program.
		const bool dualLayer = ( currentWarpSource.WarpParms.Images[eye][1].TexId > 0 );

		// Calculate predicted poses for the start and end of this eye's
		// raster scanout, so we can warp the best image we have to it.
		//
		// These prediction points will always be in the future, because we
		// need time to do the drawing before the scanout starts.
		//
		// In a portrait scanned display, it is beneficial to have the time warp calculated
		// independently for each eye, giving them the same latency profile.
		Matrix4f timeWarps[2][2];

		for ( int scan = 0; scan < 2; scan++ )
		{
			const double vsyncPoint = vsyncBase + swap.predictionPoints[eye][scan];
			const double timePoint = FramePointTimeInSeconds( vsyncPoint );
			const ovrSensorState sensor = ovrHmd_GetSensorState( InitParms.Hmd, timePoint, false);
			const Matrix4f warp = CalculateTimeWarpMatrix2(
										currentWarpSource.WarpParms.Images[eye][0].Pose.Pose.Orientation,
										sensor.Predicted.Pose.Orientation ) * velocity;
			timeWarps[0][scan] = Matrix4f( currentWarpSource.WarpParms.Images[eye][0].TexCoordsFromTanAngles ) * warp;
			if ( dualLayer )
			{
				if ( currentWarpSource.WarpParms.SwapOptions & SWAP_OPTION_FIXED_OVERLAY )
				{	// locked-to-face HUD
					timeWarps[1][scan] = Matrix4f( currentWarpSource.WarpParms.Images[eye][1].TexCoordsFromTanAngles );
				}
				else
				{	// locked-to-world surface
					timeWarps[1][scan] = Matrix4f( currentWarpSource.WarpParms.Images[eye][1].TexCoordsFromTanAngles ) * warp;
				}
			}
		}

		//---------------------------------------------------------
		// Warp a latched buffer to the screen
		//---------------------------------------------------------

		LogEyeWarpGpuTime.Begin( eye );
		LogEyeWarpGpuTime.PrintTime( eye, "GPU time for eye time warp" );

		SetWarpState( currentWarpSource );

		BindWarpProgram( currentWarpSource, timeWarps, eye, vsyncBase );

		BindEyeTextures( currentWarpSource, eye, loadingTexture );

		Screen.BeginDirectRendering( eye * screenWidth/2, 0, screenWidth/2, screenHeight );

		// Draw the warp triangles.
		glBindVertexArrayOES_( warpMesh.vertexArrayObject );
		const int indexCount = warpMesh.indexCount / 2;
		const int indexOffset = eye * indexCount;
		glDrawElements( GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, (void *)(indexOffset * 2 ) );

		// Draw the screen vignette, calibration grid, and debug graphs.
		// The grid will be based on the orientation that the warpSource
		// was rendered at, so you can see the amount of interpolated time warp applied
		// to it as a delta from the red grid to the greed or blue grid that was drawn directly
		// into the texture.
		DrawFrameworkGraphicsToWindow( eye, currentWarpSource.WarpParms.SwapOptions,
				currentWarpSource.WarpParms.DebugGraphMode != DEBUG_PERF_OFF );

		Screen.EndDirectRendering();

		LogEyeWarpGpuTime.End( eye );

		const double justBeforeFinish = TimeInSeconds();
		if ( Screen.IsFrontBuffer() )
		{
			GL_Finish();
		}

		const double postFinish = TimeInSeconds();

		const float latency = postFinish - justBeforeFinish;
		if ( latency > 0.008f )
		{
			LOG( "Frame %i Eye %i latency %5.3f", (int)vsyncBase, eye, latency );
		}

		if ( 0 )
		{
			LOG( "eye %i sleep %5.3f fin %5.3f buf %lli (%i back):%i %i",
					eye, secondsToSleep,
					postFinish - preFinish,
					thisEyeBufferNum, back,
					currentWarpSource.WarpParms.Images[0][0].TexId,
					currentWarpSource.WarpParms.Images[1][0].TexId );
		}

		// Update debug graph data
		if ( currentWarpSource.WarpParms.DebugGraphMode != DEBUG_PERF_FROZEN )
		{
			const int logIndex = (int)lastEyeLog & (EYE_LOG_COUNT-1);
			eyeLog_t & thisLog = eyeLog[logIndex];
			thisLog.skipped = false;
			thisLog.bufferNum = thisEyeBufferNum;
			thisLog.issueFinish = preFinish - sleepTargetTime;
			thisLog.completeFinish = postFinish - sleepTargetTime;
			lastEyeLog++;
		}

	}	// for eye

	UnbindEyeTextures();

	glUseProgram( 0 );

	glBindVertexArrayOES_( 0 );

	if ( !Screen.IsFrontBuffer() )
	{
		Screen.SwapBuffers();
	}
}

void TimeWarpLocal::WarpToScreenSliced( const double vsyncBase )
{
	// Fetch vsync timing information once, so we don't have to worry
	// about it changing slightly inside a given frame.
	const VsyncState vsyncState = UpdatedVsyncState.GetState();
	if ( vsyncState.vsyncBaseNano == 0 )
	{
		return;
	}

	// all necessary time points can now be calculated

	// Because there are blanking lines at the bottom, there will always be a longer
	// sleep for the first slice than the remainder.
	double	sliceTimes[NUM_SLICES_PER_SCREEN+1];

	static const double startBias = 0.0; // 8.0/1920.0/60.0;	// about 8 pixels into a 1920 screen at 60 hz
	static const double activeFraction = 112.0 / 135;			// the remainder are blanking lines
	for ( int i = 0; i <= NUM_SLICES_PER_SCREEN; i++ )
	{
		const double framePoint = vsyncBase + activeFraction * (float)i / NUM_SLICES_PER_SCREEN;
		sliceTimes[i] = ( vsyncState.vsyncBaseNano +
				( framePoint - vsyncState.vsyncCount ) * vsyncState.vsyncPeriodNano )
				* 0.000000001 + startBias;
	}

	int screenWide, screenTall;
	Screen.GetScreenResolution( screenWide, screenTall );
	glViewport( 0, 0, screenWide, screenTall );
	glScissor( 0, 0, screenWide, screenTall );

	// This must be long enough to cover CPU scheduling delays, GPU in-flight commands,
	// and the actual drawing of this slice.
	const warpSource_t & latestWarpSource = WarpSources[EyeBufferCount.GetState()%MAX_WARP_SOURCES];
	const double	schedulingCushion = latestWarpSource.WarpParms.PreScheduleSeconds;

	//LOG( "now %fv(%i) %f cush %f", GetFractionalVsync(), (int)vsyncBase, ovr_GetTimeInSeconds(), schedulingCushion );

	// Warp each slice to the display surface
	warpSource_t currentWarpSource = {};
	int	back = 0;	// frame back from most recent
	long long thisEyeBufferNum = 0;
	for ( int screenSlice = 0; screenSlice < NUM_SLICES_PER_SCREEN; screenSlice++ )
	{
		const ScreenEye	eye = (ScreenEye)( screenSlice / NUM_SLICES_PER_EYE );

		// Sleep until we are in the correct part of the screen for
		// rendering this slice.
		const double sleepTargetTime = sliceTimes[ screenSlice ] - schedulingCushion;
		const float secondsToSleep = SleepUntilTimePoint( sleepTargetTime, false );
		const double preFinish = TimeInSeconds();

		//LOG( "slice %i targ %f slept %f", screenSlice, sleepTargetTime, secondsToSleep );

		// Check for availability of updated eye renderings on the first eye
		if ( screenSlice == 0 )
		{
			const long long latestEyeBufferNum = EyeBufferCount.GetState();
			for ( back = 0; back < MAX_WARP_SOURCES - 1; back++ )
			{
				thisEyeBufferNum = latestEyeBufferNum - back;
				if ( thisEyeBufferNum <= 0 )
				{	// just starting, and we don't have any eye buffers to use
					break;
				}
				warpSource_t & testWarpSource = WarpSources[thisEyeBufferNum % MAX_WARP_SOURCES];
				if ( testWarpSource.MinimumVsync > vsyncBase )
				{	// a full frame got completed in less time than a single eye; don't use it to avoid stuttering
					continue;
				}
				if ( testWarpSource.GpuSync == 0 )
				{
					LOG( "thisEyeBufferNum %lli had 0 sync", thisEyeBufferNum );
					break;
				}

				if ( Quatf( testWarpSource.WarpParms.Images[eye][0].Pose.Pose.Orientation ).LengthSq() < 1e-18f )
				{
					LOG( "Bad Predicted.Pose.Orientation!" );
					continue;
				}

				const EGLint wait = eglClientWaitSyncKHR_( eglDisplay, testWarpSource.GpuSync,
						EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, 0 );
				if ( wait == EGL_TIMEOUT_EXPIRED_KHR )
				{
					continue;
				}
				if ( wait == EGL_FALSE )
				{
					LOG( "eglClientWaitSyncKHR returned EGL_FALSE" );
				}

				// This buffer set is good to use
				if ( testWarpSource.FirstDisplayedVsync[eye] == 0 )
				{
					testWarpSource.FirstDisplayedVsync[eye] = (long long)vsyncBase;
				}
				currentWarpSource = testWarpSource;
				break;
			}

			// Release the VR thread if it is blocking on a frame being completed.
			// It is important that this always be done, even if we wind up
			// not rendering anything because there are no current eye buffers.
			if ( screenSlice == 0 )
			{
				SwapState	state;
				state.VsyncCount = (long long)vsyncBase;
				state.EyeBufferCount = thisEyeBufferNum;
				SwapVsync.SetState( state );

				// Wake the VR thread up if it is blocked on us.
				// If the other thread happened to be scheduled out right
				// after locking the mutex, but before waiting on the condition,
				// we would rather it sleep for another frame than potentially
				// miss a raster point here in the time warp thread, so use
				// a trylock() instead of a lock().
				if ( !pthread_mutex_trylock( &swapMutex ) )
				{
					pthread_cond_signal( &swapIsLatched );
					pthread_mutex_unlock( &swapMutex );
				}
			}

			if ( Screen.windowSurface == EGL_NO_SURFACE )
			{
				static int logCount = 0;
				if ( ( logCount++ & 31 ) == 0 )
				{
					LOG( "WarpToScreen: no valid window surface" );
				}
				return;
			}

			if ( currentWarpSource.WarpParms.Images[eye][0].TexId == 0 )
			{
				// We don't have anything valid to draw, so just sleep until
				// the next time point and check again.
				LOG( "No valid eyeTexture" );
				SleepUntilTimePoint( FramePointTimeInSeconds( vsyncBase + 1.0f ), false );
				break;
			}
		}

		// Build up the external velocity transform
		Matrix4f velocity;
		const int velocitySteps = OVR::Alg::Min( 3, (int)((long long)vsyncBase - currentWarpSource.MinimumVsync) );
		for ( int i = 0; i < velocitySteps; i++ )
		{
			velocity = velocity * currentWarpSource.WarpParms.ExternalVelocity;
		}

		// If we have a second image layer, we will need to calculate
		// a second set of time warps and use a different program.
		const bool dualLayer = ( currentWarpSource.WarpParms.Images[eye][1].TexId > 0 );

		// Calculate predicted poses for the start and end of this eye's
		// raster scanout, so we can warp the best image we have to it.
		//
		// These prediction points will always be in the future, because we
		// need time to do the drawing before the scanout starts.
		//
		// In a portrait scanned display, it is beneficial to have the time warp calculated
		// independently for each eye, giving them the same latency profile.
		Matrix4f timeWarps[2][2];

		for ( int scan = 0; scan < 2; scan++ )
		{
			// We always make a new prediciton for the end of the slice,
			// but we only make a new one for the start of the slice when a
			// new eye has just started, otherwise we could get a visible
			// seam at the slice boundary when the prediction changed.
			static Matrix4f	warp;
			if ( scan == 1 || screenSlice == 0 || screenSlice == NUM_SLICES_PER_EYE )
			{
				// SliceTimes should be the actual time the pixels hit the screen,
				// but we may want a slight adjustment on the prediction time.
				const double timePoint = sliceTimes[screenSlice + scan];
				const ovrSensorState sensor = ovrHmd_GetSensorState( InitParms.Hmd, timePoint, false );
				warp = CalculateTimeWarpMatrix2(
							currentWarpSource.WarpParms.Images[eye][0].Pose.Pose.Orientation,
							sensor.Predicted.Pose.Orientation ) * velocity;
			}
			timeWarps[0][scan] = Matrix4f( currentWarpSource.WarpParms.Images[eye][0].TexCoordsFromTanAngles ) * warp;
			if ( dualLayer )
			{
				timeWarps[1][scan] = Matrix4f( currentWarpSource.WarpParms.Images[eye][1].TexCoordsFromTanAngles ) * warp;
			}
		}

		//---------------------------------------------------------
		// Warp a latched buffer to the screen
		//---------------------------------------------------------

		LogEyeWarpGpuTime.Begin( screenSlice );
		LogEyeWarpGpuTime.PrintTime( screenSlice, "GPU time for eye time warp" );

		SetWarpState( currentWarpSource );

		BindWarpProgram( currentWarpSource, timeWarps, eye, vsyncBase );

		if ( screenSlice == 0 || screenSlice == NUM_SLICES_PER_EYE )
		{
			BindEyeTextures( currentWarpSource, eye, loadingTexture );
		}

		const int sliceSize = screenWide / NUM_SLICES_PER_SCREEN;

		Screen.BeginDirectRendering( sliceSize*screenSlice, 0, sliceSize, screenTall );

		// Draw the warp triangles.
		const GlGeometry & mesh = fileMesh.indexCount ? fileMesh : sliceMesh;
		glBindVertexArrayOES_( mesh.vertexArrayObject );
		const int indexCount = mesh.indexCount / NUM_SLICES_PER_SCREEN;
		const int indexOffset = screenSlice * indexCount;
		glDrawElements( GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, (void *)(indexOffset * 2 ) );

		if ( 0 )
		{	// solid color flashing test to see if slices are rendering in the right place
			const int cycleColor = (int)vsyncBase + screenSlice;
			glClearColor( cycleColor & 1, ( cycleColor >> 1 ) & 1, ( cycleColor >> 2 ) & 1, 1 );
			glClear( GL_COLOR_BUFFER_BIT );
		}

		// Draw the screen vignette, calibration grid, and debug graphs.
		// The grid will be based on the orientation that the warpSource
		// was rendered at, so you can see the amount of interpolated time warp applied
		// to it as a delta from the red grid to the greed or blue grid that was drawn directly
		// into the texture.
		DrawFrameworkGraphicsToWindow( eye, currentWarpSource.WarpParms.SwapOptions,
				currentWarpSource.WarpParms.DebugGraphMode != DEBUG_PERF_OFF );

		Screen.EndDirectRendering();

		LogEyeWarpGpuTime.End( screenSlice );

		//GL_Finish();

		const double postFinish = TimeInSeconds();

		if ( 0 )
		{
			LOG( "slice %i sleep %7.4f fin %6.4f buf %lli (%i back)",
					screenSlice, secondsToSleep,
					postFinish - preFinish,
					thisEyeBufferNum, back );
		}

	}	// for screenSlice

	UnbindEyeTextures();

	glUseProgram( 0 );

	glBindVertexArrayOES_( 0 );

	GL_Finish();
}

/*
 * GetNanoSecondsUint64()
 */
static uint64_t GetNanoSecondsUint64()
{
	struct timespec now;
	clock_gettime( CLOCK_MONOTONIC, &now );
	return (uint64_t) now.tv_sec * 1000000000LL + now.tv_nsec;
}

/*
 * Called by the application thread to have a pair eye buffers
 * time warped to the screen.
 *
 * Blocks until it is time to render a new pair of eye buffers.
 *
 */
void TimeWarpLocal::WarpSwap( const TimeWarpParms & parms )
{
	if ( gettid() != StartupTid )
	{
		FAIL( "WarpSwap: Called with tid %i instead of %i", gettid(), StartupTid );
	}

	if ( Screen.windowSurface == EGL_NO_SURFACE )
	{
		FAIL( "WarpSwap: no valid window surface" );
	}

	// Keep track of the last time WarpSwap() was called.
	LastWarpSwapTimeInSeconds.SetState( ovr_GetTimeInSeconds() );

	// Explicitly bind the framebuffer back to the default, so the
	// eye targets will not be bound while they might be used as warp sources.
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	// If we are running the image server, let it add commands for resampling
	// the eye buffer to a transfer buffer.
 	if ( NetImageServer )
	{
		NetImageServer->EnterWarpSwap( parms.Images[0][0].TexId );
	}

	const int minimumVsyncs = ( ovr_GetPowerLevelStateThrottled() ) ? 2 : parms.MinimumVsyncs;

	// Prepare to pass this data to the background thread if we are running
	// multi-threaded.
	const long long lastBufferCount = EyeBufferCount.GetState();
	warpSource_t & ws = WarpSources[ ( lastBufferCount + 1 ) % MAX_WARP_SOURCES ];
	ws.MinimumVsync = LastSwapVsyncCount + 2 * minimumVsyncs;	// don't use it if from same frame to avoid problems with very fast frames
	ws.FirstDisplayedVsync[0] = 0;			// will be set when it becomes the currentSource
	ws.FirstDisplayedVsync[1] = 0;			// will be set when it becomes the currentSource
	ws.disableChromaticCorrection = ( ovr_GetPowerLevelStateThrottled() || ( EglGetGpuType() & OVR::GPU_TYPE_MALI ) != 0 );
	ws.WarpParms = parms;

	// Destroy the sync object that was created for this buffer set.
	if ( ws.GpuSync )
	{
		if (  EGL_FALSE == eglDestroySyncKHR_( eglDisplay, ws.GpuSync ) )
		{
			LOG( "eglDestroySyncKHR returned EGL_FALSE" );
		}
	}

	// Add a sync object for this buffer set.
	ws.GpuSync = eglCreateSyncKHR_( eglDisplay, EGL_SYNC_FENCE_KHR, NULL );
	if ( ws.GpuSync == EGL_NO_SYNC_KHR )
	{
		FAIL( "eglCreateSyncKHR_():EGL_NO_SYNC_KHR" );
	}

	// Force it to flush the commands
	if ( EGL_FALSE == eglClientWaitSyncKHR_( eglDisplay, ws.GpuSync,
			EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, 0 ) )
	{
		LOG( "eglClientWaitSyncKHR returned EGL_FALSE" );
	}

	// Submit this buffer set for use by the timewarp thread
//	LOG( "submitting bufferNum %lli: %i %i", lastBufferCount+1,
//			ws.WarpParms.Images[0][0].TexId, ws.WarpParms.Images[1][0].TexId );
	EyeBufferCount.SetState( lastBufferCount + 1 );

	// If we are running synchronously instead of using a background
	// thread, call WarpToScreen() directly.
	if ( !InitParms.AsynchronousTimeWarp )
	{
		// Make sure all eye drawing is completed before we warp the drawing
		// to the display buffer.
		GL_Finish();

		swapProgram_t * swapProg;
		if ( Screen.IsFrontBuffer() )
		{
			swapProg = &spSyncFrontBufferPortrait;
		}
		else
		{
			swapProg = &spSyncSwappedBufferPortrait;
		}

		WarpToScreen( floor( GetFractionalVsync() ), *swapProg );

		const SwapState state = SwapVsync.GetState();
		LastSwapVsyncCount = state.VsyncCount;
		// If we are running the image server, let it start a transfer
		// of the last completed image buffer.
		if ( NetImageServer )
		{
			NetImageServer->LeaveWarpSwap();
		}
		return;
	}

	for ( ; ; )
	{
		const uint64_t startSuspendNanoSeconds = GetNanoSecondsUint64();

		// block until the next vsync
		pthread_mutex_lock( &swapMutex );

		// Atomically unlock the mutex and block until the warp thread
		// has completed a warp and sampled the sensors, updating LastSwap.
		pthread_cond_wait( &swapIsLatched, &swapMutex );

		// Pthread_cond_wait re-locks the mutex before exit.
		pthread_mutex_unlock( &swapMutex );

		const uint64_t endSuspendNanoSeconds = GetNanoSecondsUint64();

		const SwapState state = SwapVsync.GetState();
		if ( state.EyeBufferCount >= lastBufferCount )
		{
			// If MinimumVsyncs was increased dynamically, it is necessary
			// to skip one or more vsyncs just as the change happens.
			LastSwapVsyncCount = Alg::Max( state.VsyncCount, LastSwapVsyncCount + minimumVsyncs );

			// If we are running the image server, let it start a transfer
			// of the last completed image buffer.
			if ( NetImageServer )
			{
				NetImageServer->LeaveWarpSwap();
			}

			// Sleep for at least one millisecond to make sure the main VR thread
			// cannot completely deny the Android watchdog from getting a time slice.
			const uint64_t suspendNanoSeconds = endSuspendNanoSeconds - startSuspendNanoSeconds;
			if ( suspendNanoSeconds < 1000 * 1000 )
			{
				const uint64_t suspendMicroSeconds = ( 1000 * 1000 - suspendNanoSeconds ) / 1000;
				LOG( "WarpSwap: usleep( %lld )", suspendMicroSeconds );
				usleep( suspendMicroSeconds );
			}
			return;
		}
	}
}

void TimeWarpLocal::WarpSwapBlack( const float fovDegrees )
{
	const Matrix4f proj = Matrix4f::PerspectiveRH( DegreeToRad( fovDegrees ), 1.0f, 1, 100 );
	const Matrix4f texCoordsFromTanAngles = TanAngleMatrixFromProjection( proj );

	TimeWarpParms parms;
	parms.WarpProgram = WP_SIMPLE;
	parms.Images[0][0].TexId = blackTexture;
	parms.Images[1][0].TexId = blackTexture;
	parms.Images[0][0].Pose.Pose.Orientation = OVR::Quat<float>();
	parms.Images[1][0].Pose.Pose.Orientation = OVR::Quat<float>();
	parms.Images[0][0].Pose.Pose.Position = OVR::Vector3f();
	parms.Images[1][0].Pose.Pose.Position = OVR::Vector3f();
	parms.Images[0][0].TexCoordsFromTanAngles = texCoordsFromTanAngles;
	parms.Images[1][0].TexCoordsFromTanAngles = texCoordsFromTanAngles;

	// Push three times so it is guaranteed to be fully displayed on screen
	// before returning.
	for ( int i = 0; i < 3; i++ )
	{
		WarpSwap( parms );
	}
}

void TimeWarpLocal::WarpSwapLoadingIcon( const float fovDegrees )
{
	const Matrix4f proj = Matrix4f::PerspectiveRH( DegreeToRad( fovDegrees ), 1.0f, 1, 100 );
	const Matrix4f texCoordsFromTanAngles = TanAngleMatrixFromProjection( proj );

	TimeWarpParms parms;
	parms.WarpProgram = WP_LOADING_ICON;
	parms.SwapOptions = SWAP_INHIBIT_SRGB_FRAMEBUFFER;
	parms.Images[0][0].TexId = blackTexture;
	parms.Images[1][0].TexId = blackTexture;
	parms.Images[0][0].Pose.Pose.Orientation = OVR::Quat<float>();
	parms.Images[1][0].Pose.Pose.Orientation = OVR::Quat<float>();
	parms.Images[0][0].Pose.Pose.Position = OVR::Vector3f();
	parms.Images[1][0].Pose.Pose.Position = OVR::Vector3f();
	parms.Images[0][0].TexCoordsFromTanAngles = texCoordsFromTanAngles;
	parms.Images[1][0].TexCoordsFromTanAngles = texCoordsFromTanAngles;

	// Push three times so it is guaranteed to be fully displayed on screen
	// before returning.
	for ( int i = 0; i < 3; i++ )
	{
		WarpSwap( parms );
	}
}

/*
 * VisualizeTiming
 *
 * Draw graphs of the latency and frame drops.
 */
struct lineVert_t
{
	unsigned short	x, y;
	unsigned int	color;
};

GlGeometry BuildTimingGraphGeometry( const int lineVertCount )
{
	GlGeometry	geo;

	glGenVertexArraysOES_( 1, &geo.vertexArrayObject );
	glBindVertexArrayOES_( geo.vertexArrayObject );

	lineVert_t	* verts = new lineVert_t[lineVertCount];
	const int byteCount = lineVertCount * sizeof( verts[0] );
	memset( verts, 0, byteCount );
	glGenBuffers( 1, &geo.vertexBuffer );
	glBindBuffer( GL_ARRAY_BUFFER, geo.vertexBuffer );
	glBufferData( GL_ARRAY_BUFFER, byteCount, (void *) verts, GL_DYNAMIC_DRAW );
	glEnableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_POSITION );
	glVertexAttribPointer( VERTEX_ATTRIBUTE_LOCATION_POSITION, 2, GL_SHORT, false, sizeof( lineVert_t ), (void *)0 );

	glEnableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_COLOR );
	glVertexAttribPointer( VERTEX_ATTRIBUTE_LOCATION_COLOR, 4, GL_UNSIGNED_BYTE, true, sizeof( lineVert_t ), (void *)4 );
	delete[] verts;

	// these will be drawn with DrawArrays, so no index buffer is needed

	geo.indexCount = lineVertCount;

	glBindVertexArrayOES_( 0 );

	return geo;
}

int ColorAsInt( const int r, const int g, const int b, const int a )
{
	return r | (g<<8) | (b<<16) | (a<<24);
}

void TimeWarpLocal::UpdateTimingGraphVerts( const debugPerfMode_t debugPerfMode, const debugPerfValue_t debugValue )
{
	if ( debugPerfMode == DEBUG_PERF_OFF )
	{
		return;
	}

	// Draw graph markers every five milliseconds
	lineVert_t	verts[EYE_LOG_COUNT*2+10];
	int			numVerts = 0;
	const int	colorRed = ColorAsInt( 255, 0, 0, 255 );
	const int	colorGreen = ColorAsInt( 0, 255, 0, 255 );
	// const int	colorYellow = ColorAsInt( 255, 255, 0, 255 );
	const int	colorWhite = ColorAsInt( 255, 255, 255, 255 );

	const float base = 250;
	const int drawLogs = 256; // VSYNC_LOG_COUNT;
	for ( int i = 0; i < drawLogs; i++ )
	{
		const int logIndex = ( lastEyeLog - 1 - i ) & ( EYE_LOG_COUNT - 1 );
		const eyeLog_t & log = eyeLog[ logIndex ];
		const int y = i*4;
		if ( log.skipped )
		{
			verts[i*2+0].y = verts[i*2+1].y = y;
			verts[i*2+0].x = base;
			verts[i*2+1].x = base + 150;
			verts[i*2+0].color = verts[i*2+1].color =  colorRed;
		}
		else
		{
			if ( debugValue == DEBUG_VALUE_LATENCY )
			{
				verts[i*2+0].y = verts[i*2+1].y = y;
				verts[i*2+0].x = base;
				verts[i*2+1].x = base + 10000 * log.poseLatencySeconds;
				verts[i*2+0].color = verts[i*2+1].color =  colorGreen;
			}
			else
			{
				verts[i*2+0].y = verts[i*2+1].y = y;
				verts[i*2+0].x = base + 10000 * log.issueFinish;
				verts[i*2+1].x = base + 10000 * log.completeFinish;
				if ( eyeLog[ ( logIndex - 2 ) & ( EYE_LOG_COUNT-1) ].bufferNum != log.bufferNum )
				{	// green = fresh buffer
					verts[i*2+0].color = verts[i*2+1].color =  colorGreen;
				}
				else
				{	// red = stale buffer reprojected
					verts[i*2+0].color = verts[i*2+1].color =  colorRed;
				}
			}
		}
	}
	numVerts = drawLogs*2;

	// markers every 5 milliseconds
	if ( debugValue == DEBUG_VALUE_LATENCY )
	{
		for ( int t = 0; t <= 64; t += 16 )
		{
			const float dt = 0.001f * t;
			const int x = base + 10000 * dt;

			verts[numVerts+0].y = 0;
			verts[numVerts+1].y = drawLogs * 4;
			verts[numVerts+0].x = x;
			verts[numVerts+1].x = x;
			verts[numVerts+0].color = verts[numVerts+1].color = colorWhite;
			numVerts += 2;
		}
	}
	else
	{
		for ( int t = 0; t <= 1; t++ )
		{
			const float dt = 1.0f/120.0f * t;
			const int x = base + 10000 * dt;

			verts[numVerts+0].y = 0;
			verts[numVerts+1].y = drawLogs * 4;
			verts[numVerts+0].x = x;
			verts[numVerts+1].x = x;
			verts[numVerts+0].color = verts[numVerts+1].color = colorWhite;
			numVerts += 2;
		}
	}

	// Update the vertex buffer.
	// We are updating buffer objects inside a vertex array object instead
	// of using client arrays to avoid messing with Unity's attribute arrays.

	// NOTE: vertex array objects do NOT include the GL_ARRAY_BUFFER_BINDING state, and
	// binding a VAO does not change GL_ARRAY_BUFFER, so we do need to track the buffer
	// in the geometry if we want to update it, or do a GetVertexAttrib( VERTEX_ATTRIB_ARRAY_BUFFER_BINDING

	// For reasons that I do not understand, if I don't bind the VAO, then all updates after the
	// first one produce no additional changes.
	glBindVertexArrayOES_( timingGraph.vertexArrayObject );
	glBindBuffer(GL_ARRAY_BUFFER, timingGraph.vertexBuffer);
	glBufferSubData( GL_ARRAY_BUFFER, 0, numVerts * sizeof( verts[0] ), (void *) verts );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	glBindVertexArrayOES_( 0 );

	timingGraph.indexCount = numVerts;

	GL_CheckErrors( "After UpdateTimingGraph" );
}


void TimeWarpLocal::DrawTimingGraph( const ScreenEye eye )
{
	// Use the same rect for viewport and scissor
	// FIXME: this is probably bad for convergence
	int	rectX, rectY, rectWidth, rectHeight;
	EyeRect( InitParms.HmdInfo, eye, RECT_SCREEN,
			rectX, rectY, rectWidth, rectHeight );

	glViewport(rectX, rectY, rectWidth, rectHeight);
	glScissor(rectX, rectY, rectWidth, rectHeight);

	glDisable( GL_DEPTH_TEST );
	glDisable( GL_BLEND );
	glLineWidth( 2.0f );
	glUseProgram( debugLineProgram.program );

	// pixel identity matrix
	// Map the rectWidth / rectHeight dimensions to -1 - 1 range
	float scale_x = 2.0f / (float)rectWidth;
	float scale_y = 2.0f / (float)rectHeight;
	const Matrix4f landscapePixelMatrix(
				0, scale_x, 0.0f, -1.0f,
				scale_y, 0, 0.0f, -1.0f,
				0.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f );


	glUniformMatrix4fv( debugLineProgram.uMvp, 1, GL_FALSE, /* not transposed */
        landscapePixelMatrix.Transposed().M[0] );

	glBindVertexArrayOES_( timingGraph.vertexArrayObject );
	glDrawArrays( GL_LINES, 0, timingGraph.indexCount );
	glBindVertexArrayOES_( 0 );

	glViewport( 0, 0, rectWidth * 2, rectHeight );
	glScissor( 0, 0, rectWidth * 2, rectHeight );

	GL_CheckErrors( "DrawTimingGraph" );
}

float calibrateFovScale = 1.0f;	// for interactive tweaking

// VAO (and FBO) are not shared across different contexts, so this needs
// to be called by the thread that will be drawing the warp.
void TimeWarpLocal::CreateFrameworkGraphics()
{
	unsigned char blackData[4] = {};
	blackTexture = LoadRGBTextureFromMemory( blackData, 1, 1, false );
	MakeTextureClamped( blackTexture );

	int w = 0;
	int h = 0;
	loadingTexture = LoadTextureFromApplicationPackage( "res/raw/loading_indicator.png", TextureFlags_t( TEXTUREFLAG_NO_MIPMAPS ), w, h );
	if ( loadingTexture == 0 )
	{
		// If not found in the application package, load the default embedded version.
		loadingTexture = LoadTextureFromBuffer( "oculus_loading_indicator.png", 
				MemBuffer( loading_indicator_bufferData, loading_indicator_bufferSize ),
				TextureFlags_t( TEXTUREFLAG_NO_MIPMAPS ), w, h );
	}
	MakeTextureClamped( loadingTexture );

	// Decide where we get our distortion mesh from
	MemBuffer buf;
	if ( InitParms.DistortionFileName )
	{	// If we have an explicit distortion file request, use that.
		MemBufferFile explicitFile( InitParms.DistortionFileName );
		if ( explicitFile.Length > 0 )
		{
			buf = explicitFile.ToMemBuffer();
		}
		if ( buf.Buffer == NULL ) 
		{
			FAIL( "Distortion file '%s' failed to load", InitParms.DistortionFileName );
		}
	}
	else
	{	// Look for the default file
		String fullPath = InitParms.ExternalStorageDirectory + DefaultDistortionFile;
		LOG( "Loading distortion file: %s", fullPath.ToCStr() );
		MemBufferFile defaultFile( fullPath );
		if ( defaultFile.Length > 0 )
		{
			buf = defaultFile.ToMemBuffer();
		}

		if ( buf.Buffer == NULL )
		{	// Synthesize the standard distortion
			buf = BuildDistortionBuffer( InitParms.HmdInfo, 32, 32 );
		}
	}

	// Create the tesselated mesh for vertex warping

	// single slice mesh for the normal rendering
	warpMesh = LoadMeshFromMemory( buf, 1, calibrateFovScale );

	// multi-slice mesh for sliced rendering
	sliceMesh = LoadMeshFromMemory( buf, NUM_SLICES_PER_EYE, calibrateFovScale );

	if ( warpMesh.indexCount == 0 || sliceMesh.indexCount == 0 )
	{
		FAIL( "WarpMesh failed to load")
	}

	buf.FreeData();

	// Vertexes and indexes for debug graph, the verts will be updated
	// dynamically each frame.
	timingGraph = BuildTimingGraphGeometry( (256+10)*2 );

	// simple cross to draw to screen
	calibrationLines2 = BuildCalibrationLines( 0, false );

	// FPS and graph text
	untexturedMvpProgram = BuildProgram(
			"uniform mat4 Mvpm;\n"
			"attribute vec4 Position;\n"
			"uniform mediump vec4 UniformColor;\n"
			"varying  lowp vec4 oColor;\n"
			"void main()\n"
			"{\n"
				"   gl_Position = Mvpm * Position;\n"
				"   oColor = UniformColor;\n"
			"}\n"
		,
			"varying lowp vec4	oColor;\n"
			"void main()\n"
			"{\n"
			"	gl_FragColor = oColor;\n"
			"}\n"
		);

    debugLineProgram = BuildProgram(
    		"uniform mediump mat4 Mvpm;\n"
    		"attribute vec4 Position;\n"
    		"attribute vec4 VertexColor;\n"
    		"varying  vec4 oColor;\n"
    		"void main()\n"
    		"{\n"
    		"   gl_Position = Mvpm * Position;\n"
    		"   oColor = VertexColor;\n"
    		"}\n"
		,
    		"varying lowp vec4 oColor;\n"
    		"void main()\n"
    		"{\n"
    		"	gl_FragColor = oColor;\n"
    		"}\n"
    	);

    // Build our warp render programs
    BuildWarpProgs();
}

void TimeWarpLocal::DestroyFrameworkGraphics()
{
	OVR::FreeTexture( blackTexture );
	OVR::FreeTexture( loadingTexture );

	calibrationLines2.Free();
	warpMesh.Free();
	sliceMesh.Free();
	fileMesh.Free();
	timingGraph.Free();

	DeleteProgram( untexturedMvpProgram );
	DeleteProgram( debugLineProgram );

	for ( int i = 0 ; i < WP_PROGRAM_MAX ; i++ )
	{
		DeleteProgram( warpPrograms[i].Prog );
	}
}

// Assumes viewport and scissor is set for the eye already.
// Assumes there is no depth buffer for the window.
void TimeWarpLocal::DrawFrameworkGraphicsToWindow( const ScreenEye eye,
		const int swapOptions, const bool drawTimingGraph )
{
	// Latency tester support.
	unsigned char latencyTesterColorToDisplay[3];

	if (ovrHmd_ProcessLatencyTest(InitParms.Hmd,  latencyTesterColorToDisplay))
	{
		glClearColor(
			latencyTesterColorToDisplay[0] / 255.0f,
			latencyTesterColorToDisplay[1] / 255.0f,
			latencyTesterColorToDisplay[2] / 255.0f,
			1.0f );
		glClear( GL_COLOR_BUFFER_BIT );
	}

	// Report latency tester results to the log.
	const char* results = ovrHmd_GetLatencyTestResult(InitParms.Hmd);
	if ( results != NULL )
	{
		LOG( "LATENCY TESTER: %s", results );
	}

	// optionally draw the calibration lines
	if ( swapOptions & SWAP_OPTION_DRAW_CALIBRATION_LINES )
	{
		const float znear = 0.5f;
		const float zfar = 150.0f;
		// flipped for portrait mode
		const Matrix4f projectionMatrix(
				0, 1, 0, 0,
				-1, 0, 0, 0,
				0, 0, zfar / (znear - zfar), (zfar * znear) / (znear - zfar),
				0, 0, -1, 0 );
		glUseProgram(untexturedMvpProgram.program);
		glLineWidth( 2.0f );
		glUniform4f(untexturedMvpProgram.uColor, 1, 0, 0, 1 );
		glUniformMatrix4fv(untexturedMvpProgram.uMvp, 1, GL_FALSE,  // not transposed
				projectionMatrix.Transposed().M[0] );
		glBindVertexArrayOES_( calibrationLines2.vertexArrayObject );

		int width, height;
		Screen.GetScreenResolution( width, height );
		glViewport( width/2 * (int)eye, 0, width/2, height );
		glDrawElements(GL_LINES, calibrationLines2.indexCount, GL_UNSIGNED_SHORT,
			NULL);
		glViewport( 0, 0, width, height );
	}

	// Draw the timing graph if it is enabled.
	if ( drawTimingGraph )
	{
		DrawTimingGraph( eye );
	}
}

}	// namespace OVR

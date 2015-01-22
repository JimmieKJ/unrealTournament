/************************************************************************************

Filename    :   TimeWarpParms.h
Content     :
Created     :   Jun 26, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/
#ifndef OVR_TimeWarpParms_h
#define OVR_TimeWarpParms_h

// The mobile displays scan from left to right, which is unfortunately
// the opposite of DK2.
enum ScreenEye
{
	SCREENEYE_LEFT,
	SCREENEYE_RIGHT
};

enum debugPerfMode_t
{
	DEBUG_PERF_OFF,			// data still being collected, just not displayed
	DEBUG_PERF_RUNNING,		// display continuously changing graph
	DEBUG_PERF_FROZEN,		// no new data collection, but displayed
	DEBUG_PERF_MAX
};

enum debugPerfValue_t
{
	DEBUG_VALUE_DRAW,		// start and end times of the draw
	DEBUG_VALUE_LATENCY,	// seconds from eye buffer orientation time
	DEBUG_VALUE_MAX
};

/*
	NOTE: the code which auto-disables chromatic aberration expects the warpProgram_t
	list to be symmetric: each program has a version with and a version without
	correction for chromatic aberration. For details see disableChromaticCorrection.
*/
enum warpProgram_t
{
	// without correction for chromatic aberration
	WP_SIMPLE,
	WP_MASKED_PLANE,						// overlay plane shows through masked areas in eyes
	WP_MASKED_PLANE_EXTERNAL,				// overlay plane shows through masked areas in eyes, using external texture as source
	WP_MASKED_CUBE,							// overlay cube shows through masked areas in eyes
	WP_CUBE,								// overlay cube only, no main scene (for power savings)
	WP_LOADING_ICON,						// overlay loading icon on top of scene
	WP_MIDDLE_CLAMP,						// stereo in a single texture
	// with correction for chromatic aberration
	WP_CHROMATIC,
	WP_CHROMATIC_MASKED_PLANE,				// overlay plane shows through masked areas in eyes
	WP_CHROMATIC_MASKED_PLANE_EXTERNAL,		// overlay plane shows through masked areas in eyes, using external texture as source
	WP_CHROMATIC_MASKED_CUBE,				// overlay cube shows through masked areas in eyes
	WP_CHROMATIC_CUBE,						// overlay cube only, no main scene (for power savings)
	WP_CHROMATIC_LOADING_ICON,				// overlay loading icon on top of scene
	WP_CHROMATIC_MIDDLE_CLAMP,				// stereo in a single texture

	WP_PROGRAM_MAX
};


// To get gamma correct sRGB filtering of the eye textures, the textures must be
// allocated with GL_SRGB8_ALPHA8 format and the window surface must be allocated
// with these attributes:
// EGL_GL_COLORSPACE_KHR,  EGL_GL_COLORSPACE_SRGB_KHR
//
// While we can reallocate textures easily enough, we can't change the window
// colorspace without relaunching the entire application, so if you want to
// be able to toggle between gamma correct and incorrect, you must allocate
// the framebuffer as sRGB, then inhibit that processing when using normal
// textures.
static const int SWAP_INHIBIT_SRGB_FRAMEBUFFER = 2;

// Draw the axis lines after warp to show the skew with the pre-warp lines.
static const int SWAP_OPTION_DRAW_CALIBRATION_LINES = 8;

// Enable / disable the sliced warp
static const int SWAP_OPTION_USE_SLICED_WARP = 16;

//===========================================================================
// Pure utility helper functions for building TimeWarpParms

// Convert a modelView matrix that would normally draw a -1 to 1 unit
// square to the view into a TanAngle matrix for an overlay surface.
//
// Note that this is NOT an mvp matrix -- the "projection" is handled
// by the distortion process.
ovrMatrix4f TanAngleMatrixFromUnitSquare( const ovrMatrix4f & modelViewMatrix );

// Utility function to convert a standard projection matrix into a
// TanAngle matrix for the primary time warp surface.
//
// We may want to provide an alternative one taking an FovPort parameter
// to avoid questions of matrix handedness.
ovrMatrix4f	TanAngleMatrixFromProjection( const ovrMatrix4f & projection );

// Utility function to calculate external velocity for smooth stick yaw
// turning in TimeWarp.
ovrMatrix4f	CalculateExternalVelocity( const ovrMatrix4f & viewMatrix,
		const float yawRadiansPerSecond );


//===========================================================================

// Note that if overlays are dynamic, they must be triple buffered just
// like the eye images.

struct TimeWarpImage
{
	TimeWarpImage() : TexId( 0 ), PlanarTexId() {}

	// If TexId == 0, this image is disabled.
	// Most applications will have the overlay image
	// disabled.
	//
	// Because OpenGL ES doesn't support clampToBorder,
	// it is the application's responsibility to make sure
	// that all mip levels of the texture have a black border
	// that will show up when time warp pushes the texture partially
	// off screen.
	//
	// Overlap textures will only show through where alpha on the
	// primary texture is not 1.0, so they do not require a border.
	unsigned		TexId;

	// Experimental separate R/G/B cube maps
	unsigned		PlanarTexId[3];

	// Points on the screen are mapped by a distortion correction
	// function into ( TanX, TanY, 1, 1 ) vectors that are transformed
	// by this matrix to get ( S, T, Q, _ ) vectors that are looked
	// up with texture2dproj() to get texels.
	ovrMatrix4f		TexCoordsFromTanAngles;

	// The sensor state for which ModelViewMatrix is correct.
	// It is ok to update the orientation for each eye, which
	// can help minimize black edge pull-in, but the position
	// must remain the same for both eyes, or the position would
	// seem to judder "backwards in time" if a frame is dropped.
	ovrPoseStatef	Pose;
};

struct TimeWarpParms
{
	TimeWarpParms() :   SwapOptions( 0 ),
			            MinimumVsyncs( 1 ),
			            PreScheduleSeconds( 0.014f ),
			            WarpProgram( WP_SIMPLE ),
                        ProgramParms(),
			            DebugGraphMode( DEBUG_PERF_OFF ),
                        DebugGraphValue( DEBUG_VALUE_DRAW )
    {
        for ( int i = 0; i < 4; i++ ) {		// this should be unnecessary, remove?
            for ( int j = 0; j < 4; j++ ) {
                ExternalVelocity.M[i][j] = ( i == j ) ? 1.0f : 0.0f;
            }
        }
    }

	static const int	MAX_WARP_EYES = 2;
	static const int	MAX_WARP_IMAGES = 2;	// 0 = world, 1 = overlay screen
	TimeWarpImage 		Images[MAX_WARP_EYES][MAX_WARP_IMAGES];
	int 				SwapOptions;

	// Rotation from a joypad can be added on generated frames to reduce
	// judder in FPS style experiences when the application framerate is
	// lower than the vsync rate.
	// This will be applied to the view space distorted
	// eye vectors before applying the rest of the time warp.
	// This will only be added when the same TimeWarpParms is used for
	// more than one vsync.
	ovrMatrix4f			ExternalVelocity;

	// WarpSwap will not return until at least this many vsyncs have
	// passed since the previous WarpSwap returned.
	// Setting to 2 will reduce power consumption and may make animation
	// more regular for applications that can't hold full frame rate.
	int					MinimumVsyncs;

	// Time in seconds to start drawing before each slice.
	// Clamped at 0.014 high and 0.002 low, but the very low
	// values will usually result in screen tearing.
	float				PreScheduleSeconds;

	// Which program to run with these images.
	warpProgram_t		WarpProgram;

	// Program-specific tuning values.
	float				ProgramParms[4];

	// Controls the collection and display of timing data.
	debugPerfMode_t		DebugGraphMode;
	debugPerfValue_t	DebugGraphValue;
};

#endif	// OVR_TimeWarpParms_h

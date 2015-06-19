/************************************************************************************

Filename    :   GazeCursor.cpp
Content     :   Global gaze cursor.
Created     :   June 6, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "GazeCursorLocal.h"

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_String_Utils.h"
#include "Android/LogUtils.h"
#include "VrApi/VrApi.h"
#include "GlTexture.h"
#include "PackageFiles.h"			// for loading images from the assets folder
#include "VrCommon.h"

namespace OVR {

static const char* GazeCursorVertexSrc =
	"uniform mat4 Mvpm;\n"
	"uniform vec4 UniformColor;\n"
	"attribute vec4 Position;\n"
	"attribute vec2 TexCoord;\n"
//	"attribute vec2 TexCoord1;\n"
	"varying  highp vec2 oTexCoord;\n"
//	"varying  highp vec2 oTexCoord1;\n"
	"varying  lowp vec4 oColor;\n"
	"void main()\n"
	"{\n"
	"   gl_Position = Mvpm * Position;\n"
	"   oTexCoord = TexCoord;\n"
//	"   oTexCoord1 = TexCoord1;\n"
	"	oColor = UniformColor;\n"
	"}\n";

static const char* GazeCursorFragmentSrc =
	"uniform sampler2D Texture0;\n"
	"varying highp vec2 oTexCoord;\n"
	"varying lowp vec4 oColor;\n"
	"void main()\n"
	"{\n"
	"	gl_FragColor = oColor * texture2D( Texture0, oTexCoord );\n"
	"}\n";

static const char * GazeCursorColorTableFragmentSrc =
	"uniform sampler2D Texture0;\n"
	"uniform sampler2D Texture1;\n"
	"uniform mediump vec2 ColorTableOffset;\n"
	"varying mediump vec2 oTexCoord;\n"
	"varying lowp vec4 oColor;\n"
	"void main()\n"
	"{\n"
	"    lowp vec4 texel = texture2D( Texture0, oTexCoord );\n"
	"    mediump vec2 colorIndex = vec2( texel.x, ColorTableOffset.y );\n"
	"    lowp vec4 outColor = texture2D( Texture1, colorIndex.xy );\n"
	"    gl_FragColor = vec4( outColor.xyz * oColor.xyz, texel.a );\n"
	"}\n";

//==============================
// OvrGazeCursorLocal::OvrGazeCursorLocal
OvrGazeCursorLocal::OvrGazeCursorLocal() :
	NextUserId( 1 ),
	CursorRotation( 0.0f ),
	RotationRateRadians( Mathf::Pi * 0.5f ),
	CursorScale( 0.025f ),
	DistanceOffset( 0.05f ),
	HiddenFrames( 0 ),
	CurrentTransform( 0 ),
	ColorTableOffset( 0.0f ),
	TimerShowTime( -1.0 ),
	TimerEndTime( -1.0 ),
	CursorTextureHandle(),
	TimerTextureHandle( 0 ),
	ColorTableHandle( 0 ),
	Initialized( false ),
	Hidden( false ),
	IsActive( false ),
	HasUser( false )
{
}

const float	OvrGazeCursorLocal::CURSOR_MAX_DIST = 2.5f;

//==============================
// OvrGazeCursorLocal::OvrGazeCursorLocal
OvrGazeCursorLocal::~OvrGazeCursorLocal()
{
}

//==============================
// OvrGazeCursorLocal::
void OvrGazeCursorLocal::Init()
{
	LOG( "OvrGazeCursorLocal::Init" );
	DROID_ASSERT( Initialized == false, "GazeCursor" );

	if ( Initialized )
	{
		LOG( "OvrGazeCursorLocal::Init - already initialized!" );
		return;
	}
	CursorGeometry = BuildTesselatedQuad( 1, 1 );
	int w = 0;
	int h = 0;
	char const * const cursorStateNames[ CURSOR_STATE_MAX ] =
	{
		//"res/raw/color_ramp_test.tga",
		//"res/raw/color_ramp_test.tga",

		//"res/raw/gaze_cursor_dot.tga",
		//"res/raw/gaze_cursor_dot_hi.tga"
		
		//"res/raw/gaze_cursor_cross.tga",
		//"res/raw/gaze_cursor_cross_hi.tga"
		
		"res/raw/gaze_cursor_cross.tga",
		"res/raw/gaze_cursor_cross.tga",	// for now, hilight is the same because the graphic needs work
		"res/raw/gaze_cursor_cross.tga",
		"res/raw/gaze_cursor_hand.tga"
	};

	for ( int i = 0; i < CURSOR_STATE_MAX; ++i )
	{
		CursorTextureHandle[i] = LoadTextureFromApplicationPackage( cursorStateNames[i], TextureFlags_t(), w, h );
	}

	TimerTextureHandle = LoadTextureFromApplicationPackage( "res/raw/gaze_cursor_timer.tga", TextureFlags_t(), w, h );

	ColorTableHandle = LoadTextureFromApplicationPackage( "res/raw/color_ramp_timer.tga", TextureFlags_t(), w, h );

	CursorProgram = BuildProgram( GazeCursorVertexSrc, GazeCursorFragmentSrc );
	TimerProgram = BuildProgram( GazeCursorVertexSrc, GazeCursorColorTableFragmentSrc );//GazeCursorFragmentSrc );

	Initialized = true;
}

//==============================
// OvrGazeCursorLocal::
void OvrGazeCursorLocal::Shutdown()
{
	LOG( "OvrGazeCursorLocal::Shutdown" );
	DROID_ASSERT( Initialized == true, "GazeCursor" );

	for ( int i = 0; i < CURSOR_STATE_MAX; ++i )
	{
		if ( CursorTextureHandle[i] != 0 )
		{
			glDeleteTextures( 1, &CursorTextureHandle[i] );
			CursorTextureHandle[i] = 0;
		}
	}

	if ( TimerTextureHandle != 0 )
	{
		glDeleteTextures( 1, & TimerTextureHandle );
		TimerTextureHandle = 0;
	}

	if ( ColorTableHandle != 0 )
	{
		glDeleteTextures( 1, &ColorTableHandle );
		ColorTableHandle = 0;
	}

	DeleteProgram( CursorProgram );
	DeleteProgram( TimerProgram );

	Initialized = false;
}

//==============================
// OvrGazeCursorLocal::GenerateUserId
gazeCursorUserId_t	OvrGazeCursorLocal::GenerateUserId()
{
	return gazeCursorUserId_t( NextUserId++ );
}

//==============================
// OvrGazeCursorLocal::UpdateForUser
void OvrGazeCursorLocal::UpdateForUser( gazeCursorUserId_t const userId, float const d,
										eGazeCursorStateType const state )
{
	//LOG( "OvrGazeCursorLocal::UpdateForUser %i", userId.Get() );
	if ( d < Info.Distance )
	{
		//LOG( "OvrGazeCursorLocal::UpdateForUser %i - new closest distace %.2f", userId.Get(), d );
		Info.Distance = d;
		Info.UserId = userId;
		Info.State = state;

		IsActive = true;
	}

	HasUser = true;
}

//==============================
// OvrGazeCursorLocal::ClearGhosts
void OvrGazeCursorLocal::ClearGhosts()
{
	CurrentTransform = 0;
}

static float frand()
{
	return ( rand() & 65535 ) / (65535.0/2.0f) - 1.0f;
}

//==============================
// OvrGazeCursorLocal::BeginFrame
void OvrGazeCursorLocal::BeginFrame()
{
	ResetCursor();
}

//==============================
// OvrGazeCursorLocal::Frame
void OvrGazeCursorLocal::Frame( Matrix4f const & viewMatrix, float const deltaTime )
{
	//LOG( "OvrGazeCursorLocal::Frame" );
	HiddenFrames -= 1;

	if ( 0 ) //IsActive ) 
	{
		CursorRotation += deltaTime * RotationRateRadians;
		if ( CursorRotation > Mathf::TwoPi )
		{
			CursorRotation -= Mathf::TwoPi;
		}
		else if ( CursorRotation < 0.0f )
		{
			CursorRotation += Mathf::TwoPi;
		}
	}
	else
	{
		CursorRotation = 0.0f;
	}

#if 1
	if ( TimerEndTime > 0.0 )
	{
		double TimeRemaining = TimerEndTime - ovr_GetTimeInSeconds();
		if ( TimeRemaining <= 0.0 )
		{
			TimerEndTime = -1.0;
			TimerShowTime = -1.0;
			ColorTableOffset = Vector2f( 0.0f );
		}
		else
		{
			double duration = TimerEndTime - TimerShowTime;
			double ratio = 1.0f - ( TimeRemaining / duration );
			//SPAM( "TimerEnd = %.2f, TimeRemaining = %.2f, Ratio = %.2f", TimerEndTime, TimeRemaining, ratio );
			ColorTableOffset.x = 0.0f;
			ColorTableOffset.y = float( ratio );
		}
	}
	else
	{
		ColorTableOffset = Vector2f( 0.0f );
	}
#else
	// cycling
	float COLOR_TABLE_CYCLE_RATE = 0.25f;
	ColorTableOffset.x = 0.0f;
	ColorTableOffset.y += COLOR_TABLE_CYCLE_RATE * deltaTime;
	if ( ColorTableOffset.y > 1.0f )
	{
		ColorTableOffset.y -= floorf( ColorTableOffset.y );
	}
	else if ( ColorTableOffset.y < 0.0f )
	{
		ColorTableOffset.y += ceilf( ColorTableOffset.y );
	}
#endif

	const Vector3f viewPos( GetViewMatrixPosition( viewMatrix ) );
	const Vector3f viewFwd( GetViewMatrixForward( viewMatrix ) );

//	Vector3f position = viewPos + viewFwd * ( Info.Distance - DistanceOffset );
	Vector3f position = viewPos + viewFwd * 1.4f; // !@# JDC fixed distance
	CursorScale = 0.0125f;

	Matrix4f viewRot = viewMatrix;
	viewRot.SetTranslation( Vector3f( 0.0f ) );

	// Add one ghost for every four milliseconds.
	// Assume we are going to be at even multiples of vsync, so we don't need to bother
	// keeping an accurate roundoff count.
	const int lerps = deltaTime / 0.004;

	const Matrix4f & prev = CursorTransform[ CurrentTransform % TRAIL_GHOSTS ];
	Matrix4f & now = CursorTransform[ ( CurrentTransform + lerps ) % TRAIL_GHOSTS ];

	now = Matrix4f::Translation( position ) * viewRot.Inverted() * Matrix4f::RotationZ( CursorRotation )
		* Matrix4f::Scaling( CursorScale, CursorScale, 1.0f );

	if ( CurrentTransform > 0 )
	{
		for ( int i = 1 ; i <= lerps ; i++ )
		{
			const float f = (float)i / lerps;
			Matrix4f & tween = CursorTransform[ ( CurrentTransform + i) % TRAIL_GHOSTS ];

			// We only need to build a scatter on the final point that is already set by now
			if ( i != lerps )
			{
				tween = ( ( now * f ) + ( prev * ( 1.0f - f ) ) );
			}

			// When the cursor depth fails, draw a scattered set of ghosts
			Matrix4f & scatter = CursorScatterTransform[ ( CurrentTransform + i) % TRAIL_GHOSTS ];

			// random point in circle
			float	rx, ry;
			while( 1 )
			{
				rx = frand();
				ry = frand();
				if ( (rx*rx + ry*ry < 1.0f ))
				{
					break;
				}
			}
			scatter = tween * Matrix4f::Translation( rx, ry, 0.0f );
		}
	}
	else
	{
		// When CurrentTransform is 0, reset "lerp" cursors to the now transform as these will be drawn in the next frame.
		// If this is not done, only the cursor at pos 0 will have the now orientation, while the others up to lerps will have old data
		// causing a brief "duplicate" to be on screen.
		for ( int i = 1 ; i < lerps ; i++ )
		{
			Matrix4f & tween = CursorTransform[ ( CurrentTransform + i) % TRAIL_GHOSTS ];
			tween = now;
		}
	}
	CurrentTransform += lerps;

	position -= viewFwd * 0.025f; // to avoid z-fight with the translucent portion of the crosshair image
	TimerTransform = Matrix4f::Translation( position ) * viewRot.Inverted() * Matrix4f::RotationZ( CursorRotation ) * Matrix4f::Scaling( CursorScale * 4.0f, CursorScale * 4.0f, 1.0f );
}

//==============================
// OvrGazeCursorLocal::Render
void OvrGazeCursorLocal::Render( int const eye, Matrix4f const & mvp ) const
{
	GL_CheckErrors( "OvrGazeCursorLocal::Render - pre" );

	//LOG( "OvrGazeCursorLocal::Render" );

	if ( HiddenFrames >= 0 )
	{
		return;
	}

	if ( !HasUser && !TimerActive() )
	{
		return;
	}

	if ( Hidden && !TimerActive() )
	{
		return;
	}

	if ( CursorScale <= 0.0f )
	{
		LOG( "OvrGazeCursorLocal::Render - scale 0" );
		return;
	}

	// It is important that glBlendFuncSeparate be used so that destination alpha
	// correctly holds the opacity over the overlay plane.  If normal blending is
	// used, the cursor ghosts will "punch holes" in things through to the overlay plane.
	glEnable( GL_BLEND );
	glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );

	// If the cursor is not active, allow it to depth sort... if it is active we know it should
	// be on top, so don't bother depth testing. However, this assumes that the cursor is placed 
	// correctly. If it isn't, then it could still appear stereoscopically behind the object it's
	// on top of, which would be bad. The reason for not using depth testing when active is to
	// solve any z-fighting issues, particularly with SwipeView, where the panel distance isn't
	// entirely accurate right now. It can also be solved by pushing the cursor in a little bit
	// from the panel, but in stereo vision it looks like it's floating further above the panel
	// than it does with other systems like the VRMenu.

	glDepthMask( GL_FALSE );	// don't write to depth, or ghost trails wouldn't work

	// We always want depth test enabled so we can see if any GUI panels have
	// been placed too close and obscured the cursor.
	glEnable( GL_DEPTH_TEST );

	glUseProgram( CursorProgram.program );
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, CursorTextureHandle[Info.State] );

	// draw from oldest and faintest to newest
	for ( int i = TRAIL_GHOSTS-1 ; i >= 0 ; i-- )
	{
		const int index = ( CurrentTransform - i ) % TRAIL_GHOSTS;
		if ( index <= 0 )
		{
			continue;
		}
		Vector4f cursorColor( 1.0f, 1.0f, 1.0f, 0.5 * ( 1.0 - (float)i / TRAIL_GHOSTS ) );
		glUniform4fv( CursorProgram.uColor, 1, &cursorColor.x );
		Matrix4f cursorMVP = mvp * CursorTransform[index];
		glUniformMatrix4fv( CursorProgram.uMvp, 1, GL_FALSE, cursorMVP.Transposed().M[0] );
		CursorGeometry.Draw();
	}

	// Reverse depth test and draw the scattered ghosts where they are occluded
	glDepthFunc( GL_GREATER );

	for ( int i = TRAIL_GHOSTS-1 ; i >= 0 ; i-- )
	{
		const int index = ( CurrentTransform - i ) % TRAIL_GHOSTS;
		if ( index <= 0 )
		{
			continue;
		}
		Vector4f cursorColor( 1.0f, 0.0f, 0.0f, 0.15 * ( 1.0 - (float)i / TRAIL_GHOSTS ) );
		glUniform4fv( CursorProgram.uColor, 1, &cursorColor.x );
		Matrix4f cursorMVP = mvp * CursorScatterTransform[index];
		glUniformMatrix4fv( CursorProgram.uMvp, 1, GL_FALSE, cursorMVP.Transposed().M[0] );
		CursorGeometry.Draw();
	}

	glDepthFunc( GL_LEQUAL );

	// draw the timer if it's enabled
	if ( TimerEndTime > 0.0 && ovr_GetTimeInSeconds() >= TimerShowTime )
	{
		glUseProgram( TimerProgram.program );
		glActiveTexture( GL_TEXTURE0 );
		glBindTexture( GL_TEXTURE_2D, TimerTextureHandle );

		glActiveTexture( GL_TEXTURE1 );
		glBindTexture( GL_TEXTURE_2D, ColorTableHandle );
		// do not do any filtering on the "palette" texture
		if ( EXT_texture_filter_anisotropic )
		{
			glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f );
		}
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

		Matrix4f timerMVP = mvp * TimerTransform;
		glUniformMatrix4fv( TimerProgram.uMvp, 1, GL_FALSE, timerMVP.Transposed().M[0] );

		Vector4f cursorColor( 0.0f, 0.643f, 1.0f, 1.0f );
		glUniform4fv( TimerProgram.uColor, 1, &cursorColor.x );
		glUniform2fv( TimerProgram.uColorTableOffset, 1, &ColorTableOffset.x );

		CursorGeometry.Draw();
	}

	glDepthMask( GL_TRUE );
	glDisable( GL_BLEND );

	GL_CheckErrors( "OvrGazeCursorLocal::Render - post" );
}

//==============================
// OvrGazeCursorLocal::IsActiveForUser
bool OvrGazeCursorLocal::IsActiveForUser( gazeCursorUserId_t const userId ) const
{
	return Info.UserId == userId;
}

//==============================
// OvrGazeCursorLocal::GetInfo
OvrGazeCursorInfo OvrGazeCursorLocal::GetInfo() const
{
	return Info;
}

//==============================
// OvrGazeCursorLocal::ForceDistance
void OvrGazeCursorLocal::ForceDistance( gazeCursorUserId_t const userId, float const d )
{
	Info.UserId = userId;
	Info.Distance = d;
}

//==============================
// OvrGazeCursorLocal::ForceDistance
void OvrGazeCursorLocal::SetRotationRate( float const degreesPerSec )
{
	RotationRateRadians = degreesPerSec * Mathf::DegreeToRadFactor;
}

//==============================
// OvrGazeCursorLocal::SetCursorScale
void OvrGazeCursorLocal::SetCursorScale( float const scale )
{
	CursorScale = scale;
}

//==============================
// OvrGazeCursorLocal::ResetCursor
void OvrGazeCursorLocal::ResetCursor()
{
	Info.Reset( CURSOR_MAX_DIST );
	IsActive = false;
	HasUser = false;
}

//==============================
// OvrGazeCursorLocal::StartTimer
void OvrGazeCursorLocal::StartTimer( float const durationSeconds, float const timeBeforeShowingTimer )
{
	double curTime = ovr_GetTimeInSeconds();
	LOG( "(%.4f) StartTimer = %.2f", curTime, durationSeconds );
	TimerShowTime =  curTime + (double)timeBeforeShowingTimer; 
	TimerEndTime = curTime + (double)durationSeconds;
}

//==============================
// OvrGazeCursorLocal::CancelTimer
void OvrGazeCursorLocal::CancelTimer()
{
	double curTime = ovr_GetTimeInSeconds();
	LOG( "(%.4f) Cancel Timer", curTime );
	TimerShowTime = -1.0;
	TimerEndTime = -1.0;
}

//==============================
// OvrGazeCursorLocal::TimerActive
bool OvrGazeCursorLocal::TimerActive() const { 
    return TimerEndTime > ovr_GetTimeInSeconds();
}

} // namespace OVR

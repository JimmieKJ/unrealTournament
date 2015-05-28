/************************************************************************************

Filename    :   SwipeView.cpp
Content     :   Gaze and swipe selection of panels.
Created     :   February 28, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "SwipeView.h"

#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_Alg.h"
#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_TypesafeNumber.h"
#include "Android/GlUtils.h"

#include "Input.h"
#include "GlTexture.h"
#include "VrCommon.h"		// LogMatrix
#include "BitmapFont.h"
#include "VRMenu/VRMenuMgr.h"
#include "VRMenu/GuiSys.h"

// don't angle upper and lower rows towards viewer
#define FLAT_ROWS
static const bool allowPageSwipes = false;

/*
 * Don't put away if velocity is high?
 * No drag out of view
 * Location in total list feedback
 * Push in the selected panel when touch goes down
 *
 * 2D scrolling doesn't make sense on a sphere because of the poles
 *
 * Stretch opening animation for smaller number of panels.
 *
 * Styling for gaze activated panels.
 *
 * Spawn orientation
 * Gaze select
 * Select border highlight
 * Multiple rows
 * Definitely need tap events, down-up in one frame is common
 * limit overshoot past edge, shouldn't snap back past closest
 * Opening animation
 * Select hysteresis
 *
 * Also support joypad as a velocity kick
 *
 * Linear array
 * Circular array : radius
 * Spherical array
 *
 * Toroidal
 *
 * Selected: Highlight panel under gaze
 * Activating: Highlight differently when touch goes down and tap is still possible
 * Activated: Highlight
 *
 * SndSelected
 * SndDeselected
 * SndSelectComplete
 * SndDeselectComplete
 * SndActivated
 * SndTouch
 * SndRelease
 * SndSpin
 *
 * Text caption
 *
 * Geometric animated panel opens.
 * Loading animation.
 *
 * Tap / long press animation
 *
 * Double tap?
 *
 * Wrap panels around with or without a separating line or just let them stop.
 *
 * Pinch operation?
 *
 * Draw offset so selected can have large view space.
 *
 * Tree view?
 *
 * Only display a certain number, then fold into stacks?
 *
 * Reflect on mirror floor?
 *
 * Explicit close button?
 *
 * Draw eye center point on dome?
 *
 * Open-up animation
 *
 * Should be robust against adding and removing panels.
 *
 * Drag with head?
 *
 * May want panel justification options when using text strings as panels.
 *
 * Vertically orientation option -- poles at sides
 *
 */



namespace OVR
{


SwipeView::SwipeView() :
		ClampPanelOffset( true ),
		CloseOnNextFrame( false ),
		ActivateOnNextFrame( false ),
		ForwardAtOffset( 0.0f ),
		ForwardYaw( 0.0f ),
		Offset( 0.0f ),
		Velocity( 0.0f ),
		HistoryIndex( 0 ),
		StartViewOrigin( 0.0f ),
		State( SVS_CLOSED ),
		TouchState( SVT_WILL_DRAG ),
		PressTime( 0.0 ),
		TouchPoint( 0.0f ),
		TouchGazePos( 0.0f ),
		PrevTouch( 0.0f ),
		PreviousButtonState( false ),
		HasMoved( false ),
		ActivationPress( false ),
		GazePos( 0.0f ),
		Radius( 2.0f ),
		SlotSize( 0.0f ),
		LayoutRows( 1 ),
		RowOffset( 0.0f ),
		EverOpened( false ),
		AnimationCenterPanel(),
		AnimationFraction( 1.0f ),
		AnimationStartTime( 0.0 ),
		SelectedPanel( -1 ),
		PageSwipeSeconds( 0.0f ),
		PageSwipeDir( 0.0f ),
		Distance( 2.0f ),
		SelectDistance( 0.4f ),
		TapMaxDistance( 30.0f ),
		GazeMaxDistance( 0.1f ),
		SpeedScale( -0.003f ),
		Friction( 5.0f ),
		SnapSpeed( 4.0f ),
		SelectTime( 0.25f ),		// time to go to or from selected state
		HighlightSlotFraction( 0.02f ),
		MaxVelocity( 8.0f ),	// radians per second
		MinVelocity( 0.1f ),	// radians per second
		VelocityMixTime( 0.1f ),	// half of the velocity is mixed in this time
		OpenAnimationTime( 0.75f )
{
	for ( int i = 0; i < MAX_TOUCH_HISTORY; i++ )
	{
		TouchPos[i] = Vector2f( 0.0f );
		TimeHistory[i] = 0.0;
	}
}

SwipeView::~SwipeView()
{
	FreeTexture( BorderTexture2_1 );
	FreeTexture( BorderTexture1_1 );
	GeoPanel.Free();
	DeleteProgram( ProgPanel );
	DeleteProgram( ProgHighlight );
}

// Texture has a single 0 pixel border around edge.
static GlTexture BuildBorderTexture( int width, int height )
{
	unsigned char * data = (unsigned char *)malloc( width * height * 4 );
	memset( data, 255, width * height );
	for ( int i = 0 ; i < width ; i++ )
	{
		data[i] = 0;
		data[(height-1)*width+i] = 0;
	}
	for ( int i = 0 ; i < height ; i++ )
	{
		data[i*width] = 0;
		data[i*width+width-1] = 0;
	}
	GlTexture	tex = LoadRTextureFromMemory( data, width, height );
	free( data );
	MakeTextureClamped( tex );
	return tex;
}

void SwipeView::Init( OvrGazeCursor & gazeCursor )
{
	GeoPanel = BuildTesselatedQuad( 1, 1 );

	BorderTexture2_1 = BuildBorderTexture( 80, 40 );
	BorderTexture1_1 = BuildBorderTexture( 40, 40 );

	ProgPanel = BuildProgram(
		"uniform mat4 Mvpm;\n"
		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"
		"varying  highp vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"   oTexCoord = TexCoord;\n"
		"}\n"
	,
		"uniform sampler2D Texture0;\n"
		"uniform sampler2D Texture1;\n"
		"varying highp vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"	gl_FragColor.xyz = texture2D(Texture0, oTexCoord).xyz;\n"
		"	gl_FragColor.w = texture2D(Texture1, oTexCoord).x;\n"
		"}\n"
	);


	ProgHighlight = BuildProgram(
		"uniform mat4 Mvpm;\n"
		"uniform vec4 UniformColor;\n"
		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"
		"varying  highp vec2 oTexCoord;\n"
		"varying  lowp vec4 oColor;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"   oTexCoord = TexCoord;\n"
		"	oColor = UniformColor;\n"
		"}\n"
	,
		"uniform sampler2D Texture1;\n"
		"varying highp vec2 oTexCoord;\n"
		"varying lowp vec4 oColor;\n"
		"void main()\n"
		"{\n"
		"	gl_FragColor = oColor;\n"
		"	gl_FragColor.w = texture2D(Texture1, oTexCoord).x;\n"
		"}\n"
	);

	GazeUserId = gazeCursor.GenerateUserId();
}

static float sqr( float s )
{
	return s * s;
}

static Vector3f MatrixOrigin( const Matrix4f & m )
{
	return Vector3f( m.M[0][3], m.M[1][3], m.M[2][3] );
}

static Vector3f MatrixForward( const Matrix4f & m )
{
	return Vector3f( -m.M[2][0], -m.M[2][1], -m.M[2][2] );
}
#if 0 // unused?
static bool GazeOnPanel( const Vector2f & gazePos, const Vector2f & panelPos, const Vector2f & panelSize )
{
	const Vector2f gazeDelta = gazePos - panelPos;
	if ( gazeDelta.x < -panelSize.x * 0.5 )
		return false;
	if ( gazeDelta.x > panelSize.x * 0.5 )
		return false;
	if ( gazeDelta.y < -panelSize.y * 0.5 )
		return false;
	if ( gazeDelta.y > panelSize.y * 0.5 )
		return false;
	return true;
}
#endif
static Vector3f	FlatForward( const Matrix4f & view )
{
	Vector3f v = MatrixForward( view );
	v.y = 0.0f;
	return v.Normalized();
}

void	SwipeView::Activate( )
{
	LOG( "Activate" );

	// Get time and view on next call to frame
	ActivateOnNextFrame = true;

	// Always open swipe views straight up
	State = SVS_OPENING;

	Velocity = 0.0f;
	SelectedPanel = -1;	// nothing initially selected
	HasMoved = true;	// don't treat the release as a tap
	ClearPanelSelectState();

	ActivationPress = true;	// a release of this touch will never activate a button

	// Should we recenter the panels? 
	bool centerList = false;
	if ( EverOpened )
	{
		// Did user close the SwipeView far enough away from the edges of swipeview?
		const float panelsHalfWidthPlus2 = ( float )( LayoutColumns( ) / 2 ) * SlotSize.x + SlotSize.x;
		centerList = fabsf( Offset ) >= panelsHalfWidthPlus2;
	}

	if ( centerList || !EverOpened )
	{
		EverOpened = true;
		AnimationCenterPanel[0] = LayoutColumns() / 2;
		AnimationCenterPanel[1] = LayoutRows/2;

		Offset = 0.0f;
	}
}

void	SwipeView::Close()
{
	LOG( "Close" );

	if ( State == SVS_OPEN )
	{
		State = SVS_CLOSING;
		LOG( "SVS_CLOSING" );

		// start animation on next frame
		CloseOnNextFrame = true;

		if ( SelectedPanel != -1 )
		{	// close from a selected panel
			AnimationCenterPanel[0] = SelectedPanel / LayoutRows;
			AnimationCenterPanel[1] = SelectedPanel % LayoutRows;
		}
		else
		{	// close from the closest panel
			AnimationCenterPanel[0] = OVR::Alg::Clamp( (int)(Offset / SlotSize.x + 0.5f), 0, LayoutColumns()-1 );
			AnimationCenterPanel[1] = (int)( ( LayoutRows - 1 ) * 0.5f );
		}
	}
}

void SwipeView::ClearPanelSelectState()
{
	for ( int index = 0 ; index < Panels.GetSizeI() ; index++ )
	{
		Panels[index].SelectState = 0.0f;
	}
}

int	SwipeView::LayoutColumns() const {
	if ( Panels.GetSizeI() <= 1 ) {
		return 0;
	}
	return 1 + ( Panels.GetSizeI() - 1 ) / LayoutRows;
}

float	SwipeView::PanelAngleY( const int panelY ) {
	return -( panelY  - ( LayoutRows - 1 ) * 0.5f - RowOffset ) * SlotSize.y;
}

static Matrix4f MatrixForPanel( const Vector3f StartViewOrigin, const float yawOffset, const float pitchOffset,
		const float animatedDistance, const float scaleX, const float scaleY )
{
#ifdef FLAT_ROWS
		Matrix4f rotMat = Matrix4f::Translation( StartViewOrigin
				+ Vector3f( 0, pitchOffset, 0 )  ) *
				Matrix4f::RotationY( yawOffset );
		Matrix4f pMat = rotMat *
				Matrix4f::Translation( 0, 0, animatedDistance ) *
				Matrix4f::Scaling( scaleX, scaleY, 1 );
#else
		Matrix4f rotMat = Matrix4f::Translation( StartViewOrigin ) *
				Matrix4f::RotationY( yawOffset ) *
				Matrix4f::RotationX( pitchOffset );
		Matrix4f pMat = rotMat *
				Matrix4f::Translation( 0, 0, animatedDistance ) *
				Matrix4f::Scaling( scaleX, scaleY, 1 );
#endif
		return pMat;
}

// Returns the intersection in local XY coordinates of the given ray on the Z=0 plane
// of the matrix, or -1 if the two points don't cross the Z=0 plane.
static float LineOnMatrix( const Vector3f & p1 , const Vector3f & p2, const Matrix4f & mat,
		Vector2f & localHit )
{
	const Matrix4f	inverted( mat.Inverted() );
	const Vector3f localp1( inverted.Transform( p1 ) );
	const Vector3f localp2( inverted.Transform( p2 ) );
	if ( ( localp1.z >= 0 ) == ( localp2.z >= 0 ) )
	{	// both on same side
		return -1.0f;
	}
	const float frac = localp1.z / ( localp1.z - localp2.z );
	const float omf = 1.0f - frac;

	localHit.x = localp2.x * frac + localp1.x * omf;
	localHit.y = localp2.y * frac + localp1.y * omf;

	return frac;
}


SwipeAction	SwipeView::Frame( OvrGazeCursor & gazeCursor, BitmapFont const & font, BitmapFontSurface & fontSurface,
        const VrFrame & vrFrame, const Matrix4f & view, const bool allowSwipe )
{
	SwipeAction	ret = {};
	ret.ActivatePanelIndex = -1;

	if ( ActivateOnNextFrame )
	{
		ActivateOnNextFrame = false;

		AnimationStartTime = vrFrame.PoseState.TimeInSeconds;
		// allowing AnimationFraction to reach 0.0 causes an invalid matrix to be formed in Draw()
		AnimationFraction = 0.00001f;

		StartViewOrigin = MatrixOrigin( view.Inverted() );
		ForwardAtOffset = FlatForward( view );

		const Vector3f viewForward = FlatForward( view );
		ForwardYaw = atan2( -viewForward.x, -viewForward.z );
	}

	if ( CloseOnNextFrame )
	{
		CloseOnNextFrame = false;
		AnimationStartTime = vrFrame.PoseState.TimeInSeconds;
	}



	const Vector2f touch( vrFrame.Input.touch[0], vrFrame.Input.touch[1] );

	// If we are in swipe mode and the touch moves away from the down point,
	// or the gaze moves away from the down point
	// it will not be considered a tap on release even if it moves back to the original point.
	{
		const float distFromPress = ( touch - TouchPoint ).Length();

		const float distFromGaze = ( TouchGazePos - GazePos ).Length();

		if ( allowSwipe && PreviousButtonState && !HasMoved )
		{
			if ( distFromPress > TapMaxDistance || distFromGaze > GazeMaxDistance  )
			{
				ret.PlaySndActiveToDrag = true;
				HasMoved = true;
			}
		}
	}

	// Adjust forwardYaw after the animation completes
	if ( State == SVS_OPEN )
	{
		const Vector3f viewForward = FlatForward( view );

		const float newForwardYaw = atan2( -viewForward.x, -viewForward.z );
		float moveRadians = newForwardYaw - ForwardYaw;
		if ( moveRadians > M_PI )
		{	// new is a bit under PI and old is a bit above -PI
			moveRadians -= 2 * M_PI;
		}
		else if ( moveRadians < -M_PI )
		{	// new is a bit above -PI and old is a bit below PI
			moveRadians += 2 * M_PI;
		}

		ForwardYaw = newForwardYaw;

		Offset += moveRadians;

		ForwardAtOffset = viewForward;

		// Don't let all the panels go completely off screen in most cases,
		// but some scenes may want them fixed in place.
		if ( ClampPanelOffset )
		{
			const float offsetRange = ( float )( ( LayoutColumns() / 2 ) * SlotSize.x + M_PI * 0.25f );
			Offset = OVR::Alg::Clamp( Offset, -offsetRange, offsetRange );
		}
	}

	// page-swipe animation
	int swipeButtons = vrFrame.Input.buttonPressed;
	if ( !allowPageSwipes )
	{
		swipeButtons &= ~(BUTTON_SWIPE_FORWARD | BUTTON_SWIPE_BACK);
	}
	{
		const float PageSwipeTime = 0.3f;
		const float PageSwipeDistance = SlotSize.x * 3;
		if ( swipeButtons &
				( BUTTON_SWIPE_FORWARD | BUTTON_SWIPE_BACK | BUTTON_DPAD_LEFT | BUTTON_DPAD_RIGHT ) )
		{
			PageSwipeDir = ( swipeButtons & (BUTTON_SWIPE_FORWARD|BUTTON_DPAD_RIGHT) ) ? -1.0 :  1.0f;
			PageSwipeSeconds += PageSwipeTime;
		}
		if ( PageSwipeSeconds > 0.0f )
		{
			Offset += PageSwipeDir * vrFrame.DeltaSeconds / PageSwipeTime * PageSwipeDistance;
			PageSwipeSeconds -= vrFrame.DeltaSeconds;
		}
	}

	if ( State != SVS_OPEN )
	{
		gazeCursor.HideCursor();
	}
	else
	{
		gazeCursor.ShowCursor();
		gazeCursor.UpdateForUser( GazeUserId, 1.0f, CURSOR_STATE_NORMAL );
	}

	{
		// Allow single dot button on joypad to also trigger actions
		const bool	buttonState = allowSwipe ? ( ( vrFrame.Input.buttonState & BUTTON_TOUCH ) || ( vrFrame.Input.buttonState & BUTTON_A ) ) : 0;
		const bool	buttonReleased = allowSwipe ? ( !buttonState && PreviousButtonState ) : 0;
		const bool	buttonPressed = allowSwipe ? ( buttonState && !PreviousButtonState ) : 0;
		PreviousButtonState = buttonState;

		if ( buttonPressed )
		{
			// on button press, fill the history with current data
			for ( int i = 0 ; i < MAX_TOUCH_HISTORY ; i++ )
			{
				TouchPos[i] = touch;
				TimeHistory[i] = vrFrame.PoseState.TimeInSeconds;
			}

			TouchPoint = touch;
			TouchGazePos = GazePos;
			PrevTouch = touch;
			PressTime = vrFrame.PoseState.TimeInSeconds;
			HasMoved = false;

			// If an Activate() was issued programatically, this needs to be
			// cleared so the first touch doesn't get ignored.
			ActivationPress = false;

			// only allow active if the panels aren't swiping
			if ( SelectedPanel != -1 && Velocity == 0.0f )
			{
				ret.PlaySndTouchActive = true;
			}
			else
			{
				ret.PlaySndTouchInactive = true;
			}
		}

		if ( buttonReleased )
		{
			// The next press will be processed normally
			ActivationPress = false;

			// To qualify as a tap, a press and release must happen
			// within a certain amount of time, and a certain amount of movement.
			if ( HasMoved )
			{
                ret.PlaySndSwipeRelease = true;
				LOG( "Not tap because HasMoved" );
			}
			else
			{
				switch ( State )
				{
				case SVS_OPEN:
					//Close( vrFrame.PoseState.TimeInSeconds );
					ret.PlaySndReleaseAction = true;

					if ( SelectedPanel != -1 )
					{	// action on selected
						LOG( "ActivatePanelIndex %i", SelectedPanel);
						ret.ActivatePanelIndex = SelectedPanel;
					}
					else
					{	// put away when clicking on nothing
						// TODO: make this an option
						LOG( "Closing swipeview");
						Close();
					}
					ret.PlaySndReleaseAction = true;
					break;
				case SVS_CLOSED:
					//Activate( view, vrFrame.PoseState.TimeInSeconds, false );

					ret.PlaySndRelease = true;
					break;
				// do nothing on opening and closing
				case SVS_OPENING:
				case SVS_CLOSING:
				default:
					ret.PlaySndRelease = true;
					break;
				}
			}
		}

		// No dragging or velocity changes will happen on the activation press
		// or if we are just acting like a button array.
		if ( ActivationPress || !allowSwipe || State == SVS_CLOSED )
		{
			Velocity = 0.0f;
		}
		else
		{
			// only adjust velocity when moved past the trigger threshold
			if ( buttonState && !buttonPressed && HasMoved )
			{
				// on button press, fill the history with current data
				const int thisIndex = HistoryIndex % MAX_TOUCH_HISTORY;
				const int oldIndex = ( HistoryIndex + 1 ) % MAX_TOUCH_HISTORY;
				HistoryIndex++;
				TouchPos[thisIndex] = touch;
				TimeHistory[thisIndex] = vrFrame.PoseState.TimeInSeconds;

				Velocity = -SpeedScale * ( TouchPos[thisIndex].x - TouchPos[oldIndex].x ) / ( TimeHistory[thisIndex] - TimeHistory[oldIndex] );

				// Clamp to a maximum speed
				Velocity = OVR::Alg::Clamp( Velocity, -MaxVelocity, MaxVelocity );
			}


			{	// Coast after release
				Offset += Velocity * vrFrame.DeltaSeconds;

				// Friction to targetVelocity
				const float velocityAdjust = Friction * vrFrame.DeltaSeconds;

				Velocity -= Velocity * velocityAdjust;
				if ( fabs( Velocity ) < MinVelocity )
				{
					Velocity = 0.0f;
				}
			}
		}
	}

	PrevTouch = touch;

	// Position on the original layout sphere, before offsets
	GazePos.x = Offset;
	const Vector3f viewForward = MatrixForward( view );
	GazePos.y = -atan2( viewForward.y, sqrt( sqr(viewForward.x) + sqr(viewForward.z) ) );

	// Only change selection state when fully open
	if ( State == SVS_OPEN )
	{
		for ( int index = 0 ; index < Panels.GetSizeI() ; index++ )
		{
			SwipePanel & panel = Panels[index];
			if ( index == SelectedPanel )
			{
				panel.SelectState = OVR::Alg::Min( 1.0f, panel.SelectState + vrFrame.DeltaSeconds / SelectTime );
			}
			else
			{	// shrink back
				panel.SelectState = OVR::Alg::Max( 0.0f, panel.SelectState - vrFrame.DeltaSeconds / SelectTime );
			}
		}
	}

	// Opening / closing animation
	AnimationFraction = OVR::Alg::Min( 1.0,
			( vrFrame.PoseState.TimeInSeconds - AnimationStartTime ) / OpenAnimationTime );
	// allowing AnimationFraction to reach 0.0 causes an invalid matrix to be formed in Draw()
	AnimationFraction = OVR::Alg::Clamp( AnimationFraction, 0.00001f, 1.0f );
	if ( State == SVS_CLOSING )
	{
		if ( AnimationFraction >= 1.0f )
		{
			State = SVS_CLOSED;
		}
		else
		{
			AnimationFraction = 1.0f - AnimationFraction;
		}
	}
	if ( State == SVS_OPENING && AnimationFraction >= 1.0f )
	{
		State = SVS_OPEN;
	}

	ret.ViewState = State;
	ret.TouchState = TouchState;

	PanelRenderList.Resize( 0 );
	if ( State != SVS_CLOSED )
	{
		// Setup for tracing against panels
		const Vector3f rayTraceP1 = MatrixOrigin( view.Inverted() );
		const Vector3f rayTraceP2 = rayTraceP1 + 10.0f * MatrixForward( view );
		float	closestPanelDist = 1.0f;
		int		updatedSelectedPanel = -1;

		// update panel transforms
		const float	animatedScale = OVR::Alg::Min( 1.0f, AnimationFraction * 10.0f );

		// Push this last
		PanelRenderInfo selectedPanelInfo;
		selectedPanelInfo.PanelIndex = -1;

		//LOG( "offset %f forward %f", Offset, ForwardYaw );
		const float animationPanelOffset = AnimationCenterPanel[0] * SlotSize.x - Offset;
		const float animationPanelOffsetY = PanelAngleY( AnimationCenterPanel[1] );
		for ( int index = 0 ; index < Panels.GetSizeI() ; index++ )
		{
			const int panelX = index / LayoutRows;
			const int panelXCentered = ( LayoutColumns( ) / 2 ) - panelX;
			const int panelY = index % LayoutRows;
			SwipePanel & panel = Panels[index];

			const float viewAngleOffset = panelXCentered * SlotSize.x - Offset;

			// Ignore all panels more than 90 degrees to either side of the current offset
			if ( viewAngleOffset < -M_PI / 2 || viewAngleOffset > M_PI/2 )
			{
				continue;
			}

			const float	angleOffset = viewAngleOffset;
			const float angleOffsetVertical = PanelAngleY( panelY );

			// Panels farther from the center hit their final spot later
			const float slotDistance = sqrt(
					sqr( panelX - AnimationCenterPanel[0] )
					+ sqr( panelY - AnimationCenterPanel[1] ) );
			const float maxDistance = 3.0f;
			const float clampedDistance = OVR::Alg::Clamp( slotDistance, 1.0f, maxDistance );

			// animate the closer panels so they land sooner
			// square it so they smack into the UI surface hard
			const float animationFraction = sqr( OVR::Alg::Min( 1.0f, AnimationFraction * maxDistance / clampedDistance ) );
			const float invAnimationFraction = 1.0 - animationFraction;

			const float distance = -( Radius.x - SelectDistance * panel.SelectState );

			const float animatedAngleOffset = animationFraction * angleOffset + invAnimationFraction * animationPanelOffset;
			float animatedAngleOffsetVertical = animationFraction * angleOffsetVertical + invAnimationFraction * animationPanelOffsetY;
#ifdef FLAT_ROWS
			animatedAngleOffsetVertical *= -distance / Radius.x;
#endif
			const float animatedDistance = distance - invAnimationFraction * slotDistance * 1.0f;

			const float scaleX = animatedScale*Radius.x*panel.Size.x*0.5;
			const float scaleY = animatedScale*Radius.x*panel.Size.y*0.5;

			Matrix4f pMat = MatrixForPanel( StartViewOrigin,
					animatedAngleOffset + ForwardYaw,
					animatedAngleOffsetVertical,
					animatedDistance, scaleX, scaleY );

			PanelRenderInfo info;
			info.Mat = pMat;
			info.PanelIndex = index;

			// check for changing selection
			Vector2f localHit;
			const float dist = LineOnMatrix( rayTraceP1, rayTraceP2, pMat, localHit );
			if ( dist >= 0.0f && localHit.x > -1.0f && localHit.x < 1.0f
					&& localHit.y > -1.0f && localHit.y < 1.0f && dist < closestPanelDist )
			{
				closestPanelDist = dist;
				updatedSelectedPanel = index;
			}

			// Place the selection highlight behind the panel
			if ( index == SelectedPanel && ( State == SVS_OPEN || State == SVS_CLOSING ) )
			{
				const float highlightDistance = animatedDistance - 0.01f;
				Matrix4f pMat = MatrixForPanel( StartViewOrigin,
						animatedAngleOffset + ForwardYaw,
						animatedAngleOffsetVertical,
						highlightDistance,
						scaleX + animatedScale * SlotSize.x * HighlightSlotFraction,
						scaleY + animatedScale * SlotSize.x * HighlightSlotFraction );
				SelectionTransform = pMat;
				info.Selected = true;
			}

			// Go ahead and tell the text to draw here before the font surface is Finish()ed. This doesn't actually
			// render text, but puts it in a buffer to be finished and rendered to both eyes.
//			Vector3f textPos = pMat.GetZBasis() * ( animatedDistance + 0.08f ) + StartViewOrigin - ( pMat.GetYBasis() * 1.1f );
			Vector3f textPos = pMat.Transform( Vector3f( 0.0f, -1.2f, 0.0f ) );
			Vector3f textNormal = pMat.GetZBasis();
			textNormal.Normalize();
			Vector3f textUp = pMat.GetYBasis();
			textUp.Normalize();

			fontParms_t parms;
			parms.AlignHoriz = HORIZONTAL_CENTER;
			parms.AlignVert = VERTICAL_CENTER;
			parms.Billboard = false;

			if ( index == SelectedPanel )
			{
				fontSurface.DrawText3D( font, parms, textPos, textNormal, textUp,
						1.0f, Vector4f( 1.0f, 1.0f, 1.0f, panel.SelectState ), panel.Text );
				selectedPanelInfo = info;
			}
			else
			{
				PanelRenderList.PushBack( info );
			}
		}

		// now push the selected panel at the end, so it always draws on top
		if ( selectedPanelInfo.PanelIndex != -1 )
		{
			PanelRenderList.PushBack( selectedPanelInfo );
		}

		if ( updatedSelectedPanel != SelectedPanel )
		{	// selection state changed
			if ( SelectedPanel != -1 )
			{
				ret.PlaySndDeselect = true;
			}
			if ( updatedSelectedPanel != -1 )
			{
				ret.PlaySndSelect = true;
			}
			SelectedPanel = updatedSelectedPanel;

			// Changing selection state will always require another touch down
			// to activate a button.
			HasMoved = true;
		}

	}

	return ret;
}

void SwipeView::Draw( const Matrix4f & mvp )
{
	if ( State == SVS_CLOSED )
	{	// full closed
		return;
	}
	if ( Panels.GetSizeI() < 1 )
	{
		return;
	}

	const GlProgram & prog = ProgPanel;

	glUseProgram( prog.program );
	glActiveTexture( GL_TEXTURE0 );
	glEnable( GL_DEPTH_TEST );

	// It is important that glBlendFuncSeparate be used so that destination alpha
	// correctly holds the opacity over the overlay plane.  If normal blending is
	// used, the cursor ghosts will "punch holes" in things through to the overlay plane.
	glEnable( GL_BLEND );
	glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );

	glActiveTexture( GL_TEXTURE1 );
	glBindTexture( GL_TEXTURE_2D, BorderTexture2_1.texture );

	for ( int index = 0 ; index < PanelRenderList.GetSizeI() ; index++ )
	{
		PanelRenderInfo const & info = PanelRenderList[index];
		SwipePanel const & panel = Panels[info.PanelIndex];

		Matrix4f pMat = mvp * info.Mat;

		// Draw the selection highlight behind the panel
		if ( info.Selected && ( State == SVS_OPEN || State == SVS_CLOSING ) )
		{
			glUseProgram( ProgHighlight.program );

			Matrix4f pMat = mvp * SelectionTransform;
			glUniformMatrix4fv(ProgHighlight.uMvp, 1, GL_FALSE, pMat.Transposed().M[0] );
            
            float const colorScale = 0.5f;  // for tuning
			const Vector4f color( 0.11764f *colorScale, 0.34902f * colorScale, 0.61569f * colorScale, 1.0f );
            float const actionScale = 0.5f; // for tuning
			const Vector4f colorAction( 0.09804f * actionScale, 0.6039f * actionScale, 0.9608f * actionScale, 1.0f );
			const Vector4f colorTouching( 0.09804f, 0.6039f, 0.9608f, 1.0f );
			glUniform4fv(ProgHighlight.uColor, 1,
					State == SVS_CLOSING ? &colorAction.x :
							( (PreviousButtonState && !HasMoved) ? &colorTouching.x : &color.x ) );
			GeoPanel.Draw();

			glUseProgram( prog.program );
		}

		glUniformMatrix4fv(prog.uMvp, 1, GL_FALSE, pMat.Transposed().M[0] );

		glActiveTexture( GL_TEXTURE0 );
		glBindTexture( GL_TEXTURE_2D, panel.Texture );
		GeoPanel.Draw();

	}

	glBindVertexArrayOES_( 0 );

	glActiveTexture( GL_TEXTURE1 );
	glBindTexture( GL_TEXTURE_2D, 0 );

	glActiveTexture( GL_TEXTURE0 );
	glDisable( GL_BLEND );
}

}	// namespace OVR


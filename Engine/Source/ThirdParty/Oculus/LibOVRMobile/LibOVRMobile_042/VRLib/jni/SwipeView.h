/************************************************************************************

Filename    :   SwipeView.h
Content     :   Gaze and swipe selection of panels.
Created     :   February 28, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_SwipeView_h
#define OVR_SwipeView_h

#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_Array.h"
#include "GlGeometry.h"
#include "GlProgram.h"
#include "GlTexture.h"
#include "GazeCursor.h"
#include "Input.h"
#include "BitmapFont.h"

namespace OVR
{

class OvrGazeCursor;

// Panels are NOT freed by the SwipeView; the owner is responsible.
class SwipePanel
{
public:
	SwipePanel() : Identifier( NULL ), Text( NULL ), Texture( 0 ), Id( -1 ), SelectState( 0.0f ) {}

	void *		Identifier;
	const char * Text;
	Vector2f	Size;			// on a unit circle (radians)
	GLuint		Texture;
	int			Id;				// unique id

	float		SelectState;	// 0.0 = on the wall, 1.0 = fully selected
};

enum SwipeViewState
{
	SVS_CLOSED,
	SVS_OPENING,
	SVS_OPEN,
	SVS_CLOSING
};

enum SwipeViewTouchState
{
	SVT_ACTIVATION,		// must release before anything else
	SVT_WILL_DRAG,		// touch down will go to drag
	SVT_WILL_ACTION,	// touch down will go to action
	SVT_DRAG,			// release will do nothing
	SVT_ACTION,			// release will do action
	// action can convert to drag, but not the other way

	SVT_NUM_STATES
};

class SwipeAction
{
public:
	SwipeViewState		ViewState;		// state of entire view

	SwipeViewTouchState	TouchState;		// change gaze cursor based on state

	bool	PlaySndSelect;			// gaze goes on panel
	bool	PlaySndDeselect;		// gaze goes off panel

	bool	PlaySndTouchActive;		// touch down on active panel
	bool	PlaySndTouchInactive;	// touch down in inactive area for drag
	bool	PlaySndActiveToDrag;	// dragged enough to change from active to drag
	bool	PlaySndRelease;			// release wihout action
	bool	PlaySndReleaseAction;	// release and action
    bool    PlaySndSwipeRelease;    // released a swipe

	int		ActivatePanelIndex;		// -1 = no action
};

class SwipeView
{
public:
	SwipeView();
	~SwipeView();

	void		Init( OvrGazeCursor & gazeCursor );
	SwipeAction	Frame( OvrGazeCursor & gazeCursor, BitmapFont const & font, BitmapFontSurface & fontSurface, 
                        const VrFrame & vrFrame, const Matrix4f & view, const bool allowSwipe );
	void		Draw( const Matrix4f & mvp );

	// The offset will be left where it was, unless
	// the view has never been opened or was closed far enough away from view
	void		Activate( );

	void		Close();

	void		ClearPanelSelectState();
	float		PanelAngleY( int panelY );
	int			LayoutColumns() const;

	eGazeCursorStateType	GazeCursorType() const;

	// True by default -- drags the panels near the edge of the view
	// if you look away from all of them.  Some scenes may want to
	// leave them fixed in place.
	bool		ClampPanelOffset;

	bool		CloseOnNextFrame;
	bool		ActivateOnNextFrame;

	// Swiping will adjust the offset.
	//
	// Turning will simultaneously adjust ForwardYaw and the
	// offset so the panels stay in the same world location,
	// but turning 360 degrees will display different panels.
	//
	// If the view would look far enough off the side to put
	// all panels off the screen, the key yaw will be dragged with
	// the view without changing the offset, pulling the panels
	// with it.
	//
	// Subsequent panels are drawn to the right of previous panels,
	// which is more negative yaw.
	Vector3f	ForwardAtOffset;		// world space vector
	float		ForwardYaw;				// increases with rotation to the left, can wrapped around
	float		Offset;					// Offset in the panel list at ForwardYaw
	float		Velocity;				// on a unit circle (radians)

	// The touchpad events don't come in very synchronized with the frames, so
	// average over several frames for velocity.
	static const int MAX_TOUCH_HISTORY = 4;
	Vector2f	TouchPos[MAX_TOUCH_HISTORY];
	double		TimeHistory[MAX_TOUCH_HISTORY];
	int			HistoryIndex;

	Vector3f	StartViewOrigin;

	GlProgram	ProgPanel;
	GlProgram	ProgHighlight;
	GlGeometry	GeoPanel;
	GlTexture	BorderTexture2_1;
	GlTexture	BorderTexture1_1;

	SwipeViewState		State;
	SwipeViewTouchState	TouchState;

	double		PressTime;			// time when touch went down, not needed now, perhaps for long-press

	Vector2f	TouchPoint;			// Where touch went down
	Vector2f	TouchGazePos;		// Where touch went down
	Vector2f	PrevTouch;			// Touch position at last frame
	bool		PreviousButtonState;// union of touch and joystick button


	bool		HasMoved;			// a release will be a tap if not moved
	bool		ActivationPress;	// no drag until release

	Vector2f	GazePos;			// on a unit circle (radians)

	Vector2f	Radius;				// if == distance, view is in center of curve. If > distance, view is near edge

	Vector2f	SlotSize;			// on a unit circle (radians)
	int			LayoutRows;
	float		RowOffset;
	Array<SwipePanel>	Panels;

	bool		EverOpened;			// if it has been opened before, subsequent opens can return to same position
	int			AnimationCenterPanel[2];
	float		AnimationFraction;	// goes 0 to 1 for opening and 1 to 0 for closing
	double		AnimationStartTime;

	int			SelectedPanel;	// put the highlight behind this one

	// Page-swipe
	float		PageSwipeSeconds;
	float		PageSwipeDir;

	// Behavior tuning, can be adjusted on a per-object basis
	float		Distance;
	float		SelectDistance;		// when selected, Radius will be reduced by this much
	float		TapMaxDistance;		// before transitioning from action to drag
	float		GazeMaxDistance;	// before transitioning from action to drag
	float		SpeedScale;
	float		Friction;
	float		SnapSpeed;
	float		SelectTime;			// time to go to or from selected state
	float		HighlightSlotFraction;
	float		MaxVelocity;		// radians per second
	float		MinVelocity;		// radians per second
	float		VelocityMixTime;	// half of the velocity is mixed in this time
	float		OpenAnimationTime;

	gazeCursorUserId_t	GazeUserId;	// id unique to this swipe view for interacting with gaze cursor

	class PanelRenderInfo
	{
	public:
		PanelRenderInfo() :
			PanelIndex( -1 ),
			Selected( false )
		{
		}

		Matrix4f		Mat;
		int				PanelIndex;
		bool			Selected;
	};

	Array< PanelRenderInfo >	PanelRenderList;	// transforms for all panels, calculated in Frame() so it's not done for each eye in Draw()
	Matrix4f					SelectionTransform;	// transform of the selection highlight
};	

void	LoadTestSwipeTextures();
void	PopulateTestSwipeView( SwipeView & sv, const int rows );

}	// namespace OVR

#endif	// OVR_SwipeView_h

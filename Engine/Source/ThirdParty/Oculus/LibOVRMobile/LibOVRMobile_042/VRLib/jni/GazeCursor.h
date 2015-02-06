/************************************************************************************

Filename    :   GazeCursor.h
Content     :   Global gaze cursor.
Created     :   June 6, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( Ovr_GazeCursor_h )
#define Ovr_GazeCursor_h

#include "Kernel/OVR_TypesafeNumber.h"
#include "Kernel/OVR_Math.h"

namespace OVR {

enum eGazeCursorUserIdType
{
	INVALID_GAZE_CURSOR_USER_ID = 0
};

typedef TypesafeNumberT< int, eGazeCursorUserIdType, INVALID_GAZE_CURSOR_USER_ID > gazeCursorUserId_t;

enum eGazeCursorStateType
{
	CURSOR_STATE_NORMAL,		// tap will not activate, but might drag
	CURSOR_STATE_HILIGHT,		// tap will activate
	CURSOR_STATE_PRESS,			// release will activate
	CURSOR_STATE_HAND,			// dragging
	CURSOR_STATE_MAX
};

//==============================================================
// OvrGazeCurorInfo
class OvrGazeCursorInfo
{
public:
	OvrGazeCursorInfo() :
		Distance( FLT_MAX ),
		State( CURSOR_STATE_NORMAL )
	{
	}

	void				Reset( float const d )
	{
		UserId.Release();
		Distance = d;
		State = CURSOR_STATE_NORMAL;
	}

	gazeCursorUserId_t		UserId;		// id of the user that placed the cursor
	float					Distance;	// distance from the view position, along the view direction, to the cursor
	eGazeCursorStateType	State;		// state of the cursor
};

//==============================================================
// OvrGazeCursor
//
// Various systems can utilize the gaze cursor, but only the user that places the 
// gaze cursor closest to the view (i.e. that detected the front-most item) should
// treat the cursor as active.  All users behind the closest system should act as if
// the gaze cursor is disabled while they are not closest.
class OvrGazeCursor
{
public:
	virtual						~OvrGazeCursor() { }

	// Initialize the gaze cursor system and loads the cursor images from
	// the application package.
	virtual	void				Init() = 0;

	// Shutdown the gaze cursor system.
	virtual void				Shutdown() = 0;

	// This should be called at the beginning of the frame before and system updates the gaze cursor.
	virtual	void				BeginFrame() = 0;

	// Each system using the gaze cursor should call this once to get its own id.
	virtual gazeCursorUserId_t	GenerateUserId() = 0;

	// Updates the gaze cursor distance if the distance passed is less than the current
	// distance.  Systems that use the gaze cursor should use this method so that they
	// interact civilly with other systems using the gaze cursor.
	virtual void				UpdateForUser(	gazeCursorUserId_t const userId, 
										float const d,
										eGazeCursorStateType const state ) = 0;

	// Call when the scene changes or the camera moves a large amount to clear out the cursor trail
	virtual void				ClearGhosts() = 0;

	// Called once per frame to update logic.
	virtual	void				Frame( Matrix4f const & viewMatrix, float const deltaTime ) = 0;

	// Renders the gaze cursor.
	virtual void				Render( int const eye, Matrix4f const & mvp ) const = 0;

	// Users should call this function to determine if the gaze cursor is relevant for them.
	virtual bool				IsActiveForUser( gazeCursorUserId_t const userId ) const = 0;

	// Returns the current info about the gaze cursor.
	virtual OvrGazeCursorInfo	GetInfo() const = 0;

	// Force the distance to a specific value -- this will set the distance even if
	// it is further away than the current distance. Unless your intent is to overload
	// the distance set by all other systems that use the gaze cursor, don't use this.
	virtual void				ForceDistance( gazeCursorUserId_t const userId, float const d ) = 0;

	// Sets the rate at which the gaze cursor icon will spin.
	virtual void				SetRotationRate( float const degreesPerSec ) = 0;

	// Sets the scale factor for the cursor's size.
	virtual void				SetCursorScale( float const scale ) = 0;

	// Hide the gaze cursor.
	virtual void				HideCursor() = 0;
	
	// Show the gaze cursor.
	virtual void				ShowCursor() = 0;

	// Hide the gaze cursor for a specified number of frames
	// Used in App::Resume to hide the cursor interpolating to the gaze orientation from its initial orientation
	virtual void				HideCursorForFrames( const int hideFrames ) = 0;

	// Sets an addition distance to offset the cursor for rendering. This can help avoid
	// z-fighting but also helps the cursor to feel more 3D by pushing it away from surfaces.
	// This is the amount to move towards the camera, so it should be positive to bring the
	// cursor towards the viewer.
	virtual void				SetDistanceOffset( float const offset ) = 0;

	// Start a timer that will be shown animating the cursor. If timeBeforeShowingTimer
	// > 0, then the cursor will not show the timer until that much time has passed.
	virtual void				StartTimer( float const durationSeconds, 
									float const timeBeforeShowingTimer ) = 0;

	// Cancels the timer if it's active.
	virtual void				CancelTimer() = 0;
};

} // namespace OVR

#endif // Ovr_GazeCursor_h

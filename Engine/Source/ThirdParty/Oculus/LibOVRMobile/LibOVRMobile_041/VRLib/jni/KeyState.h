/************************************************************************************

Filename    :   KeyState.h
Content     :   Tracking of short-press, long-press and double-tapping of keys.
Created     :   June 18, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( Ovr_KeyState_h )
#define Ovr_KeyState_h

namespace OVR {

//==============================================================
// KeyState
//
// Single-press / Short-press
// |-------------------------|
// |----------------- Double-tap Time --------------|
// down ---> down_time ---> up ---> up_time .........
//
// Because we're detecting long-presses, we don't know at the point of a down
// whether this is going to be a short- or long-press.  We must wait for one
// of the two possible state changes, which is an up event (signaling a short-
// press), or the expiration of the long-press time (signaling a long press).
// In the case of a long-press, we want the long-press to signal as soon as
// the down time has exceeded the long-press time. If we didn't do this, the
// user would not have any indication of when they exceeded the long-press
// time (unless we add a sound - which doesn't work if the device is muted -
// or something like a cursor change). Because we are acting on the time 
// threshold and not a button up, the up will come after the long-press is 
// released and we need to ignore that button up to avoid it being treated
// as if it were the up from a short-press or double-tap.
//
// Double-tap
// |--------------------------- Total Time --------------------------------|
// |----------------- Double-tap Time --------------|
// down ---> down_time ---> up ---> up_time ---> down ---> down_time ---> up
// 
// In addition to differentiating short- and long- presses, we must 
// differentiate short-press and double-tap.  We cannot know on an initial
// button-up if the user intends a double-tap.  We must wait for some time
// threshold from the initial down to be exceeded without another down before 
// we know it's not a double-tap.
//
// Long Press
// |-----------------|
// down ---> down_time
//
// It is notable that the long press is signaled as soon as the long press
// time is exceeded.

class KeyState
{
public:
	static const int MAX_EVENTS = 3;

	enum eKeyEventType
	{
		KEY_EVENT_NONE,
		KEY_EVENT_SHORT_PRESS,
		KEY_EVENT_DOUBLE_TAP,
		KEY_EVENT_LONG_PRESS,
		KEY_EVENT_DOWN,
		KEY_EVENT_UP,
		KEY_EVENT_MAX
	};

	static char const *	EventNames[KEY_EVENT_MAX];

					KeyState( double const doubleTapTime, double const longPressTime );

	void			HandleEvent( double const time, bool const down, int const repeatCount );

	eKeyEventType	Update( double const time );

	void			Reset();

	bool			IsDown() const { return Down; }

	double			GetLongPressTime() const { return LongPressTime; }
	double			GetDoubleTapTime() const { return DoubleTapTime; }

private:
	int				NumEvents;				// number of events that have occurred
	double			EventTimes[MAX_EVENTS];	// times for first down, up, second down
	float			DoubleTapTime;			// two down events must occur within this time for a double-tap to occur
	float			LongPressTime;			// a single down within this time indicates a long-press
	bool			Down;					// true if the key is down
	eKeyEventType	PendingEvent;			// next pending event== stored so that the client only has to handle events in one place
};

} // namespace OVR

#endif // Ovr_KeyState_h
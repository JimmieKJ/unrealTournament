/************************************************************************************

Filename    :   VRMenuEvent.h
Content     :   Event class for menu events.
Created     :   June 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_VRMenuEvent_h )
#define OVR_VRMenuEvent_h

#include "VRMenuObject.h"

namespace OVR {

enum eVRMenuEventType
{
	VRMENU_EVENT_FOCUS_GAINED,		// TargetHandle is the handle of the object gaining focus
	VRMENU_EVENT_FOCUS_LOST,		// TargetHandle is the handle of the object losing focus
	VRMENU_EVENT_TOUCH_DOWN,		// TargetHandle will be the focused handle
	VRMENU_EVENT_TOUCH_UP,			// TargetHandle will be the focused handle
    VRMENU_EVENT_TOUCH_RELATIVE,    // sent when a touch position changes, TargetHandle will be the focused handle
    VRMENU_EVENT_TOUCH_ABSOLUTE,    // sent whenever touch is down, TargetHandle will be the focused handle
	VRMENU_EVENT_SWIPE_FORWARD,		// sent for a forward swipe event, TargetHandle will be the focused handle
	VRMENU_EVENT_SWIPE_BACK,		// TargetHandle will be the focused handle
	VRMENU_EVENT_SWIPE_UP,			// TargetHandle will be the focused handle
	VRMENU_EVENT_SWIPE_DOWN,		// TargetHandle will be the focused handle
    VRMENU_EVENT_FRAME_UPDATE,		// TargetHandle should be empty
    VRMENU_EVENT_SUBMIT_FOR_RENDERING,
    VRMENU_EVENT_RENDER,
	VRMENU_EVENT_OPENING,			// sent when a menu starts to open
	VRMENU_EVENT_OPENED,			// sent when a menu opens
	VRMENU_EVENT_CLOSING,			// sent when a menu starts to close
	VRMENU_EVENT_CLOSED,			// sent when a menu closes
	VRMENU_EVENT_INIT,				// sent when the owning menu initializes
	
	VRMENU_EVENT_MAX
};

typedef BitFlagsT< eVRMenuEventType, uint64_t > VRMenuEventFlags_t;

enum eEventDispatchType
{
    EVENT_DISPATCH_TARGET,      // send only to target
    EVENT_DISPATCH_FOCUS,       // send to all in focus path
    EVENT_DISPATCH_BROADCAST    // send to all
};

//==============================================================
// VRMenuEvent
class VRMenuEvent
{
public:
    static char const * EventTypeNames[VRMENU_EVENT_MAX];
    
    // non-target
	VRMenuEvent( eVRMenuEventType const eventType, eEventDispatchType const dispatchType, 
            menuHandle_t const & targetHandle, Vector3f const & floatValue, HitTestResult const & hitResult ) :
		EventType( eventType ),
        DispatchType( dispatchType ),
        TargetHandle( targetHandle ),
        FloatValue( floatValue ),
        HitResult( hitResult )
	{
	}

    eVRMenuEventType	EventType;
    eEventDispatchType  DispatchType;
	menuHandle_t		TargetHandle;  // valid only if targeted to a specific object
    Vector3f            FloatValue;
    HitTestResult		HitResult;
};

} // namespace OVR

#endif // OVR_VRMenuEvent_h

/************************************************************************************

Filename    :   VRMenuComponent.h
Content     :   Menuing system for VR apps.
Created     :   June 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_VRMenuComponent_h )
#define OVR_VRMenuComponent_h

#include "VRMenuObject.h"
#include "VRMenuEvent.h"
#include "SoundLimiter.h"

namespace OVR {

enum eMsgStatus
{
    MSG_STATUS_CONSUMED,    // message was consumed, don't pass to anything else
    MSG_STATUS_ALIVE        // continue passing up
};

enum eVRMenuComponentFlags
{
    VRMENU_COMPONENT_EVENT_HANDLER, // handles events
    VRMENU_COMPONENT_FRAME_UPDATE,  // gets Frame updates
};

typedef BitFlagsT< eVRMenuComponentFlags > VRMenuComponentFlags_t;

class VRMenuEvent;
class App;
struct VrFrame;

//==============================================================
// VRMenuComponent
//
// Base class for menu components
class VRMenuComponent
{
public:
	static const int		TYPE_ID = -1;
	static const char *		TYPE_NAME;

	explicit				VRMenuComponent( VRMenuEventFlags_t const & eventFlags ) :
								EventFlags( eventFlags )
							{
							}
	virtual					~VRMenuComponent() { }

    bool                    HandlesEvent( VRMenuEventFlags_t const eventFlags ) const { return ( EventFlags & eventFlags ) != 0; }

    // only called if fla
    eMsgStatus              OnEvent( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
                                    VRMenuObject * self, VRMenuEvent const & event );

    VRMenuEventFlags_t      GetEventFlags() const { return EventFlags; }

	virtual int				GetTypeId( ) const { return TYPE_ID; }
	virtual const char *	GetTypeName( ) const { return TYPE_NAME; }

private:
    virtual eMsgStatus      OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
                                    VRMenuObject * self, VRMenuEvent const & event ) = 0;

private:
	VRMenuEventFlags_t      EventFlags; // used to dispatch events to the correct handler
};

//==============================================================
// VRMenuComponent_OnFocusGained
class VRMenuComponent_OnFocusGained : public VRMenuComponent
{
public:
	VRMenuComponent_OnFocusGained() :
		VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FOCUS_GAINED ) )
	{
	}
};

//==============================================================
// VRMenuComponent_OnFocusLost
class VRMenuComponent_OnFocusLost : public VRMenuComponent
{
public:
	VRMenuComponent_OnFocusLost() :
		VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FOCUS_LOST ) )
	{
	}
};

//==============================================================
// VRMenuComponent_OnTouchDown
class VRMenuComponent_OnTouchDown : public VRMenuComponent
{
public:
	VRMenuComponent_OnTouchDown() :
		VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_TOUCH_DOWN ) )
	{
	}
};

//==============================================================
// VRMenuComponent_OnTouchUp
class VRMenuComponent_OnTouchUp : public VRMenuComponent
{
public:
	VRMenuComponent_OnTouchUp() :
		VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_TOUCH_UP ) )
	{
	}
};

//==============================================================
// VRMenuComponent_OnSubmitForRendering
class VRMenuComponent_OnSubmitForRendering : public VRMenuComponent
{
public:
	VRMenuComponent_OnSubmitForRendering() :
		VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_SUBMIT_FOR_RENDERING ) )
	{
	}
};

//==============================================================
// VRMenuComponent_OnRender
class VRMenuComponent_OnRender : public VRMenuComponent
{
public:
	VRMenuComponent_OnRender() :
		VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_RENDER ) )
	{
	}
};

//==============================================================
// VRMenuComponent_OnTouchRelative
class VRMenuComponent_OnTouchRelative : public VRMenuComponent
{
public:
	VRMenuComponent_OnTouchRelative() :
		VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_TOUCH_RELATIVE ) )
	{
	}
};

//==============================================================
// VRMenuComponent_OnTouchAbsolute
class VRMenuComponent_OnTouchAbsolute : public VRMenuComponent
{
public:
	VRMenuComponent_OnTouchAbsolute() :
		VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_TOUCH_ABSOLUTE ) )
	{
	}
};

} // namespace OVR

#endif // OVR_VRMenuComponent_h
/************************************************************************************

Filename    :   VRMenuComponent.h
Content     :   Menuing system for VR apps.
Created     :   June 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "VRMenuComponent.h"

#include "Android/Log.h"
#include "VrApi/VrApi.h"		// ovrPoseStatef

#include "../Input.h"
#include "../VrCommon.h"
#include "../App.h"
#include "../Input.h"
#include "VRMenuMgr.h"

namespace OVR {

	const char * VRMenuComponent::TYPE_NAME = "";

//==============================
// VRMenuComponent::OnEvent
eMsgStatus VRMenuComponent::OnEvent( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
        VRMenuObject * self, VRMenuEvent const & event )
{
    OVR_ASSERT( self != NULL );

    //-------------------
    // do any pre work that every event handler must do
    //-------------------

    //DROIDLOG( "VrMenu", "OnEvent '%s' for '%s'", VRMenuEventTypeNames[event.EventType], self->GetText().ToCStr() );

    // call the overloaded implementation
    eMsgStatus status = OnEvent_Impl( app, vrFrame, menuMgr, self, event );

    //-------------------
    // do any post work that every event handle must do
    //-------------------

    return status;
}


} // namespace OVR
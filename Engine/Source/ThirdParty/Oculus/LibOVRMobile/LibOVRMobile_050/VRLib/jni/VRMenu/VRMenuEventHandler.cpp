/************************************************************************************

Filename    :   VRMenuEventHandler.cpp
Content     :   Menu component for handling hit tests and dispatching events.
Created     :   June 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "VRMenuEventHandler.h"

#include "Android/LogUtils.h"

#include "VrApi/VrApi.h"		// ovrPoseStatef

#include "../Input.h"
#include "../VrCommon.h"
#include "../App.h"
#include "../GazeCursor.h"
#include "VRMenuMgr.h"
#include "VRMenuComponent.h"

namespace OVR {

//==============================
// VRMenuEventHandler::VRMenuEventHandler
VRMenuEventHandler::VRMenuEventHandler() 
{
}

//==============================
// VRMenuEventHandler::~VRMenuEventHandler
VRMenuEventHandler::~VRMenuEventHandler() 
{ 
}

//==============================
// VRMenuEventHandler::Frame
void VRMenuEventHandler::Frame( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, BitmapFont const & font, 
        menuHandle_t const & rootHandle,Posef const & menuPose, gazeCursorUserId_t const & gazeUserId, 
		Array< VRMenuEvent > & events )
{
	VRMenuObject * root = menuMgr.ToObject( rootHandle );
	if ( root == NULL )
	{
		return;
	}

	// find the object the gaze is touching and update gaze focus
	const Matrix4f viewMatrix = app->GetLastViewMatrix();
	const Vector3f viewPos( GetViewMatrixPosition( viewMatrix ) );
	const Vector3f viewFwd( GetViewMatrixForward( viewMatrix ) );

	HitTestResult result;
	menuHandle_t hitHandle = root->HitTest( app, menuMgr, font, menuPose, viewPos, viewFwd, ContentFlags_t( CONTENT_SOLID ), result );
	result.RayStart = viewPos;
	result.RayDir = viewFwd;

	VRMenuObject * hit = hitHandle.IsValid() ? menuMgr.ToObject( hitHandle ) : NULL;
	app->GetGazeCursor().UpdateForUser( gazeUserId, result.t, ( hit != NULL && !( hit->GetFlags() & VRMenuObjectFlags_t( VRMENUOBJECT_NO_GAZE_HILIGHT ) ) ) ? CURSOR_STATE_HILIGHT : CURSOR_STATE_NORMAL );
/*
    if ( hit != NULL )
    {
        app->ShowInfoText( 0.0f, "%s", hit->GetText().ToCStr() );
    }
*/
	bool focusChanged = ( hitHandle != FocusedHandle );
	if ( focusChanged )
	{
		// focus changed
		VRMenuObject * oldFocus = menuMgr.ToObject( FocusedHandle );
		if ( oldFocus != NULL )
		{
			// setup event for item losing the focus
			VRMenuEvent event( VRMENU_EVENT_FOCUS_LOST, EVENT_DISPATCH_TARGET, FocusedHandle, Vector3f( 0.0f ), result );
			events.PushBack( event );
		}
		if ( hit != NULL )
		{
			if ( ( hit->GetFlags() & VRMENUOBJECT_FLAG_NO_FOCUS_GAINED ) == 0 )
			{
				// set up event for item gaining the focus
				VRMenuEvent event( VRMENU_EVENT_FOCUS_GAINED, EVENT_DISPATCH_FOCUS, hitHandle, Vector3f( 0.0f ), result );
				events.PushBack( event );
			}
		}
		FocusedHandle = hitHandle;
	}

	bool touchPressed = ( vrFrame.Input.buttonPressed & ( BUTTON_TOUCH | BUTTON_A ) ) != 0;
	bool touchReleased = !touchPressed && ( vrFrame.Input.buttonReleased & ( BUTTON_TOUCH | BUTTON_A ) ) != 0;
    bool touchDown = ( vrFrame.Input.buttonState & BUTTON_TOUCH ) != 0;

    if ( ( vrFrame.Input.buttonPressed & BUTTON_SWIPE_UP ) != 0 )
    {
		VRMenuEvent event( VRMENU_EVENT_SWIPE_UP, EVENT_DISPATCH_FOCUS, FocusedHandle, Vector3f( 0.0f ), result );
		events.PushBack( event );
    }
    if ( ( vrFrame.Input.buttonPressed & BUTTON_SWIPE_DOWN ) != 0 )
    {
		VRMenuEvent event( VRMENU_EVENT_SWIPE_DOWN, EVENT_DISPATCH_FOCUS, FocusedHandle, Vector3f( 0.0f ), result );
		events.PushBack( event );

    }
    if ( ( vrFrame.Input.buttonPressed & BUTTON_SWIPE_FORWARD ) != 0 )
    {
		VRMenuEvent event( VRMENU_EVENT_SWIPE_FORWARD, EVENT_DISPATCH_FOCUS, FocusedHandle, Vector3f( 0.0f ), result );
		events.PushBack( event );

    }
    if ( ( vrFrame.Input.buttonPressed & BUTTON_SWIPE_BACK ) != 0 )
    {
		VRMenuEvent event( VRMENU_EVENT_SWIPE_BACK, EVENT_DISPATCH_FOCUS, FocusedHandle, Vector3f( 0.0f ), result );
		events.PushBack( event );
    }

 /*   
    // report swipe data
    char const * swipeNames[5] = { "none", "down", "up", "back", "forward" };
    int swipeUpDown = ( vrFrame.Input.buttonPressed & BUTTON_SWIPE_UP ) ? 2 : 0;
    swipeUpDown = ( vrFrame.Input.buttonPressed & BUTTON_SWIPE_DOWN ) ? 1 : swipeUpDown;
    int swipeForwardBack = ( vrFrame.Input.buttonPressed & BUTTON_SWIPE_FORWARD ) ? 4 : 0;
    swipeForwardBack = ( vrFrame.Input.buttonPressed & BUTTON_SWIPE_BACK ) ? 3 : swipeForwardBack;
 
    app->ShowInfoText( 1.0f, "touch %s\n( %s, %s )\n( %.2f, %.2f )\n( %.2f, %.2f )", 
            touchDown ? swipeNames[1] : swipeNames[2],
            swipeNames[swipeUpDown], swipeNames[swipeForwardBack], 
            vrFrame.Input.touch[0], vrFrame.Input.touch[1],
            vrFrame.Input.touchRelative[0], vrFrame.Input.touchRelative[1] );
*/
    // if nothing is focused, send events to the root
	if ( touchPressed )
	{
		VRMenuEvent event( VRMENU_EVENT_TOUCH_DOWN, EVENT_DISPATCH_FOCUS, FocusedHandle, Vector3f( 0.0f ), result );
		events.PushBack( event );
	}
	if ( touchReleased )
	{
		VRMenuEvent event( VRMENU_EVENT_TOUCH_UP, EVENT_DISPATCH_FOCUS, FocusedHandle, Vector3f( vrFrame.Input.touchRelative, 0.0f ), result );
		events.PushBack( event );
	}
    if ( touchDown )
    {
        if ( vrFrame.Input.touchRelative.LengthSq() > Mathf::SmallestNonDenormal )
        {
            VRMenuEvent event( VRMENU_EVENT_TOUCH_RELATIVE, EVENT_DISPATCH_FOCUS, FocusedHandle, Vector3f( vrFrame.Input.touchRelative, 0.0f ), result );
            events.PushBack( event );
        }
        VRMenuEvent event( VRMENU_EVENT_TOUCH_ABSOLUTE, EVENT_DISPATCH_FOCUS, FocusedHandle, Vector3f( vrFrame.Input.touch, 0.0f ), result );
        events.PushBack( event );
    }

    {
        // always post the frame event to the root
        VRMenuEvent event( VRMENU_EVENT_FRAME_UPDATE, EVENT_DISPATCH_BROADCAST, menuHandle_t(), Vector3f( 0.0f ), result );
        events.PushBack( event );
    }
}

//==============================
// VRMenuEventHandler::InitComponents
void VRMenuEventHandler::InitComponents( Array< VRMenuEvent > & events )
{
	VRMenuEvent event( VRMENU_EVENT_INIT, EVENT_DISPATCH_BROADCAST, menuHandle_t(), Vector3f( 0.0f ), HitTestResult() );
	events.PushBack( event );
}

//==============================
// VRMenuEventHandler::Opening
void VRMenuEventHandler::Opening( Array< VRMenuEvent > & events )
{
	LOG( "Opening" );
	// broadcast the opening event
	VRMenuEvent event( VRMENU_EVENT_OPENING, EVENT_DISPATCH_BROADCAST, menuHandle_t(), Vector3f( 0.0f ), HitTestResult() );
	events.PushBack( event );
}

//==============================
// VRMenuEventHandler::Opened
void VRMenuEventHandler::Opened( Array< VRMenuEvent > & events )
{
	LOG( "Opened" );
	// broadcast the opened event
	VRMenuEvent event( VRMENU_EVENT_OPENED, EVENT_DISPATCH_BROADCAST, menuHandle_t(), Vector3f( 0.0f ), HitTestResult() );
	events.PushBack( event );
}

//==============================
// VRMenuEventHandler::Closing
void VRMenuEventHandler::Closing( Array< VRMenuEvent > & events )
{
	LOG( "Closing" );
	// broadcast the closing event
	VRMenuEvent event( VRMENU_EVENT_CLOSING, EVENT_DISPATCH_BROADCAST, menuHandle_t(), Vector3f( 0.0f ), HitTestResult() );
	events.PushBack( event );
}

//==============================
// VRMenuEventHandler::Closed
void VRMenuEventHandler::Closed( Array< VRMenuEvent > & events )
{
	LOG( "Closed" );
	// broadcast the closed event
	VRMenuEvent event( VRMENU_EVENT_CLOSED, EVENT_DISPATCH_BROADCAST, menuHandle_t(), Vector3f( 0.0f ), HitTestResult() );
	events.PushBack( event );

	if ( FocusedHandle.IsValid() )
	{
		VRMenuEvent event( VRMENU_EVENT_FOCUS_LOST, EVENT_DISPATCH_TARGET, FocusedHandle, Vector3f( 0.0f ), HitTestResult() );
		events.PushBack( event );
		FocusedHandle.Release();
		LOG( "Released FocusHandle" );
	}
}

//==============================
// LogEventType
static inline void LogEventType( VRMenuEvent const & event, char const * fmt, ... )
{
#if 1
    if ( event.EventType != VRMENU_EVENT_TOUCH_RELATIVE )
    {
        return;
    }

    char fmtBuff[256];
    va_list args;
    va_start( args, fmt );
    vsnprintf( fmtBuff, sizeof( fmtBuff ), fmt, args );
    va_end( args );

    char buffer[512];
    OVR_sprintf( buffer, sizeof( buffer ), "%s: %s", VRMenuEvent::EventTypeNames[event.EventType], fmtBuff );

    __android_log_write( ANDROID_LOG_WARN, "VrMenu", buffer );
#endif
}

//==============================
// FindTargetPath
static void FindTargetPath( OvrVRMenuMgr const & menuMgr, 
        menuHandle_t const curHandle, Array< menuHandle_t > & targetPath ) 
{
    VRMenuObject * obj = menuMgr.ToObject( curHandle );
    if ( obj != NULL )
    {
        FindTargetPath( menuMgr, obj->GetParentHandle(), targetPath );
        targetPath.PushBack( curHandle );
    }
}

//==============================
// FindTargetPath
static void FindTargetPath( OvrVRMenuMgr const & menuMgr, menuHandle_t const rootHandle, 
        menuHandle_t const curHandle, Array< menuHandle_t > & targetPath ) 
{
    FindTargetPath( menuMgr, curHandle, targetPath );
    if ( targetPath.GetSizeI() == 0 )
    {
        targetPath.PushBack( rootHandle );   // ensure at least root is in the path
    }
}

//==============================
// VRMenuEventHandler::HandleEvents
void VRMenuEventHandler::HandleEvents( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
		menuHandle_t const rootHandle, Array< VRMenuEvent > const & events ) const
{
    VRMenuObject * root = menuMgr.ToObject( rootHandle );
    if ( root == NULL )
    {
        return;
    }

    // find the list of all objects that are in the focused path
    Array< menuHandle_t > focusPath;
    FindTargetPath( menuMgr, rootHandle, FocusedHandle, focusPath );
    
    Array< menuHandle_t > targetPath;

	for ( int i = 0; i < events.GetSizeI(); ++i )
	{
		VRMenuEvent const & event = events[i];
        switch ( event.DispatchType )
        {
            case EVENT_DISPATCH_BROADCAST:
                {
                    // broadcast to everything
		            BroadcastEvent( app, vrFrame, menuMgr, event, root );
                }
                break;
            case EVENT_DISPATCH_FOCUS:
                // send to the focus path only -- this list should be parent -> child order
                DispatchToPath( app, vrFrame, menuMgr, event, focusPath, false );
                break;
            case EVENT_DISPATCH_TARGET:
                if ( targetPath.GetSizeI() == 0 || event.TargetHandle != targetPath.Back() )
                {
                    targetPath.Clear();
                    FindTargetPath( menuMgr, rootHandle, event.TargetHandle, targetPath );
                }
                DispatchToPath( app, vrFrame, menuMgr, event, targetPath, false );
                break;
            default:
                OVR_ASSERT( !"unknown dispatch type" );
                break;
        }
	}
}

//==============================
// VRMenuEventHandler::DispatchToComponents
bool VRMenuEventHandler::DispatchToComponents( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
        VRMenuEvent const & event, VRMenuObject * receiver ) const
{
	DROID_ASSERT( receiver != NULL, "VrMenu" );

	Array< VRMenuComponent* > const & list = receiver->GetComponentList();
	int numComps = list.GetSizeI();
	for ( int i = 0; i < numComps; ++i )
	{
		VRMenuComponent * item = list[i];
		if ( item->HandlesEvent( VRMenuEventFlags_t( event.EventType ) ) )
		{
            LogEventType( event, "DispatchEvent: to '%s'", receiver->GetText().ToCStr() );

			if ( item->OnEvent( app, vrFrame, menuMgr, receiver, event ) == MSG_STATUS_CONSUMED )
            {
                LogEventType( event, "DispatchEvent: receiver '%s', component %i consumed event.", receiver->GetText().ToCStr(), i );
                return true;    // consumed by component
            }
		}
	}
    return false;
}

//==============================
// VRMenuEventHandler::DispatchToPath
bool VRMenuEventHandler::DispatchToPath( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
        VRMenuEvent const & event, Array< menuHandle_t > const & path, bool const log ) const
{
    // send to the focus path only -- this list should be parent -> child order
    for ( int i = 0; i < path.GetSizeI(); ++i )
    {
        VRMenuObject * obj = menuMgr.ToObject( path[i] );
		char const * const indent = "                                                                ";
        // set to 
        if ( obj != NULL && DispatchToComponents( app, vrFrame, menuMgr, event, obj ) )
        {
			if ( log )
			{
				LOG( "%sDispatchToPath: %s, object '%s' consumed event.", &indent[64 - i * 2],
						VRMenuEvent::EventTypeNames[event.EventType], ( obj != NULL ? obj->GetText().ToCStr() : "<null>" ) );
			}	
            return true;    // consumed by a component
        }
		if ( log )
		{
			LOG( "%sDispatchToPath: %s, object '%s' passed event.", &indent[64 - i * 2],
					VRMenuEvent::EventTypeNames[event.EventType], obj != NULL ? obj->GetText().ToCStr() : "<null>" );
		}
    }
    return false;
}

//==============================
// VRMenuEventHandler::BroadcastEvent
bool VRMenuEventHandler::BroadcastEvent( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
        VRMenuEvent const & event, VRMenuObject * receiver ) const
{
	DROID_ASSERT( receiver != NULL, "VrMenu" );

    // allow parent components to handle first
    if ( DispatchToComponents( app, vrFrame, menuMgr, event, receiver ) )
    {
        return true;
    }

    // if the parent did not consume, dispatch to children
    int numChildren = receiver->NumChildren();
    for ( int i = 0; i < numChildren; ++i ) 
    {
        menuHandle_t childHandle = receiver->GetChildHandleForIndex( i );
        VRMenuObject * child = menuMgr.ToObject( childHandle );
        if ( child != NULL && BroadcastEvent( app, vrFrame, menuMgr, event, child ) )
        {
            return true;    // consumed by child
        }
    }
    return false;
}

} // namespace OVR

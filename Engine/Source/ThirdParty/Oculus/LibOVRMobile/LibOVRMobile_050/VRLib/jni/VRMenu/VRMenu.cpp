/************************************************************************************

Filename    :   VRMenu.cpp
Content     :   Class that implements the basic framework for a VR menu, holds all the
				components for a single menu, and updates the VR menu event handler to
				process menu events for a single menu.
Created     :   June 30, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

// includes required for VRMenu.h

#include "VRMenu.h"

#include <android/keycodes.h>
#include "VRMenuMgr.h"
#include "VRMenuEventHandler.h"
#include "../App.h"
#include "GuiSys.h"



namespace OVR {

char const * VRMenu::MenuStateNames[MENUSTATE_MAX] =
{
    "MENUSTATE_OPENING",
	"MENUSTATE_OPEN",
    "MENUSTATE_CLOSING",
	"MENUSTATE_CLOSED"
};

// singleton so that other ids initialized during static initialization can be based off tis
VRMenuId_t VRMenu::GetRootId()
{
	VRMenuId_t ID_ROOT( -1 );
	return ID_ROOT;
}

//==============================
// VRMenu::VRMenu
VRMenu::VRMenu( char const * name ) :
	CurMenuState( MENUSTATE_CLOSED ),
	NextMenuState( MENUSTATE_CLOSED ),
	EventHandler( NULL ),
	Name( name ),
	MenuDistance( 1.45f ),
	IsInitialized( false ),
	ComponentsInitialized( false )
{
	EventHandler = new VRMenuEventHandler;
}

//==============================
// VRMenu::~VRMenu
VRMenu::~VRMenu()
{
	delete EventHandler;
	EventHandler = NULL;
}

//==============================
// VRMenu::Create
VRMenu * VRMenu::Create( char const * menuName )
{
	return new VRMenu( menuName );
}

//==============================
// VRMenu::Init
void VRMenu::Init( OvrVRMenuMgr & menuMgr, BitmapFont const & font, float const menuDistance, VRMenuFlags_t const & flags, Array< VRMenuComponent* > comps /*= Array< VRMenuComponent* >( )*/ )
{
	OVR_ASSERT( !RootHandle.IsValid() );
    OVR_ASSERT( !Name.IsEmpty() );

	Flags = flags;
	MenuDistance = menuDistance;

	// create an empty root item
	VRMenuObjectParms rootParms( VRMENU_CONTAINER, comps,
            VRMenuSurfaceParms( "root" ), "Root", 
			Posef(), Vector3f( 1.0f, 1.0f, 1.0f ), VRMenuFontParms(), 
			GetRootId(), VRMenuObjectFlags_t(), VRMenuObjectInitFlags_t() );
	RootHandle = menuMgr.CreateObject( rootParms );
	VRMenuObject * root = menuMgr.ToObject( RootHandle );
	if ( root == NULL )
	{
		WARN( "RootHandle (%llu) is invalid!", RootHandle.Get() );
		return;
	}

	IsInitialized = true;
	ComponentsInitialized = false;
}

void VRMenu::InitWithItems( OvrVRMenuMgr & menuMgr, BitmapFont const & font, float const menuDistance, VRMenuFlags_t const & flags, Array< VRMenuObjectParms const * > & itemParms )
{
	Init( menuMgr, font, menuDistance, flags );
	AddItems( menuMgr, font, itemParms, GetRootHandle(), true );
}

//==============================================================
// ChildParmsPair
class ChildParmsPair
{
public:
	ChildParmsPair( menuHandle_t const handle, VRMenuObjectParms const * parms ) :
		Handle( handle ),
		Parms( parms )
	{
	}
	ChildParmsPair() : 
		Parms( NULL ) 
	{ 
	}

	menuHandle_t				Handle;
	VRMenuObjectParms const *	Parms;
};

//==============================
// VRMenu::AddItems
void VRMenu::AddItems( OvrVRMenuMgr & menuMgr, BitmapFont const & font, 
        OVR::Array< VRMenuObjectParms const * > & itemParms,
        menuHandle_t parentHandle, bool const recenter ) 
{
	const Vector3f fwd( 0.0f, 0.0f, 1.0f );
	const Vector3f up( 0.0f, 1.0f, 0.0f );
	const Vector3f left( 1.0f, 0.0f, 0.0f );

	// create all items in the itemParms array, add each one to the parent, and position all items
	// without the INIT_FORCE_POSITION flag vertically, one on top of the other
    VRMenuObject * root = menuMgr.ToObject( parentHandle );
    OVR_ASSERT( root != NULL );

	Array< ChildParmsPair > pairs;

	Vector3f nextItemPos( 0.0f );
	int childIndex = 0;
	for ( int i = 0; i < itemParms.GetSizeI(); ++i )
	{
        VRMenuObjectParms const * parms = itemParms[i];
		// assure all ids are different
		for ( int j = 0; j < itemParms.GetSizeI(); ++j )
		{
			if ( j != i && parms->Id != VRMenuId_t() && parms->Id == itemParms[j]->Id )
			{
				LOG( "Duplicate menu object ids for '%s' and '%s'!",
						parms->Text.ToCStr(), itemParms[j]->Text.ToCStr() );
			}
		}
		menuHandle_t handle = menuMgr.CreateObject( *parms );
		if ( handle.IsValid() && root != NULL )
		{
			pairs.PushBack( ChildParmsPair( handle, parms ) );
			root->AddChild( menuMgr, handle );
			VRMenuObject * obj = menuMgr.ToObject( root->GetChildHandleForIndex( childIndex++ ) );
			if ( obj != NULL && ( parms->InitFlags & VRMENUOBJECT_INIT_FORCE_POSITION ) == 0 )
			{
				Bounds3f const & lb = obj->GetLocalBounds( font );
				Vector3f size = lb.GetSize() * obj->GetLocalScale();
				Vector3f centerOfs( left * ( size.x * -0.5f ) );
				if ( !parms->ParentId.IsValid() )	// only contribute to height if not being reparented
				{
					// stack the items
					obj->SetLocalPose( Posef( Quatf(), nextItemPos + centerOfs ) );
					// calculate the total height
					nextItemPos += up * size.y;
				}
				else // otherwise center local to parent
				{
					obj->SetLocalPose( Posef( Quatf(), centerOfs ) );
				}
			}
		}
	}

	// reparent
	Array< menuHandle_t > reparented;
	for ( int i = 0; i < pairs.GetSizeI(); ++i )
	{
		ChildParmsPair const & pair = pairs[i];
		if ( pair.Parms->ParentId.IsValid() )
		{
			menuHandle_t parentHandle = HandleForId( menuMgr, pair.Parms->ParentId );
			VRMenuObject * parent = menuMgr.ToObject( parentHandle );
			if ( parent != NULL )
			{
				root->RemoveChild( menuMgr, pair.Handle );
				parent->AddChild( menuMgr, pair.Handle );
			}
		}
	}

    if ( recenter )
    {
	    // center the menu based on the height of the auto-placed children
	    float offset = nextItemPos.y * 0.5f;
	    Vector3f rootPos = root->GetLocalPosition();
	    rootPos -= offset * up;
	    root->SetLocalPosition( rootPos );
    }
}

//==============================
// VRMenu::Shutdown
void VRMenu::Shutdown( OvrVRMenuMgr & menuMgr )
{
	DROID_ASSERT( IsInitialized, "VrMenu" );

	// this will free the root and all children
	if ( RootHandle.IsValid() )
	{
		menuMgr.FreeObject( RootHandle );
		RootHandle.Release();
	}
}

//==============================
// VRMenu::Frame
void VRMenu::RepositionMenu( App * app, Matrix4f const & viewMatrix )
{
	const Matrix4f invViewMatrix = viewMatrix.Inverted();
	const Vector3f viewPos( GetViewMatrixPosition( viewMatrix ) );
	const Vector3f viewFwd( GetViewMatrixForward( viewMatrix ) );

	if ( Flags & VRMENU_FLAG_TRACK_GAZE )
	{
		MenuPose = CalcMenuPosition( viewMatrix, invViewMatrix, viewPos, viewFwd, MenuDistance );
	}
	else if ( Flags & VRMENU_FLAG_PLACE_ON_HORIZON )
	{
		MenuPose = CalcMenuPositionOnHorizon( viewMatrix, invViewMatrix, viewPos, viewFwd, MenuDistance );
	}
}

//==============================
// VRMenu::Frame
void VRMenu::RepositionMenu( App * app )
{
	RepositionMenu( app, app->GetLastViewMatrix() );
}

//==============================
// VRMenu::Frame
void VRMenu::Frame( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
        BitmapFont const & font, BitmapFontSurface & fontSurface, Matrix4f const & viewMatrix,
        gazeCursorUserId_t const gazeUserId )
{
	const Matrix4f invViewMatrix = viewMatrix.Inverted();
	const Vector3f viewPos( GetViewMatrixPosition( viewMatrix ) );
	const Vector3f viewFwd( GetViewMatrixForward( viewMatrix ) );

	Array< VRMenuEvent > events;

	if ( !ComponentsInitialized )
	{
		EventHandler->InitComponents( events );
		ComponentsInitialized = true;
	}

	if ( NextMenuState != CurMenuState )
	{
		switch( NextMenuState )
		{
            case MENUSTATE_OPENING:
                LOG( "MENUSTATE_OPENING" );
				RepositionMenu( app, viewMatrix );
				EventHandler->Opening( events );
                break;
			case MENUSTATE_OPEN:
				{
					LOG( "MENUSTATE_OPEN" );
					OpenSoundLimiter.PlayMenuSound( app, Name, "sv_release_active", 0.1 );
					EventHandler->Opened( events );
				}
				break;
            case MENUSTATE_CLOSING:
                LOG( "MENUSTATE_CLOSING" );
				EventHandler->Closing( events );
                break;
			case MENUSTATE_CLOSED:
				{
					LOG( "MENUSTATE_CLOSED" );
					CloseSoundLimiter.PlayMenuSound( app, Name, "sv_deselect", 0.1 );
					EventHandler->Closed( events );
				}
				break;
            default:
                OVR_ASSERT( !"Unhandled menu state!" );
                break;
		}
		CurMenuState = NextMenuState;
	}

    switch( CurMenuState )
    {
        case MENUSTATE_OPENING:
			if ( IsFinishedOpening() )
			{
				NextMenuState = MENUSTATE_OPEN;
			}
            break;
        case MENUSTATE_OPEN:
            break;
        case MENUSTATE_CLOSING:
			if ( IsFinishedClosing() )
			{
				NextMenuState = MENUSTATE_CLOSED;
			}
            break;
        case MENUSTATE_CLOSED:
	        // handle remaining events -- not focus path is empty right now, but this may still broadcast messages to controls
		    EventHandler->HandleEvents( app, vrFrame, menuMgr, RootHandle, events );	
		    return;
        default:
            OVR_ASSERT( !"Unhandled menu state!" );
            break;
	}

	if ( Flags & VRMENU_FLAG_TRACK_GAZE )
	{
		MenuPose = CalcMenuPosition( viewMatrix, invViewMatrix, viewPos, viewFwd, MenuDistance );
	}
	else if ( Flags & VRMENU_FLAG_TRACK_GAZE_HORIZONTAL )
	{
		MenuPose = CalcMenuPositionOnHorizon( viewMatrix, invViewMatrix, viewPos, viewFwd, MenuDistance );
	}

    Frame_Impl( app, vrFrame, menuMgr, font, fontSurface, gazeUserId );

	EventHandler->Frame( app, vrFrame, menuMgr, font, RootHandle, MenuPose, gazeUserId, events );

	EventHandler->HandleEvents( app, vrFrame, menuMgr, RootHandle, events );

	VRMenuObject * root = menuMgr.ToObject( RootHandle );		
	if ( root != NULL )
	{
		VRMenuRenderFlags_t renderFlags;
		menuMgr.SubmitForRendering( app->GetDebugLines(), font, fontSurface, RootHandle, MenuPose, renderFlags );
	}

}

//==============================
// VRMenu::OnKeyEvent
bool VRMenu::OnKeyEvent( App * app, int const keyCode, KeyState::eKeyEventType const eventType )
{
    if ( OnKeyEvent_Impl( app, keyCode, eventType ) )
    {
        // consumed by sub class
        return true;
    }

	if ( keyCode == AKEYCODE_BACK )
	{
		LOG( "VRMenu '%s' Back key event: %s", GetName(), KeyState::EventNames[eventType] );

		switch( eventType )
		{
			case KeyState::KEY_EVENT_LONG_PRESS:
				return false;
			case KeyState::KEY_EVENT_DOWN:
				return false;
			case KeyState::KEY_EVENT_SHORT_PRESS:
				if ( IsOpenOrOpening() )
				{
					if ( Flags & VRMENU_FLAG_BACK_KEY_EXITS_APP )
					{
						app->StartSystemActivity( PUI_CONFIRM_QUIT );
						return true;
					}
					else if ( Flags & VRMENU_FLAG_SHORT_PRESS_HANDLED_BY_APP )
					{
						return false;
					}
					else if ( !( Flags & VRMENU_FLAG_BACK_KEY_DOESNT_EXIT ) )
					{
						Close( app, app->GetGazeCursor() );
						return true;
					}
				}
				return false;
			case KeyState::KEY_EVENT_DOUBLE_TAP:
			default:
				return false;
		}
	}
	return false;
}

//==============================
// VRMenu::Open
void VRMenu::Open( App * app, OvrGazeCursor & gazeCursor, bool const instant )
{
	if ( CurMenuState == MENUSTATE_CLOSED || CurMenuState == MENUSTATE_CLOSING )
	{
        Open_Impl( app, gazeCursor );
		NextMenuState = instant ? MENUSTATE_OPEN : MENUSTATE_OPENING;
	}
}

//==============================
// VRMenu::Close
void VRMenu::Close( App * app, OvrGazeCursor & gazeCursor, bool const instant )
{
	if ( CurMenuState == MENUSTATE_OPEN || CurMenuState == MENUSTATE_OPENING )
	{
        Close_Impl( app, gazeCursor );
		NextMenuState = instant ? MENUSTATE_CLOSED : MENUSTATE_CLOSING;
	}
}

//==============================
// VRMenu::HandleForId
menuHandle_t VRMenu::HandleForId( OvrVRMenuMgr & menuMgr, VRMenuId_t const id ) const
{
	VRMenuObject * root = menuMgr.ToObject( RootHandle );
	return root->ChildHandleForId( menuMgr, id );
}

//==============================
// VRMenu::CalcMenuPosition
Posef VRMenu::CalcMenuPosition( Matrix4f const & viewMatrix, Matrix4f const & invViewMatrix,
		Vector3f const & viewPos, Vector3f const & viewFwd, float const menuDistance )
{
	// spawn directly in front 
	Quatf rotation( -viewFwd, 0.0f );
	Quatf viewRot( invViewMatrix );
	Quatf fullRotation = rotation * viewRot;

	Vector3f position( viewPos + viewFwd * menuDistance );

	return Posef( fullRotation, position );
}

//==============================
// VRMenu::CalcMenuPositionOnHorizon
Posef VRMenu::CalcMenuPositionOnHorizon( Matrix4f const & viewMatrix, Matrix4f const & invViewMatrix,
		Vector3f const & viewPos, Vector3f const & viewFwd, float const menuDistance )
{
	// project the forward view onto the horizontal plane
	Vector3f const up( 0.0f, 1.0f, 0.0f );
	float dot = viewFwd.Dot( up );
	Vector3f horizontalFwd = ( dot < -0.99999f || dot > 0.99999f ) ? Vector3f( 1.0f, 0.0f, 0.0f ) : viewFwd - ( up * dot );
	horizontalFwd.Normalize();

	Matrix4f horizontalViewMatrix = Matrix4f::LookAtRH( Vector3f( 0 ), horizontalFwd, up );
	horizontalViewMatrix.Transpose();	// transpose because we want the rotation opposite of where we're looking

	// this was only here to test rotation about the local axis
	//Quatf rotation( -horizontalFwd, 0.0f );

	Quatf viewRot( horizontalViewMatrix );
	Quatf fullRotation = /*rotation * */viewRot;

	Vector3f position( viewPos + horizontalFwd * menuDistance );

	return Posef( fullRotation, position );
}

//==============================
// VRMenu::OnItemEvent
void VRMenu::OnItemEvent( App * app, VRMenuId_t const itemId, VRMenuEvent const & event )
{
	OnItemEvent_Impl( app, itemId, event );
}

//==============================
// VRMenu::Init_Impl
bool VRMenu::Init_Impl( OvrVRMenuMgr & menuMgr, BitmapFont const & font, float const menuDistance, 
                VRMenuFlags_t const & flags, Array< VRMenuObjectParms const * > & itemParms )
{
    return true;
}

//==============================
// VRMenu::Shutdown_Impl
void VRMenu::Shutdown_Impl( OvrVRMenuMgr & menuMgr )
{
}

//==============================
// VRMenu::Frame_Impl
void VRMenu::Frame_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, BitmapFont const & font,
        BitmapFontSurface & fontSurface, gazeCursorUserId_t const gazeUserId )
{
}

//==============================
// VRMenu::OnKeyEvent_Impl
bool VRMenu::OnKeyEvent_Impl( App * app, int const keyCode, KeyState::eKeyEventType const eventType )
{
    return false;
}

//==============================
// VRMenu::Open_Impl
void VRMenu::Open_Impl( App * app, OvrGazeCursor & gazeCursor )
{
}

//==============================
// VRMenu::Close_Impl
void VRMenu::Close_Impl( App * app, OvrGazeCursor & gazeCursor )
{
}

//==============================
// VRMenu::OnItemEvent_Impl
void VRMenu::OnItemEvent_Impl( App * app, VRMenuId_t const itemId, VRMenuEvent const & event )
{
}

//==============================
// VRMenu::GetFocusedHandle()
menuHandle_t VRMenu::GetFocusedHandle() const
{
	if ( EventHandler != NULL )
	{
		return EventHandler->GetFocusedHandle(); 
	}
	return menuHandle_t();
}

void VRMenu::ResetMenuOrientation( App * app )
{
	MenuPose.Orientation = Quatf();
}

} // namespace OVR


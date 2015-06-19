/************************************************************************************

Filename    :   OvrGuiSys.cpp
Content     :   Manager for native GUIs.
Created     :   June 6, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "GuiSysLocal.h"

#include <android/keycodes.h>
#include "Android/GlUtils.h"
#include "../GlProgram.h"
#include "../GlTexture.h"
#include "../GlGeometry.h"
#include "../VrCommon.h"
#include "../App.h"
#include "../GazeCursor.h"
#include "VRMenuMgr.h"
#include "VRMenuComponent.h"
#include "SoundLimiter.h"
#include "VRMenuEventHandler.h"
#include "FolderBrowser.h"
#include "Input.h"
#include "DefaultComponent.h"



namespace OVR {

Vector4f const OvrGuiSys::BUTTON_DEFAULT_TEXT_COLOR( 0.098f, 0.6f, 0.96f, 1.0f );
Vector4f const OvrGuiSys::BUTTON_HILIGHT_TEXT_COLOR( 1.0f );

//==============================
// OvrGuiSysLocal::
OvrGuiSysLocal::OvrGuiSysLocal() :
	IsInitialized( false )
{
}

//==============================
// OvrGuiSysLocal::
OvrGuiSysLocal::~OvrGuiSysLocal()
{
	OVR_ASSERT( IsInitialized == false ); // Shutdown should already have been called
}

//==============================
// OvrGuiSysLocal::Init
// FIXME: the default items only apply to AppMenu now, and should be moved there once AppMenu overloads VRMenu.
void OvrGuiSysLocal::Init( App * app, OvrVRMenuMgr & menuMgr, BitmapFont const & font )
{
	LOG( "OvrGuiSysLocal::Init" );

	// get a use id for the gaze cursor
	GazeUserId = app->GetGazeCursor().GenerateUserId();

	IsInitialized = true;
}

//==============================
// OvrGuiSysLocal::RepositionMenus
// Reposition any open menus 
void OvrGuiSysLocal::ResetMenuOrientations( App * app )
{
	for ( int i = 0; i < Menus.GetSizeI(); ++i )
	{
		if ( VRMenu* menu = Menus.At( i ) )
		{
			LOG( "ResetMenuOrientation -> '%s'", menu->GetName( ) );
			menu->ResetMenuOrientation( app );
		}
	}
}

//==============================
// OvrGuiSysLocal::AddMenu
void OvrGuiSysLocal::AddMenu( VRMenu * menu )
{
	int menuIndex = FindMenuIndex( menu->GetName() );
	if ( menuIndex >= 0 )
	{
		WARN( "Duplicate menu name '%s'", menu->GetName() );
		OVR_ASSERT( menuIndex < 0 );
	}
	Menus.PushBack( menu );
}

//==============================
// OvrGuiSysLocal::GetMenu
VRMenu * OvrGuiSysLocal::GetMenu( char const * menuName ) const
{
	int menuIndex = FindMenuIndex( menuName );
	if ( menuIndex >= 0 )
	{
		return Menus[menuIndex];
	}
	return NULL;
}

//==============================
// OvrGuiSysLocal::DestroyMenu
void OvrGuiSysLocal::DestroyMenu( OvrVRMenuMgr & menuMgr, VRMenu * menu )
{
	OVR_ASSERT( menu != NULL );

	MakeInactive( menu );

	menu->Shutdown( menuMgr );
	delete menu;

	int idx = FindMenuIndex( menu );
	if ( idx >= 0 )
	{
		Menus.RemoveAt( idx );
	}
}

//==============================
// OvrGuiSysLocal::Shutdown
void OvrGuiSysLocal::Shutdown( OvrVRMenuMgr & menuMgr )
{
	// pointers in this list will always be in Menus list, too, so just clear it
	ActiveMenus.Clear();

	// FIXME: we need to make sure we delete any child menus here -- it's not enough to just delete them
	// in the destructor of the parent, because they'll be left in the menu list since the destructor has
	// no way to call GuiSys->DestroyMenu() for them.
	for ( int i = 0; i < Menus.GetSizeI(); ++i )
	{
		VRMenu * menu = Menus[i];
		menu->Shutdown( menuMgr );
		delete menu;
		Menus[i] = NULL;
	}
	Menus.Clear();

	IsInitialized = false;
}

//==============================
// OvrGuiSysLocal::FindMenuIndex
int OvrGuiSysLocal::FindMenuIndex( char const * menuName ) const
{
	for ( int i = 0; i < Menus.GetSizeI(); ++i )
	{
		if ( OVR_stricmp( Menus[i]->GetName(), menuName ) == 0 )
		{
			return i;
		}
	}
	return -1;
}

//==============================
// OvrGuiSysLocal::FindMenuIndex
int OvrGuiSysLocal::FindMenuIndex( VRMenu const * menu ) const
{
	for ( int i = 0; i < Menus.GetSizeI(); ++i )
	{
		if ( Menus[i] == menu ) 
		{
			return i;
		}
	}
	return -1;
}

//==============================
// OvrGuiSysLocal::FindActiveMenuIndex
int OvrGuiSysLocal::FindActiveMenuIndex( VRMenu const * menu ) const
{
	for ( int i = 0; i < ActiveMenus.GetSizeI(); ++i )
	{
		if ( ActiveMenus[i] == menu ) 
		{
			return i;
		}
	}
	return -1;
}

//==============================
// OvrGuiSysLocal::FindActiveMenuIndex
int OvrGuiSysLocal::FindActiveMenuIndex( char const * menuName ) const
{
	for ( int i = 0; i < ActiveMenus.GetSizeI(); ++i )
	{
		if ( OVR_stricmp( ActiveMenus[i]->GetName(), menuName ) == 0 )
		{
			return i;
		}
	}
	return -1;
}

//==============================
// OvrGuiSysLocal::MakeActive
void OvrGuiSysLocal::MakeActive( VRMenu * menu )
{
	int idx = FindActiveMenuIndex( menu );
	if ( idx < 0 )
	{
		ActiveMenus.PushBack( menu );
	}
}

//==============================
// OvrGuiSysLocal::MakeInactive
void OvrGuiSysLocal::MakeInactive( VRMenu * menu )
{
	int idx = FindActiveMenuIndex( menu );
	if ( idx >= 0 )
	{
		ActiveMenus.RemoveAtUnordered( idx );
	}
}

//==============================
// OvrGuiSysLocal::OpenMenu
void OvrGuiSysLocal::OpenMenu( App * app, OvrGazeCursor & gazeCursor, char const * menuName )
{
	int menuIndex = FindMenuIndex( menuName );
	if ( menuIndex < 0 )
	{
		WARN( "No menu named '%s'", menuName );
		OVR_ASSERT( menuIndex >= 0 && menuIndex < Menus.GetSizeI() );
		return;
	}
	VRMenu * menu = Menus[menuIndex];
	OVR_ASSERT( menu != NULL );
	if ( !menu->IsOpenOrOpening() )
	{
		menu->Open( app, gazeCursor );
		MakeActive( menu );
	}
}

//==============================
// OvrGuiSysLocal::CloseMenu
void OvrGuiSysLocal::CloseMenu( App * app, char const * menuName, bool const closeInstantly ) 
{
	int menuIndex = FindMenuIndex( menuName );
	if ( menuIndex < 0 )
	{
		WARN( "No menu named '%s'", menuName );
		OVR_ASSERT( menuIndex >= 0 && menuIndex < Menus.GetSizeI() );
		return;
	}
	VRMenu * menu = Menus[menuIndex];
	CloseMenu( app, menu, closeInstantly );
}

//==============================
// OvrGuiSysLocal::CloseMenu
void OvrGuiSysLocal::CloseMenu( App * app, VRMenu * menu, bool const closeInstantly )
{
	OVR_ASSERT( menu != NULL );
	if ( !menu->IsClosedOrClosing() )
	{
		menu->Close( app, app->GetGazeCursor(), closeInstantly );
	}
}

//==============================
// OvrGuiSysLocal::IsMenuActive
bool OvrGuiSysLocal::IsMenuActive( char const * menuName ) const
{
	int idx = FindActiveMenuIndex( menuName );
	return idx >= 0;
}

//==============================
// OvrGuiSysLocal::IsAnyMenuOpen
bool OvrGuiSysLocal::IsAnyMenuActive() const 
{
	return ActiveMenus.GetSizeI() > 0;
}

//==============================
// OvrGuiSysLocal::IsAnyMenuOpen
bool OvrGuiSysLocal::IsAnyMenuOpen() const
{
	for ( int i = 0; i < ActiveMenus.GetSizeI(); ++i )
	{
		if ( ActiveMenus[i]->IsOpenOrOpening() )
		{
			return true;
		}
	}
	return false;
}

//==============================
// OvrGuiSysLocal::Frame
void OvrGuiSysLocal::Frame( App * app, const VrFrame & vrFrame, OvrVRMenuMgr & menuMgr, BitmapFont const & font,
        BitmapFontSurface & fontSurface, Matrix4f const & viewMatrix )
{
	//LOG( "OvrGuiSysLocal::Frame" );

	// go backwards through the list so we can use unordered remove when a menu finishes closing
	for ( int i = ActiveMenus.GetSizeI() - 1; i >= 0; --i )
	{
		VRMenu * curMenu = ActiveMenus[i];
		OVR_ASSERT( curMenu != NULL );

		// VRMenu::Frame() CPU performance test
		if ( 0 )
		{
			//SetCurrentThreadAffinityMask( 0xF0 );
			double start = LogCpuTime::GetNanoSeconds();
			for ( int i = 0; i < 20; i++ )
			{
				menuMgr.BeginFrame();
				curMenu->Frame( app, vrFrame, menuMgr, font, fontSurface, viewMatrix, GazeUserId );
			}
			double end = LogCpuTime::GetNanoSeconds();
			LOG( "20x VRMenu::Frame() = %4.2f ms", ( end - start ) * ( 1.0 / ( 1000.0 * 1000.0 ) ) );
		}

		curMenu->Frame( app, vrFrame, menuMgr, font, fontSurface, viewMatrix, GazeUserId );

		if ( curMenu->GetCurMenuState() == VRMenu::MENUSTATE_CLOSED )
		{
			// remove from the active list
			ActiveMenus.RemoveAtUnordered( i );
			continue;
		}
	}
}

//==============================
// OvrGuiSysLocal::OnKeyEvent
bool OvrGuiSysLocal::OnKeyEvent( App * app, int const keyCode, KeyState::eKeyEventType const eventType ) 
{
	for ( int i = 0; i < ActiveMenus.GetSizeI(); ++i )
	{
		VRMenu * curMenu = ActiveMenus[i];
		OVR_ASSERT( curMenu != NULL );

		if ( keyCode == AKEYCODE_BACK ) 
		{
			LOG( "OvrGuiSysLocal back key event '%s' for menu '%s'", KeyState::EventNames[eventType], curMenu->GetName() );
		}

		if ( curMenu->OnKeyEvent( app, keyCode, eventType ) )
		{
			LOG( "VRMenu '%s' consumed key event", curMenu->GetName() );
			return true;
		}
	}
	// we ignore other keys in the app menu for now
	return false;
}

} // namespace OVR

/************************************************************************************

Filename    :   VolumePopup.cpp
Content     :   The main menu that appears in native apps when pressing the HMT button.
Created     :   July 25, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "VolumePopup.h"

#include <android/keycodes.h>
#include "App.h"
#include "VRMenuComponent.h"
#include "GuiSys.h"
#include "VRMenuMgr.h"
#include "DefaultComponent.h"
#include "BitmapFont.h"
#include "TextFade_Component.h"
#include "PackageFiles.h"
#include "Kernel/OVR_String_Utils.h"
#include "Kernel/OVR_Alg.h"

namespace OVR {

const int OvrVolumePopup::NumVolumeTics = 15;

VRMenuId_t OvrVolumePopup::ID_BACKGROUND( VRMenu::GetRootId().Get() + 1000 );
VRMenuId_t OvrVolumePopup::ID_VOLUME_ICON( VRMenu::GetRootId().Get() + 1001 );
VRMenuId_t OvrVolumePopup::ID_VOLUME_TEXT( VRMenu::GetRootId().Get() + 1002 );
VRMenuId_t OvrVolumePopup::ID_VOLUME_TICKS( VRMenu::GetRootId().Get() + 1003 );

const double OvrVolumePopup::VolumeMenuFadeDelay = 3;

const char * OvrVolumePopup::MENU_NAME = "Volume";

//==============================
// OvrVolumePopup::OvrVolumePopup
OvrVolumePopup::OvrVolumePopup() :
    VRMenu( MENU_NAME ),
	VolumeTextOffset(),
	CurrentVolume( -1 )

{
}

//==============================
// OvrVolumePopup::~OvrVolumePopup
OvrVolumePopup::~OvrVolumePopup()
{
}

//==============================
// OvrVolumePopup::Create
OvrVolumePopup * OvrVolumePopup::Create( App * app, OvrVRMenuMgr & menuMgr, BitmapFont const & font )
{
    OvrVolumePopup * menu = new OvrVolumePopup;

	Array< VRMenuObjectParms > defaultAppMenuItems;

	{
		Vector3f fwd( 0.0f, 0.0f, 1.0f );
		Vector3f up( 0.0f, 1.0f, 0.0f );
		Vector3f right( fwd.Cross( up ) * -1.0f );

		VRMenuFontParms fontParms( HORIZONTAL_LEFT, VERTICAL_CENTER, false, false, false, 0.5f );

		Vector3f menuOffset( 0.0f, 64 * VRMenuObject::DEFAULT_TEXEL_SCALE, 0.0f );

	    int backgroundWidth = 0;
	    int backgroundHeight = 0;
		GLuint backgroundTexture = LoadTextureFromApplicationPackage( "res/raw/volume_bg.png",
			TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), backgroundWidth, backgroundHeight );

	    int volumeIconWidth = 0;
	    int volumeIconHeight = 0;
		GLuint volumeIconTexture = LoadTextureFromApplicationPackage( "res/raw/volume_icon.png",
			TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), volumeIconWidth, volumeIconHeight );

	    int volumeTickOffWidth = 0;
	    int volumeTickOffHeight = 0;
		GLuint volumeTickOffTexture = LoadTextureFromApplicationPackage( "res/raw/volume_tick_off.png",
			TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), volumeTickOffWidth, volumeTickOffHeight );

	    int volumeTickOnWidth = 0;
	    int volumeTickOnHeight = 0;
		GLuint volumeTickOnTexture = LoadTextureFromApplicationPackage( "res/raw/volume_tick_on.png",
			TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), volumeTickOnWidth, volumeTickOnHeight );

		int volumeTickPadding = 4;
		int volumeTickWidth = 6 + volumeTickPadding;
		int volumeTotalWidth = volumeTickWidth * ( NumVolumeTics - 1 );

		{
			// transparent black background
			Posef backgroundPose( Quatf(), menuOffset + Vector3f( 0.0f, 0.0f, -0.02f ) );
			VRMenuSurfaceParms backgroundSurfaceParms( "background",
												 backgroundTexture, backgroundWidth, backgroundHeight, SURFACE_TEXTURE_DIFFUSE,
												 0, 0, 0, SURFACE_TEXTURE_MAX,
												 0, 0, 0, SURFACE_TEXTURE_MAX );

			VRMenuObjectParms backgroundParms( VRMENU_BUTTON, Array< VRMenuComponent* >(), backgroundSurfaceParms, NULL,
										 backgroundPose, Vector3f( 1.0f ), fontParms,
										 OvrVolumePopup::ID_BACKGROUND, VRMenuObjectFlags_t( VRMENUOBJECT_BOUND_ALL ) | VRMENUOBJECT_DONT_HIT_TEXT,
										 VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
			defaultAppMenuItems.PushBack( backgroundParms );

			// speaker icon
			VRMenuSurfaceParms speakerIconSurfaceParms( "speakerIcon",
												volumeIconTexture, volumeIconWidth, volumeIconHeight, SURFACE_TEXTURE_DIFFUSE,
												 0, 0, 0, SURFACE_TEXTURE_MAX,
												 0, 0, 0, SURFACE_TEXTURE_MAX );

			float speakerIconX = ( backgroundWidth * -0.5f + volumeIconWidth * 0.5f ) * VRMenuObject::DEFAULT_TEXEL_SCALE;
			Vector3f speakerIconOffset = menuOffset + right * speakerIconX;
			Posef speakerIconPose( Quatf(), speakerIconOffset );
			VRMenuObjectParms speakerIconParms( VRMENU_BUTTON, Array< VRMenuComponent* >(), speakerIconSurfaceParms, NULL,
										 speakerIconPose, Vector3f( 1.0f ), fontParms,
										 OvrVolumePopup::ID_VOLUME_ICON, VRMenuObjectFlags_t( VRMENUOBJECT_BOUND_ALL ) | VRMENUOBJECT_DONT_HIT_TEXT,
										 VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
			defaultAppMenuItems.PushBack( speakerIconParms );

			// volume ticks
			VRMenuSurfaceParms volumeTickSurfaceParms( "volumeTick",
												volumeTickOffTexture, volumeTickOffWidth, volumeTickOffHeight, SURFACE_TEXTURE_DIFFUSE,
												volumeTickOnTexture, volumeTickOnWidth, volumeTickOnHeight, SURFACE_TEXTURE_ADDITIVE,
												 0, 0, 0, SURFACE_TEXTURE_MAX );

			for( int i = 0; i < NumVolumeTics; i++ )
			{
				Vector3f volumeTickOffset = menuOffset + right * ( volumeTotalWidth * -0.5f + i * volumeTickWidth ) * VRMenuObject::DEFAULT_TEXEL_SCALE + Vector3f( 0.0f, 0.0f, 0.0025f * ( float )i );
				
				Posef volumeTickPose( Quatf(), volumeTickOffset );

				VRMenuObjectParms volumeTickParms( VRMENU_BUTTON, Array< VRMenuComponent* >(), volumeTickSurfaceParms, NULL,
											 volumeTickPose, Vector3f( 1.0f ), fontParms,
											 VRMenuId_t( OvrVolumePopup::ID_VOLUME_TICKS.Get() + i ), VRMenuObjectFlags_t( VRMENUOBJECT_BOUND_ALL ) | VRMENUOBJECT_DONT_HIT_TEXT,
											 VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

				defaultAppMenuItems.PushBack( volumeTickParms );
			}

			// volume text
			float volumeTextWidth = volumeIconWidth;
			menu->VolumeTextOffset = menuOffset + right * ( backgroundWidth * 0.5f - volumeTextWidth * 0.5f ) * VRMenuObject::DEFAULT_TEXEL_SCALE + Vector3f( 0.0f, 0.5f * VRMenuObject::DEFAULT_TEXEL_SCALE, 0.02f );
			Posef volumeTextPose( Quatf(), menu->VolumeTextOffset );
			VRMenuObjectParms volumeTextParms( VRMENU_BUTTON, Array< VRMenuComponent* >(), VRMenuSurfaceParms(), "0",
										 volumeTextPose, Vector3f( 1.0f ), fontParms,
										 OvrVolumePopup::ID_VOLUME_TEXT, VRMenuObjectFlags_t( VRMENUOBJECT_BOUND_ALL ) | VRMENUOBJECT_DONT_HIT_TEXT,
										 VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

			defaultAppMenuItems.PushBack( volumeTextParms );
		}
	}

	Array< VRMenuObjectParms const * > parms;

	// add all of the default items
	for ( int i = 0; i < defaultAppMenuItems.GetSizeI(); ++i )
	{
		VRMenuObjectParms * defaultParms = new VRMenuObjectParms( defaultAppMenuItems[i] );
		parms.PushBack( defaultParms );
	}

	menu->InitWithItems( menuMgr, font, 1.8f, VRMenuFlags_t( VRMENU_FLAG_TRACK_GAZE ) | VRMENU_FLAG_SHORT_PRESS_HANDLED_BY_APP, parms );
	app->GetGuiSys().AddMenu( menu );

    DeletePointerArray( parms );

    return menu;
}

//==============================
// OvrVolumePopup::Frame_Impl
void OvrVolumePopup::Frame_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, BitmapFont const & font,
        BitmapFontSurface & fontSurface, gazeCursorUserId_t const gazeUserId )
{
    if ( GetCurMenuState() == VRMenu::MENUSTATE_CLOSED )
    {
        return;
    }

    const double timeSinceLastVolumeChange = ovr_GetTimeSinceLastVolumeChange();
	if ( ( timeSinceLastVolumeChange < 0 ) || ( timeSinceLastVolumeChange > VolumeMenuFadeDelay ) )
	{
		menuHandle_t volumeTextHandle = HandleForId( app->GetVRMenuMgr(), OvrVolumePopup::ID_VOLUME_TEXT );
		VRMenuObject *volumeText = app->GetVRMenuMgr().ToObject( volumeTextHandle );
		volumeText->AddFlags( VRMENUOBJECT_DONT_RENDER );
		app->GetGuiSys().CloseMenu( app, this, false );
	}
}

//==============================
// OvrVolumePopup::
bool OvrVolumePopup::OnKeyEvent_Impl( App * app, int const keyCode, KeyState::eKeyEventType const eventType )
{
	return false;
}

//==============================
// OvrVolumePopup::ShowVolume
void OvrVolumePopup::ShowVolume( App * app, const int current )
{
	if ( CurrentVolume != current )
	{
		// highlight the volume ticks
		for( int i = 0; i < NumVolumeTics; i++ )
		{
			menuHandle_t volumeTickHandle = HandleForId( app->GetVRMenuMgr(), VRMenuId_t( OvrVolumePopup::ID_VOLUME_TICKS.Get() + i ) );
			VRMenuObject *volumeTick = app->GetVRMenuMgr().ToObject( volumeTickHandle );
			volumeTick->SetHilighted( i < current );
		}

		// update volume text
		menuHandle_t volumeTextHandle = HandleForId( app->GetVRMenuMgr(), OvrVolumePopup::ID_VOLUME_TEXT );
		VRMenuObject *volumeText = app->GetVRMenuMgr().ToObject( volumeTextHandle );
		volumeText->SetText( StringUtils::Va( "%d", current ) );

		// center the text
		Bounds3f bnds = volumeText->GetTextLocalBounds( app->GetDefaultFont() );
		volumeText->SetLocalPosition( VolumeTextOffset - Vector3f( bnds.GetSize().x * 0.5f, 0.0f, 0.0f ) );

		CurrentVolume = current;
	}

	if ( GetCurMenuState() == VRMenu::MENUSTATE_CLOSED )
    {
		menuHandle_t volumeTextHandle = HandleForId( app->GetVRMenuMgr(), OvrVolumePopup::ID_VOLUME_TEXT );
		VRMenuObject *volumeText = app->GetVRMenuMgr().ToObject( volumeTextHandle );
		volumeText->RemoveFlags( VRMENUOBJECT_DONT_RENDER );
    	app->GetGuiSys().OpenMenu( app, app->GetGazeCursor(), "Volume" );
    }
}

//==============================
// OvrVolumePopup::CheckForVolumeChange
void OvrVolumePopup::CheckForVolumeChange( App * app )
{
	// ovr_GetTimeSinceLastVolumeChange() will return -1 if the volume listener hasn't initialized yet,
	// which sometimes takes place after a frame has run in Unity.
	double timeSinceLastVolumeChange = ovr_GetTimeSinceLastVolumeChange();
	if ( ( timeSinceLastVolumeChange != -1 ) && ( timeSinceLastVolumeChange < VolumeMenuFadeDelay ) )
	{
		ShowVolume( app, ovr_GetVolume() );
	}
}

//==============================
// OvrVolumePopup::Close
void OvrVolumePopup::Close( App * app )
{
	menuHandle_t volumeTextHandle = HandleForId( app->GetVRMenuMgr(), OvrVolumePopup::ID_VOLUME_TEXT );
	VRMenuObject *volumeText = app->GetVRMenuMgr().ToObject( volumeTextHandle );
	volumeText->AddFlags( VRMENUOBJECT_DONT_RENDER );
	app->GetGuiSys().CloseMenu( app, this, false );
}

} // namespace OVR

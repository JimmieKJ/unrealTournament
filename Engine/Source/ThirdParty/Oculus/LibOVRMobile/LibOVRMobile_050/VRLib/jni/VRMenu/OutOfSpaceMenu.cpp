/************************************************************************************

Filename    :   OutOfSpaceMenu.cpp
Content     :
Created     :   Feb 18, 2015
Authors     :   Madhu Kalva

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "OutOfSpaceMenu.h"
#include "App.h"
#include "BitmapFont.h"
#include "VrLocale.h"
#include "VRMenu/VRMenuMgr.h"

namespace OVR {

	char const * OvrOutOfSpaceMenu::MENU_NAME 	= "OvrOutOfSpaceMenu";

	// Tweaks
	const float CENTER_TO_ICON_Y_OFFSET = 0.2f;
	const float CENTER_TO_TEXT_Y_OFFSET = -0.2f;

	OvrOutOfSpaceMenu::OvrOutOfSpaceMenu( App * app )
		: VRMenu( MENU_NAME )
		, AppPtr( app )
	{
		// Init with empty root
		Init( AppPtr->GetVRMenuMgr(), AppPtr->GetDefaultFont(), 0.0f, VRMenuFlags_t(), Array< VRMenuComponent* >() );
	}

	OvrOutOfSpaceMenu * OvrOutOfSpaceMenu::Create( App * app )
	{
		return new OvrOutOfSpaceMenu( app );
	}

	void OvrOutOfSpaceMenu::BuildMenu( int memoryInKB )
	{
		const VRMenuFontParms fontParms( true, true, false, false, true, 0.505f, 0.43f, 1.0f );
		Array< VRMenuObjectParms const * > parms;
		int menuId = 9000;

		// ---
		// Icon
		{
			VRMenuSurfaceParms iconSurfParms( "",
					"res/raw/out_of_disk_space_warning.png", SURFACE_TEXTURE_DIFFUSE,
					"", SURFACE_TEXTURE_MAX,
					"", SURFACE_TEXTURE_MAX );
			VRMenuObjectParms iconParms(
					VRMENU_STATIC, Array< VRMenuComponent* >(), iconSurfParms, "",
					Posef( Quatf(), Vector3f( 0.0f, CENTER_TO_ICON_Y_OFFSET, 0.0f ) ),
					Vector3f( 1.0f ), fontParms, VRMenuId_t( ++menuId ),
					VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ),
					VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
			parms.PushBack( &iconParms );
			AddItems( AppPtr->GetVRMenuMgr(), AppPtr->GetDefaultFont(), parms, GetRootHandle(), true );
			parms.Clear();
		}

		// ---
		// Message
		{
			String outOfSpaceMsg;
			VrLocale::GetString( AppPtr->GetVrJni(), AppPtr->GetJavaObject(),
					"@string/out_of_memory",
					"To use this app, please free up at least %dKB of storage space on your phone.",
					outOfSpaceMsg );

			char charBuff[10];
			sprintf( charBuff, "%d", memoryInKB );
			outOfSpaceMsg = VrLocale::GetXliffFormattedString( outOfSpaceMsg, charBuff );

			BitmapFont & font = AppPtr->GetDefaultFont();
			font.WordWrapText( outOfSpaceMsg, 1.4f );

			VRMenuObjectParms titleParms(
				VRMENU_STATIC,
				Array< VRMenuComponent* >(),
				VRMenuSurfaceParms(),
				(const char*)outOfSpaceMsg,
				Posef( Quatf(), Vector3f( 0.0f, CENTER_TO_TEXT_Y_OFFSET, 0.0f ) ),
				Vector3f( 1.0f ),
				fontParms,
				VRMenuId_t( ++menuId ),
				VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_TEXT ),
				VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
			parms.PushBack( &titleParms );
			AddItems( AppPtr->GetVRMenuMgr(), AppPtr->GetDefaultFont(), parms, GetRootHandle(), true );
			parms.Clear();
		}

		this->SetMenuPose( Posef( Quatf(), Vector3f( 0.0f, 0.0f, -3.0f ) ) );
	}
}

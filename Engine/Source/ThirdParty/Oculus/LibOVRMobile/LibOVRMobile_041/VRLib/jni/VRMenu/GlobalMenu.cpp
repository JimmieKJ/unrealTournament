/************************************************************************************

Filename    :   GlobalMenu.cpp
Content     :   The main menu that appears in native apps when pressing the HMT button.
Created     :   July 25, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "GlobalMenu.h"

#include <android/keycodes.h>
#include "App.h"
#include "VRMenuComponent.h"
#include "GuiSys.h"
#include "VRMenuMgr.h"
#include "DefaultComponent.h"
#include "BitmapFont.h"
#include "TextFade_Component.h"
#include "SliderComponent.h"
#include "AnimComponents.h"

namespace OVR {

//==============================================================
// OvrDoNotDisturb_OnUp
class OvrDoNotDisturb_OnUp : public VRMenuComponent_OnTouchUp
{
private:
	virtual eMsgStatus  OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
                                 VRMenuObject * self, VRMenuEvent const & event )
	{
        OVR_ASSERT( event.EventType == VRMENU_EVENT_TOUCH_UP );
		bool enabled = !app->GetDoNotDisturbMode();
		app->SetDoNotDisturbMode( enabled );

		char text[32];
		OVR_sprintf( text, sizeof( text ), "Do Not Disturb: %s", enabled ? "On" : "Off" );
		self->SetText( text );

        return MSG_STATUS_CONSUMED;
	}
};

//==============================================================
// OvrComfortMode_OnUp
class OvrComfortMode_OnUp : public VRMenuComponent_OnTouchUp
{
private:
	virtual eMsgStatus  OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
                                 VRMenuObject * self, VRMenuEvent const & event )
	{
        OVR_ASSERT( event.EventType == VRMENU_EVENT_TOUCH_UP );
		bool enabled = !app->GetComfortModeEnabled();
		app->SetComfortModeEnabled( enabled );

		char text[32];
		OVR_sprintf( text, sizeof( text ), "Comfort Mode: %s", enabled ? "On" : "Off" );
		self->SetText( text );

        return MSG_STATUS_CONSUMED;
	}
};

//==============================================================
// OvrPassthrough_OnUp
class OvrPassthrough_OnUp : public VRMenuComponent_OnTouchUp
{
public:
	OvrPassthrough_OnUp( OvrGlobalMenu & menu ) : 
		VRMenuComponent_OnTouchUp(),
		Menu( menu )
	{
	}

private:
	virtual eMsgStatus  OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
                                 VRMenuObject * self, VRMenuEvent const & event )
	{
        OVR_ASSERT( event.EventType == VRMENU_EVENT_TOUCH_UP );
		bool enabled = !app->IsPassThroughCameraEnabled();
		app->PassThroughCamera( enabled );

        return MSG_STATUS_CONSUMED;
	}

	OvrGlobalMenu & Menu;
};

//==============================================================
// OvrReorient_OnUp
class OvrReorient_OnUp : public VRMenuComponent_OnTouchUp
{
public:
	static const char *	TYPE_NAME;

	OvrReorient_OnUp() : IsTutorial( false ) { }

	bool				SetIsTutorial( bool const isTutorial ) { IsTutorial = isTutorial; }

	virtual const char *GetTypeName() const { return TYPE_NAME; }

private:
	virtual eMsgStatus  OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
                                 VRMenuObject * self, VRMenuEvent const & event )
	{
        OVR_ASSERT( event.EventType == VRMENU_EVENT_TOUCH_UP );
		LOG( "Reorient clicked" );

		// Notify app of the recenter to allow for example open VRMenus to reorient
		if ( !IsTutorial )
		{
			app->RecenterYaw( true );
			// this closes the menu, otherwise we may end up not looking at the menu but it will still be up
			//app->GetGuiSys().CloseCurrentMenu( app, app->GetGazeCursor() );
			app->ExitPlatformUI();
		}
        return MSG_STATUS_CONSUMED;
	}

	bool				IsTutorial;
};

const char *	OvrReorient_OnUp::TYPE_NAME = "OvrReorient_OnUp";

//==============================================================
// OvrHome_OnUp
class OvrHome_OnUp : public VRMenuComponent_OnTouchUp
{
public:
	static const char *	TYPE_NAME;

						OvrHome_OnUp() : IsTutorial( false ) { }

	bool				SetIsTutorial( bool const isTutorial ) { IsTutorial = isTutorial; }

	virtual const char *GetTypeName() const { return TYPE_NAME; }

private:
	virtual eMsgStatus  OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
                                VRMenuObject * self, VRMenuEvent const & event )
	{
        OVR_ASSERT( event.EventType == VRMENU_EVENT_TOUCH_UP );
		if ( !IsTutorial )
		{
			LOG( "Home clicked" );
			app->ReturnToLauncher();
		}
        return MSG_STATUS_CONSUMED;
	}

	bool				IsTutorial;
};

const char *	OvrHome_OnUp::TYPE_NAME = "OvrHome_OnUp";

//==============================================================
// OvrFadeTargets
// Fades an item in when any of the fade in targets gain focus
// Fades an item out when any of the fade out targets gain focus
// If the neither the fade target or its children are gaze selected
// for longer than fadeOutDelay, the fade target will fade out.
class OvrFadeTargets : public VRMenuComponent
{
public:
	OvrFadeTargets( VRMenu * menu, float const fadeTime, float fadeOutDelay,
		VRMenuId_t const fadeTargetId, 
		Array< VRMenuId_t > const & fadeInTargetIds, 
		Array< VRMenuId_t > const & fadeOutTargetIds ) :
		VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FOCUS_GAINED ) | 
				VRMENU_EVENT_FRAME_UPDATE ),
		Menu( menu ),
		FadeInTargetIds( fadeInTargetIds ),
		FadeOutTargetIds( fadeOutTargetIds ),
		FadeTargetId( fadeTargetId ),
		AlphaFader( 0.0f ),
		FadeTime( fadeTime ),
		FadeOutDelay( fadeOutDelay ),
		FadeOutTime( -1.0 ) {
	}

	virtual eMsgStatus	OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
								VRMenuObject * self, VRMenuEvent const & event );

private:
	VRMenu *			Menu;					// menu the objects are part of
	Array< VRMenuId_t >	FadeInTargetIds;		// if any of these gain focus, fade in
	Array< VRMenuId_t >	FadeOutTargetIds;		// if any of these gain focus, fade out
	VRMenuId_t			FadeTargetId;			// id of item to fade in and out
	SineFader			AlphaFader;				// fader for the alpha
	float const			FadeTime;				// time to fade in or out
	float const			FadeOutDelay;			// how long to wait after the fade targets lose focus before fading out
	double				FadeOutTime;			// time to begin the fade out after losing focus on fade target and children

private:
	eMsgStatus			OnInit( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
								VRMenuObject * self, VRMenuEvent const & event );
	eMsgStatus			OnFrameUpdate( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
								VRMenuObject * self, VRMenuEvent const & event );
	eMsgStatus			OnFocusGained( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
								VRMenuObject * self, VRMenuEvent const & event );

	VRMenuId_t			IsInList( OvrVRMenuMgr & menuMgr, VRMenuObject const * self, 
								VRMenuId_t const id, Array< VRMenuId_t > const & list, 
								bool checkChildren ) const;
};

eMsgStatus OvrFadeTargets::OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
		VRMenuObject * self, VRMenuEvent const & event )
{
	switch( event.EventType )
	{
		case VRMENU_EVENT_INIT:
			return OnInit( app, vrFrame, menuMgr, self, event );
		case VRMENU_EVENT_FRAME_UPDATE:
			return OnFrameUpdate( app, vrFrame, menuMgr, self, event );
		case VRMENU_EVENT_FOCUS_GAINED:
			return OnFocusGained( app, vrFrame, menuMgr, self, event );
	}
	OVR_ASSERT( false );	// unexpected message type?
	return MSG_STATUS_ALIVE;
}

eMsgStatus OvrFadeTargets::OnInit( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
		VRMenuObject * self, VRMenuEvent const & event )
{
	VRMenuObject * fadeTarget = menuMgr.ToObject( self->ChildHandleForId( menuMgr, FadeTargetId ) );
	if ( fadeTarget != NULL )
	{
		fadeTarget->AddFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ) );
	}
	return MSG_STATUS_ALIVE;
}

eMsgStatus OvrFadeTargets::OnFrameUpdate( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
		VRMenuObject * self, VRMenuEvent const & event )
{
	// if the focused handle is not one of the fade targets or any of their children
	// start the countdown to fading out
	VRMenuObject * focused = menuMgr.ToObject( Menu->GetFocusedHandle() );
	bool gazingAtFadeInTarget = focused != NULL && IsInList( menuMgr, self, focused->GetId(), FadeInTargetIds, true ).IsValid();
	if ( !gazingAtFadeInTarget )
	{
		//LOG( "!gazingAtFadeInTarget" );
		if ( FadeOutTime < 0.0 )
		{
			FadeOutTime = ovr_GetTimeInSeconds() + FadeOutDelay;
			LOG( "Start wait to fade" );
		}
		else if ( ovr_GetTimeInSeconds() >= FadeOutTime )
		{
			AlphaFader.StartFadeOut();
			FadeOutTime = -1.0;
			//LOG( "Begin Fade Out" );
		}
	}
	else if ( FadeOutTime >= 0.0 )
	{
		//LOG( "Clearing fade out" );
		FadeOutTime = -1.0;
	}

	float const FADE_RATE = 1.0f / FadeTime;
	AlphaFader.Update( FADE_RATE, vrFrame.DeltaSeconds );

	VRMenuObject * fadeTarget = menuMgr.ToObject( self->ChildHandleForId( menuMgr, FadeTargetId ) );
	if ( fadeTarget != NULL )
	{
		Vector4f color = fadeTarget->GetColor();
		color.w = AlphaFader.GetFinalAlpha();
		fadeTarget->SetColor( color );

		if ( AlphaFader.GetFadeState() == Fader::FADE_NONE && color.w < Mathf::SmallestNonDenormal )
		{
			fadeTarget->AddFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ) );
		}
		else
		{
			fadeTarget->RemoveFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ) );
		}
	}
	return MSG_STATUS_ALIVE;
}

eMsgStatus OvrFadeTargets::OnFocusGained( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
		VRMenuObject * self, VRMenuEvent const & event )
{

	VRMenuObject * gazeTarget = menuMgr.ToObject( event.TargetHandle );
	if ( gazeTarget != NULL )
	{
		if ( IsInList( menuMgr, self, gazeTarget->GetId(), FadeInTargetIds, false ).IsValid() )
		{
			AlphaFader.StartFadeIn();
		}
		else if ( IsInList( menuMgr, self, gazeTarget->GetId(), FadeOutTargetIds, false ).IsValid() )
		{
			AlphaFader.StartFadeOut();
		}
	}
	return MSG_STATUS_ALIVE;
}
VRMenuId_t OvrFadeTargets::IsInList( OvrVRMenuMgr & menuMgr, VRMenuObject const * self, 
		VRMenuId_t const id, Array< VRMenuId_t > const & list, bool checkChildren ) const
{
	for ( int i = 0; i < list.GetSizeI(); ++i )
	{
		if ( list[i] == id )
		{
			return list[i];
		}
		else if ( checkChildren )
		{
			VRMenuObject * target = menuMgr.ToObject( Menu->HandleForId( menuMgr, list[i] ) );
			if ( target != NULL && target->ChildHandleForId( menuMgr, id ).IsValid() )
			{
				return list[i];
			}
		}
	}
	return VRMenuId_t();
}

//==============================================================
// OvrTutorialGazeComponent
class OvrTutorialGazeComponent : public VRMenuComponent
{
public:
	static const int TYPE_ID = 1000;

	OvrTutorialGazeComponent( OvrGlobalMenu * globalMenu, VRMenuId_t const textId ) :
		VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FOCUS_GAINED ) | VRMENU_EVENT_FOCUS_LOST | VRMENU_EVENT_FRAME_UPDATE ),
		GlobalMenu( globalMenu ),
		TextId( textId ),
		TextFader( 0.0f ),
		Suppressed( false )
	{
	}

	virtual eMsgStatus  OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
                                VRMenuObject * self, VRMenuEvent const & event )
	{
		switch( event.EventType )
		{
			case VRMENU_EVENT_FRAME_UPDATE:
				return OnFrameUpdate( app, vrFrame, menuMgr, self, event );
			case VRMENU_EVENT_FOCUS_GAINED:
				return OnFocusGained( app, vrFrame, menuMgr, self, event );
			case VRMENU_EVENT_FOCUS_LOST:
				return OnFocusLost( app, vrFrame, menuMgr, self, event );
		}
		return MSG_STATUS_ALIVE;
	}

	virtual int		GetTypeId() const { return TYPE_ID; }
	void			SetSuppress( bool const suppress ) { Suppressed = suppress; }

private:
	OvrGlobalMenu *	GlobalMenu;
	VRMenuId_t		TextId;
	SineFader		TextFader;
	bool			Suppressed;

private:
	eMsgStatus  OnFrameUpdate( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
            VRMenuObject * self, VRMenuEvent const & event )
	{
		if ( Suppressed )
		{
			return MSG_STATUS_ALIVE;
		}

		float const FADE_DURATION = 0.5f;
		float const FADE_RATE = 1.0f / FADE_DURATION;
		TextFader.Update( FADE_RATE, vrFrame.DeltaSeconds );

		VRMenuObject * textObj = menuMgr.ToObject( GlobalMenu->HandleForId( menuMgr, TextId ) );
		OVR_ASSERT( textObj != NULL );
		if ( textObj != NULL )
		{
			Vector4f color = textObj->GetTextColor();
			color.w = TextFader.GetFinalAlpha();
			textObj->SetTextColor( color );
			if ( color.w <= Mathf::SmallestNonDenormal )
			{
				textObj->AddFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) );
			}
			else
			{
				textObj->RemoveFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) );
			}
		}
		return MSG_STATUS_ALIVE;
	}


	eMsgStatus  OnFocusGained( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
            VRMenuObject * self, VRMenuEvent const & event )
	{
		if ( !Suppressed )
		{
			TextFader.StartFadeIn();
			GlobalMenu->SetItemGazedOver( TextId, true );
		}
		return MSG_STATUS_ALIVE;
	}

	eMsgStatus  OnFocusLost( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
			VRMenuObject * self, VRMenuEvent const & event )
	{
		if ( !Suppressed )
		{
			TextFader.StartFadeOut();
			return MSG_STATUS_ALIVE;
		}
	}
};

VRMenuId_t OvrGlobalMenu::ID_DND( VRMenu::GetRootId().Get() - 1 );
VRMenuId_t OvrGlobalMenu::ID_HOME( VRMenu::GetRootId().Get() - 2 );
VRMenuId_t OvrGlobalMenu::ID_PASSTHROUGH( VRMenu::GetRootId().Get() - 3 );
VRMenuId_t OvrGlobalMenu::ID_REORIENT( VRMenu::GetRootId().Get() - 4 );
VRMenuId_t OvrGlobalMenu::ID_SETTINGS( VRMenu::GetRootId().Get() - 5 );
VRMenuId_t OvrGlobalMenu::ID_BRIGHTNESS( VRMenu::GetRootId().Get() - 6 );
VRMenuId_t OvrGlobalMenu::ID_COMFORT_MODE( VRMenu::GetRootId().Get() - 7 );
VRMenuId_t OvrGlobalMenu::ID_BATTERY( VRMenu::GetRootId().Get() - 8 );
VRMenuId_t OvrGlobalMenu::ID_BATTERY_TEXT( VRMenu::GetRootId().Get() - 9 );
VRMenuId_t OvrGlobalMenu::ID_WIFI( VRMenu::GetRootId().Get() - 10 );
VRMenuId_t OvrGlobalMenu::ID_BLUETOOTH( VRMenu::GetRootId().Get() - 11 );
VRMenuId_t OvrGlobalMenu::ID_SIGNAL( VRMenu::GetRootId().Get() - 12 );
VRMenuId_t OvrGlobalMenu::ID_TIME( VRMenu::GetRootId().Get() - 13 );
VRMenuId_t OvrGlobalMenu::ID_BRIGHTNESS_SLIDER( VRMenu::GetRootId().Get() - 14 );
VRMenuId_t OvrGlobalMenu::ID_BRIGHTNESS_SCRUBBER( VRMenu::GetRootId().Get() - 15 );
VRMenuId_t OvrGlobalMenu::ID_BRIGHTNESS_BUBBLE( VRMenu::GetRootId().Get() - 16 );
VRMenuId_t OvrGlobalMenu::ID_AIRPLANE_MODE( VRMenu::GetRootId().Get() + 17 );
VRMenuId_t OvrGlobalMenu::ID_TUTORIAL_START( VRMenu::GetRootId().Get() + 1000 );
VRMenuId_t OvrGlobalMenu::ID_TUTORIAL_HOME( VRMenu::GetRootId().Get() + 1001 );
VRMenuId_t OvrGlobalMenu::ID_TUTORIAL_REORIENT( VRMenu::GetRootId().Get() + 1002 );
VRMenuId_t OvrGlobalMenu::ID_TUTORIAL_BRIGHTNESS( VRMenu::GetRootId().Get() + 1003 );
VRMenuId_t OvrGlobalMenu::ID_TUTORIAL_DND( VRMenu::GetRootId().Get() + 1004 );
VRMenuId_t OvrGlobalMenu::ID_TUTORIAL_COMFORT( VRMenu::GetRootId().Get() + 1005 );
VRMenuId_t OvrGlobalMenu::ID_TUTORIAL_PASSTHROUGH( VRMenu::GetRootId().Get() + 1006 );
VRMenuId_t OvrGlobalMenu::ID_TUTORIAL_END( VRMenu::GetRootId().Get() + 1007 );

const char * OvrGlobalMenu::MENU_NAME = "Global";


//==============================
// OvrGlobalMenu::OvrGlobalMenu
OvrGlobalMenu::OvrGlobalMenu() :
    VRMenu( MENU_NAME ),
	ShowTutorial( false ),
	TutorialCount( 0 ),
	GazeOverCount( 0 )
{
}

//==============================
// OvrGlobalMenu::~OvrGlobalMenu
OvrGlobalMenu::~OvrGlobalMenu() 
{
}

struct tutorialItem_t
{
	VRMenuId_t		TextId;	// id of the item that renders the tutorial text
	VRMenuId_t		ItemId;	// id of the item that this is associated with
	char const *	Text;	// the actual tutorial text
	Vector3f		Offset;	// offset of the text from the menu root
	bool			CountGazeOver;	// true if this item counts towards the tutorial gaze over total
};

static tutorialItem_t tutorialItems[] =
{
	{ 
		OvrGlobalMenu::ID_TUTORIAL_START, 
		VRMenuId_t(), 
		"Remember, you can reach this at any\ntime by holding the Back button.\nGaze over each of the items below\nto continue.",
		Vector3f( 0.0f ),
		false
	},
	{ 
		OvrGlobalMenu::ID_TUTORIAL_HOME, 
		OvrGlobalMenu::ID_HOME,
		"HOME\nTakes you to Oculus Home." ,
		Vector3f( 0.0f ),
		true
	},
	{ 
		OvrGlobalMenu::ID_TUTORIAL_REORIENT, 
		OvrGlobalMenu::ID_REORIENT,
		"REORIENT\nCenters your screen to\nthe direction you're facing.",
		Vector3f( 0.0f ),
		true
	},
	{ 
		OvrGlobalMenu::ID_TUTORIAL_BRIGHTNESS, 
		OvrGlobalMenu::ID_BRIGHTNESS,
		"BRIGHTNESS\nControls the brightness\nof your screen.",
		Vector3f( 0.0f ),
		true
	},
	{ 
		OvrGlobalMenu::ID_TUTORIAL_DND, 
		OvrGlobalMenu::ID_DND,
		"DO NOT DISTURB\nBlocks phone call and\ntext message interruptions.",
		Vector3f( 0.0f ),
		true
	},
	{ 
		OvrGlobalMenu::ID_TUTORIAL_COMFORT, 
		OvrGlobalMenu::ID_COMFORT_MODE, 
		"COMFORT MODE\nChanges your screen's color\npalette to a warmer look.",
		Vector3f( 0.0f ),
		true
	},
	{ 
		OvrGlobalMenu::ID_TUTORIAL_PASSTHROUGH, 
		OvrGlobalMenu::ID_PASSTHROUGH, 
		"PASSTHROUGH CAMERA\nToggles the external camera view.",
		Vector3f( 0.0f ),
		true
	},
	{ 
		OvrGlobalMenu::ID_TUTORIAL_END, 
		VRMenuId_t(),
		"Let's go back.\nPress the Back button.",
		Vector3f( 0.0f ),
		false
	},
	{
		VRMenuId_t(),
		VRMenuId_t(),
		NULL,
		Vector3f( 0.0f ),
		false
	}
};

//==============================
// OvrGlobalMenu::Create
OvrGlobalMenu * OvrGlobalMenu::Create( App * app, OvrVRMenuMgr & menuMgr, BitmapFont const & font )
{
    OvrGlobalMenu * menu = new OvrGlobalMenu;

	Array< VRMenuObjectParms const * > itemParms;

	Vector3f fwd( 0.0f, 0.0f, 1.0f );
	Vector3f up( 0.0f, 1.0f, 0.0f );
	Vector3f right( fwd.Cross( up ) );

	float const ICON_WIDTH = 80.0f * VRMenuObject::DEFAULT_TEXEL_SCALE;  // we haven't loaded the icon yet so just hard-code...
	float const ICON_HEIGHT = 80.0f * VRMenuObject::DEFAULT_TEXEL_SCALE;

	{
		VRMenuFontParms itemFontParms( true, true, false, false, true, 0.525f, 0.475f, 1.0f );

		Vector3f textBaseOffset = up * ( ICON_WIDTH * -1.0f );
		Vector3f baseOffset = up * 0.3174f; // 10 degrees below horizon @ 1.8 meters distance
		Vector4f textColor(0.0f );
		Vector4f textHilightColor( 1.0f );
	
		struct itemOffset_t 
		{
			VRMenuId_t	Id;
			float		Offset;

			static float OffsetForId( VRMenuId_t const id, itemOffset_t const * itemOffsets )
			{
				for ( int i = 0; itemOffsets[i].Id.IsValid(); ++i )
				{
					if ( itemOffsets[i].Id == id )
					{
						return itemOffsets[i].Offset;
					}
				}
				return 0.0f;
			}
		};

		itemOffset_t const itemOffsets[] =
		{ 
			{ ID_HOME, 2.5f }, 
			{ ID_PASSTHROUGH, 1.5f },
			{ ID_REORIENT, 0.5f }, 
			{ ID_DND, -0.5f }, 
			{ ID_BRIGHTNESS, -1.5f }, 
			{ ID_COMFORT_MODE, -2.5f },
			{ VRMenuId_t(), 0.0f }
		};

		{
			char const * textOn = "Do Not Disturb: On";
			char const * textOff = "Do Not Disturb: Off";
			bool const enabled = app->GetDoNotDisturbMode();
			char const * text = enabled ? textOn : textOff;
			Vector3f iconRightOffset = right * ICON_WIDTH * itemOffset_t::OffsetForId( ID_DND, itemOffsets );
			Vector3f textOffset = textBaseOffset;// - iconRightOffset;
			Vector3f iconBaseOffset = iconRightOffset - baseOffset;
			Vector3f iconFadeOffset = OvrTextFade_Component::CalcIconFadeOffset( text, font, right, ICON_WIDTH );
			Posef pose( Quatf(), iconBaseOffset );
			Array< VRMenuComponent* > comps;
			comps.PushBack( new OvrDefaultComponent( Vector3f( 0.0f, 0.0f, 0.05f ), 1.0f, 0.125f, 0.125f, textColor, textHilightColor ) );
			comps.PushBack( new OvrDoNotDisturb_OnUp() );
			//comps.PushBack( new OvrTextFade_Component( iconBaseOffset, iconFadeOffset ) );
			VRMenuSurfaceParms diffuseParms( "dnd_diffuse",
					"res/raw/nav_disturb_off.png", SURFACE_TEXTURE_DIFFUSE,
					NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
			VRMenuSurfaceParms additiveParms( "dnd_additive",
					"res/raw/nav_disturb_on.png", SURFACE_TEXTURE_ADDITIVE,
					NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
			Array< VRMenuSurfaceParms > surfParms;
			surfParms.PushBack( diffuseParms );
			surfParms.PushBack( additiveParms );
			VRMenuObjectParms * parms = new VRMenuObjectParms( VRMENU_BUTTON, comps, surfParms, text,
					pose, Vector3f( 1.0f ), Posef( Quatf(), textOffset ), Vector3f( 1.0f ), itemFontParms,
					ID_DND, 
					VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_TEXT ) | VRMENUOBJECT_DONT_HIT_TEXT,
					VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

			itemParms.PushBack( parms );
		}

		{
			char const * text = "Reorient";
			Vector3f iconRightOffset = right * ICON_WIDTH * itemOffset_t::OffsetForId( ID_REORIENT, itemOffsets );
			Vector3f textOffset = textBaseOffset;// - iconRightOffset;
			Vector3f iconBaseOffset = iconRightOffset - baseOffset;
			Vector3f iconFadeOffset = OvrTextFade_Component::CalcIconFadeOffset( text, font, right, ICON_WIDTH );
			Posef pose( Quatf(), iconBaseOffset );
			Array< VRMenuComponent* > comps;
			comps.PushBack( new OvrDefaultComponent( Vector3f( 0.0f, 0.0f, 0.05f ), 1.0f, 0.125f, 0.125f, textColor, textHilightColor ) );
			comps.PushBack( new OvrReorient_OnUp );
			//comps.PushBack( new OvrTextFade_Component( iconBaseOffset, iconFadeOffset ) );
			VRMenuSurfaceParms diffuseParms( "reorient_diffuse",
					"res/raw/nav_reorient_off.png", SURFACE_TEXTURE_DIFFUSE,
					NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
			VRMenuSurfaceParms additiveParms( "reorient_additive",
					"res/raw/nav_reorient_on.png", SURFACE_TEXTURE_ADDITIVE,
					NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
			Array< VRMenuSurfaceParms > surfParms;
			surfParms.PushBack( diffuseParms );
			surfParms.PushBack( additiveParms );
			VRMenuObjectParms * parms = new VRMenuObjectParms( VRMENU_BUTTON, comps, surfParms, text,
					pose, Vector3f( 1.0f ), Posef( Quatf(), textOffset ), Vector3f( 1.0f ), itemFontParms,
					ID_REORIENT, 
					VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_TEXT ),
					VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

			itemParms.PushBack( parms );
		}

		{
			char const * text = "Oculus Home";
			Vector3f iconRightOffset = right * ICON_WIDTH * itemOffset_t::OffsetForId( ID_HOME, itemOffsets );
			Vector3f textOffset = textBaseOffset;// - iconRightOffset;
			Vector3f iconBaseOffset = iconRightOffset - baseOffset;
			Vector3f iconFadeOffset = OvrTextFade_Component::CalcIconFadeOffset( text, font, right, ICON_WIDTH );
			Posef pose( Quatf(), iconBaseOffset );
			Array< VRMenuComponent* > comps;
			comps.PushBack( new OvrDefaultComponent( Vector3f( 0.0f, 0.0f, 0.05f ), 1.0f, 0.125f, 0.125f, textColor, textHilightColor ) );
			comps.PushBack( new OvrHome_OnUp );
			//homeComps.PushBack( new OvrTextFade_Component( iconBaseOffset, iconFadeOffset ) );
			VRMenuSurfaceParms diffuseParms( "home_diffuse",
					"res/raw/nav_home_off.png", SURFACE_TEXTURE_DIFFUSE,
					NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
			VRMenuSurfaceParms additiveParms( "home_additive",
					"res/raw/nav_home_on.png", SURFACE_TEXTURE_ADDITIVE,
					NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
			Array< VRMenuSurfaceParms > surfParms;
			surfParms.PushBack( diffuseParms );
			surfParms.PushBack( additiveParms );
			VRMenuObjectParms * parms = new VRMenuObjectParms( VRMENU_BUTTON, comps, surfParms, text,
					pose, Vector3f( 1.0f ), Posef( Quatf(), textOffset ), Vector3f( 1.0f ), itemFontParms,
					ID_HOME, VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_TEXT ),
					VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

			itemParms.PushBack( parms );
		}

		{
			char const * text = "Brightness";
			Vector3f iconRightOffset = right * ICON_WIDTH * itemOffset_t::OffsetForId( ID_BRIGHTNESS, itemOffsets );
			Vector3f textOffset = textBaseOffset;// - iconRightOffset;
			Vector3f iconBaseOffset = iconRightOffset - baseOffset;
			Vector3f iconFadeOffset = OvrTextFade_Component::CalcIconFadeOffset( text, font, right, ICON_WIDTH );
			Posef pose( Quatf(), iconBaseOffset );
			Array< VRMenuComponent* > comps;
			comps.PushBack( new OvrDefaultComponent( Vector3f( 0.0f, 0.0f, 0.05f ), 1.0f, 0.125f, 0.125f, textColor, textHilightColor ) );
			//comps.PushBack( new OvrTextFade_Component( iconBaseOffset, iconFadeOffset ) );
			VRMenuSurfaceParms diffuseParms( "brightness_diffuse",
					"res/raw/nav_brightness_off.png", SURFACE_TEXTURE_DIFFUSE,
					NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
			VRMenuSurfaceParms additiveParms( "brightness_additive",
					"res/raw/nav_brightness_on.png", SURFACE_TEXTURE_ADDITIVE,
					NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
			Array< VRMenuSurfaceParms > surfParms;
			surfParms.PushBack( diffuseParms );
			surfParms.PushBack( additiveParms );
			VRMenuObjectParms * parms = new VRMenuObjectParms( VRMENU_BUTTON, comps, surfParms, text,
					pose, Vector3f( 1.0f ), Posef( Quatf(), textOffset ), Vector3f( 1.0f ), itemFontParms,
					ID_BRIGHTNESS, 
					VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_TEXT ),
					VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

			itemParms.PushBack( parms );
		}

		{
			char const * textOn = "Comfort Mode: On";
			char const * textOff = "Comfort Mode: Off";
			bool const enabled = app->GetComfortModeEnabled();
			char const * text = enabled ? textOn : textOff;
			Vector3f iconRightOffset = right * ICON_WIDTH * itemOffset_t::OffsetForId( ID_COMFORT_MODE, itemOffsets );
			Vector3f textOffset = textBaseOffset;// - iconRightOffset;
			Vector3f iconBaseOffset = iconRightOffset - baseOffset;
			Vector3f iconFadeOffset = OvrTextFade_Component::CalcIconFadeOffset( text, font, right, ICON_WIDTH );
			Posef pose( Quatf(), iconBaseOffset );
			Array< VRMenuComponent* > comps;
			comps.PushBack( new OvrDefaultComponent( Vector3f( 0.0f, 0.0f, 0.05f ), 1.0f, 0.125f, 0.125f, textColor, textHilightColor ) );
			comps.PushBack( new OvrComfortMode_OnUp() );
			//settingsComps.PushBack( new OvrTextFade_Component( iconBaseOffset, iconFadeOffset ) );
			VRMenuSurfaceParms diffuseParms( "comfort_diffuse",
					"res/raw/nav_comfort_off.png", SURFACE_TEXTURE_DIFFUSE,
					NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
			VRMenuSurfaceParms additiveParms( "comfort_additive",
					"res/raw/nav_comfort_on.png", SURFACE_TEXTURE_ADDITIVE,
					NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
			Array< VRMenuSurfaceParms > surfParms;
			surfParms.PushBack( diffuseParms );
			surfParms.PushBack( additiveParms );
			VRMenuObjectParms * parms = new VRMenuObjectParms( VRMENU_BUTTON, comps, surfParms, text,
					pose, Vector3f( 1.0f ), Posef( Quatf(), textOffset ), Vector3f( 1.0f ), itemFontParms,
					ID_COMFORT_MODE, 
					VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_TEXT ),
					VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

			itemParms.PushBack( parms );
		}

		{
			char const * textOn = "Passthrough Camera: On";
			char const * textOff = "Passthrough Camera: Off";
			bool const enabled = app->IsPassThroughCameraEnabled();
			char const * text = enabled ? textOn : textOff;
			Vector3f iconRightOffset = right * ICON_WIDTH * itemOffset_t::OffsetForId( ID_PASSTHROUGH, itemOffsets );
			Vector3f textOffset = textBaseOffset;// - iconRightOffset;
			Vector3f iconBaseOffset = iconRightOffset - baseOffset;
			Vector3f iconFadeOffset = OvrTextFade_Component::CalcIconFadeOffset( text, font, right, ICON_WIDTH );
			Posef pose( Quatf(), iconBaseOffset );
			Array< VRMenuComponent* > comps;
			comps.PushBack( new OvrDefaultComponent( Vector3f( 0.0f, 0.0f, 0.05f ), 1.0f, 0.125f, 0.125f, textColor, textHilightColor ) );
			comps.PushBack( new OvrPassthrough_OnUp( *menu ) );
			VRMenuSurfaceParms diffuseParms( "comfort_diffuse",
					"res/raw/nav_passthru_off.png", SURFACE_TEXTURE_DIFFUSE,
					NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
			VRMenuSurfaceParms additiveParms( "comfort_additive",
					"res/raw/nav_passthru_on.png", SURFACE_TEXTURE_ADDITIVE,
					NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
			Array< VRMenuSurfaceParms > surfParms;
			surfParms.PushBack( diffuseParms );
			surfParms.PushBack( additiveParms );
			VRMenuObjectParms * parms = new VRMenuObjectParms( VRMENU_BUTTON, comps, surfParms, text,
					pose, Vector3f( 1.0f ), Posef( Quatf(), textOffset ), Vector3f( 1.0f ), itemFontParms,
					ID_PASSTHROUGH, 
					VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_TEXT ),
					VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

			itemParms.PushBack( parms );
		}

		const float HEADER_ICON_HEIGHT = 32.0f * VRMenuObject::DEFAULT_TEXEL_SCALE;
		const float HEADER_ICON_WIDTH = 32.0f * VRMenuObject::DEFAULT_TEXEL_SCALE;
		const float HEADER_BASE_OFFSET = 0.0f;//ICON_WIDTH * 2.5f;
		const float HEADER_ICON_SCALE = 1.0f;
		Vector3f headerTextOffset( baseOffset + up * 0.0115f );
		Vector3f headerIconOffset( baseOffset );

#if 0
		int const NUM_ICONS = 13;
		float const iconOffsets[NUM_ICONS] =
		{
			-6.0f, -5.0f, -4.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f
		};
		for ( int i = 0; i < NUM_ICONS; ++i )
		{
			float const scale = HEADER_ICON_SCALE;
			Posef pose( Quatf(), headerIconOffset - right * ( HEADER_ICON_WIDTH * iconOffsets[i] ) );
			char name[32];
			OVR_sprintf( name, sizeof( name ), "icon_%i", i );
			char num[32];
			OVR_sprintf( num, sizeof( num ), "%i", i );
			VRMenuSurfaceParms surfParms( name,
					"res/raw/header_stub.png", SURFACE_TEXTURE_DIFFUSE,
					NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX,
					Vector2f( 0.0f, 1.0f ) );
			Array< VRMenuComponent* > comps;
			VRMenuFontParms fontParms( false, false, false, false, true, 0.465f, 0.51f, 0.55f, 0.625f );
			VRMenuObjectParms * parms = new VRMenuObjectParms( VRMENU_STATIC, comps, surfParms, num,
					pose, Vector3f( scale ), fontParms,
					VRMenuId_t(), VRMenuObjectFlags_t( VRMENUOBJECT_FLAG_NO_FOCUS_GAINED ),
					VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
			itemParms.PushBack( parms );
		}
#elif 1
		const int MAX_DIFFUSE = 5;
		struct iconInfo_t
		{
			VRMenuId_t		Id;
			float			Offset;
			char const *	DiffuseNames[MAX_DIFFUSE];
		};

		int const NUM_ICONS = 4;
		iconInfo_t iconInfo[NUM_ICONS] =
		{
			{ ID_SIGNAL, -1.0f, { "res/raw/header_signal_0.png", "res/raw/header_signal_1.png", "res/raw/header_signal_2.png", "res/raw/header_signal_3.png", "res/raw/header_signal_4.png" } },
			{ ID_BLUETOOTH, 0.0f, { "res/raw/header_bluetooth.png", NULL, NULL, NULL, NULL } },
			{ ID_WIFI, 1.0f, { "res/raw/header_wifi_0.png", "res/raw/header_wifi_1.png", "res/raw/header_wifi_2.png", "res/raw/header_wifi_3.png", "res/raw/header_wifi_4.png" } },
			{ ID_AIRPLANE_MODE, -1.0f, { "res/raw/airplane_mode.tga", NULL, NULL, NULL, NULL } }
		};

		for ( int i = 0; i < NUM_ICONS; ++i )
		{
			iconInfo_t const & info = iconInfo[i];
			float const scale = HEADER_ICON_SCALE;
			Posef pose( Quatf(), headerIconOffset - right * ( HEADER_ICON_WIDTH * iconInfo[i].Offset ) );
			Posef textPose( Quatf(), Vector3f( 0.0f ) );
			float const textScale = 1.0f;
			Array< VRMenuSurfaceParms > surfParms;
			for ( int j = 0; j < MAX_DIFFUSE && info.DiffuseNames[j] != NULL; ++j )
			{
				char name[32];
				OVR_sprintf( name, sizeof( name ), "icon_%i_surf_%j", i, j );
				VRMenuSurfaceParms sp( name,
						info.DiffuseNames[j], SURFACE_TEXTURE_DIFFUSE,
						NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX,
						Vector2f( 0.0f, 1.0f ) );
				surfParms.PushBack( sp );
			}
			Array< VRMenuComponent* > comps;
			comps.PushBack( new OvrSurfaceAnimComponent( 1.0f, false, 1 ) );
			VRMenuFontParms fontParms( false, false, false, false, true, 0.625f );
			VRMenuObjectParms * parms = new VRMenuObjectParms( VRMENU_STATIC, comps, surfParms, "",
					pose, Vector3f( scale ), textPose, Vector3f( textScale ), fontParms,
					info.Id, VRMenuObjectFlags_t( VRMENUOBJECT_FLAG_NO_FOCUS_GAINED ),
					VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
			itemParms.PushBack( parms );
		}
#endif

		{
			VRMenuFontParms fontParms( false, false, false, false, true, 0.625f );
			float const textScale = 1.0f;
			Posef pose( Quatf(), headerTextOffset - right * ( HEADER_ICON_WIDTH * -6.0f ) );
			Array< VRMenuComponent* > comps;
			VRMenuObjectParms * parms = new VRMenuObjectParms( VRMENU_STATIC, comps, VRMenuSurfaceParms( "time" ), "10:46 pm",
					pose, Vector3f( textScale ), fontParms,
					ID_TIME, VRMenuObjectFlags_t( VRMENUOBJECT_FLAG_NO_FOCUS_GAINED ),
					VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
			itemParms.PushBack( parms );
		}

		{
			VRMenuFontParms fontParms( false, false, false, false, true, 0.625f );
			float const textScale = 1.0f;
			Posef pose( Quatf(), headerTextOffset - right * ( HEADER_ICON_WIDTH * 3.70f ) );	// slightly off grid so % doesn't overlay batter at 100%
			Array< VRMenuComponent* > comps;
			VRMenuObjectParms * parms = new VRMenuObjectParms( VRMENU_STATIC, comps, VRMenuSurfaceParms( "battery_text" ), "100%",
					pose, Vector3f( textScale ), fontParms,
					ID_BATTERY_TEXT, VRMenuObjectFlags_t( VRMENUOBJECT_FLAG_NO_FOCUS_GAINED ),
					VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
			itemParms.PushBack( parms );
		}
		{
			float const scale = HEADER_ICON_SCALE;
			Posef pose( Quatf(), headerIconOffset - right * ( HEADER_ICON_WIDTH * 6.0f ) );
			VRMenuSurfaceParms surfParms( "battery",
					"res/raw/battery_indicator.tga", SURFACE_TEXTURE_DIFFUSE,
					"res/raw/battery_fill.tga", SURFACE_TEXTURE_COLOR_RAMP_TARGET,
					"res/raw/color_ramp_battery.tga", SURFACE_TEXTURE_COLOR_RAMP,
					Vector2f( 0.0f, 1.0f ) );
			Array< VRMenuComponent* > comps;
			VRMenuObjectParms * parms = new VRMenuObjectParms( VRMENU_STATIC, comps, surfParms, "",
					pose, Vector3f( scale ), itemFontParms,
					ID_BATTERY, VRMenuObjectFlags_t( VRMENUOBJECT_FLAG_NO_FOCUS_GAINED ),
					VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
			itemParms.PushBack( parms );
		}
	}

	const float SLIDER_HEIGHT = 287.0f * VRMenuObject::DEFAULT_TEXEL_SCALE;
	const float SLIDER_TRACK_HEIGHT = 145.0f * VRMenuObject::DEFAULT_TEXEL_SCALE;
	const float brightnessFrac = app->GetSystemBrightness() / 255.0f;
	const Vector3f localSlideDelta( SLIDER_TRACK_HEIGHT * up );
	Posef rootLocalPose( Quatf(), up * ( ( SLIDER_HEIGHT * 0.5f ) - ( ICON_HEIGHT * 0.5f ) ) );//+ fwd * 0.05f );
	OvrSliderComponent::GetVerticalSliderParms( *menu, 
			ID_BRIGHTNESS, ID_BRIGHTNESS_SLIDER, rootLocalPose,		
			ID_BRIGHTNESS_SCRUBBER, ID_BRIGHTNESS_BUBBLE,
			brightnessFrac, localSlideDelta, 0.0f, 10.f, 0.000625f, itemParms );

	int gazeOverCount = 0;
	for ( int i = 0; tutorialItems[i].Text != NULL; ++i )
	{
		tutorialItem_t const & item = tutorialItems[i];
		if ( item.CountGazeOver )
		{
			gazeOverCount++;
		}
		VRMenuFontParms fontParms( true, true, false, false, true, 0.625f );
		float const textScale = 1.0f;
		Posef pose( Quatf(), item.Offset );
		Array< VRMenuComponent* > comps;
		//comps.Append( new OvrTutorialComponent( item.TextId, item.ItemId );
		VRMenuObjectParms * parms = new VRMenuObjectParms( VRMENU_STATIC, comps, VRMenuSurfaceParms( "tutorial_item" ), 
				item.Text, pose, Vector3f( textScale ), fontParms,
				item.TextId, VRMenuObjectFlags_t( VRMENUOBJECT_FLAG_NO_FOCUS_GAINED ) | VRMENUOBJECT_DONT_RENDER | VRMENUOBJECT_DONT_HIT_ALL,
				VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
		itemParms.PushBack( parms );
	}
	menu->SetGazeOverCount( gazeOverCount );
	menu->InitWithItems( menuMgr, font, 1.85f, VRMenuFlags_t( VRMENU_FLAG_PLACE_ON_HORIZON ), itemParms );

	// get the slider root and add the component to make it fade in on gaze over
	VRMenuObject * root = menuMgr.ToObject( menu->GetRootHandle() );
	if ( root != NULL )
	{
		Array< VRMenuId_t > fadeInTargetIds;
		fadeInTargetIds.PushBack( ID_BRIGHTNESS );
		Array< VRMenuId_t > fadeOutTargetIds;
		fadeOutTargetIds.PushBack( ID_DND );
		fadeOutTargetIds.PushBack( ID_HOME );
		fadeOutTargetIds.PushBack( ID_REORIENT );
		fadeOutTargetIds.PushBack( ID_COMFORT_MODE );
		fadeOutTargetIds.PushBack( ID_PASSTHROUGH );
		root->AddComponent( new OvrFadeTargets( menu, 0.25f, 1.0f, ID_BRIGHTNESS_SLIDER, fadeInTargetIds, fadeOutTargetIds ) );
	} 

	// find the items that are associated with tutorial elements and add a component to them
	Array< OvrGazeOverState > gazeOverStates;
	for ( int i = 0; tutorialItems[i].Text != NULL; ++i )
	{
		tutorialItem_t const & item = tutorialItems[i];
		if ( item.ItemId != VRMenuId_t() )
		{
			OvrGazeOverState gazeOverState( item.TextId );
			gazeOverStates.PushBack( gazeOverState );		
			VRMenuObject * itemObj = menuMgr.ToObject( menu->HandleForId( menuMgr, item.ItemId ) );
			if ( itemObj != NULL )
			{
				itemObj->AddComponent( new OvrTutorialGazeComponent( menu, item.TextId ) );
				VRMenuObject * textObj = menuMgr.ToObject( menu->HandleForId( menuMgr, item.TextId ) );
				if ( textObj != NULL )
				{
					Posef pose( itemObj->GetLocalPose() );
					pose.Position -= up * 0.20f;
					textObj->SetTextLocalPose( pose );
				}
			}
		}
	}
	menu->SetGazeOverStates( gazeOverStates );

    DeletePointerArray( itemParms );

    return menu;
}

//==============================
// SetObjectRenderFlag
static void SetObjectRenderFlag( OvrVRMenuMgr & menuMgr, VRMenu const * menu, VRMenuId_t const id, bool const render )
{
	VRMenuObject * obj = menuMgr.ToObject( menu->HandleForId( menuMgr, id ) );
	if ( obj != NULL )
	{
		if ( render )
		{
			obj->RemoveFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) );
		}
		else
		{
			obj->AddFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) );
		}
	}
}

//==============================
// OvrGlobalMenu::SetTutorialTextVisibilities
void OvrGlobalMenu::SetTutorialTextVisibilities( OvrVRMenuMgr & menuMgr, bool const startVisible, bool const endVisible ) const
{
	SetObjectRenderFlag( menuMgr, this, OvrGlobalMenu::ID_TUTORIAL_START, startVisible );
	SetObjectRenderFlag( menuMgr, this, OvrGlobalMenu::ID_TUTORIAL_END, endVisible );
}

//==============================
// OvrGlobalMenu::Frame_Impl
void OvrGlobalMenu::Frame_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, BitmapFont const & font,
        BitmapFontSurface & fontSurface, gazeCursorUserId_t const gazeUserId )
{
    if ( GetCurMenuState() == VRMenu::MENUSTATE_CLOSED )
    {
        return;
    }

	bool buttonBPressed = ( vrFrame.Input.buttonPressed & BUTTON_B ) != 0;
	bool buttonBReleased = !buttonBPressed && ( vrFrame.Input.buttonReleased & BUTTON_B ) != 0;

	if ( buttonBReleased )
	{
		app->ExitPlatformUI();
		return;
	}

	if ( ShowTutorial )
	{
		// TODO: should really turn these on and off with a fade in a component
		int numGazedOver = NumItemsGazedOver();
		if ( numGazedOver == 0 )
		{
			SetTutorialTextVisibilities( menuMgr, true, false );
		}
		else if ( numGazedOver == GazeOverCount )
		{
			// turn off start text and turn on end text
			SetTutorialTextVisibilities( menuMgr, false, true );
		}
		else 
		{
			// turn off start text and end text
			SetTutorialTextVisibilities( menuMgr, false, false );
		}
	}

	// update battery level
	int batteryLevel = app->GetBatteryLevel();
	menuHandle_t batteryHandle = HandleForId( menuMgr, ID_BATTERY );
	VRMenuObject * batteryObj = menuMgr.ToObject( batteryHandle );
	if ( batteryObj != NULL )
	{
		Vector2f offset( 0.0f, 1.0f - ( batteryLevel / 100.0f ) );
		//DROIDLOG( "Battery", "batteryLevel %i, offset( %.2f, %.2f )", batteryLevel, offset.x, offset.y );
		batteryObj->SetColorTableOffset( offset );
	}

	menuHandle_t batteryTextHandle = HandleForId( menuMgr, ID_BATTERY_TEXT );
	VRMenuObject * batteryTextObj = menuMgr.ToObject( batteryTextHandle );
	if ( batteryTextObj != NULL )
	{
		char pctStr[16];
		OVR_sprintf( pctStr, sizeof( pctStr ), "%i%%", Alg::Clamp( batteryLevel, 0, 100 ) );
		batteryTextObj->SetText( pctStr );
	}

	menuHandle_t timeHandle = HandleForId( menuMgr, ID_TIME );
	VRMenuObject * timeObj = menuMgr.ToObject( timeHandle );
	if ( timeObj != NULL )
	{
		time_t rawTime;
		time( &rawTime );
		struct tm * timeInfo = localtime( &rawTime );
		char timeStr[128];

		// just time
		if ( timeInfo->tm_hour >= 12 )
		{
			strftime( timeStr, sizeof( timeStr ), "%I:%M pm", timeInfo );
		}
		else
		{
			strftime( timeStr, sizeof( timeStr ), "%I:%M am", timeInfo );
		}
		timeObj->SetText( timeStr );
	}

	bool airplaneMode = app->IsAirplaneModeEnabled();

	{
		menuHandle_t wifiHandle = HandleForId( menuMgr, ID_WIFI );
		VRMenuObject * obj = menuMgr.ToObject( wifiHandle );
		if ( obj != NULL )
		{
			int wifiLevel = app->GetWifiSignalLevel();
			//LOG( "WIFI signal level = %i", wifiLevel );
			OvrSurfaceAnimComponent * c = obj->GetComponentByName< OvrSurfaceAnimComponent >();
			if ( c != NULL )
			{
				c->SetFrame( obj, wifiLevel );
			}
			eWifiState wifiState = app->GetWifiState();
			if ( wifiState == WIFI_STATE_ENABLED || wifiState == WIFI_STATE_ENABLING )
			{
				obj->RemoveFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) );
			}
			else
			{
				obj->AddFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) );
			}
		}
	}

	{
		menuHandle_t signalHandle = HandleForId( menuMgr, ID_SIGNAL );
		VRMenuObject * obj = menuMgr.ToObject( signalHandle );
		if ( obj != NULL )
		{
			int signalLevel = app->GetCellularSignalLevel();
			//LOG( "Cellular signal level = %i", signalLevel );
			OvrSurfaceAnimComponent * c = obj->GetComponentByName< OvrSurfaceAnimComponent >();
			if ( c != NULL )
			{
				c->SetFrame( obj, signalLevel );
			}		
			if ( airplaneMode )
			{
				obj->AddFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) );
			}
			else
			{
				obj->RemoveFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) );
			}
		}
	}

	{
		menuHandle_t handle = HandleForId( menuMgr, ID_AIRPLANE_MODE );
		VRMenuObject * obj = menuMgr.ToObject( handle );
		if ( obj != NULL )
		{
			if ( !airplaneMode )
			{
				obj->AddFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) );
			}
			else
			{
				obj->RemoveFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) );
			}
		}
	}

	{
		menuHandle_t handle = HandleForId( menuMgr, ID_BLUETOOTH );
		VRMenuObject * obj = menuMgr.ToObject( handle );
		if ( obj != NULL )
		{
			bool bluetoothEnabled = app->GetBluetoothEnabled();
			if ( !bluetoothEnabled )
			{
				obj->AddFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) );
			}
			else
			{
				obj->RemoveFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) );
			}
		}
	}

	// update passthrough camera text because mounting on and sleeping may turn the camera off
	{
		menuHandle_t handle = HandleForId( menuMgr, ID_PASSTHROUGH );
		VRMenuObject * obj = menuMgr.ToObject( handle );
		if ( obj != NULL )
		{
			bool const enabled = app->IsPassThroughCameraEnabled();
			char text[32];
			OVR_sprintf( text, sizeof( text ), "Passthrough Camera: %s", enabled ? "On" : "Off" );
			obj->SetText( text );
		}
	}
}

//==============================
// OvrGlobalMenu::OnKeyEvent_Impl
bool OvrGlobalMenu::OnKeyEvent_Impl( App * app, int const keyCode, KeyState::eKeyEventType const eventType )
{
	if ( keyCode == AKEYCODE_BACK )
	{
		switch ( eventType )
		{
			case KeyState::KEY_EVENT_SHORT_PRESS:
				app->GetGuiSys().CloseMenu( app, MENU_NAME, true );
				app->ExitPlatformUI();
				return true;
			case KeyState::KEY_EVENT_LONG_PRESS:
				return true;
			case KeyState::KEY_EVENT_DOWN:
				return true;
			case KeyState::KEY_EVENT_UP:
				return true;
			case KeyState::KEY_EVENT_DOUBLE_TAP:
				// global menu must consume every back key but a short press
				return true;
		}
	}
	return false;
}

//==============================
// OvrGlobalMenu::OnItemEvent_Impl
void OvrGlobalMenu::OnItemEvent_Impl( App * app, VRMenuId_t const itemId, VRMenuEvent const & event )
{
	if ( itemId == ID_BRIGHTNESS_SLIDER && event.EventType == VRMENU_EVENT_TOUCH_RELATIVE )
	{
		VRMenuObject * sliderRoot = app->GetVRMenuMgr().ToObject( HandleForId( app->GetVRMenuMgr(), itemId ) );
		if ( sliderRoot != NULL )
		{
			OvrSliderComponent const * c = sliderRoot->GetComponentByName< OvrSliderComponent >();
			if ( c != NULL )
			{
				float const sliderFrac = c->GetSliderFrac();
				int brightness = Alg::Clamp( (int)( sliderFrac * 255.0f ), 1, 255 );
				app->SetSystemBrightness( brightness );
			}
		}
	}
}

//==============================
// OvrGlobalMenu::Open_Impl
void OvrGlobalMenu::Open_Impl( App * app, OvrGazeCursor & gazeCursor )
{
	// TODO: get this from a user preference? For now it is off by default
	// app->PassThroughCamera( EnablePassThrough );

	// reset the text since the camera was in an unknown state when the menu was actually created
	menuHandle_t handle = HandleForId( app->GetVRMenuMgr(), ID_PASSTHROUGH );
	VRMenuObject * obj = app->GetVRMenuMgr().ToObject( handle );
	if ( obj != NULL )
	{
		char text[128];
		OVR_sprintf( text, sizeof( text ), "Passthrough Camera: %s", app->IsPassThroughCameraEnabled() ? "On" : "Off" );
		obj->SetText( text );
	}
}

//==============================
// OvrGlobalMenu::Close_Impl
void OvrGlobalMenu::Close_Impl( App * app, OvrGazeCursor & gazeCursor )
{
	app->PassThroughCamera( false );
}

//==============================
// OvrGlobalMenu::NumItemsGazedOver
int OvrGlobalMenu::NumItemsGazedOver() const
{
	int count = 0;
	for ( int i = 0; i < GazeOverStates.GetSizeI(); ++i )
	{
		if ( GazeOverStates[i].GazedOver )
		{
			count++;
		}
	}
	return count;
}

//==============================
// OvrGlobalMenu::SetItemGazedOver
void OvrGlobalMenu::SetItemGazedOver( VRMenuId_t const & id, bool gazedOver )
{
	for ( int i = 0; i < GazeOverStates.GetSizeI(); ++i )
	{
		if ( GazeOverStates[i].ItemId == id )
		{
			GazeOverStates[i].GazedOver = gazedOver;
			return;
		}
	}
	OVR_ASSERT( false );	// item id should always be in the list
}

void OvrGlobalMenu::SetShowTutorial( OvrVRMenuMgr & menuMgr, bool const show ) 
{ 
	ShowTutorial = show;
	struct showInfo_t
	{
		VRMenuId_t	Id;
		bool		IsTutorialItem;
	};
	showInfo_t itemInfos[] =
	{
		{ ID_DND, false },
		{ ID_REORIENT, false },
		{ ID_HOME, false },
		{ ID_BRIGHTNESS, false },
		{ ID_COMFORT_MODE, false },
		{ ID_PASSTHROUGH, false },
		{ ID_TUTORIAL_HOME, true },
		{ ID_TUTORIAL_REORIENT, true },
		{ ID_TUTORIAL_BRIGHTNESS, true },
		{ ID_TUTORIAL_DND, true },
		{ ID_TUTORIAL_COMFORT, true },
		{ ID_TUTORIAL_PASSTHROUGH, true },
		{ VRMenuId_t(), false }
	};

	// if showing the tutorial, disable the default component on the menu buttons
	// if not showing the tutorial, disable the tutorial gaze component on the menu buttons AND
	// set the tutorial text items to not render.  We have to disable the tutorial gaze component
	// because it will reset the render flags on the tutorial text items if we don't.
	for ( int i = 0; itemInfos[i].Id != VRMenuId_t(); ++i ) 
	{
		VRMenuObject * obj = menuMgr.ToObject( HandleForId( menuMgr, itemInfos[i].Id ) );
		if ( obj != NULL )
		{
			Array< VRMenuComponent* > const & comps = obj->GetComponentList();
			for ( int c = 0; c < comps.GetSizeI(); ++c )
			{
				VRMenuComponent const * comp = comps[c];
				if ( comp->GetTypeId() == OvrDefaultComponent::TYPE_ID )
				{
					( ( OvrDefaultComponent* )comp )->SetSuppressText( ShowTutorial );
				}
				else if ( comp->GetTypeId() == OvrTutorialGazeComponent::TYPE_ID )
				{
					( ( OvrTutorialGazeComponent* )comp )->SetSuppress( !ShowTutorial );
				}
			}
			if ( itemInfos[i].IsTutorialItem )
			{
				if ( ShowTutorial )
				{
					obj->RemoveFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) );
				}
				else
				{
					obj->AddFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) );
				}
			}
		}
	}

	{
		// allow or disallow exiting the menu from Home
		VRMenuObject * obj = menuMgr.ToObject( HandleForId( menuMgr, ID_HOME ) );
		if ( obj != NULL )
		{
			OvrHome_OnUp * comp = obj->GetComponentByName< OvrHome_OnUp >();
			if ( comp != NULL )
			{
				comp->SetIsTutorial( ShowTutorial );
			}
		}
	}

	{
		// allow or disallow exiting the menu from Reorient
		VRMenuObject * obj = menuMgr.ToObject( HandleForId( menuMgr, ID_REORIENT ) );
		if ( obj != NULL )
		{
			OvrReorient_OnUp * comp = obj->GetComponentByName< OvrReorient_OnUp >();
			if ( comp != NULL )
			{
				comp->SetIsTutorial( ShowTutorial );
			}
		}
	}

	if ( !ShowTutorial )
	{
		// make sure tutorial start and end are not visible if not showing tutorial
		// the end can be "left on" after finishing the tutorial
		SetTutorialTextVisibilities( menuMgr, false, false );
	}

	// reinitialize all gaze over states
	for ( int i = 0; i < GazeOverStates.GetSizeI(); ++i )
	{
		GazeOverStates[i].GazedOver = false;
	}
}

} // namespace OVR

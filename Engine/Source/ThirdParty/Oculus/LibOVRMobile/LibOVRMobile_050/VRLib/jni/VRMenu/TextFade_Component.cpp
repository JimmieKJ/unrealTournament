/************************************************************************************

Filename    :   OvrTextFade_Component.cpp
Content     :   A reusable component that fades text in and recenters it on gaze over.
Created     :   July 25, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "TextFade_Component.h"

#include "../VrApi/Vsync.h"
#include "../Input.h"
#include "../BitmapFont.h"
#include "VRMenuMgr.h"

namespace OVR {

double const OvrTextFade_Component::FADE_DELAY = 0.05;
float const  OvrTextFade_Component::FADE_DURATION = 0.25f;


//==============================
// OvrTextFade_Component::CalcIconFadeOffset
Vector3f OvrTextFade_Component::CalcIconFadeOffset( char const * text, BitmapFont const & font, Vector3f const & axis, float const iconWidth )
{
    float textWidth = font.CalcTextWidth( text );
    float const fullWidth = textWidth + iconWidth;
    return axis * ( ( fullWidth * 0.5f ) - ( iconWidth * 0.5f ) );  // this is a bit odd, but that's because the icon's origin is its center
}

//==============================
// OvrTextFade_Component::OvrTextFade_Component
OvrTextFade_Component::OvrTextFade_Component( Vector3f const & iconBaseOffset, Vector3f const & iconFadeOffset ) :
	VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FRAME_UPDATE ) | VRMENU_EVENT_FOCUS_GAINED | VRMENU_EVENT_FOCUS_LOST ),
	TextAlphaFader( 0.0f ),
	StartFadeInTime( -1.0 ),
	StartFadeOutTime( -1.0 ),
	IconOffsetFader( 0.0f ),
	IconBaseOffset( iconBaseOffset ),
	IconFadeOffset( iconFadeOffset )
{
}

//==============================
// OvrTextFade_Component::OnEvent_Impl
eMsgStatus OvrTextFade_Component::OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
												VRMenuObject * self, VRMenuEvent const & event )
{
	OVR_ASSERT( HandlesEvent( VRMenuEventFlags_t( event.EventType ) ) );
	switch ( event.EventType )
	{
		case VRMENU_EVENT_FRAME_UPDATE:
		return Frame( app, vrFrame, menuMgr, self, event );
		case VRMENU_EVENT_FOCUS_GAINED:
		return FocusGained( app, vrFrame, menuMgr, self, event );
		case VRMENU_EVENT_FOCUS_LOST:
		return FocusLost( app, vrFrame, menuMgr, self, event );
		default:
		OVR_ASSERT( !"Event flags mismatch!" ); // the constructor is specifying a flag that's not handled
		return MSG_STATUS_ALIVE;
	}
}

//==============================
// OvrTextFade_Component::Frame
eMsgStatus OvrTextFade_Component::Frame( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
											VRMenuObject * self, VRMenuEvent const & event )
{
	double t = ovr_GetTimeInSeconds();
	if ( StartFadeInTime >= 0.0f && t >= StartFadeInTime )
	{
		TextAlphaFader.StartFadeIn();
		IconOffsetFader.StartFadeIn();
		StartFadeInTime = -1.0f;
		// start bounding all when we begin to fade out
		VRMenuObjectFlags_t flags = self->GetFlags();
		flags |= VRMENUOBJECT_BOUND_ALL;
		self->SetFlags( flags );
	}
	else if ( StartFadeOutTime >= 0.0f && t > StartFadeOutTime )
	{
		TextAlphaFader.StartFadeOut();
		IconOffsetFader.StartFadeOut();
		StartFadeOutTime = -1.0f;
		// stop bounding all when faded out
		VRMenuObjectFlags_t flags = self->GetFlags();
		flags &= ~VRMenuObjectFlags_t( VRMENUOBJECT_BOUND_ALL );
		self->SetFlags( flags );
	}

	float const FADE_RATE = 1.0f / FADE_DURATION;

	TextAlphaFader.Update( FADE_RATE, vrFrame.DeltaSeconds );
	IconOffsetFader.Update( FADE_RATE, vrFrame.DeltaSeconds );

	Vector4f textColor = self->GetTextColor();
	textColor.w = TextAlphaFader.GetFinalAlpha();
	self->SetTextColor( textColor );

	Vector3f curOffset = IconBaseOffset + ( IconOffsetFader.GetFinalAlpha() * IconFadeOffset );
	self->SetLocalPosition( curOffset );

	return MSG_STATUS_ALIVE;
}

//==============================
// OvrTextFade_Component::FocusGained
eMsgStatus OvrTextFade_Component::FocusGained( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
												VRMenuObject * self, VRMenuEvent const & event )
{

	StartFadeOutTime = -1.0;
	StartFadeInTime = FADE_DELAY + ovr_GetTimeInSeconds();

	return MSG_STATUS_ALIVE;
}

//==============================
// OvrTextFade_Component::FocusLost
eMsgStatus OvrTextFade_Component::FocusLost( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
												VRMenuObject * self, VRMenuEvent const & event )
{
	StartFadeOutTime = FADE_DELAY + ovr_GetTimeInSeconds();
	StartFadeInTime = -1.0;

	return MSG_STATUS_ALIVE;
}

} // namespace OVR
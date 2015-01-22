/************************************************************************************

Filename    :   Slider_Component.cpp
Content     :   A reusable component implementing a slider bar.
Created     :   Sept 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "SliderComponent.h"

#include "VRMenu.h"
#include "VRMenuMgr.h"
#include "../Input.h"
#include "../App.h"

namespace OVR {

char const * OvrSliderComponent::TYPE_NAME = "OvrSliderComponent";

//==============================
// OvrSliderComponent::OvrSliderComponent
OvrSliderComponent::OvrSliderComponent( VRMenu & menu, float const sliderFrac, Vector3f const & localSlideDelta, 
		float const minValue, float const maxValue, float const sensitivityScale, VRMenuId_t const rootId, 
		VRMenuId_t const scrubberId, VRMenuId_t const textId, VRMenuId_t const bubbleId ) :
		VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_TOUCH_DOWN ) | VRMENU_EVENT_TOUCH_UP |
				VRMENU_EVENT_TOUCH_RELATIVE | VRMENU_EVENT_INIT | VRMENU_EVENT_FRAME_UPDATE ),
		TouchDown( false ),
		SliderFrac( sliderFrac ),
		MinValue( minValue ),
		MaxValue( maxValue ),
		SensitivityScale( sensitivityScale ),
		LocalSlideDelta( localSlideDelta ),
		Menu( menu ),
		RootId( rootId ),
		ScrubberId( scrubberId ),
		BubbleId( bubbleId ),
		TextId( textId ),
		CaretBasePose(),
		BubbleFader( 0.0f ),
		BubbleFadeOutTime( -1.0 )
{
}

//==============================
// OvrSliderComponent::OnEvent_Impl
eMsgStatus OvrSliderComponent::OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
		VRMenuObject * self, VRMenuEvent const & event )
{
	switch ( event.EventType )
	{
		case VRMENU_EVENT_INIT:
			return OnInit( app, vrFrame, menuMgr, self, event );
		case VRMENU_EVENT_FRAME_UPDATE:
			return OnFrameUpdate( app, vrFrame, menuMgr, self, event );
		case VRMENU_EVENT_TOUCH_DOWN:
			return OnTouchDown( app, vrFrame, menuMgr, self, event );
		case VRMENU_EVENT_TOUCH_UP:
			return OnTouchUp( app, vrFrame, menuMgr, self, event );
			TouchDown = false;
		case VRMENU_EVENT_TOUCH_RELATIVE:
			return OnTouchRelative( app, vrFrame, menuMgr, self, event );
	}
    return MSG_STATUS_CONSUMED;
}

//==============================
// OvrSliderComponent::OnInit
eMsgStatus OvrSliderComponent::OnInit( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
		VRMenuObject * self, VRMenuEvent const & event )
{
	// find the starting offset of the caret
	LOG( "OvrSliderComponent - VRMENU_EVENT_INIT" );
	VRMenuObject * caret = menuMgr.ToObject( self->ChildHandleForId( menuMgr, ScrubberId ) );
	if ( caret != NULL )
	{
		CaretBasePose = caret->GetLocalPose();
	}
	SetCaretPoseFromFrac( menuMgr, self, SliderFrac );
	UpdateText( menuMgr, self, BubbleId );
}

//==============================
// OvrSliderComponent::OnFrameUpdate
eMsgStatus OvrSliderComponent::OnFrameUpdate( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
		VRMenuObject * self, VRMenuEvent const & event )
{
	if ( TouchDown )
	{
		UpdateText( menuMgr, self, BubbleId );
		UpdateText( menuMgr, self, TextId );
	}

	if ( BubbleFadeOutTime > 0.0 )
	{
		if ( ovr_GetTimeInSeconds() >= BubbleFadeOutTime )
		{
			BubbleFadeOutTime = -1.0;
			BubbleFader.StartFadeOut();
		}
	}

	VRMenuObject * bubble = menuMgr.ToObject( self->ChildHandleForId( menuMgr, BubbleId ) );
	if ( bubble != NULL )
	{
		float const fadeTime = 0.5f;
		float const fadeRate = 1.0 / fadeTime;
		BubbleFader.Update( fadeRate, vrFrame.DeltaSeconds );

		Vector4f color = bubble->GetColor();
		color.w = BubbleFader.GetFinalAlpha();
		bubble->SetColor( color );
		Vector4f textColor = bubble->GetTextColor();
		textColor.w = color.w;
		bubble->SetTextColor( textColor );
	}

	return MSG_STATUS_ALIVE;
}

//==============================
// OvrSliderComponent::OnTouchRelative
eMsgStatus OvrSliderComponent::OnTouchRelative( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
		VRMenuObject * self, VRMenuEvent const & event )
{
	//LOG( "event.FloatValue = ( %.8f, %.8f )", event.FloatValue.x, event.FloatValue.y );
	// project on to the normalized slide delta so that the movement on the pad matches the orientation of the slider
	Vector2f slideDelta( LocalSlideDelta.x, LocalSlideDelta.y );
	slideDelta.Normalize();
	Vector2f touchXY( event.FloatValue.x, event.FloatValue.y );
	
	float dot = slideDelta.Dot( touchXY );
	float const r = dot * SensitivityScale;
	SliderFrac = Alg::Clamp( SliderFrac - r, 0.0f, 1.0f );
	SetCaretPoseFromFrac( menuMgr, self, SliderFrac );
	
	Menu.OnItemEvent( app, RootId, event );

	// update the bubble text
	UpdateText( menuMgr, self, BubbleId );

	return MSG_STATUS_CONSUMED;
}

//==============================
// OvrSliderComponent::OnTouchDown
eMsgStatus OvrSliderComponent::OnTouchDown( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
		VRMenuObject * self, VRMenuEvent const & event )
{
	BubbleFader.StartFadeIn();
	BubbleFadeOutTime = -1.0;
	return MSG_STATUS_ALIVE;
}

//==============================
// OvrSliderComponent::OnTouchUp
eMsgStatus OvrSliderComponent::OnTouchUp( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
		VRMenuObject * self, VRMenuEvent const & event )
{
	BubbleFadeOutTime = ovr_GetTimeInSeconds() + 1.5;
	return MSG_STATUS_ALIVE;
}

//==============================
// OvrSliderComponent::SetCaretPoseFromFrac
void OvrSliderComponent::SetCaretPoseFromFrac( OvrVRMenuMgr & menuMgr, VRMenuObject * self, float const frac )
{
	VRMenuObject * caret = menuMgr.ToObject( self->ChildHandleForId( menuMgr, ScrubberId ) );
	if ( caret != NULL )
	{
		Posef curPose = CaretBasePose;
		float range = MaxValue - MinValue;
		float frac = floor( SliderFrac * range ) / range;
		curPose.Position += ( LocalSlideDelta * -0.5f ) + LocalSlideDelta * frac;
		caret->SetLocalPose( curPose );
	}
}

//==============================
// OvrSliderComponent::UpdateText
void OvrSliderComponent::UpdateText( OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuId_t const id )
{
	if ( !id.IsValid() )
	{
		return;
	}

	VRMenuObject * obj = menuMgr.ToObject( self->ChildHandleForId( menuMgr, id ) );
	if ( obj != NULL )
	{
		char valueStr[127];
		GetStringValue( valueStr, sizeof( valueStr ) );
		obj->SetText( valueStr );
	}
}

//==============================
// OvrSliderComponent::GetStringValue
void OvrSliderComponent::GetStringValue( char * valueStr, int maxLen ) const
{
	int curValue = (int)floor( ( MaxValue - MinValue ) * SliderFrac + MinValue );
	OVR_sprintf( valueStr, maxLen, "%i", curValue );
}

enum eSliderImage
{
	SLIDER_IMAGE_BASE,
	SLIDER_IMAGE_TRACK,
	SLIDER_IMAGE_TRACK_FULL,
	SLIDER_IMAGE_SCRUBBER,
	SLIDER_IMAGE_BUBBLE,
	SLIDER_IMAGE_MAX
};

String GetSliderImage( eSliderImage const type, bool vertical )
{
	static char const * images[SLIDER_IMAGE_MAX] =
	{
		"res/raw/slider_base_%s.png",
		"res/raw/slider_track_%s.png",
		"res/raw/slider_track_full_%s.png",
		"res/raw/slider_scrubber.png",
		"res/raw/slider_bubble_%s.png"
	};

	char buff[256];
	OVR_sprintf( buff, sizeof( buff ), images[type], vertical ? "vert" : "horz" );
	return String( buff );
}

void OvrSliderComponent::GetVerticalSliderParms( VRMenu & menu, VRMenuId_t const parentId, VRMenuId_t const rootId, 
		Posef const & rootLocalPose, VRMenuId_t const scrubberId, VRMenuId_t const bubbleId, 
		float const sliderFrac, Vector3f const & localSlideDelta,
		float const minValue, float const maxValue, float const sensitivityScale, 
		Array< VRMenuObjectParms const* > & parms )
{
	Vector3f const fwd( 0.0f, 0.0f, -1.0f );
	Vector3f const right( 1.0f, 0.0f, 0.0f );
	Vector3f const up( 0.0f, 1.0f, 0.0f );

	// would be nice to determine these sizes from the images, but we do not load
	// images until much later, meaning we'd need to do the positioning after creation / init.
	float const SLIDER_BUBBLE_WIDTH = 59.0f * VRMenuObject::DEFAULT_TEXEL_SCALE;
	float const SLIDER_TRACK_WIDTH = 9.0f * VRMenuObject::DEFAULT_TEXEL_SCALE;
	float const TRACK_OFFSET = 35.0f * VRMenuObject::DEFAULT_TEXEL_SCALE;
	float const vertical = true;

	// add parms for the root object that holds all the slider components
	{
		Array< VRMenuComponent* > comps;
		comps.PushBack( new OvrSliderComponent( menu, sliderFrac, localSlideDelta, minValue, maxValue, sensitivityScale, rootId, scrubberId, VRMenuId_t(), bubbleId ) );
		Array< VRMenuSurfaceParms > surfParms;
		char const * text = "slider_root";
		Vector3f scale( 1.0f );
		Posef pose( rootLocalPose );
		Posef textPose( Quatf(), Vector3f( 0.0f ) );
		Vector3f textScale( 1.0f );
		VRMenuFontParms fontParms;
		VRMenuObjectFlags_t objectFlags;
		VRMenuObjectInitFlags_t initFlags( VRMENUOBJECT_INIT_FORCE_POSITION );
		VRMenuObjectParms * itemParms = new VRMenuObjectParms( VRMENU_CONTAINER, comps,
				surfParms, text, pose, scale, textPose, textScale, fontParms, rootId, 
				objectFlags, initFlags );
		itemParms->ParentId = parentId;
		parms.PushBack( itemParms );
	}

	// add parms for the base image that underlays the whole slider
	{
		Array< VRMenuComponent* > comps;
		Array< VRMenuSurfaceParms > surfParms;
		VRMenuSurfaceParms baseParms( "base", GetSliderImage( SLIDER_IMAGE_BASE, vertical ), SURFACE_TEXTURE_DIFFUSE,
				NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
		surfParms.PushBack( baseParms );
		char const * text = "base";
		Vector3f scale( 1.0f );
		Posef pose( Quatf(), Vector3f() + fwd * 0.1f );
		Posef textPose( Quatf(), Vector3f( 0.0f ) );
		Vector3f textScale( 1.0f );
		VRMenuFontParms fontParms;
		VRMenuObjectFlags_t objectFlags( VRMENUOBJECT_DONT_RENDER_TEXT );
		VRMenuObjectInitFlags_t initFlags( VRMENUOBJECT_INIT_FORCE_POSITION );
		VRMenuObjectParms * itemParms = new VRMenuObjectParms( VRMENU_BUTTON, comps,
				surfParms, text, pose, scale, textPose, textScale, fontParms, VRMenuId_t(), 
				objectFlags, initFlags );
		itemParms->ParentId = rootId;
		parms.PushBack( itemParms );
	}

	// add parms for the track image
	{
		Array< VRMenuComponent* > comps;
		Array< VRMenuSurfaceParms > surfParms;
		VRMenuSurfaceParms baseParms( "track", GetSliderImage( SLIDER_IMAGE_TRACK, vertical ), SURFACE_TEXTURE_DIFFUSE,
				NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
		surfParms.PushBack( baseParms );
		char const * text = "track";
		Vector3f scale( 1.0f );
		Posef pose( Quatf(), up * TRACK_OFFSET + fwd * 0.09f );
		Posef textPose( Quatf(), Vector3f( 0.0f ) );
		Vector3f textScale( 1.0f );
		VRMenuFontParms fontParms;
		VRMenuObjectFlags_t objectFlags( VRMENUOBJECT_DONT_RENDER_TEXT );
		VRMenuObjectInitFlags_t initFlags( VRMENUOBJECT_INIT_FORCE_POSITION );
		VRMenuObjectParms * itemParms = new VRMenuObjectParms( VRMENU_BUTTON, comps,
				surfParms, text, pose, scale, textPose, textScale, fontParms, VRMenuId_t(), 
				objectFlags, initFlags );
		itemParms->ParentId = rootId;
		parms.PushBack( itemParms );
	}

	// add parms for the filled track image
	{
		Array< VRMenuComponent* > comps;
		Array< VRMenuSurfaceParms > surfParms;
		VRMenuSurfaceParms baseParms( "track_full", GetSliderImage( SLIDER_IMAGE_TRACK_FULL, vertical ), SURFACE_TEXTURE_DIFFUSE,
				NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
		surfParms.PushBack( baseParms );
		char const * text = "track_full";
		Vector3f scale( 1.0f );
		Posef pose( Quatf(), up * TRACK_OFFSET + fwd * 0.08f );
		Posef textPose( Quatf(), Vector3f( 0.0f ) );
		Vector3f textScale( 1.0f );
		VRMenuFontParms fontParms;
		VRMenuObjectFlags_t objectFlags( VRMENUOBJECT_DONT_RENDER_TEXT );
		VRMenuObjectInitFlags_t initFlags( VRMENUOBJECT_INIT_FORCE_POSITION );
		VRMenuObjectParms * itemParms = new VRMenuObjectParms( VRMENU_BUTTON, comps,
				surfParms, text, pose, scale, textPose, textScale, fontParms, VRMenuId_t(), 
				objectFlags, initFlags );
		itemParms->ParentId = rootId;
		parms.PushBack( itemParms );
	}

	// add parms for the scrubber
	{
		Array< VRMenuComponent* > comps;
		Array< VRMenuSurfaceParms > surfParms;
		VRMenuSurfaceParms baseParms( "scrubber", GetSliderImage( SLIDER_IMAGE_SCRUBBER, vertical ), SURFACE_TEXTURE_DIFFUSE,
				NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
		surfParms.PushBack( baseParms );
		char const * text = "scrubber";
		Vector3f scale( 1.0f );
		Posef pose( Quatf(), up * TRACK_OFFSET + fwd * 0.07f );
		Posef textPose( Quatf(), Vector3f( 0.0f ) );
		Vector3f textScale( 1.0f );
		VRMenuFontParms fontParms;
		VRMenuObjectFlags_t objectFlags( VRMENUOBJECT_DONT_RENDER_TEXT );
		VRMenuObjectInitFlags_t initFlags( VRMENUOBJECT_INIT_FORCE_POSITION );
		VRMenuObjectParms * itemParms = new VRMenuObjectParms( VRMENU_BUTTON, comps,
				surfParms, text, pose, scale, textPose, textScale, fontParms, scrubberId, 
				objectFlags, initFlags );
		itemParms->ParentId = rootId;
		parms.PushBack( itemParms );
	}

	// add parms for the bubble
	{
		Array< VRMenuComponent* > comps;
		Array< VRMenuSurfaceParms > surfParms;
		VRMenuSurfaceParms baseParms( "bubble", GetSliderImage( SLIDER_IMAGE_BUBBLE, vertical ), SURFACE_TEXTURE_DIFFUSE,
				NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
		surfParms.PushBack( baseParms );
		char const * text = NULL;
		Vector3f scale( 1.0f );
		Posef pose( Quatf(), right * ( SLIDER_TRACK_WIDTH + SLIDER_BUBBLE_WIDTH * 0.5f ) + fwd * 0.06f );
		Posef textPose( Quatf(), Vector3f( 0.0f ) );
		Vector3f textScale( 1.0f );
		VRMenuFontParms fontParms( true, true, false, false, true, 0.66f );
		VRMenuObjectFlags_t objectFlags;
		VRMenuObjectInitFlags_t initFlags( VRMENUOBJECT_INIT_FORCE_POSITION );
		VRMenuObjectParms * itemParms = new VRMenuObjectParms( VRMENU_BUTTON, comps,
				surfParms, text, pose, scale, textPose, textScale, fontParms, bubbleId, 
				objectFlags, initFlags );
		itemParms->ParentId = scrubberId;
		parms.PushBack( itemParms );
	}
}

} // namespace OVR
/************************************************************************************

Filename    :   ScrollBarComponent.h
Content     :   A reusable component implementing a scroll bar.
Created     :   Jan 15, 2014
Authors     :   Warsam Osman

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.


*************************************************************************************/

#include "ScrollBarComponent.h"
#include "App.h"
#include "VRMenuMgr.h"
#include "PackageFiles.h"

namespace OVR {

char const * OvrScrollBarComponent::TYPE_NAME = "OvrScrollBarComponent";

static const Vector3f FWD( 0.0f, 0.0f, -1.0f );
static const Vector3f RIGHT( 1.0f, 0.0f, 0.0f );
static const Vector3f DOWN( 0.0f, -1.0f, 0.0f );

static const float BASE_THUMB_WIDTH 		= 4.0f;
static const float THUMB_FROM_BASE_OFFSET 	= 0.001f;
static const float THUMB_SHRINK_FACTOR 		= 0.5f;

//==============================
// OvrScrollBarComponent::OvrScrollBarComponent
OvrScrollBarComponent::OvrScrollBarComponent( const VRMenuId_t rootId, const VRMenuId_t baseId,
	const VRMenuId_t thumbId, const int startElementIndex, const int numElements )
		: VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FRAME_UPDATE ) )
		, Fader( 0.0f )
		, FadeInRate( 1.0f / 0.2f )
		, FadeOutRate( 1.0f / 0.5f )
		, NumOfItems( 0 )
		, ScrollBarBaseWidth( 0.0f )
		, ScrollBarBaseHeight( 0.0f )
		, ScrollBarCurrentbaseLength( 0.0f )
		, ScrollBarThumbWidth( 0.0f )
		, ScrollBarThumbHeight( 0.0f )
		, ScrollBarCurrentThumbLength( 0.0f )
		, ScrollRootId( rootId )
		, ScrollBarBaseId( baseId )
		, ScrollBarThumbId( thumbId )
		, CurrentScrollState( SCROLL_STATE_HIDDEN )
		, IsVertical( false )
{
}

void OvrScrollBarComponent::SetScrollFrac( OvrVRMenuMgr & menuMgr, VRMenuObject * self, const float pos )
{
	if ( NumOfItems <= 0 )
		return;

	// Updating thumb length when pulled beyond its bounds
	const float scrollEnd 			= (float)( NumOfItems - 1 );
	const float outOfBoundsOffset 	= 0.5f;
	const float minThumbLength		= ScrollBarCurrentThumbLength * THUMB_SHRINK_FACTOR;
	const float maxThumbLength		= ScrollBarCurrentThumbLength;
	const float minThumbOffset		= 0.0f;
	const float maxThumbOffset		= ( maxThumbLength - minThumbLength ) * 0.5f;
	float currentThumbLength 		= ScrollBarCurrentThumbLength;
	float thumbPosOffset 			= 0.0f;

	if ( pos < 0.0f )
	{
		currentThumbLength = LinearRangeMapFloat( pos, -outOfBoundsOffset, 0.0f, minThumbLength, maxThumbLength );
		thumbPosOffset = LinearRangeMapFloat( pos, -outOfBoundsOffset, 0.0f, -maxThumbOffset, minThumbOffset );
	}
	else if ( pos > scrollEnd )
	{
		currentThumbLength = LinearRangeMapFloat( pos, scrollEnd + outOfBoundsOffset, scrollEnd, minThumbLength, maxThumbLength );
		thumbPosOffset = LinearRangeMapFloat( pos, scrollEnd + outOfBoundsOffset, scrollEnd, maxThumbOffset, minThumbOffset );
	}

	float thumbWidth = ScrollBarThumbWidth;
	float thumbHeight = ScrollBarThumbHeight;

	if ( IsVertical )
	{
		thumbHeight = currentThumbLength;
	}
	else
	{
		thumbWidth = currentThumbLength;
	}

	VRMenuObject * thumb = menuMgr.ToObject( self->ChildHandleForId( menuMgr, ScrollBarThumbId ) );
	if ( thumb != NULL && NumOfItems > 0 )
	{
		thumb->SetSurfaceDims( 0, Vector2f( thumbWidth, thumbHeight ) );
		thumb->RegenerateSurfaceGeometry( 0, false );
	}

	// Updating thumb position
	float clampedPos = Alg::Clamp( pos, 0.0f, (float)(NumOfItems - 1) );
	float thumbPos = LinearRangeMapFloat( clampedPos, 0.0f, (float)(NumOfItems - 1), 0.0f, ( ScrollBarCurrentbaseLength - currentThumbLength ) );
	thumbPos -= ( ScrollBarCurrentbaseLength - currentThumbLength ) * 0.5f;
	thumbPos += thumbPosOffset;
	thumbPos *= VRMenuObject::DEFAULT_TEXEL_SCALE;

	if ( thumb != NULL  )
	{
		Vector3f direction = RIGHT;
		if ( IsVertical )
		{
			direction = DOWN;
		}
		thumb->SetLocalPosition( ( direction * thumbPos ) - ( FWD * THUMB_FROM_BASE_OFFSET ) );
	}
}

void OvrScrollBarComponent::UpdateScrollBar( OvrVRMenuMgr & menuMgr, VRMenuObject * self, const int numElements )
{
	NumOfItems = numElements;

	if ( IsVertical )
	{
		ScrollBarCurrentbaseLength = ScrollBarBaseHeight;
	}
	else
	{
		ScrollBarCurrentbaseLength = ScrollBarBaseWidth;
	}

	ScrollBarCurrentThumbLength = ScrollBarCurrentbaseLength / NumOfItems;
	ScrollBarCurrentThumbLength =  ( ScrollBarCurrentThumbLength < BASE_THUMB_WIDTH ) ? BASE_THUMB_WIDTH : ScrollBarCurrentThumbLength;

	if ( IsVertical )
	{
		ScrollBarThumbHeight = ScrollBarCurrentThumbLength;
	}
	else
	{
		ScrollBarThumbWidth = ScrollBarCurrentThumbLength;
	}

	VRMenuObject * thumb = menuMgr.ToObject( self->ChildHandleForId( menuMgr, ScrollBarThumbId ) );
	if ( thumb != NULL && NumOfItems > 0 )
	{
		thumb->SetSurfaceDims( 0, Vector2f( ScrollBarThumbWidth, ScrollBarThumbHeight ) );
		thumb->RegenerateSurfaceGeometry( 0, false );
	}

	VRMenuObject * base = menuMgr.ToObject( self->ChildHandleForId( menuMgr, ScrollBarBaseId ) );
	if ( thumb != NULL )
	{
		base->SetSurfaceDims( 0, Vector2f( ScrollBarBaseWidth, ScrollBarBaseHeight ) );
		base->RegenerateSurfaceGeometry( 0, false );
	}
}

void OvrScrollBarComponent::SetBaseColor( OvrVRMenuMgr & menuMgr, VRMenuObject * self, const Vector4f & color )
{
	// Set alpha on the base - move this to somewhere more explicit if needed
	VRMenuObject * base = menuMgr.ToObject( self->ChildHandleForId( menuMgr, ScrollBarBaseId ) );
	if ( base != NULL )
	{
		base->SetSurfaceColor( 0, color );
	}
}

enum eScrollBarImage
{
	SCROLLBAR_IMAGE_BASE,
	SCROLLBAR_IMAGE_THUMB,
	SCROLLBAR_IMAGE_MAX
};

String GetImage( eScrollBarImage const type, const bool vertical )
{
	static char const * images[ SCROLLBAR_IMAGE_MAX ] =
	{
		"res/raw/scrollbar_base_%s.png",
		"res/raw/scrollbar_thumb_%s.png",
	};

	char buff[ 256 ];
	OVR_sprintf( buff, sizeof( buff ), images[ type ], vertical ? "vert" : "horz" );
	return String( buff );
}

void OvrScrollBarComponent::GetScrollBarParms( VRMenu & menu, float scrollBarLength, const VRMenuId_t parentId, const VRMenuId_t rootId, const VRMenuId_t xformId,
	const VRMenuId_t baseId, const VRMenuId_t thumbId, const Posef & rootLocalPose, const Posef & xformPose, const int startElementIndex, 
	const int numElements, const bool verticalBar, const Vector4f & thumbBorder, Array< const VRMenuObjectParms* > & parms )
{
	// Build up the scrollbar parms
	OvrScrollBarComponent * scrollComponent = new OvrScrollBarComponent( rootId, baseId, thumbId, startElementIndex, numElements );
	scrollComponent->SetIsVertical( verticalBar );

	// parms for the root object that holds all the scrollbar components
	{
		Array< VRMenuComponent* > comps;
		comps.PushBack( scrollComponent );
		Array< VRMenuSurfaceParms > surfParms;
		char const * text = "scrollBarRoot";
		Vector3f scale( 1.0f );
		Posef pose( rootLocalPose );
		Posef textPose( Quatf(), Vector3f( 0.0f ) );
		Vector3f textScale( 1.0f );
		VRMenuFontParms fontParms;
		VRMenuObjectFlags_t objectFlags( VRMENUOBJECT_DONT_HIT_ALL );
		objectFlags |= VRMENUOBJECT_DONT_RENDER_TEXT;
		VRMenuObjectInitFlags_t initFlags( VRMENUOBJECT_INIT_FORCE_POSITION  );
		VRMenuObjectParms * itemParms = new VRMenuObjectParms( VRMENU_CONTAINER, comps,
			surfParms, text, pose, scale, textPose, textScale, fontParms, rootId,
			objectFlags, initFlags );
		itemParms->ParentId = parentId;
		parms.PushBack( itemParms );
	}

	// add parms for the object that serves as a transform 
	{
		Array< VRMenuComponent* > comps;
		Array< VRMenuSurfaceParms > surfParms;
		char const * text = "scrollBarTransform";
		Vector3f scale( 1.0f );
		Posef pose( xformPose );
		Posef textPose( Quatf(), Vector3f( 0.0f ) );
		Vector3f textScale( 1.0f );
		VRMenuFontParms fontParms;
		VRMenuObjectFlags_t objectFlags( VRMENUOBJECT_DONT_HIT_ALL );
		objectFlags |= VRMENUOBJECT_DONT_RENDER_TEXT;
		VRMenuObjectInitFlags_t initFlags( VRMENUOBJECT_INIT_FORCE_POSITION );
		VRMenuObjectParms * itemParms = new VRMenuObjectParms( VRMENU_CONTAINER, comps,
			surfParms, text, pose, scale, textPose, textScale, fontParms, xformId,
			objectFlags, initFlags );
		itemParms->ParentId = rootId;
		parms.PushBack( itemParms );
	}

	// add parms for the base image that underlays the whole scrollbar
	{
		int sbWidth, sbHeight = 0;
		GLuint sbTexture = OVR::LoadTextureFromApplicationPackage( GetImage( SCROLLBAR_IMAGE_BASE, verticalBar ), TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), sbWidth, sbHeight );
		if ( verticalBar )
		{
			scrollComponent->SetScrollBarBaseWidth( (float)( sbWidth ) );
			scrollComponent->SetScrollBarBaseHeight( scrollBarLength );
		}
		else
		{
			scrollComponent->SetScrollBarBaseWidth( scrollBarLength );
			scrollComponent->SetScrollBarBaseHeight( (float)( sbHeight ) );
		}

		Array< VRMenuComponent* > comps;
		Array< VRMenuSurfaceParms > surfParms;
		char const * text = "scrollBase";
		VRMenuSurfaceParms baseParms( text,
				sbTexture, sbWidth, sbHeight, SURFACE_TEXTURE_DIFFUSE,
				0, 0, 0, SURFACE_TEXTURE_MAX,
				0, 0, 0, SURFACE_TEXTURE_MAX );
		surfParms.PushBack( baseParms );
		Vector3f scale( 1.0f );
		Posef pose( Quatf(), Vector3f( 0.0f ) );
		Posef textPose( Quatf(), Vector3f( 0.0f ) );
		Vector3f textScale( 1.0f );
		VRMenuFontParms fontParms;
		VRMenuObjectFlags_t objectFlags( VRMENUOBJECT_DONT_HIT_ALL );
		objectFlags |= VRMENUOBJECT_DONT_RENDER_TEXT;
		VRMenuObjectInitFlags_t initFlags( VRMENUOBJECT_INIT_FORCE_POSITION  );
		VRMenuObjectParms * itemParms = new VRMenuObjectParms( VRMENU_BUTTON, comps,
			surfParms, text, pose, scale, textPose, textScale, fontParms, baseId,
			objectFlags, initFlags );
		itemParms->ParentId = xformId;
		parms.PushBack( itemParms );
	}

	// add parms for the thumb image of the scrollbar
	{
		int stWidth, stHeight = 0;
		GLuint stTexture = OVR::LoadTextureFromApplicationPackage( GetImage( SCROLLBAR_IMAGE_THUMB, verticalBar ), TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), stWidth, stHeight );
		scrollComponent->SetScrollBarThumbWidth(  (float)( stWidth ) );
		scrollComponent->SetScrollBarThumbHeight( (float)( stHeight ) );

		Array< VRMenuComponent* > comps;
		Array< VRMenuSurfaceParms > surfParms;
		char const * text = "scrollThumb";
		VRMenuSurfaceParms thumbParms( text,
				stTexture, stWidth, stHeight, SURFACE_TEXTURE_DIFFUSE,
				0, 0, 0, SURFACE_TEXTURE_MAX,
				0, 0, 0, SURFACE_TEXTURE_MAX );
		//thumbParms.Border = thumbBorder;
		surfParms.PushBack( thumbParms );
		Vector3f scale( 1.0f );
		Posef pose( Quatf(), -FWD * THUMB_FROM_BASE_OFFSET );
		// Since we use left aligned anchors on the base and thumb, we offset the root once to center the scrollbar
		Posef textPose( Quatf(), Vector3f( 0.0f ) );
		Vector3f textScale( 1.0f );
		VRMenuFontParms fontParms;
		VRMenuObjectFlags_t objectFlags( VRMENUOBJECT_DONT_HIT_ALL );
		objectFlags |= VRMENUOBJECT_DONT_RENDER_TEXT;
		objectFlags |= VRMENUOBJECT_FLAG_POLYGON_OFFSET;
		VRMenuObjectInitFlags_t initFlags( VRMENUOBJECT_INIT_FORCE_POSITION  );
		VRMenuObjectParms * itemParms = new VRMenuObjectParms( VRMENU_BUTTON, comps,
			surfParms, text, pose, scale, textPose, textScale, fontParms, thumbId,
			objectFlags, initFlags );
		itemParms->ParentId = xformId;
		parms.PushBack( itemParms );
	}
}

//==============================
// OvrScrollBarComponent::OnEvent_Impl
eMsgStatus OvrScrollBarComponent::OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
		VRMenuObject * self, VRMenuEvent const & event )
{
	switch ( event.EventType )
	{
		case VRMENU_EVENT_FRAME_UPDATE:
			return OnFrameUpdate( app, vrFrame, menuMgr, self, event );
		default:
			OVR_ASSERT( false );
			return MSG_STATUS_ALIVE;
	}
    return MSG_STATUS_CONSUMED;
}

const char * stateNames [ ] =
{
	"SCROLL_STATE_NONE",
	"SCROLL_STATE_FADE_IN",
	"SCROLL_STATE_VISIBLE",
	"SCROLL_STATE_FADE_OUT",
	"SCROLL_STATE_HIDDEN",
	"NUM_SCROLL_STATES"
};

const char* StateString( const OvrScrollBarComponent::eScrollBarState state )
{
	OVR_ASSERT( state >= 0 && state < OvrScrollBarComponent::NUM_SCROLL_STATES );
	return stateNames[ state ];
}

//==============================
// OvrScrollBarComponent::SetScrollState
void OvrScrollBarComponent::SetScrollState( VRMenuObject * self, const eScrollBarState state )
{
	eScrollBarState lastState = CurrentScrollState;
	CurrentScrollState = state;
	if ( CurrentScrollState == lastState )
	{
		return;
	}
	
	switch ( CurrentScrollState )
	{
	case SCROLL_STATE_NONE:
		break;
	case SCROLL_STATE_FADE_IN:
		if ( lastState == SCROLL_STATE_HIDDEN || lastState == SCROLL_STATE_FADE_OUT )
		{
			LOG( "%s to %s", StateString( lastState ), StateString( CurrentScrollState ) );
			Fader.StartFadeIn();
		}
		break;
	case SCROLL_STATE_VISIBLE:
		self->SetVisible( true );
		break;
	case SCROLL_STATE_FADE_OUT:
		if ( lastState == SCROLL_STATE_VISIBLE || lastState == SCROLL_STATE_FADE_IN )
		{
			LOG( "%s to %s", StateString( lastState ), StateString( CurrentScrollState ) );
			Fader.StartFadeOut();
		}
		break;
	case SCROLL_STATE_HIDDEN:
		self->SetVisible( false );
		break;
	default:
		OVR_ASSERT( false );
		break;
	}
}

//==============================
// OvrScrollBarComponent::OnFrameUpdate
eMsgStatus OvrScrollBarComponent::OnFrameUpdate( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
		VRMenuObject * self, VRMenuEvent const & event )
{
	OVR_ASSERT( self != NULL );
	if ( Fader.GetFadeState() != Fader::FADE_NONE )
	{
		const float fadeRate = ( Fader.GetFadeState() == Fader::FADE_IN ) ? FadeInRate : FadeOutRate;
		Fader.Update( fadeRate, vrFrame.DeltaSeconds );
		const float CurrentFadeLevel = Fader.GetFinalAlpha();
		self->SetColor( Vector4f( 1.0f, 1.0f, 1.0f, CurrentFadeLevel ) );
	}
	else if ( Fader.GetFadeAlpha() == 1.0f )
	{
		SetScrollState( self, SCROLL_STATE_VISIBLE );
	}
	else if ( Fader.GetFadeAlpha() == 0.0f )
	{
		SetScrollState( self, SCROLL_STATE_HIDDEN );
	}

	return MSG_STATUS_ALIVE;
}

} // namespace OVR

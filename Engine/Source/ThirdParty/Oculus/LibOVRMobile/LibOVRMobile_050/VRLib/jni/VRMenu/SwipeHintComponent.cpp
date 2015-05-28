/************************************************************************************

Filename    :   SwipeHintComponent.cpp
Content     :
Created     :   Feb 12, 2015
Authors     :   Madhu Kalva, Jim Dose

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.

*************************************************************************************/

#include "SwipeHintComponent.h"
#include "VRMenuMgr.h"
#include "VRMenu.h"
#include "PackageFiles.h"
#include "../App.h"

namespace OVR
{
	const char * OvrSwipeHintComponent::TYPE_NAME = "OvrSwipeHintComponent";
	bool OvrSwipeHintComponent::ShowSwipeHints = true;

	//==============================
	//  OvrSwipeHintComponent::OvrSwipeHintComponent()
	OvrSwipeHintComponent::OvrSwipeHintComponent( const bool isRightSwipe, const float totalTime, const float timeOffset, const float delay )
	: VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FRAME_UPDATE ) | VRMENU_EVENT_OPENING )
	, IsRightSwipe( isRightSwipe )
	, TotalTime( totalTime )
	, TimeOffset( timeOffset )
	, Delay( delay )
	, StartTime( 0 )
	, ShouldShow( false )
	, IgnoreDelay( false )
	, TotalAlpha()
	{
	}

	//=======================================================
	//  OvrSwipeHintComponent::CreateSwipeSuggestionIndicator
	menuHandle_t OvrSwipeHintComponent::CreateSwipeSuggestionIndicator( App * appPtr, VRMenu * rootMenu, const menuHandle_t rootHandle, const int menuId, const char * img, const Posef pose, const Vector3f direction )
	{
		const int NumSwipeTrails = 3;
		int imgWidth, imgHeight;
		OvrVRMenuMgr & menuManager = appPtr->GetVRMenuMgr();
		VRMenuFontParms fontParms( HORIZONTAL_CENTER, VERTICAL_CENTER, false, false, false, 1.0f );

		// Create Menu item for scroll hint root
		VRMenuId_t swipeHintId( menuId );
		Array< VRMenuObjectParms const * > parms;
		VRMenuObjectParms parm( VRMENU_CONTAINER, Array< VRMenuComponent* >(), VRMenuSurfaceParms(),
								"swipe hint root", pose, Vector3f( 1.0f ), VRMenuFontParms(),
								swipeHintId, VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ),
								VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
		parms.PushBack( &parm );
		rootMenu->AddItems( menuManager, appPtr->GetDefaultFont(), parms, rootHandle, false );
		parms.Clear();

		menuHandle_t scrollHintHandle = rootMenu->HandleForId( menuManager, swipeHintId );
		OVR_ASSERT( scrollHintHandle.IsValid() );
		GLuint swipeHintTexture = LoadTextureFromApplicationPackage( img, TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), imgWidth, imgHeight );
		VRMenuSurfaceParms swipeHintSurfParms( "", swipeHintTexture, imgWidth, imgHeight, SURFACE_TEXTURE_DIFFUSE,
												0, 0, 0, SURFACE_TEXTURE_MAX,
												0, 0, 0, SURFACE_TEXTURE_MAX );

		Posef swipePose = pose;
		for( int i = 0; i < NumSwipeTrails; i++ )
		{
			swipePose.Position.y = ( imgHeight * ( i + 2 ) ) * 0.5f * direction.y * VRMenuObject::DEFAULT_TEXEL_SCALE;
			swipePose.Position.z = 0.01f * ( float )i;

			Array< VRMenuComponent* > hintArrowComps;
			OvrSwipeHintComponent* hintArrowComp = new OvrSwipeHintComponent(false, 1.3333f, 0.4f + (float)i * 0.13333f, 5.0f);
			hintArrowComps.PushBack( hintArrowComp );
			hintArrowComp->Show( ovr_GetTimeInSeconds() );

			VRMenuObjectParms * swipeIconLeftParms = new VRMenuObjectParms( VRMENU_STATIC, hintArrowComps,
				swipeHintSurfParms, "", swipePose, Vector3f( 1.0f ), fontParms, VRMenuId_t(),
				VRMenuObjectFlags_t( VRMENUOBJECT_FLAG_NO_DEPTH ) | VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ),
				VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
			parms.PushBack( swipeIconLeftParms );
		}

		rootMenu->AddItems( menuManager, appPtr->GetDefaultFont(), parms, scrollHintHandle, false );
		DeletePointerArray( parms );
		parms.Clear();

		return scrollHintHandle;
	}

	//==============================
	//  OvrSwipeHintComponent::Reset
	void OvrSwipeHintComponent::Reset( VRMenuObject * self )
	{
		IgnoreDelay 		= true;
		ShouldShow 			= false;
		const double now 	= ovr_GetTimeInSeconds();
		TotalAlpha.Set( now, TotalAlpha.Value( now ), now, 0.0f );
		self->SetColor( Vector4f( 1.0f, 1.0f, 1.0f, 0.0f ) );
	}

	//==============================
	//  OvrSwipeHintComponent::Show
	void OvrSwipeHintComponent::Show( const double now )
	{
		if ( !ShouldShow )
		{
			ShouldShow 	= true;
			StartTime 	= now + TimeOffset + ( IgnoreDelay ? 0.0f : Delay );
			TotalAlpha.Set( now, TotalAlpha.Value( now ), now + 0.5f, 1.0f );
		}
	}

	//==============================
	//  OvrSwipeHintComponent::Hide
	void OvrSwipeHintComponent::Hide( const double now )
	{
		if ( ShouldShow )
		{
			TotalAlpha.Set( now, TotalAlpha.Value( now ), now + 0.5f, 0.0f );
			ShouldShow = false;
		}
	}

	//==============================
	//  OvrSwipeHintComponent::OnEvent_Impl
	eMsgStatus OvrSwipeHintComponent::OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
													VRMenuObject * self, VRMenuEvent const & event )
	{
		switch ( event.EventType )
		{
		case VRMENU_EVENT_OPENING:
			return Opening( app, vrFrame, menuMgr, self, event );
		case VRMENU_EVENT_FRAME_UPDATE:
			return Frame( app, vrFrame, menuMgr, self, event );
		default:
			OVR_ASSERT( !"Event flags mismatch!" );
			return MSG_STATUS_ALIVE;
		}
	}

	//==============================
	//  OvrSwipeHintComponent::Opening
	eMsgStatus OvrSwipeHintComponent::Opening( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
	{
		Reset( self );
		return MSG_STATUS_ALIVE;
	}

	//==============================
	//  OvrSwipeHintComponent::Frame
	eMsgStatus OvrSwipeHintComponent::Frame( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
	{
		if ( ShowSwipeHints /* && Carousel->HasSelection() && CanSwipe() */ )
		{
			Show( vrFrame.PoseState.TimeInSeconds );
		}
		else
		{
			Hide( vrFrame.PoseState.TimeInSeconds );
		}

		IgnoreDelay = false;

		float alpha = TotalAlpha.Value( vrFrame.PoseState.TimeInSeconds );
		if ( alpha > 0.0f )
		{
			double time = vrFrame.PoseState.TimeInSeconds - StartTime;
			if ( time < 0.0f )
			{
				alpha = 0.0f;
			}
			else
			{
				float normTime = time / TotalTime;
				alpha *= sin( M_PI * 2.0f * normTime );
				alpha = OVR::Alg::Max( alpha, 0.0f );
			}
		}

		self->SetColor( Vector4f( 1.0f, 1.0f, 1.0f, alpha ) );

		return MSG_STATUS_ALIVE;
	}
}

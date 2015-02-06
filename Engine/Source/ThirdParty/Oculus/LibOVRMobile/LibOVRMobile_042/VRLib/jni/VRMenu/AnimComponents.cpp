 /************************************************************************************

Filename    :   SurfaceAnim_Component.cpp
Content     :   A reusable component for animating VR menu object surfaces.
Created     :   Sept 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.


*************************************************************************************/

#include "AnimComponents.h"

#include "VRMenuObject.h"
#include "../VrApi/VrApi.h"
#include "VRMenuMgr.h"

namespace OVR {

//================================
// OvrAnimComponent::OvrAnimComponent
OvrAnimComponent::OvrAnimComponent( float const framesPerSecond, bool const looping ) :
	VRMenuComponent( VRMENU_EVENT_FRAME_UPDATE ),
	BaseTime( 0.0 ),
	BaseFrame( 0 ),
	CurFrame( 0 ),
	FramesPerSecond( framesPerSecond ),
	AnimState( ANIMSTATE_PAUSED ),
	Looping( looping ),
	ForceVisibilityUpdate( false ),
	FractionalFrame( 0.0f ),
	FloatFrame( 0.0 )
{
}

//================================
// OvrAnimComponent::Frame
eMsgStatus OvrAnimComponent::Frame( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
	VRMenuObject * self, VRMenuEvent const & event ) 
{
	// only recalculate the current frame if playing
	if ( AnimState == ANIMSTATE_PLAYING )
	{
		double timePassed = ovr_GetTimeInSeconds() - BaseTime;
		FloatFrame = timePassed * FramesPerSecond;
		int totalFrames = ( int )floor( FloatFrame );
		FractionalFrame = FloatFrame - totalFrames;
		int numFrames = GetNumFrames( self );
		int frame = BaseFrame + totalFrames;
		CurFrame = !Looping ? Alg::Clamp( frame, 0, numFrames - 1 ) : frame % numFrames;
		SetFrameVisibilities( app, vrFrame, menuMgr, self );
	} 
	else if ( ForceVisibilityUpdate )
	{
		SetFrameVisibilities( app, vrFrame, menuMgr, self );
		ForceVisibilityUpdate = false;
	}

	return MSG_STATUS_ALIVE;
}

//================================
// OvrAnimComponent::SetFrame
void OvrAnimComponent::SetFrame( VRMenuObject * self, int const frameNum )
{
	CurFrame = Alg::Clamp( frameNum, 0, GetNumFrames( self ) - 1 );
	// we must reset the base frame and the current time so that the frame calculation
	// remains correct if we're playing.  If we're not playing, this will cause the
	// next Play() to start from this frame.
	BaseFrame = frameNum;
	BaseTime = ovr_GetTimeInSeconds();
	ForceVisibilityUpdate = true;	// make sure visibilities are set next frame update
}

//================================
// OvrAnimComponent::Play
void OvrAnimComponent::Play()
{
	AnimState = ANIMSTATE_PLAYING;
	BaseTime = ovr_GetTimeInSeconds();
	// on a play we offset the base frame to the current frame so a resume from pause doesn't restart
	BaseFrame = CurFrame;
}

//================================
// OvrAnimComponent::Pause
void OvrAnimComponent::Pause()
{
	AnimState = ANIMSTATE_PAUSED;
}

eMsgStatus OvrAnimComponent::OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
{
	switch ( event.EventType )
	{
	case VRMENU_EVENT_FRAME_UPDATE:
		return Frame( app, vrFrame, menuMgr, self, event );
	default:
		OVR_ASSERT( !"Event flags mismatch!" ); // the constructor is specifying a flag that's not handled
		return MSG_STATUS_ALIVE;
	}
}

//==============================================================================================
// OvrSurfaceAnimComponent
//==============================================================================================

const char * OvrSurfaceAnimComponent::TYPE_NAME = "OvrSurfaceAnimComponent";

//================================
// OvrSurfaceAnimComponent::
OvrSurfaceAnimComponent::OvrSurfaceAnimComponent( float const framesPerSecond, bool const looping, int const surfacesPerFrame ) :
	OvrAnimComponent( framesPerSecond, looping ),
	SurfacesPerFrame( surfacesPerFrame )
{
}

//================================
// OvrSurfaceAnimComponent::SetFrameVisibilities
void OvrSurfaceAnimComponent::SetFrameVisibilities( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self ) const
{
	int minIndex = GetCurFrame() * SurfacesPerFrame;
	int maxIndex = ( GetCurFrame() + 1 ) * SurfacesPerFrame;
	for ( int i = 0; i < self->NumSurfaces(); ++i )
	{
		self->SetSurfaceVisible( i, i >= minIndex && i < maxIndex );
	}
}

//================================
// OvrSurfaceAnimComponent::NumFrames
int OvrSurfaceAnimComponent::GetNumFrames( VRMenuObject * self ) const
{
	return self->NumSurfaces() / SurfacesPerFrame;
}

//==============================================================
// OvrChildrenAnimComponent
//
const char * OvrTrailsAnimComponent::TYPE_NAME = "OvrChildrenAnimComponent";

OvrTrailsAnimComponent::OvrTrailsAnimComponent( float const framesPerSecond, bool const looping, 
	int const numFrames, int const numFramesAhead, int const numFramesBehind )
	: OvrAnimComponent( framesPerSecond, looping )
	, NumFrames( numFrames )
	, FramesAhead( numFramesAhead )
	, FramesBehind( numFramesBehind )
{

}

float OvrTrailsAnimComponent::GetAlphaForFrame( const int frame ) const
{
	const int currentFrame = GetCurFrame( );
	if ( frame == currentFrame )
		return	1.0f;

	const float alpha = GetFractionalFrame( );
	const float aheadFactor = 1.0f / FramesAhead;
	for ( int ahead = 1; ahead <= FramesAhead; ++ahead )
	{
		if ( frame == ( currentFrame + ahead ) )
		{
			return ( alpha * aheadFactor ) + ( aheadFactor * ( FramesAhead - ahead ) );
		}
	}

	const float invAlpha = 1.0f - alpha;
	const float behindFactor = 1.0f / FramesBehind;
	for ( int behind = 1; behind < FramesBehind; ++behind )
	{
		if ( frame == ( currentFrame - behind ) )
		{
			return ( invAlpha * behindFactor ) + ( behindFactor * ( FramesBehind - behind ) );
		}
	}

	return 0.0f;
}

void OvrTrailsAnimComponent::SetFrameVisibilities( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self ) const
{
//	LOG( "FracFrame: %f", GetFractionalFrame() );
	for ( int i = 0; i < self->NumChildren(); ++i )
	{
		menuHandle_t childHandle = self->GetChildHandleForIndex( i );
		if ( VRMenuObject * childObject = menuMgr.ToObject( childHandle ) )
		{
			Vector4f color = childObject->GetColor();
			color.w = GetAlphaForFrame( i );
			childObject->SetColor( color );
		}
	}
}

int OvrTrailsAnimComponent::GetNumFrames( VRMenuObject * self ) const
{
	return NumFrames;
}

} // namespace OVR
/************************************************************************************

Filename    :   OvrTextFade_Component.h
Content     :   A reusable component that fades text in and recenters it on gaze over.
Created     :   July 25, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_TextFade_Component_h )
#define OVR_TextFade_Component_h

#include "VRMenuComponent.h"
#include "Fader.h"

namespace OVR {

//==============================================================
// OvrTextFade_Component
class OvrTextFade_Component : public VRMenuComponent
{
public:
	static double const FADE_DELAY;
	static float const  FADE_DURATION;

	OvrTextFade_Component( Vector3f const & iconBaseOffset, Vector3f const & iconFadeOffset );

	virtual eMsgStatus      OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
										  VRMenuObject * self, VRMenuEvent const & event );

	eMsgStatus Frame( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event );

	eMsgStatus FocusGained( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event );

	eMsgStatus FocusLost( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event );

	static Vector3f CalcIconFadeOffset( char const * text, BitmapFont const & font, Vector3f const & axis, float const iconWidth );


private:
	SineFader   TextAlphaFader;
	double      StartFadeInTime;    // when focus is gained, this is set to the time when the fade in should begin
	double      StartFadeOutTime;   // when focus is lost, this is set to the time when the fade out should begin
	SineFader   IconOffsetFader;
	Vector3f    IconBaseOffset;     // base offset for text
	Vector3f    IconFadeOffset;     // text offset when fully faded
};

} // namespace OVR

#endif // OVR_TextFade_Component_h
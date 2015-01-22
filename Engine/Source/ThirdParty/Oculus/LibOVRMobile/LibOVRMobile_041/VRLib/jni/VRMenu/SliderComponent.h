/************************************************************************************

Filename    :   Slider_Component.h
Content     :   A reusable component implementing a slider bar.
Created     :   Sept 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_SliderComponent_h )
#define OVR_SliderComponent_h

#include "VRMenuComponent.h"
#include "Fader.h"

namespace OVR {

class VRMenu;

//==============================================================
// OvrSliderComponent
class OvrSliderComponent : public VRMenuComponent
{
public:
	static const char * TYPE_NAME;

	OvrSliderComponent( VRMenu & menu, float const sliderFrac, Vector3f const & localSlideDelta, 
			float const minValue, float const maxValue, float const sensitivity, VRMenuId_t const rootId,
			VRMenuId_t const scrubberId, VRMenuId_t const textId, VRMenuId_t bubbleId );

	static void			GetVerticalSliderParms( VRMenu & menu, VRMenuId_t const parentId, VRMenuId_t const rootId, 
							Posef const & rootLocalPose, VRMenuId_t const scrubberId, VRMenuId_t const bubbleId, 
							float const sliderFrac, Vector3f const & localSlideDelta,
							float const minValue, float const maxValue, float const sensitvityScale, 
							Array< VRMenuObjectParms const* > & parms );

	char const *		GetTypeName() const { return TYPE_NAME; }

	float				GetSliderFrac() const { return SliderFrac; }

private:
	virtual eMsgStatus  OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
								VRMenuObject * self, VRMenuEvent const & event );

	eMsgStatus			OnInit( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
								VRMenuObject * self, VRMenuEvent const & event );

	eMsgStatus			OnFrameUpdate( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
								VRMenuObject * self, VRMenuEvent const & event );
	eMsgStatus			OnTouchDown( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
								VRMenuObject * self, VRMenuEvent const & event );
	eMsgStatus			OnTouchUp( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
								VRMenuObject * self, VRMenuEvent const & event );
	eMsgStatus			OnTouchRelative( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
								VRMenuObject * self, VRMenuEvent const & event );

	void				GetStringValue( char * valueStr, int maxLen ) const;

private:
	void				SetCaretPoseFromFrac( OvrVRMenuMgr & menuMgr, VRMenuObject * self, float const frac );
	void				UpdateText( OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuId_t const id );

private:
	bool			TouchDown;			// true if 
	float			SliderFrac;			// the current position of the slider represented as a value between 0 and 1.0
	float			MinValue;			// the minumum value shown on the slider
	float			MaxValue;			// the maximum value shown on the slider
	float			SensitivityScale;	// additional multiplyer for sensitivity
	Vector3f		LocalSlideDelta;	// the total delta when the slider frac is at 1.0
	VRMenu &		Menu;				// the menu that will receive updates
	VRMenuId_t		RootId;				// the id of the slider's root object
	VRMenuId_t		ScrubberId;			// the caret is the object that slides up and down along the bar
	VRMenuId_t		BubbleId;			// the bubble is a child of the caret, but offset orthogonal to the bar it has text that displays the value of the slider bar
	VRMenuId_t		TextId;				// the text is an object that always displays the value of the slider, independent of the bubble
	Posef			CaretBasePose;		// the base location of the caret in it's parent's space
	SineFader		BubbleFader;		// the fader for the bubble's alpha
	double			BubbleFadeOutTime;	// time when the bubble fader should start fading out
};

} // namespace OVR

#endif // OVR_SliderComponent_h
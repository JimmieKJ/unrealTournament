/************************************************************************************

Filename    :   DefaultComponent.h
Content     :   A default menu component that handles basic actions most menu items need.
Created     :   July 25, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( Ovr_DefaultComponent_h )
#define Ovr_DefaultComponent_h

#include "VRMenuComponent.h"
#include "Fader.h"

namespace OVR {

//==============================================================
// OvrDefaultComponent
class OvrDefaultComponent : public VRMenuComponent
{
public:
	static const int TYPE_ID = 1;

    OvrDefaultComponent( Vector3f const & hilightOffset = Vector3f( 0.0f, 0.0f, 0.05f ), 
            float const hilightScale = 1.05f, 
            float const fadeDuration = 0.25f, 
            float const fadeDelay = 0.25f,
			Vector4f const & textNormalColor = Vector4f( 1.0f ), 
			Vector4f const & textHilightColor = Vector4f( 1.0f ) );

	virtual int		GetTypeId() const { return TYPE_ID; }

	void			SetSuppressText( bool const suppress ) { SuppressText = suppress; }

private:
    // private variables
    // We may actually want these to be static...
    SoundLimiter    GazeOverSoundLimiter;
    SoundLimiter    DownSoundLimiter;
    SoundLimiter    UpSoundLimiter;

    SineFader       HilightFader;
    double          StartFadeInTime;
    double          StartFadeOutTime;
    Vector3f        HilightOffset;
    float           HilightScale;
    float           FadeDuration;
    float           FadeDelay;
	Vector4f		TextNormalColor;
	Vector4f		TextHilightColor;
	bool			SuppressText;	// true if text should not be faded in

private:
    virtual eMsgStatus      OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
                                    VRMenuObject * self, VRMenuEvent const & event );

    eMsgStatus              Frame( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
                                    VRMenuObject * self, VRMenuEvent const & event );
    eMsgStatus              FocusGained( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
                                    VRMenuObject * self, VRMenuEvent const & event );
    eMsgStatus              FocusLost( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
                                    VRMenuObject * self, VRMenuEvent const & event );
};

//==============================================================
// OvrSurfaceToggleComponent
// Toggles surfaced based on pair index, the current is default state, +1 is hover
class OvrSurfaceToggleComponent : public VRMenuComponent
{
public:
	static const char *		TYPE_NAME;
	OvrSurfaceToggleComponent()
		: VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FRAME_UPDATE ) )
		, GroupIndex( 0 )
	{}

	void SetGroupIndex( const int index )	{ GroupIndex = index; }
	int GetGroupIndex() const				{ return GroupIndex;  }

	virtual const char *	GetTypeName( ) const { return TYPE_NAME; }

private:
	virtual eMsgStatus      OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
		VRMenuObject * self, VRMenuEvent const & event );

	eMsgStatus      Frame( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
		VRMenuObject * self, VRMenuEvent const & event );

	int GroupIndex;
};

} // namespace OVR

#endif // Ovr_Default_Component
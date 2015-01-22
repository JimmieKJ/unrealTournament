/************************************************************************************

Filename    :   SurfaceAnim_Component.h
Content     :   A reusable component for animating VR menu object surfaces.
Created     :   Sept 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_AnimComponents_h )
#define OVR_AnimComponents_h

#include "VRMenuComponent.h"

namespace OVR {

//==============================================================
// OvrAnimComponent
//
// A component that contains the logic for animating a 
class OvrAnimComponent : public VRMenuComponent
{
public:
	enum eAnimState
	{
		ANIMSTATE_PAUSED,
		ANIMSTATE_PLAYING
	};

	OvrAnimComponent( float const framesPerSecond, bool const looping );
	virtual ~OvrAnimComponent() { }

	virtual eMsgStatus      OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
										  VRMenuObject * self, VRMenuEvent const & event );

	eMsgStatus Frame( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event );

	void			SetFrame( VRMenuObject * self, int const frameNum );
	void			Play();
	void			Pause();
	void			SetLooping( bool const loop ) { Looping = loop; }
	bool			GetLooping() const { return Looping; }
	void			SetRate( float const fps ) { FramesPerSecond = fps; }
	float			GetRate() const { return FramesPerSecond; }
	int				GetCurFrame() const { return CurFrame; }

protected:
	virtual void	SetFrameVisibilities( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self ) const = 0;
	virtual int		GetNumFrames( VRMenuObject * self ) const = 0;

	double			GetBaseTime() const { return BaseTime;  }
	double			GetFloatFrame() const { return FractionalFrame; }
	float			GetFractionalFrame() const { return FractionalFrame; }

private:
	double			BaseTime;			// time when animation started or resumed
	int				BaseFrame;			// base frame animation started on (for unpausing, this can be > 0 )
	int				CurFrame;			// current frame the animation is on
	float			FramesPerSecond;	// the playback rate of the animatoin
	eAnimState		AnimState;			// the current animation state
	bool			Looping;			// true if the animation should loop
	bool			ForceVisibilityUpdate;	// set to force visibilities to update even when paused (used by SetFrame )
	float			FractionalFrame;	// 0-1
	double			FloatFrame;			// Animation floating point time
};

//==============================================================
// OvrSurfaceAnimComponent
//
class OvrSurfaceAnimComponent : public OvrAnimComponent
{
public:
	static const char *		TYPE_NAME;

	// Surfaces per frame must be set to the number of surfaces that should be renderered
	// for each frame. If only a single surface is shown per frame, surfacesPerFrame should be 1.
	// If multiple surfaces (for instance one diffuse and one additive) are shown then
	// surfacesPerFrame should be set to 2.
	// Frame numbers never
	OvrSurfaceAnimComponent( float const framesPerSecond, bool const looping, int const surfacesPerFrame );

	char const *	GetTypeName() const { return TYPE_NAME; }

protected:
	virtual void	SetFrameVisibilities( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self ) const;
	virtual int		GetNumFrames( VRMenuObject * self ) const;

private:
	int				SurfacesPerFrame;	// how many surfaces are shown per frame
};


//==============================================================
// OvrChildrenAnimComponent
//
class OvrTrailsAnimComponent : public OvrAnimComponent
{
public:
	static const char *		TYPE_NAME;

	OvrTrailsAnimComponent( float const framesPerSecond, bool const looping, int const numFrames,
		int const numFramesAhead, int const numFramesBehind );

	virtual const char *	GetTypeName( ) const
	{
		return TYPE_NAME;
	}

protected:
	virtual void	SetFrameVisibilities( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self ) const;
	virtual int		GetNumFrames( VRMenuObject * self ) const;

private:
	float			GetAlphaForFrame( const int frame ) const;
	int				NumFrames;	
	int				FramesAhead;
	int				FramesBehind;
};

} // namespace OVR

#endif // OVR_AnimComponents_h
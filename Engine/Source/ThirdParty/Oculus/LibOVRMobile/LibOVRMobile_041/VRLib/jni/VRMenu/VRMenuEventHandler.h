/************************************************************************************

Filename    :   VRMenuFrame.h
Content     :   Menu component for handling hit tests and dispatching events.
Created     :   June 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_VRMenuFrame_h )
#define OVR_VRMenuFrame_h

#include "VRMenuObject.h"
#include "VRMenuEvent.h"
#include "../GazeCursor.h"
#include "SoundLimiter.h"

namespace OVR {

struct VrFrame;
class App;

//==============================================================
// VRMenuEventHandler
class VRMenuEventHandler
{
public:
	VRMenuEventHandler();
	~VRMenuEventHandler();

	void			Frame( App * app, const VrFrame & vrFrame, OvrVRMenuMgr & menuMgr, BitmapFont const & font, 
                            menuHandle_t const & rootHandle, Posef const & menuPose, 
                            gazeCursorUserId_t const & gazeUserId, Array< VRMenuEvent > & events );

	void			HandleEvents( App * app, const VrFrame & vrFrame, OvrVRMenuMgr & menuMgr, 
                            menuHandle_t const rootHandle, Array< VRMenuEvent > const & events ) const;

	void			InitComponents( Array< VRMenuEvent > & events );
	void			Opening( Array< VRMenuEvent > & events );
	void			Opened( Array< VRMenuEvent > & events );
	void			Closing( Array< VRMenuEvent > & events );
	void			Closed( Array< VRMenuEvent > & events );

	menuHandle_t	GetFocusedHandle() const { return FocusedHandle; }

private:
	menuHandle_t	FocusedHandle;

	SoundLimiter	GazeOverSoundLimiter;
	SoundLimiter	DownSoundLimiter;
	SoundLimiter	UpSoundLimiter;

private:
    bool            DispatchToComponents( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
                            VRMenuEvent const & event, VRMenuObject * receiver ) const;
    bool            DispatchToPath( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
                            VRMenuEvent const & event, Array< menuHandle_t > const & path, bool const log ) const;
	bool            BroadcastEvent( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
                            VRMenuEvent const & event, VRMenuObject * receiver ) const;
};

} // namespace OVR

#endif // OVR_VRMenuFrame_h
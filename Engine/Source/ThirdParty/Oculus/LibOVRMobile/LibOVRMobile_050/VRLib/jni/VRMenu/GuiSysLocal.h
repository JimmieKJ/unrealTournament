/************************************************************************************

Filename    :   GuiSysLocal.h
Content     :   The main menu that appears in native apps when pressing the HMT button.
Created     :   July 22, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_OvrGuiSysLocal_h )
#define OVR_OvrGuiSysLocal_h

#include "VRMenu.h"
#include "GuiSys.h"

namespace OVR {

//==============================================================
// OvrGuiSysLocal
class OvrGuiSysLocal : public OvrGuiSys
{
public:
							OvrGuiSysLocal();
	virtual					~OvrGuiSysLocal();

	virtual void			Init( App * app, OvrVRMenuMgr & menuMgr, BitmapFont const & font );
	virtual void			Shutdown( OvrVRMenuMgr & menuMgr );
	virtual void			Frame( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, 
                                    BitmapFont const & font, BitmapFontSurface & fontSurface,
									Matrix4f const & viewMatrix );
	virtual bool			OnKeyEvent( App * app, int const keyCode, KeyState::eKeyEventType const eventType );
	virtual void			ResetMenuOrientations( App * app );

    virtual void            AddMenu( VRMenu * menu );
	virtual VRMenu *		GetMenu( char const * menuName ) const;
	virtual void			DestroyMenu( OvrVRMenuMgr & menuMgr, VRMenu * menu );
    virtual void			OpenMenu( App * app, OvrGazeCursor & gazeCursor, char const * menuName );
	virtual void			CloseMenu( App * app, char const * menuName, bool const closeInstantly );
	virtual void			CloseMenu( App * app, VRMenu * menu, bool const closeInstantly );
	virtual bool			IsMenuActive( char const * menuName ) const;
	virtual bool			IsAnyMenuActive() const;
	virtual bool			IsAnyMenuOpen() const;

	gazeCursorUserId_t		GetGazeUserId() const { return GazeUserId; }

private:
	Array< VRMenu* >	    Menus;
	Array< VRMenu* >		ActiveMenus;

    bool					IsInitialized;

    gazeCursorUserId_t		GazeUserId;			// user id for the gaze cursor

private:
    int                     FindMenuIndex( char const * menuName ) const;
	int						FindMenuIndex( VRMenu const * menu ) const;
	int						FindActiveMenuIndex( VRMenu const * menu ) const;
	int						FindActiveMenuIndex( char const * menuName ) const;
	void					MakeActive( VRMenu * menu );
	void					MakeInactive( VRMenu * menu );

    Array< VRMenuComponent* > GetDefaultComponents();
};

} // namespace OVR

#endif // OVR_OvrGuiSysLocal_h

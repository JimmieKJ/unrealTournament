/************************************************************************************

Filename    :   VRMenu.h
Content     :   Class that implements the basic framework for a VR menu, holds all the
				components for a single menu, and updates the VR menu event handler to
				process menu events for a single menu.
Created     :   June 30, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( OVR_VRMenu_h )
#define OVR_VRMenu_h

#include "Android/LogUtils.h"

#include "VRMenuObject.h"
#include "SoundLimiter.h"
#include "../GazeCursor.h"
#include "../KeyState.h"

namespace OVR {

class App;
struct VrFrame;
class BitmapFont;
class BitmapFontSurface;
class VRMenuEventHandler;

//==============================
// DeletePointerArray
// helper function for cleaning up an array of pointers
template< typename T > 
void DeletePointerArray( Array< T* > & a )
{
    for ( int i = 0; i < a.GetSizeI(); ++i )
    {
        delete a[i];
        a[i] = NULL;
    }
    a.Clear();
}

enum eVRMenuFlags
{
    // initially place the menu in front of the user's view on the horizon plane but do not move to follow
    // the user's gaze.
    VRMENU_FLAG_PLACE_ON_HORIZON,
	// place the menu directly in front of the user's view on the horizon plane each frame. The user will
	// sill be able to gaze track vertically.
	VRMENU_FLAG_TRACK_GAZE_HORIZONTAL,
	// place the menu directly in front of the user's view each frame -- this means gaze tracking won't
	// be available since the user can't look at different parts of the menu.
	VRMENU_FLAG_TRACK_GAZE,
	// If set, just consume the back key but do nothing with it (for warning menu that must be accepted)
	VRMENU_FLAG_BACK_KEY_DOESNT_EXIT,
	// If set, a short-press of the back key will exit the app when in this menu
	VRMENU_FLAG_BACK_KEY_EXITS_APP,
	// If set, return false so short press is passed to app
	VRMENU_FLAG_SHORT_PRESS_HANDLED_BY_APP
};

typedef BitFlagsT< eVRMenuFlags, int > VRMenuFlags_t;

//==============================================================
// VRMenu
class VRMenu
{
public:
	friend class OvrGuiSysLocal;

	enum eMenuState {
        MENUSTATE_OPENING,
		MENUSTATE_OPEN,
        MENUSTATE_CLOSING,
		MENUSTATE_CLOSED,
        MENUSTATE_MAX
	};

    static char const * MenuStateNames[MENUSTATE_MAX];

	static VRMenuId_t GetRootId();

	static VRMenu *			Create( char const * menuName );

	void					Init( OvrVRMenuMgr & menuMgr, BitmapFont const & font, float const menuDistance, 
									VRMenuFlags_t const & flags, Array< VRMenuComponent* > comps = Array< VRMenuComponent* >( ) );
	void					InitWithItems( OvrVRMenuMgr & menuMgr, BitmapFont const & font, float const menuDistance, 
									VRMenuFlags_t const & flags, Array< VRMenuObjectParms const * > & itemParms );
    void                    AddItems( OvrVRMenuMgr & menuMgr, BitmapFont const & font, 
                                    OVR::Array< VRMenuObjectParms const * > & itemParms,
                                    menuHandle_t parentHandle, bool const recenter );
	void					Shutdown( OvrVRMenuMgr & menuMgr );
	void					Frame( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, BitmapFont const & font,
                                    BitmapFontSurface & fontSurface, Matrix4f const & viewMatrix, gazeCursorUserId_t const gazeUserId );
	bool					OnKeyEvent( App * app, int const keyCode, KeyState::eKeyEventType const eventType );
	void					Open( App * app, OvrGazeCursor & gazeCursor, bool const instant = false );
	void					Close( App * app, OvrGazeCursor & gazeCursor, bool const instant = false );

    bool                    IsOpen() const { return CurMenuState == MENUSTATE_OPEN; }
    bool                    IsOpenOrOpening() const { return CurMenuState == MENUSTATE_OPEN || CurMenuState == MENUSTATE_OPENING || NextMenuState == MENUSTATE_OPEN || NextMenuState == MENUSTATE_OPENING; }
    
    bool                    IsClosed() const { return CurMenuState == MENUSTATE_CLOSED; }
    bool                    IsClosedOrClosing() const { return CurMenuState == MENUSTATE_CLOSED || CurMenuState == MENUSTATE_CLOSING || NextMenuState == MENUSTATE_CLOSED || NextMenuState == MENUSTATE_CLOSING; }

	void					OnItemEvent( App * app, VRMenuId_t const itemId, class VRMenuEvent const & event );

	// Clients can query the current menu state but to change it they must use
	// SetNextMenuState() and allow the menu to switch states when it can.
	eMenuState				GetCurMenuState() const { return CurMenuState; }

	// Returns the next menu state.
	eMenuState				GetNextMenuState() const { return NextMenuState; }
	// Sets the next menu state.  The menu will switch to that state at the next
	// opportunity.
	void					SetNextMenuState( eMenuState const s ) { NextMenuState = s; }

	menuHandle_t			GetRootHandle() const { return RootHandle; }
	menuHandle_t			GetFocusedHandle() const;
	Posef const &			GetMenuPose() const { return MenuPose; }
	void					SetMenuPose( Posef const & pose ) { MenuPose = pose; }

	menuHandle_t			HandleForId( OvrVRMenuMgr & menuMgr, VRMenuId_t const id ) const;

	char const *			GetName() const { return Name; }
    bool                    IsMenu( char const * menuName ) const { return OVR_stricmp( Name.ToCStr(), menuName ) == 0; }

	// Use an arbitrary view matrix. This is used when updating the menus and passing the current matrix
	void					RepositionMenu( App * app, Matrix4f const & viewMatrix );
	// Use app's lastViewMatrix. This is used when positioning a menu more or less in front of the user
	// when we do not have the most current view matrix available.
	void					RepositionMenu( App * app );

	//	Reset the MenuPose orientation - for now we assume identity orientation as the basis for all menus
	virtual void			ResetMenuOrientation( App * app );
	
    VRMenuFlags_t const &	GetFlags() const { return Flags; }
	void					SetFlags( VRMenuFlags_t	const & flags ) { Flags = flags; }

protected:
	// only derived classes can instance a VRMenu
	VRMenu( char const * name );
	// only GuiSysLocal can free a VRMenu
	virtual ~VRMenu();

private:
	menuHandle_t			RootHandle;			// handle to the menu's root item (to which all other items must be attached)

	eMenuState				CurMenuState;		// the current menu state
	eMenuState				NextMenuState;		// the state the menu should move to next

	Posef					MenuPose;			// world-space position and orientation of this menu's root item

	SoundLimiter			OpenSoundLimiter;	// prevents the menu open sound from playing too often
	SoundLimiter			CloseSoundLimiter;	// prevents the menu close sound from playing too often

	VRMenuEventHandler *	EventHandler;

	String                  Name;				// name of the menu

	VRMenuFlags_t			Flags;				// various flags that dictate menu behavior
	float					MenuDistance;		// distance from eyes
	bool					IsInitialized;		// true if Init was called
	bool					ComponentsInitialized;	// true if init message has been sent to components

private:
	static Posef			CalcMenuPosition( Matrix4f const & viewMatrix, Matrix4f const & invViewMatrix,
									Vector3f const & viewPos, Vector3f const & viewFwd, float const menuDistance );
	static Posef			CalcMenuPositionOnHorizon( Matrix4f const & viewMatrix, Matrix4f const & invViewMatrix,
									Vector3f const & viewPos, Vector3f const & viewFwd, float const menuDistance );

    // return true to continue with normal initialization (adding items) or false to skip.
    virtual bool    Init_Impl( OvrVRMenuMgr & menuMgr, BitmapFont const & font, float const menuDistance, 
                            VRMenuFlags_t const & flags, Array< VRMenuObjectParms const * > & itemParms );
    virtual void    Shutdown_Impl( OvrVRMenuMgr & menuMgr );
    virtual void    Frame_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, BitmapFont const & font,
                                    BitmapFontSurface & fontSurface, gazeCursorUserId_t const gazeUserId );
    // return true if the key was consumed.
    virtual bool    OnKeyEvent_Impl( App * app, int const keyCode, KeyState::eKeyEventType const eventType );
    virtual void    Open_Impl( App * app, OvrGazeCursor & gazeCursor );
    virtual void    Close_Impl( App * app, OvrGazeCursor & gazeCursor );
	virtual void	OnItemEvent_Impl( App * app, VRMenuId_t const itemId, class VRMenuEvent const & event );

	// return true when finished opening/closing - allowing derived menus to animate etc. during open/close
	virtual bool	IsFinishedOpening() const { return true;  }
	virtual bool	IsFinishedClosing() const { return true; }
};

} // namespace OVR

#endif // OVR_VRMenu_h

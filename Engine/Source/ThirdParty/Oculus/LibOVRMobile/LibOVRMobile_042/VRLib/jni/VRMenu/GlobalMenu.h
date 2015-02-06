/************************************************************************************

Filename    :   GlobalMenu.h
Content     :   The main menu that appears in native apps when pressing the HMT button.
Created     :   July 25, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_GlobalMenu_h )
#define OVR_GlobalMenu_h

#include "VRMenu.h"

namespace OVR {

//==============================================================
// OvrGlobalMenu
class OvrGlobalMenu : public VRMenu
{
public:
	class OvrGazeOverState
	{
	public:
		explicit OvrGazeOverState( VRMenuId_t const itemId ) : 
			ItemId( itemId ),
			GazedOver( false ) 
		{ 
		}
		OvrGazeOverState() : 
			GazedOver( false ) 
		{ 
		}

		VRMenuId_t	ItemId;
		bool		GazedOver;
	};

	static	VRMenuId_t			ID_DND;
	static	VRMenuId_t			ID_HOME;
	static	VRMenuId_t			ID_PASSTHROUGH;
	static	VRMenuId_t			ID_REORIENT;
	static	VRMenuId_t			ID_SETTINGS;
	static	VRMenuId_t			ID_BRIGHTNESS;
	static	VRMenuId_t			ID_COMFORT_MODE;
	static	VRMenuId_t			ID_BATTERY;
	static	VRMenuId_t			ID_BATTERY_TEXT;
	static	VRMenuId_t			ID_WIFI;
	static	VRMenuId_t			ID_BLUETOOTH;
	static	VRMenuId_t			ID_SIGNAL;
	static	VRMenuId_t			ID_TIME;
	static	VRMenuId_t			ID_BRIGHTNESS_SLIDER;
	static	VRMenuId_t			ID_BRIGHTNESS_SCRUBBER;
	static	VRMenuId_t			ID_BRIGHTNESS_BUBBLE;
	static	VRMenuId_t			ID_AIRPLANE_MODE;
	static	VRMenuId_t			ID_TUTORIAL_START;
	static	VRMenuId_t			ID_TUTORIAL_HOME;
	static	VRMenuId_t			ID_TUTORIAL_REORIENT;
	static	VRMenuId_t			ID_TUTORIAL_BRIGHTNESS;
	static	VRMenuId_t			ID_TUTORIAL_DND;
	static	VRMenuId_t			ID_TUTORIAL_COMFORT;
	static	VRMenuId_t			ID_TUTORIAL_PASSTHROUGH;
	static	VRMenuId_t			ID_TUTORIAL_END;

	static const char *			MENU_NAME;

								OvrGlobalMenu();
    virtual						~OvrGlobalMenu();

    // only one of these every needs to be created
	static  OvrGlobalMenu *		Create( App * app, OvrVRMenuMgr & menuMgr, BitmapFont const & font );

	void						SetShowTutorial( OvrVRMenuMgr & menuMgr, bool const show );
	bool						GetShowTutorial() const { return ShowTutorial; }
	void						SetGazeOverStates( Array< OvrGazeOverState > const & states ) { GazeOverStates = states; }
	void						SetItemGazedOver( VRMenuId_t const & id, bool gazedOver );
	void						SetGazeOverCount( int const c ) { GazeOverCount = c; }

	void						SetLaunchPackage( const String & package )	{ LaunchPackage = package; }
	const String &				LaunchingPackage( ) const { return LaunchPackage; }

private:
    // overloads
    virtual void				Frame_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, BitmapFont const & font,
								                BitmapFontSurface & fontSurface, gazeCursorUserId_t const gazeUserId );
	virtual bool				OnKeyEvent_Impl( App * app, int const keyCode, KeyState::eKeyEventType const eventType );

	virtual void				OnItemEvent_Impl( App * app, VRMenuId_t const itemId, VRMenuEvent const & event );

	virtual void				Open_Impl( App * app, OvrGazeCursor & gazeCursor );
	virtual void				Close_Impl( App * app, OvrGazeCursor & gazeCursor );

	int							NumItemsGazedOver() const;

	void						SetTutorialTextVisibilities( OvrVRMenuMgr & menuMgr, bool const startVisible, bool const endVisible ) const;

private:
	String						LaunchPackage;		// The package that launched the global menu
	bool						ShowTutorial;		// true to show the tutorial
	int							TutorialCount;		// tracks how many elements have been gazed over during tutorial
	int							GazeOverCount;		// number of items that have to be gazed over before secret koala is shown

	Array< OvrGazeOverState >	GazeOverStates;
};

} // namespace OVR

#endif // OVR_GlobalMenu_h
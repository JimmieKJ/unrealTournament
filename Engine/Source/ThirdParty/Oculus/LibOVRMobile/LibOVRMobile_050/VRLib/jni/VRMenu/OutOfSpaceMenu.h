/************************************************************************************

Filename    :   OutOfSpaceMenu.h
Content     :
Created     :   Feb 18, 2015
Authors     :   Madhu Kalva

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OUTOFSPACEMENU_H_
#define OUTOFSPACEMENU_H_

#include "VRMenu/VRMenu.h"

namespace OVR {

	class App;

	class OvrOutOfSpaceMenu : public VRMenu
	{
	public:
		static char const *	MENU_NAME;
		static OvrOutOfSpaceMenu * Create( App * app );

		void 	BuildMenu( int memoryInKB );

	private:
		OvrOutOfSpaceMenu( App * app );
		App * AppPtr;
	};
}

#endif /* OUTOFSPACEMENU_H_ */

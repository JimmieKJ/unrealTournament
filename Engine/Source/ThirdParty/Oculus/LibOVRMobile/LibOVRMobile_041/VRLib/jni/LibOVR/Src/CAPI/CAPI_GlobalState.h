/************************************************************************************

Filename    :   CAPI_GlobalState.h
Content     :   Maintains global state of the CAPI
Created     :   January 24, 2013
Authors     :   Michael Antonov

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_CAPI_GlobalState_h
#define OVR_CAPI_GlobalState_h

#include "../OVR_CAPI.h"
#include "CAPI_HMDState.h"

namespace OVR { namespace CAPI {

//-------------------------------------------------------------------------------------
// ***** OVRGlobalState

// Global DeviceManager state - singleton instance of this is created
// by ovr_Initialize().
class GlobalState : public MessageHandler, public NewOverrideBase
{
public:
	GlobalState();
	~GlobalState();

    static GlobalState *pInstance;
    
	int         EnumerateDevices();
	HMDDevice*  CreateDevice(int index);

	// MessageHandler implementation
	void        OnMessage(const Message& msg);

	// Helpers used to keep track of HMDs and notify them of sensor changes.
	void        AddHMD(HMDState* hmd);
	void        RemoveHMD(HMDState* hmd);
	void        NotifyHMDs_AddDevice(DeviceType deviceType);

	const char* GetLastError()
	{
		return 0;
	}

	DeviceManager* GetManager() { return pManager; }

private:

	Ptr<DeviceManager> pManager;
	int                DevicesEnumerated;

	// Currently created hmds; protected by Manager lock.
	List<HMDState>     HMDs;
};

}} // namespace OVR::CAPI

#endif



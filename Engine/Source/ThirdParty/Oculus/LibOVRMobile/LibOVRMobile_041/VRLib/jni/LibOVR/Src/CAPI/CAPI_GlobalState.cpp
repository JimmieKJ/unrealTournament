/************************************************************************************

Filename    :   CAPI_GlobalState.cpp
Content     :   Maintains global state of the CAPI
Created     :   January 24, 2014
Authors     :   Michael Antonov

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "CAPI_GlobalState.h"

namespace OVR { namespace CAPI {


//-------------------------------------------------------------------------------------
// Open Questions / Notes

// 2. Detect HMDs.
// Challenge: If we do everything through polling, it would imply we want all the devices
//            initialized. However, there may be multiple rifts, extra sensors, etc,
//            which shouldn't be allocated.
// 

// How do you reset orientation Quaternion?
// Can you change IPD?



//-------------------------------------------------------------------------------------
// ***** OVRGlobalState

// Global instance
GlobalState* GlobalState::pInstance = 0;


GlobalState::GlobalState()
{
	pManager = *DeviceManager::Create();
	pManager->SetMessageHandler(this);
	EnumerateDevices();
}

GlobalState::~GlobalState()
{
	RemoveHandlerFromDevices();
	OVR_ASSERT(HMDs.IsEmpty());
}

int GlobalState::EnumerateDevices()
{
	DevicesEnumerated = 0;

	DeviceEnumerator<HMDDevice> e = pManager->EnumerateDevices<HMDDevice>();
	while (e.IsAvailable())
	{
		DevicesEnumerated++;
		e.Next();
	}

	return DevicesEnumerated;
}


HMDDevice* GlobalState::CreateDevice(int index)
{
	if (index >= DevicesEnumerated)
		return 0;

	// This isn't quite right since indices aren't guaranteed to match,
	// we should cache handles instead... but this should be ok for now.
	int i = 0;

	DeviceEnumerator<HMDDevice> e = pManager->EnumerateDevices<HMDDevice>();
	while (e.IsAvailable())
	{
		if (i == index)
		{
			HMDDevice* device = e.CreateDevice();
			return device;
		}

		i++;
		e.Next();
	}

	return 0;
}


void GlobalState::AddHMD(HMDState* hmd)
{
	Lock::Locker lock(pManager->GetHandlerLock());
	HMDs.PushBack(hmd);
}
void GlobalState::RemoveHMD(HMDState* hmd)
{
	Lock::Locker lock(pManager->GetHandlerLock());
	hmd->RemoveNode();
}

void GlobalState::NotifyHMDs_AddDevice(DeviceType deviceType)
{
	Lock::Locker lock(pManager->GetHandlerLock());
	for (HMDState* hmd = HMDs.GetFirst(); !HMDs.IsNull(hmd); hmd = hmd->pNext)
		hmd->NotifyAddDevice(deviceType);
}

void GlobalState::OnMessage(const Message& msg)
{
	if (msg.Type == Message_DeviceAdded || msg.Type == Message_DeviceRemoved)
	{
		if (msg.pDevice == pManager)
		{
			const MessageDeviceStatus& statusMsg =
				static_cast<const MessageDeviceStatus&>(msg);

			if (msg.Type == Message_DeviceAdded)
			{
				//LogText("OnMessage DeviceAdded.\n");

				// We may have added a sensor/other device; notify any HMDs that might
				// need it to check for it later.
				NotifyHMDs_AddDevice(statusMsg.Handle.GetType());
			}
			else
			{
				//LogText("OnMessage DeviceRemoved.\n");
			}
		}
	}
}


}} // namespace OVR::CAPI
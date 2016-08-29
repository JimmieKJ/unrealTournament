// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OVR_Platform.h"

DECLARE_DELEGATE_TwoParams(FOculusMessageOnCompleteDelegate, ovrMessageHandle, bool);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOculusMulticastMessageOnCompleteDelegate, ovrMessageHandle, bool);

class FOnlineMessageTaskManagerOculus
{
private:
	void OnReceiveMessage(ovrMessageHandle Message);

	/** Direct Requests waiting for a Message response */
	TMap<ovrRequest, FOculusMessageOnCompleteDelegate> RequestDelegates;

	/** Notif Requests waiting for a Message response */
	TMap<ovrMessageType, FOculusMulticastMessageOnCompleteDelegate> NotifDelegates;

public:

	void AddRequestDelegate(ovrRequest RequestId, FOculusMessageOnCompleteDelegate&& Delegate);

	FOculusMulticastMessageOnCompleteDelegate& GetNotifDelegate(ovrMessageType MessageType);

	void RemoveNotifDelegate(ovrMessageType MessageType, const FDelegateHandle& Delegate);

	bool Tick(float DeltaTime);
};

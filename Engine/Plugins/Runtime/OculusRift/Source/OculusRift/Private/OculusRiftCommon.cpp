// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OculusRiftPrivatePCH.h"

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#include "OculusRiftHMD.h"

//////////////////////////////////////////////////////////////////////////
ovrResult FOvrSessionShared::Create(ovrGraphicsLuid& luid)
{
	Destroy();
	ovrResult result = FOculusRiftPlugin::Get().CreateSession(&Session, &luid);
	if (OVR_SUCCESS(result))
	{
		UE_LOG(LogHMD, Log, TEXT("New ovr session is created"));
		DelegatesLock.Lock();
		CreateDelegate.Broadcast(Session);
		DelegatesLock.Unlock();
	}
	else
	{
		UE_LOG(LogHMD, Log, TEXT("Failed to create new ovr session, err = %d"), int(result));
	}
	return result;
}

void FOvrSessionShared::Destroy()
{
	if (Session)
	{
		UE_LOG(LogHMD, Log, TEXT("Destroying ovr session"));
		FSuspendRenderingThread SuspendRenderingThread(false);
		check(LockCnt.GetValue() == 0);

		DelegatesLock.Lock();
		DestoryDelegate.Broadcast(Session);
		DelegatesLock.Unlock();

		FOculusRiftPlugin::Get().DestroySession(Session);
		Session = nullptr;
	}
}

#endif // #if OCULUS_RIFT_SUPPORTED_PLATFORMS

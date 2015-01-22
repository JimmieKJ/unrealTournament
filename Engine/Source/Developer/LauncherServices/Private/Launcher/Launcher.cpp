// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LauncherServicesPrivatePCH.h"


/* ILauncher overrides
 *****************************************************************************/

ILauncherWorkerPtr FLauncher::Launch( const ITargetDeviceProxyManagerRef& DeviceProxyManager, const ILauncherProfileRef& Profile )
{
	if (Profile->IsValidForLaunch())
	{
		FLauncherWorker* LauncherWorker = new FLauncherWorker(DeviceProxyManager, Profile);

		if ((LauncherWorker != nullptr) && (FRunnableThread::Create(LauncherWorker, TEXT("LauncherWorker")) != nullptr))
		{
			return MakeShareable(LauncherWorker);
		}			
	}

	return nullptr;
}

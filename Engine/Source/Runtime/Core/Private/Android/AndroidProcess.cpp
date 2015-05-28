// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidProcess.cpp: Android implementations of Process functions
=============================================================================*/

#include "CorePrivatePCH.h"
#include "AndroidPlatformRunnableThread.h"

#include <sys/syscall.h>
#include <pthread.h>


const TCHAR* FAndroidPlatformProcess::ComputerName()
{
	return TEXT("Android Device"); 
}

void FAndroidPlatformProcess::SetThreadAffinityMask( uint64 InAffinityMask )
{
	/* Skip setting mask for now due to issues with Big/Little CPUs
	pid_t ThreadId = gettid();
	int AffinityMask = (int)InAffinityMask;
	syscall(__NR_sched_setaffinity, ThreadId, sizeof(AffinityMask), &AffinityMask);
	*/
}

const TCHAR* FAndroidPlatformProcess::BaseDir()
{
	return TEXT("");
}

const TCHAR* FAndroidPlatformProcess::ExecutableName(bool bRemoveExtension)
{
	extern FString GAndroidProjectName;
	return *GAndroidProjectName;
}

FRunnableThread* FAndroidPlatformProcess::CreateRunnableThread()
{
	return new FRunnableThreadAndroid();
}

DECLARE_DELEGATE_OneParam(FAndroidLaunchURLDelegate, const FString&);

CORE_API FAndroidLaunchURLDelegate OnAndroidLaunchURL;

void FAndroidPlatformProcess::LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error)
{
	check(URL);
	const FString URLWithParams = FString::Printf(TEXT("%s %s"), URL, Parms ? Parms : TEXT("")).TrimTrailing();

	OnAndroidLaunchURL.ExecuteIfBound(URLWithParams);

	if (Error != nullptr)
	{
		*Error = TEXT("");
	}
}

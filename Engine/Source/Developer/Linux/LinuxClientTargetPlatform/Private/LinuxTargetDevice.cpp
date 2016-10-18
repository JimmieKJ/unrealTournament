// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LinuxTargetDevice.cpp: Implements the LinuxTargetDevice class.
=============================================================================*/

#include "LinuxClientTargetPlatformPrivatePCH.h"

#include "LinuxTargetDevice.h"



/* ITargetDevice interface
 *****************************************************************************/

bool FLinuxTargetDevice::Deploy( const FString& SourceFolder, FString& OutAppId )
{
	return false;
}

int32 FLinuxTargetDevice::GetProcessSnapshot( TArray<FTargetDeviceProcessInfo>& OutProcessInfos )
{
	return 0;
}

bool FLinuxTargetDevice::Launch( const FString& AppId, EBuildConfigurations::Type BuildConfiguration, EBuildTargets::Type BuildTarget, const FString& Params, uint32* OutProcessId )
{
	return false;
}

bool FLinuxTargetDevice::Reboot( bool bReconnect)
{
	return false;
}

bool FLinuxTargetDevice::Run( const FString& ExecutablePath, const FString& Params, uint32* OutProcessId )
{
	return false;
}

bool FLinuxTargetDevice::SupportsFeature( ETargetDeviceFeatures Feature ) const
{
	return false;
}

bool FLinuxTargetDevice::SupportsSdkVersion( const FString& VersionString ) const
{
	return true;
}

void FLinuxTargetDevice::SetUserCredentials( const FString& InUserName, const FString& InUserPassword )
{
	UserName = InUserName;
	UserPassword = InUserPassword;
}

bool FLinuxTargetDevice::GetUserCredentials( FString& OutUserName, FString& OutUserPassword )
{
	OutUserName = UserName;
	OutUserPassword = UserPassword;
	return true;
}

bool FLinuxTargetDevice::TerminateProcess( const int32 ProcessId )
{
	return false;
}


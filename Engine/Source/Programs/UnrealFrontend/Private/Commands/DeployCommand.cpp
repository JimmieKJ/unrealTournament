// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealFrontendPrivatePCH.h"
#include "DeployCommand.h"
#include "Projects.h"
#include "TargetPlatform.h"
#include "TaskGraphInterfaces.h"


bool FDeployCommand::Run( )
{
	bool bDeployed = false;

	// get the target device
	FString Device;
	FParse::Value(FCommandLine::Get(), TEXT("-DEVICE="), Device);

	// get the file manifest
	FString Manifest;
	FParse::Value(FCommandLine::Get(), TEXT("-MANIFEST="), Manifest);

	FString SourceDir;
	FParse::Value(FCommandLine::Get(), TEXT("-SOURCEDIR="), SourceDir);

	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();

	if (!TPM)
	{
		return false;
	}

	// Initialize the messaging subsystem so we can do device discovery.
	FModuleManager::Get().LoadModuleChecked("Messaging");

	// load plug-in modules
	// @todo: allow for better plug-in support in standalone Slate apps
	IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::PreDefault);

	double DeltaTime = 0.0;
	double LastTime = FPlatformTime::Seconds();

	// We track the message sent time because we have to keep updating the loop until the message is *actually sent*. (ie all packets queued, sent, buffer flushed, etc.)
	double MessageSentTime = 0.0;
	bool bMessageSent = false;

	while (!GIsRequestingExit && ((MessageSentTime > LastTime + 1.0) || (MessageSentTime <= 0.1)))
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		FTicker::GetCoreTicker().Tick(DeltaTime);
		FPlatformProcess::Sleep(0);

		DeltaTime = FPlatformTime::Seconds() - LastTime;
		LastTime = FPlatformTime::Seconds();

		if ( !bMessageSent )
		{
			const TArray<ITargetPlatform*>& Platforms = TPM->GetActiveTargetPlatforms();

			FString Platform;
			FString DeviceName;

			Device.Split(TEXT("@"), &Platform, &DeviceName);

			FTargetDeviceId DeviceId(Platform, DeviceName);
			ITargetDevicePtr TargetDevice;

			for (int32 Index = 0; Index < Platforms.Num(); ++Index)
			{
				TargetDevice = Platforms[Index]->GetDevice(DeviceId);

				if (TargetDevice.IsValid())
				{
					FString OutId;

					if (Platforms[Index]->PackageBuild(SourceDir))
					{
						if (TargetDevice->Deploy(SourceDir, OutId))
						{
							bDeployed = true;
						}

						MessageSentTime = LastTime;
						bMessageSent = true;
					}
					else
					{
						MessageSentTime = LastTime;
						bMessageSent = true;
					}
				}
			}
		}
	}

	return bDeployed;
}

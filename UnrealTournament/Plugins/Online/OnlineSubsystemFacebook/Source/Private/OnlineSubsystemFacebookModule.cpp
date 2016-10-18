// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemFacebookPrivatePCH.h"

// FOnlineSubsystemFacebookModule

IMPLEMENT_MODULE(FOnlineSubsystemFacebookModule, OnlineSubsystemFacebook);

/**
 * Class responsible for creating instance(s) of the subsystem
 */
class FOnlineFactoryFacebook : public IOnlineFactory
{
public:

	FOnlineFactoryFacebook() {}
	virtual ~FOnlineFactoryFacebook() {}

	virtual IOnlineSubsystemPtr CreateSubsystem(FName InstanceName)
	{
		FOnlineSubsystemFacebookPtr OnlineSub = MakeShareable(new FOnlineSubsystemFacebook());
		if (OnlineSub->IsEnabled())
		{
			if(!OnlineSub->Init())
			{
				UE_LOG(LogOnline, Warning, TEXT("Facebook API failed to initialize!"));
				OnlineSub->Shutdown();
				OnlineSub = NULL;
			}
		}
		else
		{
			UE_LOG(LogOnline, Warning, TEXT("Facebook API disabled!"));
			OnlineSub->Shutdown();
			OnlineSub = NULL;
		}

		return OnlineSub;
	}
};

void FOnlineSubsystemFacebookModule::StartupModule()
{
	UE_LOG(LogOnline, Log, TEXT("Facebook Startup!"));

	FacebookFactory = new FOnlineFactoryFacebook();

	// Create and register our singleton factory with the main online subsystem for easy access
	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
	OSS.RegisterPlatformService(FACEBOOK_SUBSYSTEM, FacebookFactory);
}

void FOnlineSubsystemFacebookModule::ShutdownModule()
{
	UE_LOG(LogOnline, Log, TEXT("Facebook Shutdown!"));

	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
	OSS.UnregisterPlatformService(FACEBOOK_SUBSYSTEM);

	delete FacebookFactory;
	FacebookFactory = NULL;
}
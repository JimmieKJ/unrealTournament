// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemAmazonPCH.h"
#include "OnlineSubsystemAmazon.h"
#include "OnlineSubsystemAmazonModule.h"
#include "OnlineIdentityAmazon.h"

// FOnlineSubsystemAmazonModule
IMPLEMENT_MODULE(FOnlineSubsystemAmazonModule, OnlineSubsystemAmazon);

/**
 * Class responsible for creating instance(s) of the subsystem
 */
class FOnlineFactoryAmazon : public IOnlineFactory
{
public:

	FOnlineFactoryAmazon() {}
	virtual ~FOnlineFactoryAmazon() {}

	virtual IOnlineSubsystemPtr CreateSubsystem(FName InstanceName)
	{
		FOnlineSubsystemAmazonPtr OnlineSub = MakeShareable(new FOnlineSubsystemAmazon());
		if (OnlineSub->IsEnabled())
		{
			if(!OnlineSub->Init())
			{
				UE_LOG(LogOnline, Warning, TEXT("Amazon API failed to initialize!"));
				OnlineSub->Shutdown();
				OnlineSub = NULL;
			}
		}
		else
		{
			UE_LOG(LogOnline, Warning, TEXT("Amazon API disabled!"));
			OnlineSub->Shutdown();
			OnlineSub = NULL;
		}

		return OnlineSub;
	}
};

void FOnlineSubsystemAmazonModule::StartupModule()
{
	UE_LOG(LogOnline, Log, TEXT("Amazon Startup!"));

	AmazonFactory = new FOnlineFactoryAmazon();

	// Create and register our singleton factory with the main online subsystem for easy access
	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
	OSS.RegisterPlatformService(AMAZON_SUBSYSTEM, AmazonFactory);
}

void FOnlineSubsystemAmazonModule::ShutdownModule()
{
	UE_LOG(LogOnline, Log, TEXT("Amazon Shutdown!"));

	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
	OSS.UnregisterPlatformService(AMAZON_SUBSYSTEM);
	
	delete AmazonFactory;
	AmazonFactory = NULL;
}

IOnlineIdentityPtr FOnlineSubsystemAmazon::GetIdentityInterface() const
{
	return IdentityInterface;
}


bool FOnlineSubsystemAmazon::Tick(float DeltaTime)
{
	if (!FOnlineSubsystemImpl::Tick(DeltaTime))
	{
		return false;
	}

	if (IdentityInterface.IsValid())
	{
		TickToggle ^= 1;
		IdentityInterface->Tick(DeltaTime, TickToggle);
	}
	return true;
}

bool FOnlineSubsystemAmazon::Init()
{
	IdentityInterface = MakeShareable(new FOnlineIdentityAmazon());
	return true;
}

bool FOnlineSubsystemAmazon::Shutdown()
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineSubsystemAmazon::Shutdown()"));
	IdentityInterface = NULL;
	FOnlineSubsystemImpl::Shutdown();
	return true;
}

FString FOnlineSubsystemAmazon::GetAppId() const
{
	return TEXT("Amazon");
}

bool FOnlineSubsystemAmazon::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) 
{
	if (FOnlineSubsystemImpl::Exec(InWorld, Cmd, Ar))
	{
		return true;
	}
	return false;
}

bool FOnlineSubsystemAmazon::IsEnabled(void)
{
	// Check the ini for disabling Amazon
	bool bEnableAmazon = true;
	if (GConfig->GetBool(TEXT("OnlineSubsystemAmazon"), TEXT("bEnabled"), bEnableAmazon, GEngineIni) && 
		bEnableAmazon)
	{
		// Check the commandline for disabling Amazon
		bEnableAmazon = !FParse::Param(FCommandLine::Get(),TEXT("NOAMAZON"));
#if UE_EDITOR
		if (bEnableAmazon)
		{
			bEnableAmazon = IsRunningDedicatedServer() || IsRunningGame();
		}
#endif
	}
	return bEnableAmazon;
}

FOnlineSubsystemAmazon::FOnlineSubsystemAmazon() :
	IdentityInterface(NULL),
	TickToggle(0)
{

}

FOnlineSubsystemAmazon::~FOnlineSubsystemAmazon()
{

}

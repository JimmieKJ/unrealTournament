// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "OnlineSubsystemUtilsModule.h"
#include "ModuleManager.h"
#include "OnlinePIESettings.h"

IMPLEMENT_MODULE(FOnlineSubsystemUtilsModule, OnlineSubsystemUtils);

/**
 * Concrete implementation of IOnlineSubsystemUtils interface 
 */
class FOnlineSubsystemUtils : public IOnlineSubsystemUtils
{
public:

	FOnlineSubsystemUtils() {}
	virtual ~FOnlineSubsystemUtils() {}

	FName GetOnlineIdentifier(const FWorldContext& WorldContext) override
	{
#if WITH_EDITOR
		return FName(*FString::Printf(TEXT(":%s"), *WorldContext.ContextHandle.ToString()));
#else
		return NAME_None;
#endif
	}

	FName GetOnlineIdentifier(UWorld* World, const FName Subsystem = NAME_None) override
	{
#if WITH_EDITOR
		if (const FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(World))
		{
			return FName(
				*FString::Printf(TEXT("%s:%s"), !Subsystem.IsNone() ? *Subsystem.ToString() : TEXT(""), *WorldContext->ContextHandle.ToString()));
		}

		return NAME_None;
#else
		return Subsystem;
#endif
	}

#if WITH_EDITOR
	virtual bool SupportsOnlinePIE() const override
	{
		const UOnlinePIESettings* OnlinePIESettings = GetDefault<UOnlinePIESettings>();
		if (OnlinePIESettings->bOnlinePIEEnabled && GetNumPIELogins() > 0)
		{
			// If we can't get the identity interface then things are either not configured right or disabled
			IOnlineIdentityPtr IdentityInt = Online::GetIdentityInterface();
			return IdentityInt.IsValid();
		}

		return false;
	}

	virtual bool IsOnlinePIEEnabled() const override
	{
		const UOnlinePIESettings* OnlinePIESettings = GetDefault<UOnlinePIESettings>();
		return OnlinePIESettings->bOnlinePIEEnabled;
	}

	virtual int32 GetNumPIELogins() const override
	{
		int32 NumValidLogins = 0;
		const UOnlinePIESettings* OnlinePIESettings = GetDefault<UOnlinePIESettings>();
		for (const FPIELoginSettingsInternal& Login : OnlinePIESettings->Logins)
		{
			if (Login.IsValid())
			{
				NumValidLogins++;
			}
		}
	
		return NumValidLogins;
	}

	virtual void GetPIELogins(TArray<FOnlineAccountCredentials>& Logins) override
	{
		const UOnlinePIESettings* OnlinePIESettings = GetDefault<UOnlinePIESettings>();
		if (OnlinePIESettings->Logins.Num() > 0)
		{
			FOnlineAccountCredentials TempLogin;

			Logins.Empty(OnlinePIESettings->Logins.Num());
			for (const FPIELoginSettingsInternal& Login : OnlinePIESettings->Logins)
			{
				if (Login.IsValid())
				{
					new (Logins)FOnlineAccountCredentials(Login.Type, Login.Id, Login.Token);
				}
			}
		}
	}
#endif // WITH_EDITOR
};

void FOnlineSubsystemUtilsModule::StartupModule()
{
	SubsystemUtils = new FOnlineSubsystemUtils();
}

void FOnlineSubsystemUtilsModule::ShutdownModule()
{
	delete SubsystemUtils;
	SubsystemUtils = nullptr;
}

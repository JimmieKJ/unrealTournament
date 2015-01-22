// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Runtime/Analytics/AnalyticsMulticast/Private/AnalyticsMulticastPrivatePCH.h"

#include "Core.h"
#include "Guid.h"
#include "Json.h"
#include "SecureHash.h"
#include "Runtime/Analytics/AnalyticsMulticast/Public/AnalyticsMulticast.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"

DEFINE_LOG_CATEGORY_STATIC(LogAnalytics, Display, All);

IMPLEMENT_MODULE( FAnalyticsMulticast, AnalyticsMulticast );

class FAnalyticsProviderMulticast : public IAnalyticsProvider
{
public:
	FAnalyticsProviderMulticast(const FAnalyticsMulticast::Config& ConfigValues, const FAnalytics::FProviderConfigurationDelegate& GetConfigValue);

	virtual bool StartSession(const TArray<FAnalyticsEventAttribute>& Attributes) override;
	virtual void EndSession() override;
	virtual void FlushEvents() override;

	virtual void SetUserID(const FString& InUserID) override;
	virtual FString GetUserID() const override;

	virtual FString GetSessionID() const override;
	virtual bool SetSessionID(const FString& InSessionID) override;

	virtual void RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes) override;
	virtual void RecordItemPurchase(const FString& ItemId, const FString& Currency, int PerItemCost, int ItemQuantity) override;
	virtual void RecordCurrencyPurchase(const FString& GameCurrencyType, int GameCurrencyAmount, const FString& RealCurrencyType, float RealMoneyCost, const FString& PaymentProvider) override;
	virtual void RecordCurrencyGiven(const FString& GameCurrencyType, int GameCurrencyAmount) override;

	virtual ~FAnalyticsProviderMulticast();

	bool HasValidProviders() const { return Providers.Num() > 0; }
private:
	TArray<TSharedPtr<IAnalyticsProvider> > Providers;
	TArray<FString> ProviderModules;
};

void FAnalyticsMulticast::StartupModule()
{
}

void FAnalyticsMulticast::ShutdownModule()
{
}

TSharedPtr<IAnalyticsProvider> FAnalyticsMulticast::CreateAnalyticsProvider(const FAnalytics::FProviderConfigurationDelegate& GetConfigValue) const
{
	if (GetConfigValue.IsBound())
	{
		Config ConfigValues;
		ConfigValues.ProviderModuleNames = GetConfigValue.Execute(Config::GetKeyNameForProviderModuleNames(), true);
		if (ConfigValues.ProviderModuleNames.IsEmpty())
		{
			UE_LOG(LogAnalytics, Warning, TEXT("CreateAnalyticsProvider delegate did not contain required parameter %s"), *Config::GetKeyNameForProviderModuleNames());
			return NULL;
		}
		return CreateAnalyticsProvider(ConfigValues, GetConfigValue);
	}
	else
	{
		UE_LOG(LogAnalytics, Warning, TEXT("CreateAnalyticsProvider called with an unbound delegate"));
	}
	return NULL;
}

TSharedPtr<IAnalyticsProvider> FAnalyticsMulticast::CreateAnalyticsProvider(const Config& ConfigValues, const FAnalytics::FProviderConfigurationDelegate& GetConfigValue) const
{
	TScopedPointer<FAnalyticsProviderMulticast> Provider(new FAnalyticsProviderMulticast(ConfigValues, GetConfigValue));
	// If we didn't have any valid providers, return NULL
	if (!Provider->HasValidProviders())
	{
		return NULL;
	}
	// if we were configured ok, bind thee result to a shared pointer.
	return TSharedPtr<IAnalyticsProvider>(Provider.Release());
}

/**
 * Perform any initialization.
 */
FAnalyticsProviderMulticast::FAnalyticsProviderMulticast(const FAnalyticsMulticast::Config& ConfigValues, const FAnalytics::FProviderConfigurationDelegate& GetConfigValue)
{
	UE_LOG(LogAnalytics, Verbose, TEXT("Initializing Multicast Analytics provider"));

	if (GetConfigValue.IsBound())
	{
		TArray<FString> ModuleNamesArray;
		ConfigValues.ProviderModuleNames.ParseIntoArray(&ModuleNamesArray, TEXT(","), true);
		for (TArray<FString>::TConstIterator it(ModuleNamesArray);it;++it)
		{
			TSharedPtr<IAnalyticsProvider> NewProvider = FAnalytics::Get().CreateAnalyticsProvider(FName(**it), GetConfigValue);
			if (NewProvider.IsValid())
			{
				Providers.Add(NewProvider);
				ProviderModules.Add(*it);
			}
		}
	}
}

FAnalyticsProviderMulticast::~FAnalyticsProviderMulticast()
{
	UE_LOG(LogAnalytics, Verbose, TEXT("Destroying Multicast Analytics provider"));
}

bool FAnalyticsProviderMulticast::StartSession(const TArray<FAnalyticsEventAttribute>& Attributes)
{
	bool bResult = false;
	for (TArray<TSharedPtr<IAnalyticsProvider> >::TConstIterator it(Providers);it;++it)
	{
		bResult |= (*it)->StartSession(Attributes);
	}
	return bResult;
}

void FAnalyticsProviderMulticast::EndSession()
{
	for (TArray<TSharedPtr<IAnalyticsProvider> >::TConstIterator it(Providers);it;++it)
	{
		(*it)->EndSession();
	}
}

void FAnalyticsProviderMulticast::FlushEvents()
{
	for (TArray<TSharedPtr<IAnalyticsProvider> >::TConstIterator it(Providers);it;++it)
	{
		(*it)->FlushEvents();
	}
}

void FAnalyticsProviderMulticast::SetUserID( const FString& InUserID )
{
	for (TArray<TSharedPtr<IAnalyticsProvider> >::TConstIterator it(Providers);it;++it)
	{
		(*it)->SetUserID(InUserID);
	}
}

FString FAnalyticsProviderMulticast::GetUserID() const 
{
	for (TArray<TSharedPtr<IAnalyticsProvider> >::TConstIterator it(Providers);it;++it)
	{
		return (*it)->GetUserID();
	}
	return FString();
}

FString FAnalyticsProviderMulticast::GetSessionID() const
{
	// combine all session IDs into Module@@Session##Module@@Session...
	FString Result;
	for (int i=0;i<Providers.Num();++i)
	{
		if (i>0) Result += "##";
		Result += ProviderModules[i] + "@@" + Providers[i]->GetSessionID();
	}
	return Result;
}

bool FAnalyticsProviderMulticast::SetSessionID(const FString& InSessionID)
{
	bool bResult = false;
	// parse out the format from GetSessionID and set the SessionID in each provider.
	TArray<FString> SessionIDs;
	// parse out the module/session pairs
	InSessionID.ParseIntoArray(&SessionIDs, TEXT("##"), true);
	for (TArray<FString>::TConstIterator it(SessionIDs);it;++it)
	{
		FString ModuleName, SessionID;
		// split out the module name/sessionID pair
		if (it->Split(TEXT("@@"), &ModuleName, &SessionID))
		{
			// find the module name in the list of existing providers
			for (int i=0;i<Providers.Num();++i)
			{
				// only set the session ID if it is not empty.
				if (ProviderModules[i] == ModuleName && !SessionID.IsEmpty())
				{
					bResult |= Providers[i]->SetSessionID(SessionID);
				}
			}
		}
	}
	return bResult;
}


void FAnalyticsProviderMulticast::RecordEvent( const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes )
{
	for (TArray<TSharedPtr<IAnalyticsProvider> >::TConstIterator it(Providers);it;++it)
	{
		(*it)->RecordEvent(EventName, Attributes);
	}
}

void FAnalyticsProviderMulticast::RecordItemPurchase( const FString& ItemId, const FString& Currency, int PerItemCost, int ItemQuantity )
{
	for (TArray<TSharedPtr<IAnalyticsProvider> >::TConstIterator it(Providers);it;++it)
	{
		(*it)->RecordItemPurchase(ItemId, Currency, PerItemCost, ItemQuantity);
	}
}

void FAnalyticsProviderMulticast::RecordCurrencyPurchase( const FString& GameCurrencyType, int GameCurrencyAmount, const FString& RealCurrencyType, float RealMoneyCost, const FString& PaymentProvider )
{
	for (TArray<TSharedPtr<IAnalyticsProvider> >::TConstIterator it(Providers);it;++it)
	{
		(*it)->RecordCurrencyPurchase(GameCurrencyType, GameCurrencyAmount, RealCurrencyType, RealMoneyCost, PaymentProvider);
	}
}

void FAnalyticsProviderMulticast::RecordCurrencyGiven( const FString& GameCurrencyType, int GameCurrencyAmount )
{
	for (TArray<TSharedPtr<IAnalyticsProvider> >::TConstIterator it(Providers);it;++it)
	{
		(*it)->RecordCurrencyGiven(GameCurrencyType, GameCurrencyAmount);
	}
}

// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "QosPrivatePCH.h"
#include "QosRegionManager.h"
#include "QosInterface.h"
#include "QosEvaluator.h"
#include "QosModule.h"

FOnlineSessionSettingsQos::FOnlineSessionSettingsQos(bool bInIsDedicated)
{
	NumPublicConnections = 1;
	NumPrivateConnections = 0;

	bIsLANMatch = false;
	bShouldAdvertise = true;
	bAllowJoinInProgress = true;
	bAllowInvites = true;
	bUsesPresence = false;
	bAllowJoinViaPresence = true;
	bAllowJoinViaPresenceFriendsOnly = false;

	FString GameModeStr(GAMEMODE_QOS);
	Set(SETTING_GAMEMODE, GameModeStr, EOnlineDataAdvertisementType::ViaOnlineService);
	Set(SETTING_QOS, 1, EOnlineDataAdvertisementType::ViaOnlineService);
	Set(SETTING_REGION, FQosInterface::Get()->GetRegionId(), EOnlineDataAdvertisementType::ViaOnlineService);
	bIsDedicated = bInIsDedicated;
}

UQosRegionManager::UQosRegionManager(const FObjectInitializer& ObjectInitializer)
	: bUseOldQosServers(false)
	, NumTestsPerRegion(3)
	, PingTimeout(5.0f)
	, LastCheckTimestamp(0)
	, Evaluator(nullptr)
	, QosEvalResult(EQosCompletionResult::Invalid)
{
	check(GConfig);
	GConfig->GetString(TEXT("Qos"), TEXT("ForceRegionId"), ForceRegionId, GGameIni);

	// get a forced region id from the command line as an override
	FParse::Value(FCommandLine::Get(), TEXT("McpRegion="), ForceRegionId);
}

void UQosRegionManager::PostReloadConfig(UProperty* PropertyThatWasLoaded)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		for (int32 RegionIdx = RegionOptions.Num() - 1; RegionIdx >= 0; RegionIdx--)
		{
			FQosRegionInfo& Region = RegionOptions[RegionIdx];

			bool bFound = false;
			for (FQosDatacenterInfo& Datacenter : Datacenters)
			{
				if (Datacenter.RegionId == Region.Region.RegionId)
				{
					bFound = true;
				}
			}

			if (!bFound)
			{
				// Old value needs to be removed, preserve order
				RegionOptions.RemoveAt(RegionIdx);
			}
		}

		for (int32 MetaIdx = 0; MetaIdx < Datacenters.Num(); MetaIdx++)
		{
			FQosDatacenterInfo& Datacenter = Datacenters[MetaIdx];

			bool bFound = false;
			for (FQosRegionInfo& Region : RegionOptions)
			{
				if (Datacenter.RegionId == Region.Region.RegionId)
				{
					// Overwrite the metadata
					Region.Region = Datacenter;
					bFound = true;
				}
			}

			if (!bFound)
			{
				// Add new value not in old list
				FQosRegionInfo NewRegion(Datacenter);
				RegionOptions.Insert(NewRegion, MetaIdx);
			}
		}

		// Validate the current region selection
		TrySetDefaultRegion();
	}
}

int32 UQosRegionManager::GetMaxPingMs() const
{
	int32 MaxPing = -1;
	if (GConfig->GetInt(TEXT("Qos"), TEXT("MaximumPingMs"), MaxPing, GGameIni) && MaxPing > 0)
	{
		return MaxPing;
	}
	return -1;
}

// static
FString UQosRegionManager::GetDatacenterId()
{
	struct FDcidInfo
	{
		FDcidInfo()
		{
			FString OverrideDCID;
			if (FParse::Value(FCommandLine::Get(), TEXT("DCID="), OverrideDCID))
			{
				// DCID specified on command line
				DCIDString = OverrideDCID.ToUpper();
			}
			else
			{
				FString DefaultDCID;
				check(GConfig);
				if (GConfig->GetString(TEXT("Qos"), TEXT("DCID"), DefaultDCID, GGameIni))
				{
					// DCID specified in ini file
					DCIDString = DefaultDCID.ToUpper();
				}
			}
		}

		FString DCIDString;
	};
	static FDcidInfo DCID;
	return DCID.DCIDString;
}

void UQosRegionManager::BeginQosEvaluation(UWorld* World, const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider, const FSimpleDelegate& OnComplete)
{
	check(World);

	// no point doing the qos tests at all if we're forcing this
	if (!ForceRegionId.IsEmpty())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([OnComplete]()
		{
			OnComplete.ExecuteIfBound();
		}));
		return;
	}

	// There are valid cached results, use them
	if ((RegionOptions.Num() > 0) &&
		(QosEvalResult == EQosCompletionResult::Success) &&
		(FDateTime::UtcNow() - LastCheckTimestamp).GetTotalSeconds() <= 3)
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([OnComplete]()
		{
			OnComplete.ExecuteIfBound();
		}));
		return;
	}

	// add to the completion delegate
	OnQosEvalCompleteDelegate.Add(OnComplete);

	// if we're already evaluating, simply return
	if (Evaluator != nullptr)
	{
		return;
	}

	// create a new evaluator and start the process of running
	Evaluator = NewObject<UQosEvaluator>();
	Evaluator->SetWorld(World);
	Evaluator->SetAnalyticsProvider(AnalyticsProvider);

	FQosParams Params;
	Params.ControllerId = 0;
	Params.bUseOldQosServers = bUseOldQosServers;
	Params.NumTestsPerRegion = NumTestsPerRegion;
	Params.Timeout = PingTimeout;
	Evaluator->FindDatacenters(Params, Datacenters, FOnQosSearchComplete::CreateUObject(this, &UQosRegionManager::OnQosEvaluationComplete));
}

void UQosRegionManager::OnQosEvaluationComplete(EQosCompletionResult Result, const TArray<FQosRegionInfo>& RegionInfo)
{
	// toss the evaluator
	Evaluator = nullptr;
	QosEvalResult = Result;
	RegionOptions.Empty(RegionInfo.Num());

	// Always capture the region information (its still correct, even if in a bad state)
	RegionOptions = RegionInfo;

	LastCheckTimestamp = FDateTime::UtcNow();

	if (!SelectedRegionId.IsEmpty() && SelectedRegionId == TEXT("NONE"))
	{
		// Put the dev region back into the list and select it
		ForceSelectRegion(SelectedRegionId);
	}

	// treat lack of any regions as a failure
	if (RegionInfo.Num() <= 0)
	{
		QosEvalResult = EQosCompletionResult::Failure;
	}

	if (QosEvalResult == EQosCompletionResult::Success)
	{
		if (RegionOptions.Num() > 0)
		{
			TrySetDefaultRegion();
		}
	}
	
	// fire notifications
	TArray<FSimpleDelegate> NotifyList = OnQosEvalCompleteDelegate;
	OnQosEvalCompleteDelegate.Empty();
	for (const auto& Callback : NotifyList)
	{
		Callback.ExecuteIfBound();
	}
}

FString UQosRegionManager::GetRegionId() const
{
	if (!ForceRegionId.IsEmpty())
	{
		// we may have updated INI to bypass this process
		return ForceRegionId;
	}

	if (QosEvalResult == EQosCompletionResult::Invalid)
	{
		// if we haven't run the evaluator just use the region from settings
		// development dedicated server will come here, live services should use -mcpregion
		return TEXT("NONE");
	}

	return SelectedRegionId;
}

const TArray<FQosRegionInfo>& UQosRegionManager::GetRegionOptions() const
{
	return RegionOptions;
}

void UQosRegionManager::ForceSelectRegion(const FString& InRegionId)
{
	QosEvalResult = EQosCompletionResult::Success;
	ForceRegionId.Empty(); // remove any override (not typically used)

	// make sure we can select this region
	FString RegionId = InRegionId.ToUpper();
	if (!SetSelectedRegion(RegionId))
	{
		// if not, add a fake entry and try again
		FQosRegionInfo RegionInfo;
		RegionInfo.Region.DisplayName = NSLOCTEXT("MMRegion", "Dev", "Development");
		RegionInfo.Region.RegionId = RegionId;
		RegionInfo.Region.bEnabled = true;
		RegionInfo.Region.bVisible = true;
		RegionInfo.Region.bBeta = false;
		RegionInfo.Result = EQosCompletionResult::Success;
		RegionInfo.AvgPingMs = 0;
		RegionOptions.Add(RegionInfo);
		verify(SetSelectedRegion(RegionId));
	}
}

void UQosRegionManager::TrySetDefaultRegion()
{
	if (!IsRunningDedicatedServer())
	{
		// Try to set a default region if one hasn't already been selected
		if (!SetSelectedRegion(GetRegionId()))
		{
			// try to select the lowest ping
			int32 BestPing = INT_MAX;
			FString BestRegionId;
			for (const FQosRegionInfo& Region : RegionOptions)
			{
				if (!Region.Region.bBeta && Region.AvgPingMs < BestPing)
				{
					BestPing = Region.AvgPingMs;
					BestRegionId = Region.Region.RegionId;
				}
			}
			if (!SetSelectedRegion(BestRegionId))
			{
				UE_LOG(LogQos, Warning, TEXT("Unable to set a good region!"));
				UE_LOG(LogQos, Warning, TEXT("Wanted to set %s, failed to fall back to %s"), *GetRegionId(), *BestRegionId);
				DumpRegionStats();
			}
		}
	}
}

bool UQosRegionManager::SetSelectedRegion(const FString& InRegionId)
{
	// make sure we've enumerated
	if (QosEvalResult != EQosCompletionResult::Success)
	{
		// can't select region until we've enumerated them
		return false;
	}
	
	// make sure it's in the option list
	FString RegionId = InRegionId.ToUpper();
	for (const FQosRegionInfo& RegionInfo : RegionOptions)
	{
		if (RegionInfo.Region.RegionId == RegionId)
		{
			if (RegionInfo.IsUsable())
			{
				SelectedRegionId = RegionId;
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	// can't select a region not in the options list
	return false;
}

void UQosRegionManager::DumpRegionStats()
{
	UE_LOG(LogQos, Display, TEXT("Region Info:"));
	UE_LOG(LogQos, Display, TEXT("Current: %s "), *SelectedRegionId);
	if (!ForceRegionId.IsEmpty())
	{
		UE_LOG(LogQos, Display, TEXT("Forced: %s "), *ForceRegionId);
	}
	
	for (const FQosRegionInfo& Region : RegionOptions)
	{
		UE_LOG(LogQos, Display, TEXT("Region: %s [%s] Ping: %d"), *Region.Region.DisplayName.ToString(), *Region.Region.RegionId, Region.AvgPingMs);
		UE_LOG(LogQos, Display, TEXT("\tEnabled: %d Visible: %d Beta: %d Result: %s"), Region.Region.bEnabled, Region.Region.bVisible, Region.Region.bBeta, ToString(Region.Result));
	}
}

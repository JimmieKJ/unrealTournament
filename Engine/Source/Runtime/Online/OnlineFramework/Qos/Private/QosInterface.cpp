// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "QosPrivatePCH.h"
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

//static 
TSharedRef<FQosInterface> FQosInterface::Get()
{
	return FQosModule::Get().GetQosInterface();
}

FQosInterface::FQosInterface()
	: MaximumPingMs(250)
	, BestRegionPingMs(MAX_int32)
	, QosEvalResult(EQosCompletionResult::Invalid)
{
	check(GConfig);
	GConfig->GetInt(TEXT("Qos"), TEXT("MinimumPingMs"), MaximumPingMs, GGameIni);
	GConfig->GetString(TEXT("Qos"), TEXT("ForceRegionId"), ForceRegionId, GGameIni);

	// get a forced region id from the command line as an override
	FParse::Value(FCommandLine::Get(), TEXT("McpRegion="), ForceRegionId);
}

// static
FString FQosInterface::GetDatacenterId()
{
	struct FDcidInfo
	{
		FDcidInfo()
		{
			FString OverrideDCID;
			if (FParse::Value(FCommandLine::Get(), TEXT("DCID="), OverrideDCID))
			{
				// Region specified on command line
				DCIDString = OverrideDCID.ToUpper();
			}
			else
			{
				FString DefaultDCID;
				check(GConfig);
				if (GConfig->GetString(TEXT("Qos"), TEXT("DCID"), DefaultDCID, GGameIni))
				{
					// Region specified in ini file
					DCIDString = DefaultDCID.ToUpper();
				}
			}
		}

		FString DCIDString;
	};
	static FDcidInfo DCID;
	return DCID.DCIDString;
}

void FQosInterface::BeginQosEvaluation(UWorld* World, const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider, const FSimpleDelegate& OnComplete)
{
	check(World);

	// no point doing the qos tests at all if we're forcing this
	if (!ForceRegionId.IsEmpty())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([OnComplete]() {
			OnComplete.ExecuteIfBound();
		}));
		return;
	}

	// add to the completion delegate
	OnQosEvalCompleteDelegate.Add(OnComplete);

	// if we're already evaluating, simply return
	if (Evaluator.IsValid())
	{
		return;
	}

	// create a new evaluator and start the process of running
	Evaluator = MakeShareable(new FQosEvaluator(World));
	Evaluator->SetAnalyticsProvider(AnalyticsProvider);
	Evaluator->FindDatacenters(0, FOnQosSearchComplete::CreateSP(this, &FQosInterface::OnQosEvaluationComplete));
}

bool FQosInterface::DidRegionTestSucceed() const
{
	return QosEvalResult == EQosCompletionResult::Success;
}

void FQosInterface::OnQosEvaluationComplete(EQosCompletionResult Result, const TArray<FQosRegionInfo>& RegionInfo)
{
	// toss this object
	Evaluator.Reset();
	QosEvalResult = Result;
	SelectedRegion.Empty();
	RegionOptions.Empty(RegionInfo.Num());

	// treat lack of any regions as a failure
	if (RegionInfo.Num() <= 0)
	{
		QosEvalResult = EQosCompletionResult::Failure;
	}

	BestRegionPingMs = MAX_int32;

	if (QosEvalResult == EQosCompletionResult::Success)
	{
		// copy over region info that meets our minimum ping
		for (const FQosRegionInfo& Region : RegionInfo)
		{
			BestRegionPingMs = FMath::Min(Region.PingMs, BestRegionPingMs);
			if ((MaximumPingMs <= 0) || (Region.PingMs <= MaximumPingMs))
			{
				UE_LOG(LogQos, Log, TEXT("Region: %s Ping: %d ACCEPTED"), *Region.RegionId, Region.PingMs);
				RegionOptions.Add(Region);
			}
			else
			{
				UE_LOG(LogQos, Log, TEXT("Region: %s Ping: %d REJECTED"), *Region.RegionId, Region.PingMs);
			}
		}

		if (RegionOptions.Num() > 0)
		{
			// try to select the default region
			if (!SetSelectedRegion(GetSavedRegionId()))
			{
				// try to select the lowest ping
				int32 BestPing = INT_MAX;
				FString BestRegionId;
				for (const FQosRegionInfo& Region : RegionOptions)
				{
					if (Region.PingMs < BestPing)
					{
						BestPing = Region.PingMs;
						BestRegionId = Region.RegionId;
					}
				}
				SetSelectedRegion(BestRegionId);
			}
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

FString FQosInterface::GetRegionId() const
{
	if (!ForceRegionId.IsEmpty())
	{
		// we may have updated INI to bypass this process
		return ForceRegionId;
	}
	if (QosEvalResult == EQosCompletionResult::Invalid)
	{
		// if we haven't run the evaluator just use the region from settings
		return GetSavedRegionId();
	}
	return SelectedRegion;
}

int32 FQosInterface::GetLowestReportedPing() const
{
	return BestRegionPingMs;
}

const TArray<FQosRegionInfo>& FQosInterface::GetRegionOptions() const
{
	return RegionOptions;
}

void FQosInterface::ForceSelectRegion(const FString& InRegionId)
{
	QosEvalResult = EQosCompletionResult::Success;
	ForceRegionId.Empty(); // remove any override (not typically used)

	// make sure we can select this region
	FString RegionId = InRegionId.ToUpper();
	if (!SetSelectedRegion(RegionId))
	{
		// if not, add a fake entry and try again
		FQosRegionInfo RegionInfo;
		RegionInfo.RegionId = RegionId;
		RegionInfo.PingMs = 0;
		RegionOptions.Add(RegionInfo);
		verify(SetSelectedRegion(RegionId));
	}
}

bool FQosInterface::SetSelectedRegion(const FString& InRegionId)
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
		if (RegionInfo.RegionId == RegionId)
		{
			// ok, save it
			SelectedRegion = RegionId;

			// apply this to saved settings
			SaveSelectedRegionId();
			return true;
		}
	}

	// can't select a region not in the options list
	return false;
}

void FQosInterface::SaveSelectedRegionId()
{
	// TODO: write to saved settings
}

FString FQosInterface::GetSavedRegionId() const
{
	// check saved settings
	// TODO:

	// if nothing is set, return hardcoded default
	return TEXT("NONE");
}
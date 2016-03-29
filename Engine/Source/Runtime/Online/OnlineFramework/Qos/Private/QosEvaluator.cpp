// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "QosPrivatePCH.h"
#include "QosEvaluator.h"
#include "QosInterface.h"
#include "QosBeaconClient.h"
#include "QosStats.h"
#include "OnlineSubsystemUtils.h"

FOnlineSessionSearchQos::FOnlineSessionSearchQos()
{
	bIsLanQuery = false;
	MaxSearchResults = 100;

	FString GameModeStr(GAMEMODE_QOS);
	QuerySettings.Set(SETTING_GAMEMODE, GameModeStr, EOnlineComparisonOp::Equals);
	QuerySettings.Set(SETTING_QOS, 1, EOnlineComparisonOp::Equals);
}

TSharedPtr<FOnlineSessionSettings> FOnlineSessionSearchQos::GetDefaultSessionSettings() const
{
	return MakeShareable(new FOnlineSessionSettingsQos());
}

void FOnlineSessionSearchQos::SortSearchResults()
{
	TMap<FString, int32> RegionCounts;

	static const int32 MaxPerRegion = 3;

	UE_LOG(LogQos, Verbose, TEXT("Sorting QoS results"));
	for (int32 SearchResultIdx = 0; SearchResultIdx < SearchResults.Num();)
	{
		FString QosRegion;
		FOnlineSessionSearchResult& SearchResult = SearchResults[SearchResultIdx];

		if (!SearchResult.Session.SessionSettings.Get(SETTING_REGION, QosRegion) || QosRegion.IsEmpty())
		{
			UE_LOG(LogQos, Verbose, TEXT("Removed Qos search result, invalid region."));
			SearchResults.RemoveAtSwap(SearchResultIdx);
			continue;
		}

		int32& ResultCount = RegionCounts.FindOrAdd(QosRegion);
		ResultCount++;
		if (ResultCount > MaxPerRegion)
		{
			SearchResults.RemoveAtSwap(SearchResultIdx);
			continue;
		}

		SearchResultIdx++;
	}

	for (auto& It : RegionCounts)
	{
		UE_LOG(LogQos, Verbose, TEXT("Region: %s Count: %d"), *It.Key, It.Value);
	}
}

FQosEvaluator::FQosEvaluator(UWorld* World) 
:	ParentWorld(World)
,	QosSearchQuery(nullptr)
,	QosBeaconClient(nullptr)
,	bInProgress(false)
,	bCancelOperation(false)
,	AnalyticsProvider(nullptr)
,	QosStats(nullptr)
{
	check(World != nullptr);
}

void FQosEvaluator::SetAnalyticsProvider(TSharedPtr<IAnalyticsProvider> InAnalyticsProvider)
{
	AnalyticsProvider = InAnalyticsProvider;
}

void FQosEvaluator::Cancel()
{
	bCancelOperation = true;
}

void FQosEvaluator::FindDatacenters(int32 ControllerId, const FOnQosSearchComplete& InCompletionDelegate)
{
	EQosCompletionResult Result = EQosCompletionResult::Failure;

	if (!bInProgress)
	{
		bInProgress = true;

		double CurTimestamp = FPlatformTime::Seconds();

		StartAnalytics();

		IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
		if (OnlineSub)
		{
			IOnlineSessionPtr SessionInt = OnlineSub->GetSessionInterface();
			if (SessionInt.IsValid())
			{
				const TSharedRef<FOnlineSessionSearch> QosSearchParams = MakeShareable(new FOnlineSessionSearchQos());
				QosSearchQuery = QosSearchParams;

				if ( OnFindDatacentersCompleteDelegateHandle.IsValid() )
				{
					SessionInt->ClearOnFindSessionsCompleteDelegate_Handle( OnFindDatacentersCompleteDelegateHandle );
				}
				// now make a new bind
				{
					FOnFindSessionsCompleteDelegate OnFindDatacentersCompleteDelegate = FOnFindSessionsCompleteDelegate::CreateSP( this, &FQosEvaluator::OnFindDatacentersComplete, InCompletionDelegate );
					OnFindDatacentersCompleteDelegateHandle = SessionInt->AddOnFindSessionsCompleteDelegate_Handle( OnFindDatacentersCompleteDelegate );
				}

				SessionInt->FindSessions(ControllerId, QosSearchParams);
				Result = EQosCompletionResult::Success;
			}
		}

		if (Result != EQosCompletionResult::Success)
		{
			TWeakPtr<FQosEvaluator> WeakThisCap(AsShared());
			GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([InCompletionDelegate, Result, WeakThisCap]() {
				auto StrongThis = WeakThisCap.Pin();
				if (StrongThis.IsValid())
				{
					StrongThis->FinalizeDatacenterResult(InCompletionDelegate, Result, TArray<FQosRegionInfo>());
				}
			}));
		}
	}
	else
	{
		UE_LOG(LogQos, Log, TEXT("Qos evaluation already in progress, ignoring"));
		// Just trigger delegate now (Finalize resets state vars)
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([InCompletionDelegate]() {
			InCompletionDelegate.ExecuteIfBound(EQosCompletionResult::Failure, TArray<FQosRegionInfo>());
		}));
	}
}

void FQosEvaluator::OnFindDatacentersComplete(bool bWasSuccessful, FOnQosSearchComplete InCompletionDelegate)
{
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	if (OnlineSub)
	{
		IOnlineSessionPtr SessionInt = OnlineSub->GetSessionInterface();
		if (SessionInt.IsValid())
		{
			SessionInt->ClearOnFindSessionsCompleteDelegate_Handle(OnFindDatacentersCompleteDelegateHandle);
		}
	}

	if (!bCancelOperation)
	{
		if (bWasSuccessful)
		{
			// Mark not in progress so it falls into EvaluateServerPing
			bInProgress = false;

			FOnQosPingEvalComplete CompletionDelegate = FOnQosPingEvalComplete::CreateSP(this, &FQosEvaluator::OnEvaluateForDatacenterComplete, InCompletionDelegate);
			EvaluateServerPing(QosSearchQuery, CompletionDelegate);
		}
		else
		{
			// Assume users default region and continue
			OnEvaluateForDatacenterComplete(EQosCompletionResult::Failure, InCompletionDelegate);
		}
	}
	else
	{
		OnEvaluateForDatacenterComplete(EQosCompletionResult::Canceled, InCompletionDelegate);
	}
}

void FQosEvaluator::OnEvaluateForDatacenterComplete(EQosCompletionResult Result, FOnQosSearchComplete InCompletionDelegate)
{
	TArray<FQosRegionInfo> RegionInfo;
	if (Result == EQosCompletionResult::Success)
	{
		if (QosSearchQuery.IsValid() && QosSearchQuery->SearchResults.Num() > 0)
		{
			struct FQosResults
			{
				int32 TotalPingInMs;
				int32 NumResults;
				int32 AvgPing;
				FQosResults() :
					TotalPingInMs(0),
					NumResults(0),
					AvgPing(MAX_QUERY_PING)
				{}
			};

			TMap<FString, FQosResults> RegionAggregates;
			for (const FOnlineSessionSearchResult& PotentialDatacenter : QosSearchQuery->SearchResults)
			{
				FString TmpRegion;
				if (PotentialDatacenter.Session.SessionSettings.Get(SETTING_REGION, TmpRegion) && !TmpRegion.IsEmpty())
				{
					FQosResults& QosResult = RegionAggregates.FindOrAdd(TmpRegion);
					if (PotentialDatacenter.PingInMs != MAX_QUERY_PING)
					{
						QosResult.TotalPingInMs += PotentialDatacenter.PingInMs;
						QosResult.NumResults++;
					}
					else
					{
						// Penalize unreachable Qos?
						QosResult.TotalPingInMs += 150;
						QosResult.NumResults++;
						UE_LOG(LogQos, Log, TEXT("Region[%s]: qos unreachable"), *TmpRegion);
					}
				}
				else
				{
					UE_LOG(LogQos, Log, TEXT("Qos search result missing region identifier"));
				}
			}

			//@TODO: This is a temporary measure to reduce the effect of framerate on data center ping estimation
			// (due to the use of beacons that are ticked on the game thread for the estimation)
			// This value can be 0 to disable discounting or something like 1 or 2
			const float FractionOfFrameTimeToDiscount = 2.0f;

			extern ENGINE_API float GAverageMS;
			const int32 TimeToDiscount = (int32)(FractionOfFrameTimeToDiscount * GAverageMS);

			for (auto& QosResult : RegionAggregates)
			{
				int32 RawAvgPing = MAX_QUERY_PING;
				int32 AdjustedAvgPing = RawAvgPing;
				if (QosResult.Value.NumResults > 0)
				{
					RawAvgPing = QosResult.Value.TotalPingInMs / QosResult.Value.NumResults;
					AdjustedAvgPing = FMath::Max<int32>(RawAvgPing - TimeToDiscount, 1);

					FQosRegionInfo Region;
					Region.RegionId = QosResult.Key;
					Region.PingMs = AdjustedAvgPing;
					RegionInfo.Add(Region);
				}

				UE_LOG(LogQos, Verbose, TEXT("Region[%s] Avg: %d Num: %d; Adjusted: %d"), *QosResult.Key, RawAvgPing, QosResult.Value.NumResults, AdjustedAvgPing);

				if (QosStats.IsValid())
				{
					QosStats->RecordRegionInfo(QosResult.Key, AdjustedAvgPing, QosResult.Value.NumResults);
				}
			}
		}
		else
		{
			UE_LOG(LogQos, Log, TEXT("Qos search ended with no results"));
		}
	}

	FinalizeDatacenterResult(InCompletionDelegate, Result, RegionInfo);
}

void FQosEvaluator::FinalizeDatacenterResult(const FOnQosSearchComplete& InCompletionDelegate, EQosCompletionResult CompletionResult, const TArray<FQosRegionInfo>& RegionInfo)
{
	UE_LOG(LogQos, Log, TEXT("Datacenter evaluation complete. Result: %s "), ToString(CompletionResult));

	EndAnalytics(CompletionResult);

	// Broadcast this data next frame
	TWeakPtr<FQosEvaluator> WeakThisCap(AsShared());
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([InCompletionDelegate, CompletionResult, RegionInfo, WeakThisCap]() {
		InCompletionDelegate.ExecuteIfBound(CompletionResult, RegionInfo); 
		auto StrongThis = WeakThisCap.Pin();
		if (StrongThis.IsValid())
		{
			StrongThis->ResetSearchVars();
		}
	}));
}

void FQosEvaluator::EvaluateServerPing(TSharedPtr<FOnlineSessionSearch>& SearchResults, const FOnQosPingEvalComplete& InCompletionDelegate)
{
	if (!bInProgress)
	{
		bInProgress = true;
		OnQosPingEvalComplete = InCompletionDelegate;

		if (SearchResults.IsValid())
		{
			if (SearchResults->SearchResults.Num() > 0)
			{
				QosSearchQuery = SearchResults;
				ContinuePingServers();
			}
			else
			{
				FinalizePingServers(EQosCompletionResult::Success);
			}
		}
		else
		{
			FinalizePingServers(EQosCompletionResult::Failure);
		}
	}
	else
	{
		InCompletionDelegate.ExecuteIfBound(EQosCompletionResult::Failure);
	}
}

void FQosEvaluator::ContinuePingServers()
{
	if (!bCancelOperation)
	{
		CurrentSearchPass.CurrentSessionIdx++;
		if (QosSearchQuery.IsValid() && QosSearchQuery->SearchResults.IsValidIndex(CurrentSearchPass.CurrentSessionIdx))
		{
			// There are more valid search results, keep attempting Qos requests
			UWorld* World = GetWorld();
			QosBeaconClient = World->SpawnActor<AQosBeaconClient>(AQosBeaconClient::StaticClass());

			QosBeaconClient->OnQosRequestComplete().BindSP(this, &FQosEvaluator::OnQosRequestComplete);
			QosBeaconClient->OnHostConnectionFailure().BindSP(this, &FQosEvaluator::OnQosConnectionFailure);
			QosBeaconClient->SendQosRequest(QosSearchQuery->SearchResults[CurrentSearchPass.CurrentSessionIdx]);
		}
		else
		{
			// Ran out of search results, notify the caller
			FinalizePingServers(EQosCompletionResult::Success);
		}
	}
	else
	{
		// Operation cancelled
		FinalizePingServers(EQosCompletionResult::Canceled);
	}
}

void FQosEvaluator::FinalizePingServers(EQosCompletionResult Result)
{
	UE_LOG(LogQos, Log, TEXT("Ping evaluation complete. Result:%s"), ToString(Result));

	TWeakPtr<FQosEvaluator> WeakThisCap(AsShared());
	FTimerDelegate TimerDelegate = FTimerDelegate::CreateLambda([Result, WeakThisCap]() {
		auto StrongThis = WeakThisCap.Pin();
		if (StrongThis.IsValid())
		{
			StrongThis->OnQosPingEvalComplete.ExecuteIfBound(Result);
			StrongThis->ResetSearchVars();
		}
	});
	GetWorldTimerManager().SetTimerForNextTick(TimerDelegate);
}

void FQosEvaluator::OnQosRequestComplete(EQosResponseType QosResponse, int32 ResponseTime)
{
	if (QosBeaconClient.IsValid())
	{
		DestroyClientBeacons();
	}

	if (QosSearchQuery.IsValid())
	{
		FOnlineSessionSearchResult& SearchResult = QosSearchQuery->SearchResults[CurrentSearchPass.CurrentSessionIdx];
		if (QosResponse == EQosResponseType::Success)
		{
			SearchResult.PingInMs = ResponseTime;
		}
		else
		{
			SearchResult.PingInMs = MAX_QUERY_PING;
		}

		FString QosRegion;
		SearchResult.Session.SessionSettings.Get(SETTING_REGION, QosRegion);

		extern ENGINE_API float GAverageFPS;
		extern ENGINE_API float GAverageMS;
		UE_LOG(LogQos, Verbose, TEXT("Qos response received for region %s: %d ms FPS: %0.2f MS: %0.2f"), *QosRegion, ResponseTime, GAverageFPS, GAverageMS);

		if (QosStats.IsValid())
		{
			QosStats->RecordQosAttempt(SearchResult, QosResponse == EQosResponseType::Success);
		}
		
		// Cancel operation will occur next tick if applicable
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateSP(this, &FQosEvaluator::ContinuePingServers));
	}
}

void FQosEvaluator::OnQosConnectionFailure()
{
	OnQosRequestComplete(EQosResponseType::Failure, MAX_QUERY_PING);
}

void FQosEvaluator::ResetSearchVars()
{
	bInProgress = false;
	bCancelOperation = false;
	CurrentSearchPass.CurrentSessionIdx = INDEX_NONE;
	OnQosPingEvalComplete.Unbind();
	QosSearchQuery = nullptr;
}

void FQosEvaluator::DestroyClientBeacons()
{
	if (QosBeaconClient.IsValid())
	{
		QosBeaconClient->OnQosRequestComplete().Unbind();
		QosBeaconClient->OnHostConnectionFailure().Unbind();
		QosBeaconClient->DestroyBeacon();
		QosBeaconClient.Reset();
	}
}

void FQosEvaluator::StartAnalytics()
{
	if (AnalyticsProvider.IsValid())
	{
		ensure(!QosStats.IsValid());
		QosStats = MakeShareable(new FQosDatacenterStats());
		QosStats->StartQosPass();
	}
}

void FQosEvaluator::EndAnalytics(EQosCompletionResult CompletionResult)
{
	if (QosStats.IsValid())
	{
		if (CompletionResult != EQosCompletionResult::Canceled)
		{
			EDatacenterResultType ResultType = EDatacenterResultType::Failure;
			if (CompletionResult != EQosCompletionResult::Failure)
			{
				ResultType = EDatacenterResultType::Normal;
			}

			QosStats->EndQosPass(ResultType);
			QosStats->Upload(AnalyticsProvider);
		}

		QosStats = nullptr;
	}
}

UWorld* FQosEvaluator::GetWorld() const
{
	UWorld* World = ParentWorld.Get();
	check(World);
	return World;
}

FTimerManager& FQosEvaluator::GetWorldTimerManager() const
{
	return GetWorld()->GetTimerManager();
}

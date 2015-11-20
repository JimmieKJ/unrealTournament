// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "QosPrivatePCH.h"
#include "QosEvaluator.h"
#include "QosInterface.h"
#include "QosBeaconClient.h"
#include "QosStats.h"
#include "OnlineSubsystemUtils.h"

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
	Set(SETTING_REGION, UQosEvaluator::GetDefaultRegionString(), EOnlineDataAdvertisementType::ViaOnlineService);
	bIsDedicated = bInIsDedicated;
}

FOnlineSessionSearchQos::FOnlineSessionSearchQos()
{
	bIsLanQuery = false;
	MaxSearchResults = 100;

	FString GameModeStr(GAMEMODE_QOS);
	QuerySettings.Set(SETTING_GAMEMODE, GameModeStr, EOnlineComparisonOp::Equals);
	QuerySettings.Set(SETTING_QOS, 1, EOnlineComparisonOp::Equals);
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


UQosEvaluator::UQosEvaluator(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	QosSearchQuery(nullptr),
	QosBeaconClient(nullptr),
	bInProgress(false),
	bCancelOperation(false),
	AnalyticsProvider(nullptr),
	QosStats(nullptr)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		SearchState.DatacenterId = GetDefaultRegionString();
	}
}

void UQosEvaluator::SetAnalyticsProvider(TSharedPtr<IAnalyticsProvider> InAnalyticsProvider)
{
	AnalyticsProvider = InAnalyticsProvider;
}

void UQosEvaluator::Cancel()
{
	bCancelOperation = true;
}

void UQosEvaluator::FindDatacenters(int32 ControllerId, const FOnQosSearchComplete& InCompletionDelegate)
{
	EQosCompletionResult Result = EQosCompletionResult::Failure;

	if (!bInProgress)
	{
		bInProgress = true;

		double CurTimestamp = FPlatformTime::Seconds();

		StartAnalytics();

		bool bForcedCached = QOSConsoleVariables::CVarForceCached.GetValueOnGameThread() != 0;
		bool bOverrideTimestamp = QOSConsoleVariables::CVarOverrideTimestamp.GetValueOnGameThread() != 0;
		if (!bForcedCached && ((CurTimestamp - SearchState.LastDatacenterIdTimestamp >= DATACENTERQUERY_INTERVAL) || bOverrideTimestamp))
		{
			IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
			if (OnlineSub)
			{
				IOnlineSessionPtr SessionInt = OnlineSub->GetSessionInterface();
				if (SessionInt.IsValid())
				{
					const TSharedRef<FOnlineSessionSearch> QosSearchParams = MakeShareable(new FOnlineSessionSearchQos());
					QosSearchQuery = QosSearchParams;

					FOnFindSessionsCompleteDelegate OnFindDatacentersCompleteDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindDatacentersComplete, InCompletionDelegate);
					OnFindDatacentersCompleteDelegateHandle = SessionInt->AddOnFindSessionsCompleteDelegate_Handle(OnFindDatacentersCompleteDelegate);
					SessionInt->FindSessions(ControllerId, QosSearchParams);
					Result = EQosCompletionResult::Success;
				}
			}
		}
		else
		{
			Result = EQosCompletionResult::Cached;
		}

		if (Result != EQosCompletionResult::Success)
		{
			FinalizeDatacenterResult(InCompletionDelegate, Result);
		}
	}
	else
	{
		UE_LOG(LogQos, Log, TEXT("Qos evaluation already in progress, ignoring"));
		// Just trigger delegate now (Finalize resets state vars)
		InCompletionDelegate.ExecuteIfBound(Result, SearchState.DatacenterId);
	}
}

void UQosEvaluator::OnFindDatacentersComplete(bool bWasSuccessful, FOnQosSearchComplete InCompletionDelegate)
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

			FOnQosPingEvalComplete CompletionDelegate = FOnQosPingEvalComplete::CreateUObject(this, &ThisClass::OnEvaluateForDatacenterComplete, InCompletionDelegate);
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

void UQosEvaluator::OnEvaluateForDatacenterComplete(EQosCompletionResult Result, FOnQosSearchComplete InCompletionDelegate)
{
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

			int32 BestAvgPing = MAX_QUERY_PING;
			for (auto& QosResult : RegionAggregates)
			{
				if (QosResult.Value.NumResults > 0)
				{
					QosResult.Value.AvgPing = QosResult.Value.TotalPingInMs / QosResult.Value.NumResults;
					if (QosResult.Value.AvgPing < BestAvgPing)
					{
						BestAvgPing = QosResult.Value.AvgPing;
						SearchState.DatacenterId = QosResult.Key;
					}
				}

				UE_LOG(LogQos, Verbose, TEXT("Region[%s] Avg: %d Num: %d"), *QosResult.Key, QosResult.Value.AvgPing, QosResult.Value.NumResults);

				if (QosStats.IsValid())
				{
					QosStats->RecordRegionInfo(QosResult.Key, QosResult.Value.AvgPing, QosResult.Value.NumResults);
				}
			}

			SearchState.LastDatacenterIdTimestamp = FPlatformTime::Seconds();
		}
		else
		{
			UE_LOG(LogQos, Log, TEXT("Qos search ended with no results"));
		}
	}

	FinalizeDatacenterResult(InCompletionDelegate, Result);
}

void UQosEvaluator::FinalizeDatacenterResult(const FOnQosSearchComplete& InCompletionDelegate, EQosCompletionResult CompletionResult)
{
	bool bForceDefault = QOSConsoleVariables::CVarForceDefaultRegion.GetValueOnGameThread() != 0;
	FString ForceRegion = QOSConsoleVariables::CVarForceRegion.GetValueOnGameThread();

	if (!ForceRegion.IsEmpty())
	{
		SearchState.DatacenterId = ForceRegion.ToUpper();
	}
	else if (bForceDefault)
	{
		SearchState.DatacenterId = GetDefaultRegionString();
	}

	UE_LOG(LogQos, Log, TEXT("Datacenter evaluation complete. Result: %s Region: %s%s"), 
		ToString(CompletionResult), *SearchState.DatacenterId, ForceRegion.IsEmpty() ? (bForceDefault ? TEXT("[Default]") : TEXT("")) : TEXT("[Forced]")
		);


	EndAnalytics(CompletionResult);

	// Broadcast this data next frame
	FString& DatacenterId = SearchState.DatacenterId;
	FTimerDelegate TimerDelegate = FTimerDelegate::CreateLambda(
		[InCompletionDelegate, CompletionResult, DatacenterId, this](){ InCompletionDelegate.ExecuteIfBound(CompletionResult, DatacenterId); ResetSearchVars(); }
	);
	GetWorldTimerManager().SetTimerForNextTick(TimerDelegate);
}

void UQosEvaluator::EvaluateServerPing(TSharedPtr<FOnlineSessionSearch>& SearchResults, const FOnQosPingEvalComplete& InCompletionDelegate)
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

void UQosEvaluator::ContinuePingServers()
{
	if (!bCancelOperation)
	{
		CurrentSearchPass.CurrentSessionIdx++;
		if (QosSearchQuery.IsValid() && QosSearchQuery->SearchResults.IsValidIndex(CurrentSearchPass.CurrentSessionIdx))
		{
			// There are more valid search results, keep attempting Qos requests
			UWorld* World = GetWorld();
			QosBeaconClient = World->SpawnActor<AQosBeaconClient>(AQosBeaconClient::StaticClass());

			QosBeaconClient->OnQosRequestComplete().BindUObject(this, &ThisClass::OnQosRequestComplete);
			QosBeaconClient->OnHostConnectionFailure().BindUObject(this, &ThisClass::OnQosConnectionFailure);
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

void UQosEvaluator::FinalizePingServers(EQosCompletionResult Result)
{
	UE_LOG(LogQos, Log, TEXT("Ping evaluation complete. Result:%s"), ToString(Result));

	FTimerDelegate TimerDelegate = FTimerDelegate::CreateLambda(
		[Result, this](){ OnQosPingEvalComplete.ExecuteIfBound(Result); ResetSearchVars(); }
	);
	GetWorldTimerManager().SetTimerForNextTick(TimerDelegate);
}

void UQosEvaluator::OnQosRequestComplete(EQosResponseType QosResponse, int32 ResponseTime)
{
	if (QosBeaconClient)
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

		UE_LOG(LogQos, Verbose, TEXT("Qos response received: %d ms"), ResponseTime);

		if (QosStats.IsValid())
		{
			QosStats->RecordQosAttempt(SearchResult, QosResponse == EQosResponseType::Success);
		}
		
		// Cancel operation will occur next tick if applicable
		GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::ContinuePingServers);
	}
}

void UQosEvaluator::OnQosConnectionFailure()
{
	OnQosRequestComplete(EQosResponseType::Failure, MAX_QUERY_PING);
}

void UQosEvaluator::ResetSearchVars()
{
	bInProgress = false;
	bCancelOperation = false;
	CurrentSearchPass.CurrentSessionIdx = INDEX_NONE;
	OnQosPingEvalComplete.Unbind();
	QosSearchQuery = nullptr;
}

void UQosEvaluator::DestroyClientBeacons()
{
	if (QosBeaconClient)
	{
		QosBeaconClient->OnQosRequestComplete().Unbind();
		QosBeaconClient->OnHostConnectionFailure().Unbind();
		QosBeaconClient->DestroyBeacon();
		QosBeaconClient = nullptr;
	}
}

void UQosEvaluator::StartAnalytics()
{
	if (AnalyticsProvider.IsValid())
	{
		ensure(!QosStats.IsValid());
		QosStats = MakeShareable(new FQosDatacenterStats());
		QosStats->StartQosPass();
	}
}

void UQosEvaluator::EndAnalytics(EQosCompletionResult CompletionResult)
{
	if (QosStats.IsValid())
	{
		if (CompletionResult != EQosCompletionResult::Canceled)
		{
			bool bForceDefault = QOSConsoleVariables::CVarForceDefaultRegion.GetValueOnGameThread() != 0;
			FString ForceRegion = QOSConsoleVariables::CVarForceRegion.GetValueOnGameThread();

			EDatacenterResultType ResultType = EDatacenterResultType::Failure;
			if (CompletionResult != EQosCompletionResult::Failure)
			{
				if (CompletionResult == EQosCompletionResult::Cached)
				{
					ResultType = EDatacenterResultType::Cached;
				}
				else
				{
					ResultType = EDatacenterResultType::Normal;
				}

				if (!ForceRegion.IsEmpty())
				{
					ResultType = EDatacenterResultType::Forced;
				}
				else if (bForceDefault)
				{
					ResultType = EDatacenterResultType::Default;
				}
			}

			QosStats->EndQosPass(ResultType, SearchState.DatacenterId);
			QosStats->Upload(AnalyticsProvider);
		}

		QosStats = nullptr;
	}
}

FString UQosEvaluator::GetDefaultRegionString()
{
	static FString RegionString = TEXT("");
	if (RegionString.IsEmpty())
	{
		FString OverrideRegion;
		if (FParse::Value(FCommandLine::Get(), TEXT("McpRegion="), OverrideRegion))
		{
			// Region specified on command line
			RegionString = OverrideRegion.ToUpper();
		}
		else
		{
			FString DefaultRegion;
			if (GConfig->GetString(TEXT("Qos"), TEXT("DefaultRegion"), DefaultRegion, GGameIni))
			{
				// Region specified in ini file
				RegionString = DefaultRegion.ToUpper();
			}
			else
			{
				// No Region specified. Assume USA.
				RegionString = TEXT("USA");
			}
		}
	}

	return RegionString;
}

UWorld* UQosEvaluator::GetWorld() const
{
	check(GEngine);
	check(GEngine->GameViewport);
	UGameInstance* GameInstance = GEngine->GameViewport->GetGameInstance();
	return GameInstance->GetWorld();
}

FTimerManager& UQosEvaluator::GetWorldTimerManager() const
{
	return GetWorld()->GetTimerManager();
}

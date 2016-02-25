// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSessionInterface.h"
#include "QosEvaluator.generated.h"

class AQosBeaconClient;
class FQosDatacenterStats;
enum class EQosResponseType : uint8;

#define GAMEMODE_QOS	TEXT("QOSSERVER")

/** Time before refreshing the datacenter id value during the next search */
#define DATACENTERQUERY_INTERVAL (5.0 * 60.0)

/** Enum for possible QoS return codes */
UENUM()
enum class EQosCompletionResult : uint8
{
	/** Incomplete, invalid result */
	Invalid,
	/** QoS operation was successful */
	Success,
	/** QoS operation was skipped in favor of a cached result */
	Cached,
	/** QoS operation ended in failure */
	Failure,
	/** QoS operation was canceled */
	Canceled
};

/**
 * Generic settings a server runs when hosting a simple QoS response service
 */
class QOS_API FOnlineSessionSettingsQos : public FOnlineSessionSettings
{
public:

	FOnlineSessionSettingsQos(bool bInIsDedicated = true);
	virtual ~FOnlineSessionSettingsQos() {}
};

/**
 * Search settings for QoS advertised sessions
 */
class QOS_API FOnlineSessionSearchQos : public FOnlineSessionSearch
{
public:
	FOnlineSessionSearchQos();
	virtual ~FOnlineSessionSearchQos() {}

	// FOnlineSessionSearch Interface begin
	virtual TSharedPtr<FOnlineSessionSettings> GetDefaultSessionSettings() const override { return MakeShareable(new FOnlineSessionSettingsQos()); }
	virtual void SortSearchResults() override;
	// FOnlineSessionSearch Interface end
};

/**
 * Internal state for a given search pass
 */
USTRUCT()
struct FQosSearchState
{
	GENERATED_USTRUCT_BODY()

	/** Datacenter Id */
	UPROPERTY()
	FString DatacenterId;
	/** Last datacenter lookup timestamp */
	UPROPERTY()
	double LastDatacenterIdTimestamp;

	FQosSearchState() :
		LastDatacenterIdTimestamp(0.0)
	{}
};

/**
 * Internal state for a given QoS pass
 */
USTRUCT()
struct FQosSearchPass
{
	GENERATED_USTRUCT_BODY()

	/** Current search result choice to test */
	UPROPERTY()
	int32 CurrentSessionIdx;
	/** Current array of QoS search results to evaluate */
	TArray<FOnlineSessionSearchResult> SearchResults;

	FQosSearchPass() :
		CurrentSessionIdx(INDEX_NONE)
	{}
};

/*
 * Delegate triggered when an evaluation of ping for all servers in a search query have completed
 *
 * @param Result the ping operation result
 */
DECLARE_DELEGATE_OneParam(FOnQosPingEvalComplete, EQosCompletionResult /** Result */);

/** 
 * Delegate triggered when all QoS search results have been investigated 
 *
 * @param Result the QoS operation result
 * @param BestDatacenterId the datacenter chosen by the operation (may be forced, cached, or default)
 */
DECLARE_DELEGATE_TwoParams(FOnQosSearchComplete, EQosCompletionResult /** Result */, const FString& /** BestDatacenterId */);

/**
 * Evaluates QoS metrics to determine the best datacenter under current conditions
 * Additionally capable of generically pinging an array of servers that have a QosBeaconHost active
 */
UCLASS(config = Engine, notplaceable)
class QOS_API UQosEvaluator : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * QoS services
	 */

	/**
	 * Find all the advertised datacenters and begin the process of evaluating ping results
	 * Will return the default datacenter in the event of failure or no advertised datacenters
	 *
	 * @param ControllerId id of player initiating request
	 * @param InCompletionDelegate delegate to fire when a datacenter choice has been made
	 */
	void FindDatacenters(int32 ControllerId, const FOnQosSearchComplete& InCompletionDelegate);

	/**
	 * Evaluate ping reachability for a given list of servers.  Assumes there is a AQosBeaconHost on the machine
	 * to receive the request and send a response
	 *
	 * @param SearchResults array of search results to test
	 * @param InCompletionDelegate delegate to fire when test is complete
	 */
	void EvaluateServerPing(TSharedPtr<FOnlineSessionSearch>& SearchResults, const FOnQosPingEvalComplete& InCompletionDelegate);

	/**
	 * Is a QoS operation active
	 *
	 * @return true if QoS is active, false otherwise
	 */
	bool IsActive() const { return bInProgress; }

	/**
	 * Cancel the current QoS operation at the earliest opportunity
	 */
	void Cancel();

	/**
	 * Get the default region for this instance, checking ini and commandline overrides
	 *
	 * @return the default region identifier
	 */
	static const FString& GetDefaultRegionString();

private:

	/** Current QoS data */
	UPROPERTY()
	FQosSearchState SearchState;
	/** Current QoS search/eval state */
	UPROPERTY()
	FQosSearchPass CurrentSearchPass;

	/** QoS search results */
	TSharedPtr<FOnlineSessionSearch> QosSearchQuery;

	/** Delegate for finding datacenters */
	FOnFindSessionsCompleteDelegate OnQosSessionsSearchCompleteDelegate;
	FOnQosPingEvalComplete OnQosPingEvalComplete;

	/** Beacon for sending QoS requests */
	UPROPERTY(Transient)
	AQosBeaconClient* QosBeaconClient;

	/** A QoS operation is in progress */
	bool bInProgress;
	/** Should cancel occur at the next available opportunity */
	bool bCancelOperation;

	/**
	 * Signal QoS operation has completed next frame
	 *
	 * @param InCompletionDelegate the delegate to trigger
	 * @param CompletionResult the QoS operation result
	 */
	void FinalizeDatacenterResult(const FOnQosSearchComplete& InCompletionDelegate, EQosCompletionResult CompletionResult);

	/**
	 * Continue with the next datacenter endpoint and gather its ping result
	 */
	void ContinuePingServers();

	/**
	 * Finalize the search results from pinging various datacenter endpoints to find the best datacenter
	 *
	 * @param Result the QoS operation result
	 */
	void FinalizePingServers(EQosCompletionResult Result);

	/**
	 * Delegate fired when all servers within a given search query have been evaluated
	 *
	 * @param Result the QoS operation result
	 * @param InCompletionDelegate the datacenter complete delegate to fire
	 */
	void OnEvaluateForDatacenterComplete(EQosCompletionResult Result, FOnQosSearchComplete InCompletionDelegate);

	/**
	 * Delegate fired when a datacenter search query has completed
	 *
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 * @param InCompletionDelegate the datacenter complete delegate to fire
	 */
	void OnFindDatacentersComplete(bool bWasSuccessful, FOnQosSearchComplete InCompletionDelegate);

	/**
	 * Delegate from the QoS beacon fired with each beacon response
	 *
	 * @param QosResponse the response from the server, success or otherwise
	 * @param ResponseTime the time to complete the request, or MAX_QUERY_PING if unsuccessful
	 */
	void OnQosRequestComplete(EQosResponseType QosResponse, int32 ResponseTime);

	/**
	 * Delegate from the QoS beacon fired when there is a failure to connect to an endpoint
	 */
	void OnQosConnectionFailure();

private:

	FDelegateHandle OnFindDatacentersCompleteDelegateHandle;

public:

	/**
	 * Analytics
	 */

	void SetAnalyticsProvider(TSharedPtr<IAnalyticsProvider> InAnalyticsProvider);

private:

	void StartAnalytics();
	void EndAnalytics(EQosCompletionResult CompletionResult);

	/** Reference to the provider to submit data to */
	TSharedPtr<IAnalyticsProvider> AnalyticsProvider;
	/** Stats related to these operations */
	TSharedPtr<FQosDatacenterStats> QosStats;

private:

	/**
	 * Helpers
	 */

	/** Reset all variables related to the QoS evaluator */
	void ResetSearchVars();

	/** Quick access to the current world */
	UWorld* GetWorld() const override;

	/** Quick access to the world timer manager */
	FTimerManager& GetWorldTimerManager() const;

	/** Client beacon cleanup */
	void DestroyClientBeacons();
};

inline const TCHAR* ToString(EQosCompletionResult Result)
{
	switch (Result)
	{
		case EQosCompletionResult::Invalid:
		{
			return TEXT("Invalid");
		}
		case EQosCompletionResult::Success:
		{
			return TEXT("Success");
		}
		case EQosCompletionResult::Cached:
		{
			return TEXT("Cached");
		}
		case EQosCompletionResult::Failure:
		{
			return TEXT("Failure");
		}
		case EQosCompletionResult::Canceled:
		{
			return TEXT("Canceled");
		}
	}

	return TEXT("");
}

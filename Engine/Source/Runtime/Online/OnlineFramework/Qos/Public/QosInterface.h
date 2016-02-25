// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class IAnalyticsProvider;
class FQosEvaluator;
enum class EQosCompletionResult : uint8;

/**
 * Information for a given region
 */
struct QOS_API FQosRegionInfo
{
	FString RegionId;
	int32 PingMs;
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
 * Main Qos interface for actions related to server quality of service
 */
class QOS_API FQosInterface : public TSharedFromThis<FQosInterface>
{
public:

	/**
	 * Get the interface singleton
	 */
	static TSharedRef<FQosInterface> Get();

	/**
	 * Start running the async QoS evaluation 
	 */
	void BeginQosEvaluation(UWorld* World, const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider, const FSimpleDelegate& OnComplete);

	/**
	 * Get the region ID for this instance, checking ini and commandline overrides.
	 * 
	 * Dedicated servers will have this value specified on the commandline
	 * 
	 * Clients pull this value from the settings (or command line) and do a ping test to determine if the setting is viable.
	 *
	 * @return the default region identifier
	 */
	FString GetRegionId() const;

	/**
	 * Returns the lowest ping of any datacenter we queried (or MAX_int32 if none were found or the queries have not completed yet)
	 */
	int32 GetLowestReportedPing() const;

	/**
	 * Get the list of regions that the client can choose from (returned from search and must meet min ping requirements)
	 *
	 * If this list is empty, the client cannot play.
	 */
	const TArray<FQosRegionInfo>& GetRegionOptions() const;

	/**
	 * return whether or not the region test succeeded
	 */
	bool DidRegionTestSucceed() const;

	/**
	 * Try to set the selected region ID (must be present in GetRegionOptions)
	 */
	bool SetSelectedRegion(const FString& RegionId);

	/**
	 * Force the selected region creating a fake RegionOption if necessary
	 */
	void ForceSelectRegion(const FString& RegionId);

	/**
	 * Get the datacenter id for this instance, checking ini and commandline overrides
	 * This is only relevant for dedicated servers (so they can advertise). Client does 
	 * not search on this (but may choose to prioritize results later)
	 *
	 * @return the default datacenter identifier
	 */
	static FString GetDatacenterId();

	/**
	 * Legacy static accessor for GetRegionId()
	 */
	inline static FString GetDefaultRegionString()
	{
		return FQosInterface::Get()->GetRegionId();
	}

protected:
	friend class FQosModule;
	FQosInterface();

private:

	void OnQosEvaluationComplete(EQosCompletionResult Result, const TArray<FQosRegionInfo>& RegionInfo);

	FString GetSavedRegionId() const;
	void SaveSelectedRegionId();

	// The threshold that a region ping must be below to consider as a valid option
	int32 MaximumPingMs;

	// The best region ping we found, even if we failed all tests
	int32 BestRegionPingMs;

	FString ForceRegionId;
	
	TSharedPtr<FQosEvaluator> Evaluator;
	EQosCompletionResult QosEvalResult;
	TArray<FQosRegionInfo> RegionOptions;
	FString SelectedRegion;

	TArray<FSimpleDelegate> OnQosEvalCompleteDelegate;
};


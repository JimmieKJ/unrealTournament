// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the AutomationController module.
 */
class FAutomationControllerManager
	: public IAutomationControllerManager
{
public:

public:

	// IAutomationController Interface

	virtual void RequestAvailableWorkers( const FGuid& InSessionId ) override;
	virtual void RequestTests() override;
	virtual void RunTests( const bool bIsLocalSession) override;
	virtual void StopTests() override;
	virtual void Init() override;
	virtual void RequestLoadAsset( const FString& InAssetName ) override;
	virtual void Tick() override;

	virtual void SetNumPasses(const int32 InNumPasses) override
	{
		NumTestPasses = InNumPasses;
	}

	virtual int32 GetNumPasses() override
	{
		return NumTestPasses;
	}

	virtual bool IsUsingFullSizeScreenshots() const override
	{
		return bScreenshotsEnabled && bRequestFullScreenScreenshots;
	}

	virtual void SetUsingFullSizeScreenshots( const bool bNewValue ) override
	{
		bRequestFullScreenScreenshots = bNewValue;
	}

	virtual bool IsSendAnalytics() const override
	{
		return bSendAnalytics;
	}

	virtual void SetSendAnalytics(const bool bNewValue) override
	{
		bSendAnalytics = bNewValue;
	}

	virtual bool IsScreenshotAllowed() const override
	{
		return bScreenshotsEnabled;
	}

	virtual void SetScreenshotsEnabled( const bool bNewValue ) override
	{
		bScreenshotsEnabled = bNewValue;
	}

	virtual void SetFilter( TSharedPtr< AutomationFilterCollection > InFilter ) override
	{
		ReportManager.SetFilter( InFilter );
	}

	virtual TArray <TSharedPtr <IAutomationReport> >& GetReports() override
	{
		return ReportManager.GetFilteredReports();
	}

	virtual int32 GetNumDeviceClusters() const override
	{
		return DeviceClusterManager.GetNumClusters();
	}

	virtual int32 GetNumDevicesInCluster(const int32 ClusterIndex) const override
	{
		return DeviceClusterManager.GetNumDevicesInCluster(ClusterIndex);
	}

	virtual FString GetClusterGroupName(const int32 ClusterIndex) const override
	{
		return DeviceClusterManager.GetClusterGroupName(ClusterIndex);
	}

	virtual FString GetDeviceTypeName(const int32 ClusterIndex) const override
	{
		return DeviceClusterManager.GetClusterDeviceType(ClusterIndex);
	}

	virtual FString GetGameInstanceName(const int32 ClusterIndex, const int32 DeviceIndex) const override
	{
		return DeviceClusterManager.GetClusterDeviceName(ClusterIndex, DeviceIndex);
	}

	virtual void SetVisibleTestsEnabled(const bool bEnabled) override
	{
		ReportManager.SetVisibleTestsEnabled (bEnabled);
	}

	virtual int32 GetEnabledTestsNum() const override
	{
		return ReportManager.GetEnabledTestsNum();
	}

	virtual void GetEnabledTestNames(TArray<FString>& OutEnabledTestNames) const override
	{
		ReportManager.GetEnabledTestNames(OutEnabledTestNames);
	}

	virtual void SetEnabledTests(const TArray<FString>& EnabledTests) override
	{
		ReportManager.SetEnabledTests(EnabledTests);
	}

	virtual EAutomationControllerModuleState::Type GetTestState( ) const override
	{
		return AutomationTestState;
	}

	virtual void SetDeveloperDirectoryIncluded(const bool bInDeveloperDirectoryIncluded) override
	{
		bDeveloperDirectoryIncluded = bInDeveloperDirectoryIncluded;
	}

	virtual bool IsDeveloperDirectoryIncluded(void) const override
	{
		return bDeveloperDirectoryIncluded;
	}

	virtual void SetRequestedTestFlags(const uint32 InRequestedTestFlags) override
	{
		RequestedTestFlags = InRequestedTestFlags;
		RequestTests();
	}

	virtual const bool CheckTestResultsAvailable() const override
	{
		return 	bTestResultsAvailable;
	}

	virtual const bool ReportsHaveErrors() const override
	{
		return bHasErrors;
	}

	virtual const bool ReportsHaveWarnings() const override
	{
		return bHasWarning;
	}

	virtual const bool ReportsHaveLogs() const override
	{
		return bHasLogs;
	}

	virtual void ClearAutomationReports() override
	{
		ReportManager.Empty();
	}

	virtual const bool ExportReport(uint32 FileExportTypeMask) override;
	virtual bool IsTestRunnable( IAutomationReportPtr InReport ) const override;
	virtual void RemoveCallbacks() override;
	virtual void Shutdown() override;
	virtual void Startup() override;

	virtual FOnAutomationControllerManagerShutdown& OnShutdown( ) override
	{
		return ShutdownDelegate;
	}

	virtual FOnAutomationControllerManagerTestsAvailable& OnTestsAvailable( ) override
	{
		return TestsAvailableDelegate;
	}

	virtual FOnAutomationControllerTestsRefreshed& OnTestsRefreshed( ) override
	{
		return TestsRefreshedDelegate;
	}

	virtual FOnAutomationControllerTestsComplete& OnTestsComplete() override
	{
		return TestsCompleteDelegate;
	}

	virtual FOnAutomationControllerReset& OnControllerReset() override
	{
		return ControllerResetDelegate;
	}


	virtual bool IsDeviceGroupFlagSet( EAutomationDeviceGroupTypes::Type InDeviceGroup ) const override;
	virtual void ToggleDeviceGroupFlag( EAutomationDeviceGroupTypes::Type InDeviceGroup ) override;
	virtual void UpdateDeviceGroups() override;

	virtual void TrackReportHistory(const bool bShouldTrack, const int32 NumReportsToTrack) override;
	virtual const bool IsTrackingHistory() const override;
	virtual const int32 GetNumberHistoryItemsTracking() const override;

protected:

	/**
	 * Adds a ping result from a running test.
	 *
	 * @param ResponderAddress The address of the message endpoint that responded to a ping.
	 */
	void AddPingResult( const FMessageAddress& ResponderAddress );

	/**
	 * Checks the child result.
	 *
	 * @param InReport The child result to check.
	 */
	void CheckChildResult( TSharedPtr< IAutomationReport > InReport );

	/**
	 * Execute the next task thats available.
	 *
	 * @param ClusterIndex The Cluster index of the device type we intend to use.
	 * @param bAllTestsCompleted Whether all tests have been completed.
	 */
	void ExecuteNextTask( int32 ClusterIndex, OUT bool& bAllTestsCompleted );

	/** Distributes any tests that are pending and deal with tests finishing. */
	void ProcessAvailableTasks();

	/** Processes the results after tests are complete. */
	void ProcessResults();

	/**
	 * Removes the test info.
	 *
	 * @param TestToRemove The test to remove.
	 */
	void RemoveTestRunning( const FMessageAddress& TestToRemove );

	/** Resets the data holders which have been used to generate the tests available from a worker. */
	void ResetIntermediateTestData()
	{
		TestInfo.Empty();
	}

	/** Changes the controller state. */
	void SetControllerStatus( EAutomationControllerModuleState::Type AutomationTestState );

	/** stores the tests that are valid for a particular device classification. */
	void SetTestNames(const FMessageAddress& AutomationWorkerAddress);

	/** Updates the tests to ensure they are all still running. */
	void UpdateTests();

private:

	/** Handles FAutomationWorkerFindWorkersResponse messages. */
	void HandleFindWorkersResponseMessage( const FAutomationWorkerFindWorkersResponse& Message, const IMessageContextRef& Context );

	/** Handles FAutomationWorkerPong messages. */
	void HandlePongMessage( const FAutomationWorkerPong& Message, const IMessageContextRef& Context );

	/** Handles FAutomationWorkerScreenImage messages. */
	void HandleReceivedScreenShot( const FAutomationWorkerScreenImage& Message, const IMessageContextRef& Context );

	/** Handles FAutomationWorkerRequestNextNetworkCommand messages. */
	void HandleRequestNextNetworkCommandMessage( const FAutomationWorkerRequestNextNetworkCommand& Message, const IMessageContextRef& Context );

	/** Handles FAutomationWorkerRequestTestsReply messages. */
	void HandleRequestTestsReplyMessage( const FAutomationWorkerRequestTestsReply& Message, const IMessageContextRef& Context );

	/** Handles FAutomationWorkerRequestTestsReplyComplete messages. */
	void HandleRequestTestsReplyCompleteMessage(const FAutomationWorkerRequestTestsReplyComplete& Message, const IMessageContextRef& Context);

	/** Handles FAutomationWorkerRunTestsReply messages. */
	void HandleRunTestsReplyMessage( const FAutomationWorkerRunTestsReply& Message, const IMessageContextRef& Context );

	/** Handles FAutomationWorkerWorkerOffline messages. */
	void HandleWorkerOfflineMessage( const FAutomationWorkerWorkerOffline& Message, const IMessageContextRef& Context );

private:

	/** Session this controller is currently communicating with */
	FGuid ActiveSessionId;

	/** The automation test state */
	EAutomationControllerModuleState::Type AutomationTestState;

	/** Which grouping flags are enabled */
	uint32 DeviceGroupFlags;

	/** Whether to include developer content in the automation tests */
	bool bDeveloperDirectoryIncluded;

	/** Some tests have errors */
	bool bHasErrors;

	/** Some tests have warnings */
	bool bHasWarning;

	/** Some tests have logs */
	bool bHasLogs;

	/** Is this a local session */
	bool bIsLocalSession;

	/** Are tests results available */
	bool bTestResultsAvailable;

	/** Which sets of tests to consider */
	uint32 RequestedTestFlags;

	/** Timer to keep track of the last time tests were updated */
	double CheckTestTimer;

	/** Whether tick is still executing tests for different clusters */
	uint32 ClusterDistributionMask;

	/** Available worker GUIDs */
	FAutomationDeviceClusterManager DeviceClusterManager;

	/** The iteration number of executing the tests.  Ensures restarting the tests won't allow stale results to try and commit */
	uint32 ExecutionCount;

	/** Last time the update tests function was ticked */
	double LastTimeUpdateTicked;

	/** Holds the messaging endpoint. */
	FMessageEndpointPtr MessageEndpoint;

	/** Counter for number of workers we have received responses from for Refreshing the Test List */
	uint32 RefreshTestResponses;

	/** Available stats/status for all tests. */
	FAutomationReportManager ReportManager;

	/** The number we are expecting to receive from a worker */
	int32 NumOfTestsToReceive;

	/** The collection of test data we are to send to a controller. */
	TArray< FAutomationTestInfo > TestInfo;

	/** A data holder to keep track of how long tests have been running. */
	struct FTestRunningInfo
	{
		FTestRunningInfo( FMessageAddress InMessageAddress ):
			OwnerMessageAddress( InMessageAddress),
			LastPingTime( 0.f )
		{
		}
		/** The test runners message address */
		FMessageAddress OwnerMessageAddress;
		/** The time since we had a ping from the instance*/
		float LastPingTime;
	};

	/** A array of running tests. */
	TArray< FTestRunningInfo > TestRunningArray;

	/** The number of test passes to perform. */
	int32 NumTestPasses;

	/** The current test pass we are on. */
	int32 CurrentTestPass;

	/** If screenshots are enabled. */
	bool bScreenshotsEnabled;

	/** If we should request full screen screen shots. */
	bool bRequestFullScreenScreenshots;

	/** If we should send result to analytics */
	bool bSendAnalytics;

	/** If we should track any test history for the next run. */
	bool bTrackHistory;

	/** The number of history items we wish to track. */
	int32 NumberOfHistoryItemsTracked;

private:

	/** Holds a delegate that is invoked when the controller shuts down. */
	FOnAutomationControllerManagerShutdown ShutdownDelegate;

	/** Holds a delegate that is invoked when the controller has tests available. */
	FOnAutomationControllerManagerTestsAvailable TestsAvailableDelegate;

	/** Holds a delegate that is invoked when the controller's tests are being refreshed. */
	FOnAutomationControllerTestsRefreshed TestsRefreshedDelegate;

	/** Holds a delegate that is invoked when the controller's reset. */
	FOnAutomationControllerReset ControllerResetDelegate;	

	/** Holds a delegate that is invoked when the tests have completed. */
	FOnAutomationControllerTestsComplete TestsCompleteDelegate;
};

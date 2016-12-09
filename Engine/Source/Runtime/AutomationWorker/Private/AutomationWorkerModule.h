// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "IMessageContext.h"
#include "Helpers/MessageEndpoint.h"
#include "Interfaces/IAutomationWorkerModule.h"

struct FAutomationWorkerFindWorkers;
struct FAutomationWorkerImageComparisonResults;
struct FAutomationWorkerNextNetworkCommandReply;
struct FAutomationWorkerPing;
struct FAutomationWorkerRequestTests;
struct FAutomationWorkerResetTests;
struct FAutomationWorkerRunTests;

/**
 * Implements the Automation Worker module.
 */
class FAutomationWorkerModule
	: public IAutomationWorkerModule
{
public:

	//~ Begin IModuleInterface Interface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual bool SupportsDynamicReloading() override;

	//~ End IModuleInterface Interface

public:

	//~ Begin IAutomationWorkerModule Interface

	virtual void Tick() override;

	//~ End IAutomationWorkerModule Interface

protected:

	/**
	 * Execute all latent commands and when complete send results message back to the automation controller
	 */
	bool ExecuteLatentCommands();

	/**
	 * Execute all network commands and when complete send results message back to the automation controller
	 */
	bool ExecuteNetworkCommands();

	/**
	 * Initializes the automation worker.
	 */
	void Initialize();

	/**
	 * Network phase is complete (if there were any network commands).  Send ping back to the controller
	 */
	void ReportNetworkCommandComplete();

	/**
	 * Test is complete. Send results back to controller
	 */
	void ReportTestComplete();

	/** 
	 * Send a list of all the tests supported by the worker
	 *
	 * @param ControllerAddress The message address of the controller that requested the tests.
	 */
	void SendTests( const FMessageAddress& ControllerAddress );

private:

	// Handles FAutomationWorkerFindWorkers messages.
	void HandleFindWorkersMessage( const FAutomationWorkerFindWorkers& Message, const IMessageContextRef& Context );

	//deferred handler for sending "find worker" response in case the asset registry isn't loaded yet
	void SendWorkerFound();

	// Handles message endpoint shutdowns.
	void HandleMessageEndpointShutdown();

	// Handles FAutomationWorkerNextNetworkCommandReply messages.
	void HandleNextNetworkCommandReplyMessage( const FAutomationWorkerNextNetworkCommandReply& Message, const IMessageContextRef& Context );

	// Handles FAutomationWorkerPing messages.
	void HandlePingMessage( const FAutomationWorkerPing& Message, const IMessageContextRef& Context );

	// Handles FAutomationWorkerResetTests messages.
	void HandleResetTests( const FAutomationWorkerResetTests& Message, const IMessageContextRef& Context );

	// Handles FAutomationWorkerRequestTests messages.
	void HandleRequestTestsMessage( const FAutomationWorkerRequestTests& Message, const IMessageContextRef& Context );

	// Handles FAutomationWorkerRunTests messages.
	void HandleRunTestsMessage( const FAutomationWorkerRunTests& Message, const IMessageContextRef& Context );

	// Handles FAutomationWorkerImageComparisonResults messages.
	void HandleScreenShotCompared(const FAutomationWorkerImageComparisonResults& Message, const IMessageContextRef& Context);

	// Handles FAutomationTestFramework PreTestingEvents.
	void HandlePreTestingEvent();

	// Handles FAutomationTestFramework PostTestingEvents.
	void HandlePostTestingEvent();

#if WITH_ENGINE
	/** Invoked when we have screen shot to send. */
	void HandleScreenShotCaptured(int32 Width, int32 Height, const TArray<FColor>& Bitmap);
	void HandleScreenShotCapturedWithName(const TArray<FColor>& RawImageData, const FAutomationScreenshotData& Data);
#endif

	//dispatches analytics events to the data collector
	void SendAnalyticsEvents(TArray<FString>& InAnalyticsItems);

	// Helper for Performance Capture Analytics
	void RecordPerformanceAnalytics( const FAutomationPerformanceSnapshot& PerfSnapshot );

private:

	// The collection of test data we are to send to a controller
	TArray<FAutomationTestInfo> TestInfo;

private:

	/** Holds the messaging endpoint. */
	FMessageEndpointPtr MessageEndpoint;

	/** Message address of the controller sending the test request. */
	FMessageAddress TestRequesterAddress;

	/** Identifier for the controller to know if the results should be discarded or not. */
	uint32 ExecutionCount;

	/** Execute one of the tests by request of the controller. */
	FString TestName;

	/** Beautified name of the test */
	FString BeautifiedTestName;

	/** Whether to send analytics events to the backend - sent from controller */
	bool bSendAnalytics;

	/** Whether the controller has requested that the network command should execute */
	bool bExecuteNextNetworkCommand;

	/** Whether we are processing sub-commands of a network command */
	bool bExecutingNetworkCommandResults;

	/** Delegate to fire when the test is complete */
	FStopTestEvent StopTestEvent;
};

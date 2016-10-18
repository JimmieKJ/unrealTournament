// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AutomationWorkerMessages.generated.h"


/* Worker discovery messages
 *****************************************************************************/

/**
 * Implements a message that is published to find automation workers.
 */
USTRUCT()
struct FAutomationWorkerFindWorkers
{
	GENERATED_USTRUCT_BODY()

	/** Holds the change list number to find workers for. */
	UPROPERTY(EditAnywhere, Category="Message")
	int32 Changelist;

	/** The name of the game. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString GameName;

	/** The name of the process. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString ProcessName;

	/** Holds the session identifier to find workers for. */
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid SessionId;

	/** Default constructor. */
	FAutomationWorkerFindWorkers() { }

	/** Creates and initializes a new instance. */
	FAutomationWorkerFindWorkers(int32 InChangelist, const FString& InGameName, const FString& InProcessName, const FGuid& InSessionId)
		: Changelist(InChangelist)
		, GameName(InGameName)
		, ProcessName(InProcessName)
		, SessionId(InSessionId)
	{ }
};


/**
 * Implements a message that is sent in response to FAutomationWorkerFindWorkers.
 */
USTRUCT()
struct FAutomationWorkerFindWorkersResponse
{
	GENERATED_USTRUCT_BODY()

	/** Holds the name of the device that the worker is running on. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString DeviceName;

	/** Holds the name of the worker's application instance. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString InstanceName;

	/** Holds the name of the platform that the worker is running on. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString Platform;

	/** Holds the name of the operating system version. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString OSVersionName;

	/** Holds the name of the device model. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString ModelName;

	/** Holds the name of the GPU. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString GPUName;

	/** Holds the name of the CPU model. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString CPUModelName;

	/** Holds the amount of RAM this device has in gigabytes. */
	UPROPERTY(EditAnywhere, Category="Message")
	uint32 RAMInGB;

	/** Holds the name of the current render mode. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString RenderModeName;

	/** Holds the worker's application session identifier. */
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid SessionId;

	/** Default constructor. */
	FAutomationWorkerFindWorkersResponse() { }
};


/**
 * Implements a message that notifies automation controllers that a worker went off-line.
 */
USTRUCT()
struct FAutomationWorkerWorkerOffline
{
	GENERATED_USTRUCT_BODY()
};


/**
 */
USTRUCT()
struct FAutomationWorkerPing
{
	GENERATED_USTRUCT_BODY()
};


/**
 */
USTRUCT()
struct FAutomationWorkerResetTests
{
	GENERATED_USTRUCT_BODY()
};


/**
 */
USTRUCT()
struct FAutomationWorkerPong
{
	GENERATED_USTRUCT_BODY()
};


/**
 * Implements a message for requesting available automation tests from a worker.
 */
USTRUCT()
struct FAutomationWorkerRequestTests
{
	GENERATED_USTRUCT_BODY()

	/** Holds a flag indicating whether the developer directory should be included. */
	UPROPERTY(EditAnywhere, Category="Message")
	bool DeveloperDirectoryIncluded;

	/** Holds a flag indicating which tests we'd like to request. */
	UPROPERTY(EditAnywhere, Category="Message")
	uint32 RequestedTestFlags;

	/** Default constructor. */
	FAutomationWorkerRequestTests() { }

	/** Creates and initializes a new instance. */
	FAutomationWorkerRequestTests(bool InDeveloperDirectoryIncluded, uint32 InRequestedTestFlags)
		: DeveloperDirectoryIncluded(InDeveloperDirectoryIncluded)
		, RequestedTestFlags(InRequestedTestFlags)
	{ }
};


/**
 * Implements a message that is sent in response to FAutomationWorkerRequestTests.
 */
USTRUCT()
struct FAutomationWorkerRequestTestsReply
{
	GENERATED_USTRUCT_BODY()

	/** Holds the test information serialized into a string. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString TestInfo;

	/** Holds the total number of tests returned. */
	UPROPERTY(EditAnywhere, Category="Message")
	int32 TotalNumTests;

	/** Default constructor. */
	FAutomationWorkerRequestTestsReply() { }

	/** Creates and initializes a new instance. */
	FAutomationWorkerRequestTestsReply(const FString& InTestInfo, const int32& InTotalNumTests)
		: TestInfo(InTestInfo)
		, TotalNumTests(InTotalNumTests)
	{ }
};


/**
*/
USTRUCT()
struct FAutomationWorkerRequestTestsReplyComplete
{
	GENERATED_USTRUCT_BODY()
};


/**
 * Implements a message to request the running of automation tests on a worker.
 */
USTRUCT()
struct FAutomationWorkerRunTests
{
	GENERATED_USTRUCT_BODY()

	/** */
	UPROPERTY(EditAnywhere, Category="Message")
	uint32 ExecutionCount;

	/** */
	UPROPERTY(EditAnywhere, Category="Message")
	int32 RoleIndex;

	/** Holds the name of the test to run. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString TestName;

	/** Holds the name of the test to run. */
	UPROPERTY()
	FString BeautifiedTestName;

	/** If true, we will save out screenshots for tests that support them. */
	UPROPERTY(EditAnywhere, Category="Message")
	bool bScreenshotsEnabled;

	/** If true, we will not resize screen shots. */
	UPROPERTY(EditAnywhere, Category="Message")
	bool bUseFullSizeScreenShots;

	/** If true, send results to analytics when complete */
	UPROPERTY()
	bool bSendAnalytics;

	/** Default constructor. */
	FAutomationWorkerRunTests( ) { }

	/** Creates and initializes a new instance. */
	FAutomationWorkerRunTests( uint32 InExecutionCount, int32 InRoleIndex, FString InTestName, FString InBeautifiedTestName, bool InScreenshotsEnabled, bool InFullSizeScreenshots, bool InSendAnalytics)
		: ExecutionCount(InExecutionCount)
		, RoleIndex(InRoleIndex)
		, TestName(InTestName)
		, BeautifiedTestName(InBeautifiedTestName)
		, bScreenshotsEnabled(InScreenshotsEnabled)
		, bUseFullSizeScreenShots(InFullSizeScreenshots)
		, bSendAnalytics(InSendAnalytics)
	{ }
};

USTRUCT()
struct FAutomationWorkerEvent
{
	GENERATED_USTRUCT_BODY()

	/** */
	UPROPERTY(EditAnywhere, Category="Event")
	FString Message;

	/** */
	UPROPERTY(EditAnywhere, Category="Event")
	FString Context;

	/** */
	UPROPERTY(EditAnywhere, Category="Event")
	FString Filename;

	/** */
	UPROPERTY(EditAnywhere, Category="Event")
	int32 LineNumber;

	FAutomationWorkerEvent()
		: LineNumber(0)
	{
	}

	FAutomationWorkerEvent(const FAutomationEvent& Event)
		: Message(Event.Message)
		, Context(Event.Context)
		, Filename(Event.Filename)
		, LineNumber(Event.LineNumber)
	{
	}

	FAutomationEvent ToAutomationEvent() const
	{
		return FAutomationEvent(Message, Context, Filename, LineNumber);
	}
};

/**
 * Implements a message that is sent in response to FAutomationWorkerRunTests.
 */
USTRUCT()
struct FAutomationWorkerRunTestsReply
{
	GENERATED_USTRUCT_BODY()

	/** */
	UPROPERTY(EditAnywhere, Category="Message")
	float Duration;

	/** */
	UPROPERTY(EditAnywhere, Category="Message")
	TArray<FAutomationWorkerEvent> Errors;

	/** */
	UPROPERTY(EditAnywhere, Category="Message")
	uint32 ExecutionCount;

	/** */
	UPROPERTY(EditAnywhere, Category="Message")
	TArray<FString> Logs;

	/** */
	UPROPERTY(EditAnywhere, Category="Message")
	bool Success;

	/** */
	UPROPERTY(EditAnywhere, Category="Message")
	FString TestName;

	/** */
	UPROPERTY(EditAnywhere, Category="Message")
	TArray<FString> Warnings;
};


/**
 */
USTRUCT()
struct FAutomationWorkerRequestNextNetworkCommand
{
	GENERATED_USTRUCT_BODY()

	/** */
	UPROPERTY(EditAnywhere, Category="Message")
	uint32 ExecutionCount;

	/** Default constructor. */
	FAutomationWorkerRequestNextNetworkCommand() { }

	/** Creates and initializes a new instance. */
	FAutomationWorkerRequestNextNetworkCommand(uint32 InExecutionCount)
		: ExecutionCount(InExecutionCount)
	{ }
};


/**
 */
USTRUCT()
struct FAutomationWorkerNextNetworkCommandReply
{
	GENERATED_USTRUCT_BODY()
};


/**
 * Implements a message that is sent in containing a screen shot run during performance test.
 */
USTRUCT()
struct FAutomationWorkerScreenImage
{
	GENERATED_USTRUCT_BODY()

	/** The screen shot data. */
	UPROPERTY(EditAnywhere, Category="Message")
	TArray<uint8> ScreenImage;

	/** The screen shot name. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString ScreenShotName;
};

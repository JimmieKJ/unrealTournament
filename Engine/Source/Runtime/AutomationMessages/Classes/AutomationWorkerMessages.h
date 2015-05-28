// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
	UPROPERTY()
	int32 Changelist;

	/** The name of the game. */
	UPROPERTY()
	FString GameName;

	/** The name of the process. */
	UPROPERTY()
	FString ProcessName;

	/** Holds the session identifier to find workers for. */
	UPROPERTY()
	FGuid SessionId;

	/** Default constructor. */
	FAutomationWorkerFindWorkers( ) { }

	/** Creates and initializes a new instance. */
	FAutomationWorkerFindWorkers( int32 InChangelist, const FString& InGameName, const FString& InProcessName, const FGuid& InSessionId )
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
	UPROPERTY()
	FString DeviceName;

	/** Holds the name of the worker's application instance. */
	UPROPERTY()
	FString InstanceName;

	/** Holds the name of the platform that the worker is running on. */
	UPROPERTY()
	FString Platform;

	/** Holds the name of the operating system version. */
	UPROPERTY()
	FString OSVersionName;

	/** Holds the name of the device model. */
	UPROPERTY()
	FString ModelName;

	/** Holds the name of the GPU. */
	UPROPERTY()
	FString GPUName;

	/** Holds the name of the CPU model. */
	UPROPERTY()
	FString CPUModelName;

	/** Holds the amount of RAM this device has in gigabytes. */
	UPROPERTY()
	uint32 RAMInGB;

	/** Holds the name of the current render mode. */
	UPROPERTY()
	FString RenderModeName;

	/** Holds the worker's application session identifier. */
	UPROPERTY()
	FGuid SessionId;

	/**
	 * Default constructor.
	 */
	FAutomationWorkerFindWorkersResponse( ) { }
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
	UPROPERTY()
	bool DeveloperDirectoryIncluded;

	/** Holds a flag indicating whether the visual commandlet filter is enabled. */
	UPROPERTY()
	bool VisualCommandletFilterOn;

	/**
	 * Default constructor.
	 */
	FAutomationWorkerRequestTests( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FAutomationWorkerRequestTests( bool InDeveloperDirectoryIncluded, bool InVisualCommandletFilterOn )
		: DeveloperDirectoryIncluded(InDeveloperDirectoryIncluded)
		, VisualCommandletFilterOn(InVisualCommandletFilterOn)
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
	UPROPERTY()
	FString TestInfo;

	/** Holds the total number of tests returned. */
	UPROPERTY()
	int32 TotalNumTests;

	/**
	 * Default constructor.
	 */
	FAutomationWorkerRequestTestsReply( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FAutomationWorkerRequestTestsReply( const FString& InTestInfo, const int32& InTotalNumTests )
		: TestInfo(InTestInfo)
		, TotalNumTests(InTotalNumTests)
	{ }
};


/**
 * Implements a message to request the running of automation tests on a worker.
 */
USTRUCT()
struct FAutomationWorkerRunTests
{
	GENERATED_USTRUCT_BODY()

	/** */
	UPROPERTY()
	uint32 ExecutionCount;

	/** */
	UPROPERTY()
	int32 RoleIndex;

	/** Holds the name of the test to run. */
	UPROPERTY()
	FString TestName;

	/** If true, we will save out screenshots for tests that support them. */
	UPROPERTY()
	bool bScreenshotsEnabled;

	/** If true, we will not resize screen shots. */
	UPROPERTY()
	bool bUseFullSizeScreenShots;

	/**
	 * Default constructor.
	 */
	FAutomationWorkerRunTests( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FAutomationWorkerRunTests( uint32 InExecutionCount, int32 InRoleIndex, FString InTestName, bool InScreenshotsEnabled, bool InFullSizeScreenshots )
		: ExecutionCount(InExecutionCount)
		, RoleIndex(InRoleIndex)
		, TestName(InTestName)
		, bScreenshotsEnabled(InScreenshotsEnabled)
		, bUseFullSizeScreenShots(InFullSizeScreenshots)
	{ }
};


/**
 * Implements a message that is sent in response to FAutomationWorkerRunTests.
 */
USTRUCT()
struct FAutomationWorkerRunTestsReply
{
	GENERATED_USTRUCT_BODY()

	/** */
	UPROPERTY()
	float Duration;

	/** */
	UPROPERTY()
	TArray<FString> Errors;

	/** */
	UPROPERTY()
	uint32 ExecutionCount;

	/** */
	UPROPERTY()
	TArray<FString> Logs;

	/** */
	UPROPERTY()
	bool Success;

	/** */
	UPROPERTY()
	FString TestName;

	/** */
	UPROPERTY()
	TArray<FString> Warnings;
};


/**
 */
USTRUCT()
struct FAutomationWorkerRequestNextNetworkCommand
{
	GENERATED_USTRUCT_BODY()

	/** */
	UPROPERTY()
	uint32 ExecutionCount;

	/**
	 * Default constructor.
	 */
	FAutomationWorkerRequestNextNetworkCommand( ) { }

	/**
	 * Creates and initializes a new instance.
	 */
	FAutomationWorkerRequestNextNetworkCommand( uint32 InExecutionCount )
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
	UPROPERTY()
	TArray<uint8> ScreenImage;

	/** The screen shot name. */
	UPROPERTY()
	FString ScreenShotName;
};

// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Misc/AutomationTest.h"
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

	UPROPERTY(EditAnywhere, Category="Message")
	FString DisplayName;

	UPROPERTY(EditAnywhere, Category="Message")
	FString FullTestPath;

	UPROPERTY(EditAnywhere, Category="Message")
	FString TestName;

	UPROPERTY(EditAnywhere, Category="Message")
	FString TestParameter;

	UPROPERTY(EditAnywhere, Category="Message")
	FString SourceFile;

	UPROPERTY(EditAnywhere, Category="Message")
	int32 SourceFileLine;

	UPROPERTY(EditAnywhere, Category="Message")
	FString AssetPath;

	UPROPERTY(EditAnywhere, Category="Message")
	FString OpenCommand;

	UPROPERTY(EditAnywhere, Category="Message")
	uint32 TestFlags;

	UPROPERTY(EditAnywhere, Category="Message")
	uint32 NumParticipantsRequired;

	/** Holds the total number of tests returned. */
	UPROPERTY(EditAnywhere, Category="Message")
	int32 TotalNumTests;

	/** Default constructor. */
	FAutomationWorkerRequestTestsReply() { }

	/** Creates and initializes a new instance. */
	FAutomationWorkerRequestTestsReply(const FAutomationTestInfo& InTestInfo, const int32& InTotalNumTests)
		: TotalNumTests(InTotalNumTests)
	{
		DisplayName = InTestInfo.GetDisplayName();
		FullTestPath = InTestInfo.GetFullTestPath();
		TestName = InTestInfo.GetTestName();
		TestParameter = InTestInfo.GetTestParameter();
		SourceFile = InTestInfo.GetSourceFile();
		SourceFileLine = InTestInfo.GetSourceFileLine();
		AssetPath = InTestInfo.GetAssetPath();
		OpenCommand = InTestInfo.GetOpenCommand();
		TestFlags = InTestInfo.GetTestFlags();
		NumParticipantsRequired = InTestInfo.GetNumParticipantsRequired();
	}

	FAutomationTestInfo GetTestInfo() const
	{
		return FAutomationTestInfo(
			DisplayName,
			FullTestPath,
			TestName,
			TestFlags,
			NumParticipantsRequired,
			TestParameter,
			SourceFile,
			SourceFileLine,
			AssetPath,
			OpenCommand);
	}
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

	/** If true, send results to analytics when complete */
	UPROPERTY()
	bool bSendAnalytics;

	/** Default constructor. */
	FAutomationWorkerRunTests( ) { }

	/** Creates and initializes a new instance. */
	FAutomationWorkerRunTests( uint32 InExecutionCount, int32 InRoleIndex, FString InTestName, FString InBeautifiedTestName, bool InScreenshotsEnabled, bool InSendAnalytics)
		: ExecutionCount(InExecutionCount)
		, RoleIndex(InRoleIndex)
		, TestName(InTestName)
		, BeautifiedTestName(InBeautifiedTestName)
		, bScreenshotsEnabled(InScreenshotsEnabled)
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

USTRUCT()
struct FAutomationScreenshotMetadata
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Message")
	FString Name;
	UPROPERTY(EditAnywhere, Category="Message")
	FString Context;

	UPROPERTY(EditAnywhere, Category="Message")
	int32 Width;
	UPROPERTY(EditAnywhere, Category="Message")
	int32 Height;

	// RHI Details
	UPROPERTY(EditAnywhere, Category="Message")
	FString Rhi;
	UPROPERTY(EditAnywhere, Category="Message")
	FString Platform;
	UPROPERTY(EditAnywhere, Category="Message")
	FString FeatureLevel;
	UPROPERTY(EditAnywhere, Category="Message")
	bool bIsStereo;

	// Hardware Details
	UPROPERTY(EditAnywhere, Category="Message")
	FString Vendor;
	UPROPERTY(EditAnywhere, Category="Message")
	FString AdapterName;
	UPROPERTY(EditAnywhere, Category="Message")
	FString AdapterInternalDriverVersion;
	UPROPERTY(EditAnywhere, Category="Message")
	FString AdapterUserDriverVersion;
	UPROPERTY(EditAnywhere, Category="Message")
	FString UniqueDeviceId;

	// Quality Levels
	UPROPERTY(EditAnywhere, Category="Message")
	float ResolutionQuality;
	UPROPERTY(EditAnywhere, Category="Message")
	int32 ViewDistanceQuality;
	UPROPERTY(EditAnywhere, Category="Message")
	int32 AntiAliasingQuality;
	UPROPERTY(EditAnywhere, Category="Message")
	int32 ShadowQuality;
	UPROPERTY(EditAnywhere, Category="Message")
	int32 PostProcessQuality;
	UPROPERTY(EditAnywhere, Category="Message")
	int32 TextureQuality;
	UPROPERTY(EditAnywhere, Category="Message")
	int32 EffectsQuality;
	UPROPERTY(EditAnywhere, Category="Message")
	int32 FoliageQuality;

	// Comparison Requests
	UPROPERTY(EditAnywhere, Category="Message")
	bool bHasComparisonRules;
	UPROPERTY(EditAnywhere, Category="Message")
	uint8 ToleranceRed;
	UPROPERTY(EditAnywhere, Category="Message")
	uint8 ToleranceGreen;
	UPROPERTY(EditAnywhere, Category="Message")
	uint8 ToleranceBlue;
	UPROPERTY(EditAnywhere, Category="Message")
	uint8 ToleranceAlpha;
	UPROPERTY(EditAnywhere, Category="Message")
	uint8 ToleranceMinBrightness;
	UPROPERTY(EditAnywhere, Category="Message")
	uint8 ToleranceMaxBrightness;
	UPROPERTY(EditAnywhere, Category="Message")
	float MaximumLocalError;
	UPROPERTY(EditAnywhere, Category="Message")
	float MaximumGlobalError;
	UPROPERTY(EditAnywhere, Category="Message")
	bool bIgnoreAntiAliasing;
	UPROPERTY(EditAnywhere, Category="Message")
	bool bIgnoreColors;

public:
	FAutomationScreenshotMetadata()
	{
	}

	FAutomationScreenshotMetadata(const FAutomationScreenshotData& Data)
	{
		Name = Data.Name;
		Context = Data.Context;

		Width = Data.Width;
		Height = Data.Height;

		// RHI Details
		Rhi = Data.Rhi;
		Platform = Data.Platform;
		FeatureLevel = Data.FeatureLevel;
		bIsStereo = Data.bIsStereo;

		// Hardware Details
		Vendor = Data.Vendor;
		AdapterName = Data.AdapterName;
		AdapterInternalDriverVersion = Data.AdapterInternalDriverVersion;
		AdapterUserDriverVersion = Data.AdapterUserDriverVersion;
		UniqueDeviceId = Data.UniqueDeviceId;

		// Quality Levels
		ResolutionQuality = Data.ResolutionQuality;
		ViewDistanceQuality = Data.ViewDistanceQuality;
		AntiAliasingQuality = Data.AntiAliasingQuality;
		ShadowQuality = Data.ShadowQuality;
		PostProcessQuality = Data.PostProcessQuality;
		TextureQuality = Data.TextureQuality;
		EffectsQuality = Data.EffectsQuality;
		FoliageQuality = Data.FoliageQuality;

		// Comparison Requests
		bHasComparisonRules = Data.bHasComparisonRules;
		ToleranceRed = Data.ToleranceRed;
		ToleranceGreen = Data.ToleranceGreen;
		ToleranceBlue = Data.ToleranceBlue;
		ToleranceAlpha = Data.ToleranceAlpha;
		ToleranceMinBrightness = Data.ToleranceMinBrightness;
		ToleranceMaxBrightness = Data.ToleranceMaxBrightness;
		
		MaximumLocalError = Data.MaximumLocalError;
		MaximumGlobalError = Data.MaximumGlobalError;

		bIgnoreAntiAliasing = Data.bIgnoreAntiAliasing;
		bIgnoreColors = Data.bIgnoreColors;
	}
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

	UPROPERTY(EditAnywhere, Category="Message")
	FAutomationScreenshotMetadata Metadata;
};



/**
 * Implements a message that is sent in containing a screen shot run during performance test.
 */
USTRUCT()
struct FAutomationWorkerImageComparisonResults
{
	GENERATED_USTRUCT_BODY()

public:
	FAutomationWorkerImageComparisonResults()
		: bNew(false)
		, bSimilar(false)
	{
	}

	FAutomationWorkerImageComparisonResults(bool InIsNew, bool InAreSimilar)
		: bNew(InIsNew)
		, bSimilar(InAreSimilar)
	{
	}

	/** Was this a new image we've never seen before and have no ground truth for? */
	UPROPERTY(EditAnywhere, Category="Message")
	bool bNew;

	/** Were the images similar?  If they're not you should log an error. */
	UPROPERTY(EditAnywhere, Category="Message")
	bool bSimilar;
};

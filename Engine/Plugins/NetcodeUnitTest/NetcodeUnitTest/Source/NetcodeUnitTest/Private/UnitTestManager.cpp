// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NetcodeUnitTestPCH.h"

#include "UnitTestManager.h"
#include "LogWindowManager.h"

#include "UnitTest.h"
#include "ClientUnitTest.h"

#include "SLogWidget.h"
#include "SLogWindow.h"
#include "SLogDialog.h"

#include "NUTUtilNet.h"
#include "NUTUtilProfiler.h"
#include "NUTUtil.h"

// @todo JohnB: Add an overall-timer, and then start debugging the memory management in more detail


UUnitTestManager* GUnitTestManager = NULL;

UUnitTestBase* GActiveLogUnitTest = NULL;
UUnitTestBase* GActiveLogEngineEvent = NULL;
UWorld* GActiveLogWorld = NULL;


// @todo JohnB: If you need the main unit test manager, to add more logs to the final summary, replace below with a generalized solution

/** Stores a list of log messages for 'unsupported' unit tests, for printout in the final summary */
static TMap<FString, FString> UnsupportedUnitTests;



// @todo JohnB: Add detection for unit tests that either 1: Never end (e.g. if they become outdated and break, or 2: keep on hitting
//				memory limits and get auto-closed and re-run recursively, forever

// @todo JohnB: For unit tests that are running in the background (no log window for controlling them),
//				add a drop-down listbox button to the status window, for re-opening background unit test log windows
//				(can even add an option, to have them all running in background by default, so you have to do this to open
//				log window at all)


/**
 * UUnitTestManager
 */

UUnitTestManager::UUnitTestManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bCapUnitTestCount(false)
	, MaxUnitTestCount(0)
	, bCapUnitTestMemory(false)
	, MaxMemoryPercent(0)
	, AutoCloseMemoryPercent(0)
	, PendingUnitTests()
	, ActiveUnitTests()
	, FinishedUnitTests()
	, bAbortedFirstRunUnitTest(false)
	, bAllowRequeuingUnitTests(true)
	, LogWindowManager(NULL)
	, bStatusLog(false)
	, StatusColor(FSlateColor::UseForeground())
	, DialogWindows()
	, StatusWindow()
	, AbortAllDialog()
	, MemoryTickCountdown(0)
	, MemoryUsageUponCountdown(0)
{
}

UUnitTestManager* UUnitTestManager::Get()
{
	if (GUnitTestManager == NULL)
	{
		GUnitTestManager = NewObject<UUnitTestManager>();

		if (GUnitTestManager != NULL)
		{
			GUnitTestManager->Initialize();
		}
	}

	return GUnitTestManager;
}

void UUnitTestManager::Initialize()
{
	// Detect if the configuration file doesn't exist, and initialize it if that's the case
	if (MaxUnitTestCount == 0)
	{
		bCapUnitTestCount = false;
		MaxUnitTestCount = 4;
		bCapUnitTestMemory = true;

		// Being a little conservative here, as the code estimating memory usage, can undershoot a bit
		MaxMemoryPercent = 75;

		// Since the above can undershoot, the limit at which unit tests are automatically terminated, is a bit higher
		AutoCloseMemoryPercent = 90;

		UE_LOG(LogUnitTest, Log, TEXT("Creating initial unit test config file"));

		SaveConfig();
	}

	// Add this object to the root set, to disable garbage collection until desired (it is not referenced by any UProperties)
	SetFlags(RF_RootSet);

	// Add a log hook
	if (!GLog->IsRedirectingTo(this))
	{
		GLog->AddOutputDevice(this);
	}

	if (LogWindowManager == NULL)
	{
		LogWindowManager = new FLogWindowManager();
		LogWindowManager->Initialize(800, 400);
	}
}

UUnitTestManager::~UUnitTestManager()
{
	if (LogWindowManager != NULL)
	{
		delete LogWindowManager;
		LogWindowManager = NULL;
	}

	GLog->RemoveOutputDevice(this);
}

bool UUnitTestManager::QueueUnitTest(UClass* UnitTestClass, bool bRequeued/*=false*/)
{
	bool bSuccess = false;

	// Before anything else, open up the unit test status window (but do not pop up again if closed, for re-queued unit tests)
	if (!bRequeued && !FApp::IsUnattended())
	{
		OpenStatusWindow();
	}


	bool bValidUnitTestClass = UnitTestClass->IsChildOf(UUnitTest::StaticClass()) && UnitTestClass != UUnitTest::StaticClass() &&
								UnitTestClass != UClientUnitTest::StaticClass();

	UUnitTest* UnitTestDefault = (bValidUnitTestClass ? Cast<UUnitTest>(UnitTestClass->GetDefaultObject()) : NULL);

	bValidUnitTestClass = UnitTestDefault != NULL;


	if (bValidUnitTestClass && UUnitTest::UnitEnv != NULL)
	{
		FString UnitTestName = UnitTestDefault->GetUnitTestName();
		bool bCurrentGameSupported = UnitTestDefault->GetSupportedGames().Contains(FApp::GetGameName());

		if (bCurrentGameSupported)
		{
			// Check that the unit test is not already active or queued
			bool bActiveOrQueued = PendingUnitTests.Contains(UnitTestClass) ||
				ActiveUnitTests.ContainsByPredicate(
					[&](UUnitTest* Element)
					{
						return Element->GetClass() == UnitTestClass;
					});

			if (!bActiveOrQueued)
			{
				// Ensure the CDO has its environment settings setup
				UnitTestDefault->InitializeEnvironmentSettings();

				// Now validate the unit test settings, using the CDO, prior to queueing
				if (UnitTestDefault->ValidateUnitTestSettings(true))
				{
					PendingUnitTests.Add(UnitTestClass);
					bSuccess = true;

					STATUS_LOG(ELogType::StatusImportant, TEXT("Successfully queued unit test '%s' for execution."), *UnitTestName);
				}
				else
				{
					STATUS_LOG(ELogType::StatusError, TEXT("Failed to validate unit test '%s' for execution."), *UnitTestName);
				}
			}
			else
			{
				STATUS_LOG(, TEXT("Unit test '%s' is already queued or active"), *UnitTestName);
			}
		}
		else
		{
			TArray<FString> SupportedGamesList = UnitTestDefault->GetSupportedGames();
			FString SupportedGames = TEXT("");

			for (auto CurGame : SupportedGamesList)
			{
				SupportedGames += (SupportedGames.Len() == 0 ? CurGame : FString(TEXT(", ")) + CurGame);
			}


			FString LogMsg = FString::Printf(TEXT("Unit test '%s' doesn't support the current game ('%s'). Supported games: %s"),
												*UnitTestName, FApp::GetGameName(), *SupportedGames);

			UnsupportedUnitTests.Add(UnitTestName, LogMsg);


			STATUS_SET_COLOR(FLinearColor(1.f, 1.f, 0.f));

			STATUS_LOG(ELogType::StatusWarning | ELogType::StyleBold, TEXT("%s"), *LogMsg);

			STATUS_RESET_COLOR();
		}
	}
	else if (!bValidUnitTestClass)
	{
		STATUS_LOG(ELogType::StatusError | ELogType::StyleBold, TEXT("Class '%s' is not a valid unit test class"),
						(UnitTestClass != NULL ? *UnitTestClass->GetName() : TEXT("")));
	}
	else if (UUnitTest::UnitEnv == NULL)
	{
		STATUS_LOG(ELogType::StatusError | ELogType::StyleBold,
					TEXT("No unit test environment found (need to load unit test environment module for this game, or create it)."));
	}

	return bSuccess;
}

void UUnitTestManager::PollUnitTestQueue()
{
	// @todo JohnB: Maybe consider staggering the start of unit tests, as perhaps this may give late-starters
	//				a boost from OS precaching of file data, and may also help stagger the 'peak memory' time,
	//				which often gets multiple unit tests closed early due to happening at the same time?

	// Keep kicking off unit tests in order, until the list is empty, or until the unit test cap is reached
	for (int32 i=0; i<PendingUnitTests.Num(); i++)
	{
		bool bAlreadyRemoved = false;

		// Lambda for remove-safe and multi-remove-safe handling within this loop
		auto RemoveCurrent = [&]()
			{
				if (!bAlreadyRemoved)
				{
					bAlreadyRemoved = true;
					PendingUnitTests.RemoveAt(i);
					i--;
				}
			};


		UClass* CurUnitTestClass = PendingUnitTests[i];
		bool bWithinUnitTestLimits = ActiveUnitTests.Num() == 0 || WithinUnitTestLimits(CurUnitTestClass);

		// This unit test isn't within limits, continue to the next one and see if it fits
		if (!bWithinUnitTestLimits)
		{
			continue;
		}


		UUnitTest* CurUnitTestDefault = Cast<UUnitTest>(CurUnitTestClass->GetDefaultObject());

		if (CurUnitTestDefault != NULL)
		{
			auto CurUnitTest = NewObject<UUnitTest>(GetTransientPackage(), CurUnitTestClass);

			if (CurUnitTest != NULL)
			{
				if (UUnitTest::UnitEnv != NULL)
				{
					CurUnitTest->InitializeEnvironmentSettings();
				}

				// Remove from PendingUnitTests, and add to ActiveUnitTests
				RemoveCurrent();
				ActiveUnitTests.Add(CurUnitTest);

				// Create the log window (if starting the unit test fails, this is unset during cleanup)
				if (!FApp::IsUnattended())
				{
					OpenUnitTestLogWindow(CurUnitTest);
				}


				if (CurUnitTest->StartUnitTest())
				{
					STATUS_LOG(ELogType::StatusImportant, TEXT("Started unit test '%s'"), *CurUnitTest->GetUnitTestName());
				}
				else
				{
					STATUS_LOG(ELogType::StatusError | ELogType::StyleBold, TEXT("Failed to kickoff unit test '%s'"),
									*CurUnitTestDefault->GetUnitTestName());
				}
			}
			else
			{
				STATUS_LOG(ELogType::StatusError | ELogType::StyleBold, TEXT("Failed to construct unit test: %s"),
								*CurUnitTestDefault->GetUnitTestName());
			}
		}
		else
		{
			STATUS_LOG(ELogType::StatusError | ELogType::StyleBold, TEXT("Failed to find default object for unit test class '%s'"),
							*CurUnitTestClass->GetName());
		}

		RemoveCurrent();
	}
}

bool UUnitTestManager::WithinUnitTestLimits(UClass* PendingUnitTest/*=NULL*/)
{
	bool bReturnVal = false;

	// @todo JohnB: Could do with non-spammy logging of when limits are reached

	// Check max unit test count
	bReturnVal = !bCapUnitTestCount || ActiveUnitTests.Num() < MaxUnitTestCount;


	// Limit the number of first-run unit tests (which don't have any stats gathered), to MaxUnitTestCount, even if !bCapUnitTestCount.
	// If any first-run unit tests have had to be aborted, this might signify a problem, so make the cap very strict (two at a time)
	uint8 FirstRunCap = (bAbortedFirstRunUnitTest ? 2 : MaxUnitTestCount);

	if (bReturnVal && !bCapUnitTestCount && ActiveUnitTests.Num() >= FirstRunCap)
	{
		// @todo JohnB: Add prominent logging for hitting this cap - preferably 'status' log window

		uint32 FirstRunCount = 0;

		for (auto CurUnitTest : ActiveUnitTests)
		{
			if (CurUnitTest->IsFirstTimeStats())
			{
				FirstRunCount++;
			}
		}

		bReturnVal = FirstRunCount < FirstRunCap;
	}


	// Check that physical memory usage is currently within limits (does not factor in any unit tests)
	SIZE_T TotalPhysicalMem = 0;
	SIZE_T UsedPhysicalMem = 0;
	SIZE_T MaxPhysicalMem = 0;

	if (bReturnVal)
	{
		TotalPhysicalMem = FPlatformMemory::GetConstants().TotalPhysical;
		UsedPhysicalMem = TotalPhysicalMem - FPlatformMemory::GetStats().AvailablePhysical;
		MaxPhysicalMem = ((TotalPhysicalMem / (SIZE_T)100) * (SIZE_T)MaxMemoryPercent);

		bReturnVal = MaxPhysicalMem > UsedPhysicalMem;
	}


	// Iterate through running plus pending unit tests, calculate the time at which each unit test will reach peak memory usage,
	// and estimate the total memory consumption of all unit tests combined, at the time of each peak.
	// The highest value, gives an estimate of the peak system memory consumption that will be reached, which we check is within limits.
	//
	// TLDR: Estimate worst-case peak memory usage for all unit tests together (active+pending), and check it's within limits.
	if (bReturnVal)
	{
		UUnitTest* PendingUnitTestDefObj = (PendingUnitTest != NULL ? Cast<UUnitTest>(PendingUnitTest->GetDefaultObject()) : NULL);
		double CurrentTime = FPlatformTime::Seconds();

		// Lambda for estimating how much memory an individual unit test will be using, at a specific time
		auto UnitMemUsageForTime = [&](UUnitTest* InUnitTest, double TargetTime)
			{
				SIZE_T ReturnVal = 0;

				// The calculation is based on previously collected stats for the unit test - peak mem usage and time it took to reach 
				float UnitTimeToPeakMem = InUnitTest->TimeToPeakMem;
				double UnitStartTime = InUnitTest->StartTime;
				double PeakMemTime = UnitStartTime + (double)UnitTimeToPeakMem;

				if (UnitTimeToPeakMem > 0.5f && PeakMemTime >= CurrentTime)
				{
					// Only return a value, if we expect the unit test to still be running at TargetTime
					if (PeakMemTime > TargetTime)
					{
						// Simple/dumb memory usage estimate, calculating linearly from 0 to PeakMem, based on unit test execution time
						float RunningTime = (float)(CurrentTime - UnitStartTime);
						SIZE_T PercentComplete = (SIZE_T)((RunningTime * 100.f) / UnitTimeToPeakMem);

						ReturnVal = (InUnitTest->PeakMemoryUsage / (SIZE_T)100) * PercentComplete;
					}
				}
				// If the unit test is running past TimeToPeakMem (or if that time is unknown), return worst case peak mem
				else
				{
					ReturnVal = InUnitTest->PeakMemoryUsage;
				}

				return ReturnVal;
			};

		// Lambda for estimating how much memory ALL unit tests will be using, when a specific unit test is at its peak memory usage
		auto TotalUnitMemUsageForUnitPeak = [&](UUnitTest* InUnitTest)
			{
				SIZE_T ReturnVal = 0;
				double PeakMemTime = InUnitTest->StartTime + (double)InUnitTest->TimeToPeakMem;

				for (auto CurUnitTest : ActiveUnitTests)
				{
					if (CurUnitTest == InUnitTest)
					{
						ReturnVal += InUnitTest->PeakMemoryUsage;
					}
					else
					{
						ReturnVal += UnitMemUsageForTime(CurUnitTest, PeakMemTime);
					}
				}

				// Duplicate of above
				if (PendingUnitTestDefObj != NULL)
				{
					if (PendingUnitTestDefObj == InUnitTest)
					{
						ReturnVal += PendingUnitTestDefObj->PeakMemoryUsage;
					}
					else
					{
						ReturnVal += UnitMemUsageForTime(PendingUnitTestDefObj, PeakMemTime);
					}
				}

				return ReturnVal;
			};


		// Iterate unit tests, estimating the total memory usage for all unit tests, at the time of each unit test reaching peak mem,
		// and determine the worst case value for this - this worst case value should be the peak memory usage for all unit tests
		SIZE_T WorstCaseTotalUnitMemUsage = 0;
		SIZE_T CurrentTotalUnitMemUsage = 0;

		for (auto CurUnitTest : ActiveUnitTests)
		{
			CurrentTotalUnitMemUsage += CurUnitTest->CurrentMemoryUsage;

			SIZE_T EstTotalUnitMemUsage = TotalUnitMemUsageForUnitPeak(CurUnitTest);

			if (EstTotalUnitMemUsage > WorstCaseTotalUnitMemUsage)
			{
				WorstCaseTotalUnitMemUsage = EstTotalUnitMemUsage;
			}
		}

		// Duplicate of above
		if (PendingUnitTestDefObj != NULL)
		{
			SIZE_T EstTotalUnitMemUsage = TotalUnitMemUsageForUnitPeak(PendingUnitTestDefObj);

			if (EstTotalUnitMemUsage > WorstCaseTotalUnitMemUsage)
			{
				WorstCaseTotalUnitMemUsage = EstTotalUnitMemUsage;
			}
		}

		// Now that we've got the worst case, estimate peak memory usage for the whole system, and see that it falls within limits
		SIZE_T EstimatedPeakPhysicalMem = (UsedPhysicalMem - CurrentTotalUnitMemUsage) + WorstCaseTotalUnitMemUsage;

		bReturnVal = MaxPhysicalMem > EstimatedPeakPhysicalMem;


		// @todo JohnB: Remove this debug code
		//UE_LOG(LogUnitTest, Log, TEXT("Estimated peak physical mem: %llumb"), (EstimatedPeakPhysicalMem / (SIZE_T)1048576));
	}


	// @todo JohnB: How to improve unit test memory estimates:
	// NOTE: Not to be done, unless above implementation proves problematic (seems to provide decent estimates so-far)
	//	- Can improve future-memory-usage estimation, by storing time-series stats, mapping the memory usage of each unit test
	//		- Problem here though, is this is liable to vary over time, so perhaps only do this for each unit tests 'most recent run'
	//	- Can improve timing of estimations, by tracking the unit tests current memory usage, and marking at what stage of
	//		the above time-series stats it is at; check the difference between how long it took the unit test to get to that
	//		position in the time series stat, and the timing of that original stat, then scale all time estimates based on that


	return bReturnVal;
}

void UUnitTestManager::NotifyUnitTestComplete(UUnitTest* InUnitTest, bool bAborted)
{
	if (bAborted)
	{
		STATUS_LOG(ELogType::StatusWarning, TEXT("Aborted unit test '%s'"), *InUnitTest->GetUnitTestName());

		if (InUnitTest->IsFirstTimeStats())
		{
			bAbortedFirstRunUnitTest = true;
		}
	}
	else
	{
		PrintUnitTestResult(InUnitTest);
	}

	FinishedUnitTests.Add(InUnitTest);

	// Every time a unit test completes, poll the unit test queue, for any pending unit tests waiting for a space
	PollUnitTestQueue();
}

void UUnitTestManager::NotifyUnitTestCleanup(UUnitTest* InUnitTest)
{
	ActiveUnitTests.Remove(InUnitTest);

	UClientUnitTest* CurClientUnitTest = Cast<UClientUnitTest>(InUnitTest);

	if (CurClientUnitTest != NULL)
	{
		CurClientUnitTest->OnServerSuspendState.Unbind();
	}


	TSharedPtr<SLogWindow>& LogWindow = InUnitTest->LogWindow;

	if (LogWindow.IsValid())
	{
		TSharedPtr<SLogWidget>& LogWidget = LogWindow->LogWidget;

		if (LogWidget.IsValid())
		{
			LogWidget->OnSuspendClicked.Unbind();
			LogWidget->OnDeveloperClicked.Unbind();
			LogWidget->OnConsoleCommand.Unbind();

			if (LogWidget->bAutoClose)
			{
				LogWindow->RequestDestroyWindow();
			}
		}

		LogWindow.Reset();
	}

	// Remove any open dialogs for this window
	for (auto It = DialogWindows.CreateIterator(); It; ++It)
	{
		if (It.Value() == InUnitTest)
		{
			// Don't let the dialog return the 'window closed' event as user input
			It.Key()->SetOnWindowClosed(FOnWindowClosed());

			It.Key()->RequestDestroyWindow();
			It.RemoveCurrent();

			break;
		}
	}
}

void UUnitTestManager::NotifyLogWindowClosed(const TSharedRef<SWindow>& ClosedWindow)
{
	if (ClosedWindow == StatusWindow)
	{
		if (!AbortAllDialog.IsValid() && IsRunningUnitTests())
		{
			FText CloseAllMsg = FText::FromString(FString::Printf(TEXT("Abort all active unit tests?")));
			FText ClostAllTitle = FText::FromString(FString::Printf(TEXT("Abort unit tests?")));

			TSharedRef<SWindow> CurDialogWindow = OpenLogDialog_NonModal(EAppMsgType::YesNo, CloseAllMsg, ClostAllTitle,
				FOnLogDialogResult::CreateUObject(this, &UUnitTestManager::NotifyCloseAllDialogResult));

			AbortAllDialog = CurDialogWindow;
		}

		StatusWindow.Reset();
	}
	else
	{
		// Match the log window to a unit test
		UUnitTest** CurUnitTestRef = ActiveUnitTests.FindByPredicate(
			[&](const UUnitTest* Element)
			{
				return Element->LogWindow == ClosedWindow;
			});

		UUnitTest* CurUnitTest = (CurUnitTestRef != NULL ? *CurUnitTestRef : NULL);

		if (CurUnitTest != NULL)
		{
			UClientUnitTest* CurClientUnitTest = Cast<UClientUnitTest>(CurUnitTest);

			if (CurClientUnitTest != NULL)
			{
				CurClientUnitTest->OnServerSuspendState.Unbind();
			}

			if (!CurUnitTest->bCompleted && !CurUnitTest->bAborted)
			{
				// Show a message box, asking the player if they'd like to also abort the unit test
				// @todo JohnB: Perhaps add an option somewhere, to abort-by-default as well, to save the time spent on UI
				//				(could even make it a tickbox on the status window)
				FString UnitTestName = CurUnitTest->GetUnitTestName();

				FText CloseMsg = FText::FromString(FString::Printf(TEXT("Abort unit test '%s'? (currently running in background)"),
																	*UnitTestName));

				FText CloseTitle = FText::FromString(FString::Printf(TEXT("Abort '%s'?"), *UnitTestName));

				TSharedRef<SWindow> CurDialogWindow = OpenLogDialog_NonModal(EAppMsgType::YesNoCancel, CloseMsg, CloseTitle,
					FOnLogDialogResult::CreateUObject(this, &UUnitTestManager::NotifyCloseDialogResult));

				DialogWindows.Add(CurDialogWindow, CurUnitTest);
			}

			CurUnitTest->LogWindow.Reset();
		}
	}
}

void UUnitTestManager::NotifyCloseDialogResult(const TSharedRef<SWindow>& DialogWindow, EAppReturnType::Type Result, bool bNoResult)
{
	UUnitTest* CurUnitTest = DialogWindows.FindAndRemoveChecked(DialogWindow);

	if (CurUnitTest != NULL && ActiveUnitTests.Contains(CurUnitTest))
	{
		if (!bNoResult && Result == EAppReturnType::Yes)
		{
			CurUnitTest->AbortUnitTest();
		}
		// If the answer was 'cancel', or if the dialog was closed without answering, re-open the unit test log window
		else if (bNoResult || Result == EAppReturnType::Cancel)
		{
			if (ActiveUnitTests.Contains(CurUnitTest))
			{
				OpenUnitTestLogWindow(CurUnitTest);
			}
		}
	}
}

void UUnitTestManager::NotifyCloseAllDialogResult(const TSharedRef<SWindow>& DialogWindow, EAppReturnType::Type Result, bool bNoResult)
{
	if (!bNoResult && Result == EAppReturnType::Yes)
	{
		// First delete the pending list, to prevent any unit tests from being added
		PendingUnitTests.Empty();

		// Now abort all active unit tests
		TArray<UUnitTest*> ActiveUnitTestsCopy(ActiveUnitTests);

		for (auto CurUnitTest : ActiveUnitTestsCopy)
		{
			CurUnitTest->AbortUnitTest();
		}
	}
	else
	{
		// Re-open the status window if 'no' was clicked; don't allow it to be closed, or the client loses the ability to 'abort-all'
		if (IsRunningUnitTests())
		{
			OpenStatusWindow();
		}
	}

	AbortAllDialog.Reset();
}


void UUnitTestManager::DumpStatus(bool bForce/*=false*/)
{
	static bool bLastDumpWasBlank = false;

	bool bCurDumpIsBlank = ActiveUnitTests.Num() == 0 && PendingUnitTests.Num() == 0;

	// When no unit tests are active, don't keep dumping stats
	if (bForce || !bLastDumpWasBlank || !bCurDumpIsBlank)
	{
		bLastDumpWasBlank = bCurDumpIsBlank;

		// Give the status update logs a unique colour, so that dumping so much text into the status window,
		// doesn't disrupt the flow of text (otherwise, all the other events/updates in the status window, become hard to read)
		STATUS_SET_COLOR(FLinearColor(0.f, 1.f, 1.f));

		// @todo JohnB: Prettify the status command, by seeing if you can evenly space unit test info into columns;
		//				use the console monospacing to help do this (if it looks ok)
		SIZE_T TotalMemoryUsage = 0;

		STATUS_LOG(, TEXT(""));
		STATUS_LOG(ELogType::StyleUnderline, TEXT("Unit test status:"))
		STATUS_LOG(, TEXT("- Number of active unit tests: %i"), ActiveUnitTests.Num());

		for (auto CurUnitTest : ActiveUnitTests)
		{
			TotalMemoryUsage += CurUnitTest->CurrentMemoryUsage;

			STATUS_LOG(, TEXT("     - (%s) %s (Memory usage: %uMB)"), *CurUnitTest->GetUnitTestType(), *CurUnitTest->GetUnitTestName(),
					(CurUnitTest->CurrentMemoryUsage / 1048576));
		}

		STATUS_LOG(, TEXT("- Total unit test memory usage: %uMB"), (TotalMemoryUsage / 1048576));

		STATUS_LOG(, TEXT("- Number of pending unit tests: %i"), PendingUnitTests.Num());

		for (auto CurClass : PendingUnitTests)
		{
			UUnitTest* CurUnitTest =  Cast<UUnitTest>(CurClass->GetDefaultObject());

			if (CurUnitTest != NULL)
			{
				STATUS_LOG(, TEXT("     - (%s) %s"), *CurUnitTest->GetUnitTestType(), *CurUnitTest->GetUnitTestName());
			}
		}

		STATUS_LOG(, TEXT(""));


		STATUS_RESET_COLOR();
	}
}

void UUnitTestManager::PrintUnitTestResult(UUnitTest* InUnitTest, bool bFinalSummary/*=false*/)
{
	static const UEnum* VerificationStateEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EUnitTestVerification"));

	EUnitTestVerification UnitTestResult = InUnitTest->VerificationState;

	// Only include the automation flag, if this is the final summary
	ELogType StatusAutomationFlag = (bFinalSummary ? ELogType::StatusAutomation : ELogType::None);

	if (!bFinalSummary)
	{
		STATUS_LOG_OBJ(InUnitTest, ELogType::StatusImportant,	TEXT("Unit test '%s' completed:"), *InUnitTest->GetUnitTestName());
	}

	STATUS_LOG_OBJ(InUnitTest, ELogType::StatusImportant,	TEXT("  - Result: %s"),
					*VerificationStateEnum->GetEnumName((uint32)UnitTestResult));
	STATUS_LOG_OBJ(InUnitTest, ELogType::StatusVerbose,	TEXT("  - Execution Time: %f"), InUnitTest->LastExecutionTime);


	auto PrintShortList =
		[&](TArray<FString>& ListSource, FString ListDesc)
		{
			FString ListStr;
			bool bMultiLineList = false;

			for (int32 i=0; i<ListSource.Num(); i++)
			{
				// If any list entry look like a lengthy description, have a line for each entry, instead of a one-line summary
				if (ListSource[i].Len() > 32)
				{
					bMultiLineList = true;
					break;
				}

				ListStr += ListSource[i];

				if (i+1 < ListSource.Num())
				{
					ListStr += TEXT(", ");
				}
			}

			if (bMultiLineList)
			{
				STATUS_LOG_OBJ(InUnitTest, ELogType::StatusVerbose, TEXT("  - %s:"), *ListDesc);

				for (auto CurEntry : ListSource)
				{
					STATUS_LOG_OBJ(InUnitTest, ELogType::StatusVerbose, TEXT("    - %s"), *CurEntry);
				}
			}
			else
			{
				STATUS_LOG_OBJ(InUnitTest, ELogType::StatusVerbose, TEXT("  - %s: %s"), *ListDesc, *ListStr);
			}
		};

	// Print bug-tracking information
	if (InUnitTest->UnitTestBugTrackIDs.Num() > 0)
	{
		PrintShortList(InUnitTest->UnitTestBugTrackIDs, TEXT("Bug tracking"));
	}

	// Print changelist information
	if (InUnitTest->UnitTestCLs.Num() > 0)
	{
		PrintShortList(InUnitTest->UnitTestCLs, TEXT("Changelists"));
	}


	EUnitTestVerification ExpectedResult = InUnitTest->GetExpectedResult();

	// @todo JohnB: Perhaps make this an execution-blocking error instead? (simplifies the code/error-cases here)
	if (ExpectedResult == EUnitTestVerification::Unverified)
	{
		STATUS_LOG_OBJ(InUnitTest, ELogType::StatusError | StatusAutomationFlag | ELogType::StyleBold,
						TEXT("  - Unit test does not have 'ExpectedResult' set"));
	}
	else
	{
		if (UnitTestResult == EUnitTestVerification::VerifiedFixed && ExpectedResult == UnitTestResult)
		{
			STATUS_SET_COLOR(FLinearColor(0.f, 1.f, 0.f));

			STATUS_LOG_OBJ(InUnitTest, ELogType::StatusSuccess | StatusAutomationFlag,
							TEXT("  - Unit test issue has been fixed."));

			STATUS_RESET_COLOR();
		}
		else
		{
			bool bExpectedResult = ExpectedResult == UnitTestResult;

			if (!bExpectedResult)
			{
				if (UnitTestResult == EUnitTestVerification::VerifiedNeedsUpdate)
				{
					STATUS_LOG_OBJ(InUnitTest, ELogType::StatusWarning | StatusAutomationFlag | ELogType::StyleBold,
								TEXT("  - WARNING: Unit test returned 'needs update' as its result."))
				}
				else
				{
					STATUS_LOG_OBJ(InUnitTest, ELogType::StatusWarning | StatusAutomationFlag | ELogType::StyleBold,
									TEXT("  - Unit test did not return expected result - unit test needs an update."));
				}

				if (InUnitTest->bUnreliable || UnitTestResult == EUnitTestVerification::VerifiedUnreliable)
				{
					STATUS_LOG_OBJ(InUnitTest, ELogType::StatusWarning | StatusAutomationFlag,
								TEXT("  - NOTE: Unit test marked 'unreliable' - may need multiple runs to get expected result."));
				}
			}
			// For when the unit test is expected to be unreliable
			else if (bExpectedResult && UnitTestResult == EUnitTestVerification::VerifiedUnreliable)
			{
				STATUS_LOG_OBJ(InUnitTest, ELogType::StatusWarning | StatusAutomationFlag,
							TEXT("  - NOTE: Unit test expected to be unreliable - multiple runs may not change result/outcome."));
			}


			if (UnitTestResult != EUnitTestVerification::VerifiedFixed)
			{
				if (ExpectedResult == EUnitTestVerification::VerifiedFixed)
				{
					STATUS_LOG_OBJ(InUnitTest, ELogType::StatusError | StatusAutomationFlag | ELogType::StyleBold,
									TEXT("  - Unit test issue is no longer fixed."));
				}
				else
				{
					STATUS_LOG_OBJ(InUnitTest, ELogType::StatusError | StatusAutomationFlag,
									TEXT("  - Unit test issue has NOT been fixed."));
				}
			}
		}
	}
}

void UUnitTestManager::PrintFinalSummary()
{
	// @todo JohnB: Add some extra stats eventually, such as overall time taken etc.

	STATUS_LOG(ELogType::StatusImportant, TEXT(""));
	STATUS_LOG(ELogType::StatusImportant, TEXT(""));
	STATUS_LOG(ELogType::StatusImportant | ELogType::StatusAutomation | ELogType::StyleBold,
				TEXT("----------------------------------------------------------------FINAL UNIT TEST SUMMARY")
				TEXT("----------------------------------------------------------------"));
	STATUS_LOG(ELogType::StatusImportant, TEXT(""));
	STATUS_LOG(ELogType::StatusImportant, TEXT(""));


	// First print the unsupported unit tests
	for (auto It = UnsupportedUnitTests.CreateConstIterator(); It; ++It)
	{
		STATUS_LOG(ELogType::StatusWarning | ELogType::StatusAutomation | ELogType::StyleBold, TEXT("%s: %s"), *It.Key(), *It.Value());
	}

	if (UnsupportedUnitTests.Num() > 0)
	{
		STATUS_LOG(ELogType::StatusImportant, TEXT(""));
	}

	UnsupportedUnitTests.Empty();


	// Then print the aborted unit tests
	TArray<FString> AbortList;

	for (auto CurUnitTest : FinishedUnitTests)
	{
		if (CurUnitTest->bAborted)
		{
			AbortList.Add(CurUnitTest->GetUnitTestName());
		}
	}

	for (int32 AbortIdx=0; AbortIdx<AbortList.Num(); AbortIdx++)
	{
		FString CurAbort = AbortList[AbortIdx];
		uint8 NumberOfAborts = 1;

		// Count and remove duplicate aborts
		for (int32 DupeIdx=AbortList.Num()-1; DupeIdx>AbortIdx; DupeIdx--)
		{
			if (CurAbort == AbortList[DupeIdx])
			{
				NumberOfAborts++;
				AbortList.RemoveAt(DupeIdx);
				DupeIdx--;
			}
		}

		if (NumberOfAborts == 1)
		{
			STATUS_LOG(ELogType::StatusWarning | ELogType::StyleBold, TEXT("%s: Aborted."), *CurAbort);
		}
		else
		{
			STATUS_LOG(ELogType::StatusWarning | ELogType::StyleBold, TEXT("%s: Aborted ('%i' times)."), *CurAbort, NumberOfAborts);
		}
	}


	if (AbortList.Num() > 0 || UnsupportedUnitTests.Num() > 0)
	{
		STATUS_LOG(ELogType::StatusImportant, TEXT(""));
		STATUS_LOG(ELogType::StatusImportant, TEXT(""));
	}

	// Now print the completed unit tests, which have more detailed information
	for (auto CurUnitTest : FinishedUnitTests)
	{
		if (!CurUnitTest->bAborted)
		{
			STATUS_SET_COLOR(FLinearColor(0.25f, 0.25f, 0.25f));

			STATUS_LOG(ELogType::StatusImportant | ELogType::StatusAutomation | ELogType::StyleBold | ELogType::StyleUnderline,
						TEXT("%s:"), *CurUnitTest->GetUnitTestName());

			STATUS_RESET_COLOR();


			// Print out the main result header
			PrintUnitTestResult(CurUnitTest, true);


			// Now print out the full event history
			bool bHistoryContainsImportant = CurUnitTest->StatusLogSummary.ContainsByPredicate(
				[](const TSharedPtr<FUnitStatusLog>& CurEntry)
				{
					return ((CurEntry->LogType & ELogType::StatusImportant) == ELogType::StatusImportant);
				});


			if (bHistoryContainsImportant)
			{
				STATUS_LOG(ELogType::StatusImportant, TEXT("  - Log summary:"));
			}
			else
			{
				STATUS_LOG(ELogType::StatusVerbose, TEXT("  - Log summary:"));
			}

			for (auto CurStatusLog : CurUnitTest->StatusLogSummary)
			{
				STATUS_LOG(CurStatusLog->LogType, TEXT("      %s"), *CurStatusLog->LogLine);
			}


			STATUS_LOG(ELogType::StatusImportant, TEXT(""));
		}
	}
}


void UUnitTestManager::OpenUnitTestLogWindow(UUnitTest* InUnitTest)
{
	if (LogWindowManager != NULL)
	{
		InUnitTest->LogWindow = LogWindowManager->CreateLogWindow(InUnitTest->GetUnitTestName(), InUnitTest->GetExpectedLogTypes());

		UClientUnitTest* CurClientUnitTest = Cast<UClientUnitTest>(InUnitTest);

		if (InUnitTest->LogWindow.IsValid() && CurClientUnitTest != NULL)
		{
			TSharedPtr<SLogWidget> CurLogWidget = InUnitTest->LogWindow->LogWidget;

			if (CurLogWidget.IsValid())
			{
				CurLogWidget->OnSuspendClicked.BindUObject(CurClientUnitTest, &UClientUnitTest::NotifySuspendRequest);
				CurClientUnitTest->OnServerSuspendState.BindSP(CurLogWidget.Get(), &SLogWidget::OnSuspendStateChanged);

				CurLogWidget->OnDeveloperClicked.BindUObject(CurClientUnitTest, &UClientUnitTest::NotifyDeveloperModeRequest);


				// Setup the widget console command context list, and then bind the console command delegate
				CurClientUnitTest->GetCommandContextList(CurLogWidget->ConsoleContextList, CurLogWidget->DefaultConsoleContext);

				CurLogWidget->OnConsoleCommand.BindUObject(CurClientUnitTest, &UClientUnitTest::NotifyConsoleCommandRequest);
			}
		}
	}
}

void UUnitTestManager::OpenStatusWindow()
{
	if (!StatusWindow.IsValid() && LogWindowManager != NULL)
	{
		StatusWindow = LogWindowManager->CreateLogWindow(TEXT("Unit Test Status"), ELogType::None, true);

		TSharedPtr<SLogWidget> CurLogWidget = (StatusWindow.IsValid() ? StatusWindow->LogWidget : NULL);

		if (CurLogWidget.IsValid())
		{
			// Bind the status window console command event
			CurLogWidget->OnConsoleCommand.BindLambda(
				[&](FString CommandContext, FString Command)
				{
					bool bSuccess = false;

					// @todo JohnB: Revisit this - doesn't STATUS_LOG on its own (>not< STATUS_LOG_BASE),
					//				already output to log? Don't think the extra code here is needed.

					// Need an output device redirector, to send console command log output to both GLog and unit test status log,
					// and need the 'dynamic' device, to implement a custom output device, which does the unit test status log output
					FOutputDeviceRedirector LogSplitter;
					FDynamicOutputDevice StatusLogOutput;

					StatusLogOutput.OnSerialize.AddStatic(
						[](const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
						{
							FString LogLine = FOutputDevice::FormatLogLine(Verbosity, Category, V);

							STATUS_LOG_BASE(, TEXT("%s"), *LogLine);
						});


					LogSplitter.AddOutputDevice(GLog);
					LogSplitter.AddOutputDevice(&StatusLogOutput);

					bSuccess = GEngine->Exec(NULL, *Command, LogSplitter);

					return bSuccess;
				});
		}
	}
}


void UUnitTestManager::Tick(float DeltaTime)
{
	// Tick unit tests here, instead of them using FTickableGameObject, as the order of ticking is important (for memory stats)
	{
		TArray<UUnitTest*> ActiveUnitTestsCopy(ActiveUnitTests);

		double CurTime = FPlatformTime::Seconds();
		const double NetTickInterval = 1.0 / 60.0;

		for (auto CurUnitTest : ActiveUnitTestsCopy)
		{
			if (!CurUnitTest->IsPendingKill())
			{
				if (CurUnitTest->IsTickable())
				{
					UNIT_EVENT_BEGIN(CurUnitTest);

					CurUnitTest->UnitTick(DeltaTime);

					if ((CurTime - CurUnitTest->LastNetTick) > NetTickInterval)
					{
						CurUnitTest->NetTick();
						CurUnitTest->LastNetTick = CurTime;
					}

					CurUnitTest->PostUnitTick(DeltaTime);

					UNIT_EVENT_END;
				}

				CurUnitTest->TickIsComplete(DeltaTime);
			}
		}

		ActiveUnitTestsCopy.Empty();
	}


	// Poll the unit test queue
	PollUnitTestQueue();


	// If there are no pending or active unit tests, but there are finished unit tests waiting for a summary printout, then do that now
	if (FinishedUnitTests.Num() > 0 && ActiveUnitTests.Num() == 0 && PendingUnitTests.Num() == 0)
	{
		PrintFinalSummary();


		// Now mark all of these unit tests, for garbage collection
		for (auto CurUnitTest : FinishedUnitTests)
		{
			CurUnitTest->MarkPendingKill();
		}

		FinishedUnitTests.Empty();
	}


	// If we've exceeded system memory limits, kill enough recently launched unit tests, to get back within limits
	SIZE_T TotalPhysicalMem = 0;
	SIZE_T UsedPhysicalMem = 0;
	SIZE_T MaxPhysicalMem = 0;

	TotalPhysicalMem = FPlatformMemory::GetConstants().TotalPhysical;
	UsedPhysicalMem = TotalPhysicalMem - FPlatformMemory::GetStats().AvailablePhysical;
	MaxPhysicalMem = ((TotalPhysicalMem / (SIZE_T)100) * (SIZE_T)AutoCloseMemoryPercent);


	// If we've recently force-closed a unit test, wait a number of ticks for memory stats to update
	//	(unless memory consumption increases yet again, in which case end the countdown immediately)
	if (MemoryTickCountdown > 0)
	{
		--MemoryTickCountdown;

		if (UsedPhysicalMem > MemoryUsageUponCountdown)
		{
			MemoryTickCountdown = 0;
		}
	}

	if (ActiveUnitTests.Num() > 0 && MemoryTickCountdown <= 0 && UsedPhysicalMem > MaxPhysicalMem)
	{
		SIZE_T MemOvershoot = UsedPhysicalMem - MaxPhysicalMem;

		STATUS_LOG(ELogType::StatusImportant | ELogType::StyleBold | ELogType::StyleItalic,
						TEXT("Unit test system memory limit exceeded (Used: %lluMB, Limit: %lluMB), closing some unit tests"),
							(UsedPhysicalMem / 1048576), (MaxPhysicalMem / 1048576));


		// Wait a number of ticks, before re-enabling auto-close of unit tests
		MemoryTickCountdown = 10;
		MemoryUsageUponCountdown = UsedPhysicalMem;


		for (int32 i=ActiveUnitTests.Num()-1; i>=0; i--)
		{
			UUnitTest* CurUnitTest = ActiveUnitTests[i];
			SIZE_T CurMemUsage = CurUnitTest->CurrentMemoryUsage;


			// Kill the unit test and return it to the pending queue
			UClass* UnitTestClass = CurUnitTest->GetClass();

			CurUnitTest->AbortUnitTest();
			CurUnitTest = NULL;

			QueueUnitTest(UnitTestClass, true);


			// Keep closing unit tests, until we get back within memory limits
			if (CurMemUsage < MemOvershoot)
			{
				MemOvershoot -= CurMemUsage;
			}
			else
			{
				break;
			}
		}
	}


	// Dump unit test status every now and then
	static double LastStatusDump = 0.0;
	static bool bResetTimer = true;

	if (ActiveUnitTests.Num() > 0 || PendingUnitTests.Num() > 0)
	{
		double CurSeconds = FPlatformTime::Seconds();

		if (bResetTimer)
		{
			bResetTimer = false;
			LastStatusDump = CurSeconds;
		}
		else if (CurSeconds - LastStatusDump > 10.0)
		{
			LastStatusDump = CurSeconds;

			DumpStatus();
		}
	}
	// If no unit tests are active, reset the status dump counter next time unit tests are running/queued
	else
	{
		bResetTimer = true;
	}
}


bool UUnitTestManager::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bReturnVal = true;

	// @todo JohnB: Detecting the originating unit test, by World, no longer works due to delayed fake client launches;
	//				either find another way of determining the originating unit test, or give all unit tests a unique World early on,
	//				before the fake client launch

	FString UnitTestName = FParse::Token(Cmd, false);
	bool bValidUnitTestName = false;

	// @todo JohnB: Refactor some of this function, so that the unit test defaults scanning/updating,
	//				is done in a separate specific function
	//				IMPORTANT: It should be called multiple times, when needed, not just once,
	//				as there will likely be unit tests in multiple loaded packages in the future

	// First off, gather a full list of unit test classes
	TArray<UUnitTest*> UnitTestClassDefaults;

	NUTUtil::GetUnitTestClassDefList(UnitTestClassDefaults);

	// All unit tests should be given a proper date, so give big errors when this is not set
	// @todo JohnB: Move to the unit test validation function
	for (int i=0; i<UnitTestClassDefaults.Num(); i++)
	{
		if (UnitTestClassDefaults[i]->GetUnitTestDate() == FDateTime::MinValue())
		{
			Ar.Logf(TEXT("ERROR: Unit Test '%s' does not have a date set!!!! A date must be added to every unit test!"),
					*UnitTestClassDefaults[i]->GetUnitTestName());
		}
	}


	if (UnitTestName == TEXT("status"))
	{
		DumpStatus(true);

		bValidUnitTestName = true;
	}
	else if (UnitTestName == TEXT("detector"))
	{
		FString DetectorClass = FParse::Token(Cmd, false);

		if (DetectorClass == TEXT("FFrameProfiler"))
		{
#if STATS
			FString TargetEvent = FParse::Token(Cmd, false);
			uint8 FramePercentThreshold = FCString::Atoi(*FParse::Token(Cmd, false));

			if (!TargetEvent.IsEmpty() && FramePercentThreshold > 0)
			{
				FFrameProfiler* NewProfiler = new FFrameProfiler(FName(*TargetEvent), FramePercentThreshold);
				NewProfiler->Start();

				// @todo JohnB: Perhaps add tracking at some stage, and remove self-destruct code from above class
			}
			else
			{
				UE_LOG(LogUnitTest, Log, TEXT("TargetEvent (%s) must be set and FramePercentThreshold (%u) must be non-zero"),
						*TargetEvent, FramePercentThreshold);
			}
#else
			UE_LOG(LogUnitTest, Log, TEXT("Can't use FFrameProfiler, stats not enable during compile."));
#endif
		}
		else
		{
			UE_LOG(LogUnitTest, Log, TEXT("Unknown detector class '%s'"), *DetectorClass);
		}

		bValidUnitTestName = true;
	}
	// If this command was triggered within a specific unit test (as identified by InWorld), abort it
	else if (UnitTestName == TEXT("abort"))
	{
		if (InWorld != NULL)
		{
			UUnitTest** AbortUnitTestRef = ActiveUnitTests.FindByPredicate(
				[&InWorld](const UUnitTest* InElement)
				{
					auto CurUnitTest = Cast<UClientUnitTest>(InElement);

					return CurUnitTest != NULL && CurUnitTest->UnitWorld == InWorld;
				});

			UUnitTest* AbortUnitTest = (AbortUnitTestRef != NULL ? *AbortUnitTestRef : NULL);

			if (AbortUnitTest != NULL)
			{
				AbortUnitTest->AbortUnitTest();
			}
		}
		else
		{
			UE_LOG(LogUnitTest, Log, TEXT("Unit test abort command, must be called within a specific unit test window."));
		}
	}
	// Debug unit test commands
	else if (UnitTestName == TEXT("debug"))
	{
		if (InWorld != NULL)
		{
			UUnitTest** TargetUnitTestRef = ActiveUnitTests.FindByPredicate(
				[&InWorld](const UUnitTest* InElement)
				{
					auto CurUnitTest = Cast<UClientUnitTest>(InElement);

					return CurUnitTest != NULL && CurUnitTest->UnitWorld == InWorld;
				});

			UClientUnitTest* TargetUnitTest = (TargetUnitTestRef != NULL ? Cast<UClientUnitTest>(*TargetUnitTestRef) : NULL);

			if (TargetUnitTest != NULL)
			{
				if (FParse::Command(&Cmd, TEXT("Requirements")))
				{
					EUnitTestFlags RequirementsFlags = (TargetUnitTest->UnitTestFlags & EUnitTestFlags::RequirementsMask);
					EUnitTestFlags MetRequirements = TargetUnitTest->GetMetRequirements();

					// Iterate over the requirements mask flag bits
					FString RequiredBits = TEXT("");
					FString MetBits = TEXT("");
					FString FailBits = TEXT("");
					TArray<FString> FlagResults;
					EUnitTestFlags FirstFlag =
						(EUnitTestFlags)(1UL << (31 - FMath::CountLeadingZeros((uint32)EUnitTestFlags::RequirementsMask)));

					for (EUnitTestFlags CurFlag=FirstFlag; !!(CurFlag & EUnitTestFlags::RequirementsMask);
							CurFlag = (EUnitTestFlags)((uint32)CurFlag >> 1))
					{
						bool bCurFlagReq = !!(CurFlag & RequirementsFlags);
						bool bCurFlagSet = !!(CurFlag & MetRequirements);

						RequiredBits += (bCurFlagReq ? TEXT("1") : TEXT("0"));
						MetBits += (bCurFlagSet ? TEXT("1") : TEXT("0"));
						FailBits += ((bCurFlagReq && !bCurFlagSet) ? TEXT("1") : TEXT("0"));

						FlagResults.Add(FString::Printf(TEXT(" - %s: Required: %i, Set: %i, Failed: %i"), *GetUnitTestFlagName(CurFlag),
										(uint32)bCurFlagReq, (uint32)bCurFlagSet, (uint32)(bCurFlagReq && !bCurFlagSet)));
					}

					Ar.Logf(TEXT("Requirements flags for unit test '%s': Required: %s, Set: %s, Failed: %s"),
							*TargetUnitTest->GetUnitTestName(), *RequiredBits, *MetBits, *FailBits);

					for (auto CurResult : FlagResults)
					{
						Ar.Logf(*CurResult);
					}
				}
			}
		}
		else
		{
			UE_LOG(LogUnitTest, Log, TEXT("Unit test 'debug' command, must be called from within a specific unit test window."));
		}

		bValidUnitTestName = true;
	}
	else if (UnitTestName == TEXT("all"))
	{
		// When executing all unit tests, allow them to be requeued if auto-aborted
		bAllowRequeuingUnitTests = true;

		for (int i=0; i<UnitTestClassDefaults.Num(); i++)
		{
			if (!UnitTestClassDefaults[i]->bWorkInProgress)
			{
				if (!QueueUnitTest(UnitTestClassDefaults[i]->GetClass()))
				{
					Ar.Logf(TEXT("Failed to add unit test '%s' to queue"), *UnitTestClassDefaults[i]->GetUnitTestName());
				}
			}
		}

		// After queuing the unit tests, poll the queue to see we're ready to execute more
		PollUnitTestQueue();

		bValidUnitTestName = true;
	}
	else if (UnitTestName.Len() > 0)
	{
		UClass* CurUnitTestClass = NULL;

		for (int i=0; i<UnitTestClassDefaults.Num(); i++)
		{
			UUnitTest* CurDefault = Cast<UUnitTest>(UnitTestClassDefaults[i]);

			if (CurDefault->GetUnitTestName() == UnitTestName)
			{
				CurUnitTestClass = UnitTestClassDefaults[i]->GetClass();
				break;
			}
		}

		bValidUnitTestName = CurUnitTestClass != NULL;

		if (bValidUnitTestName && QueueUnitTest(CurUnitTestClass))
		{
			// Don't allow requeuing of single unit tests, if they are auto-aborted
			bAllowRequeuingUnitTests = false;

			// Now poll the unit test queue, to see we're ready to execute more
			PollUnitTestQueue();
		}
		else
		{
			Ar.Logf(TEXT("Failed to add unit test '%s' to queue"), *UnitTestName);
		}
	}

	// List all unit tests
	if (!bValidUnitTestName)
	{
		Ar.Logf(TEXT("Could not find unit test '%s', listing all unit tests:"), *UnitTestName);
		Ar.Logf(TEXT("- 'status': Lists status of all currently running unit tests"));
		Ar.Logf(TEXT("- 'all': Executes all unit tests at once"));

		// First sort the unit test class defaults
		NUTUtil::SortUnitTestClassDefList(UnitTestClassDefaults);

		// Now list them, now that they are ordered in terms of type and date
		FString LastType;

		for (int i=0; i<UnitTestClassDefaults.Num(); i++)
		{
			FString CurType = UnitTestClassDefaults[i]->GetUnitTestType();

			if (LastType != CurType)
			{
				Ar.Logf(TEXT("- '%s' unit tests:"), *CurType);

				LastType = CurType;
			}

			Ar.Logf(TEXT("     - %s"), *UnitTestClassDefaults[i]->GetUnitTestName());
		}
	}

	return bReturnVal;
}


void UUnitTestManager::Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	if (bStatusLog)
	{
		if (StatusWindow.IsValid())
		{
			TSharedPtr<SLogWidget>& LogWidget = StatusWindow->LogWidget;

			if (LogWidget.IsValid())
			{
				ELogType CurLogType = ELogType::Local | GActiveLogTypeFlags;
				bool bSetTypeColor = false;


				// Colour-in some log types, that are typically passed in from unit tests (unless the colour was overridden)
				if (!StatusColor.IsColorSpecified())
				{
					bSetTypeColor = true;

					if ((CurLogType & ELogType::StatusError) == ELogType::StatusError)
					{
						SetStatusColor(FLinearColor(1.f, 0.f, 0.f));
					}
					else if ((CurLogType & ELogType::StatusWarning) == ELogType::StatusWarning)
					{
						SetStatusColor(FLinearColor(1.f, 1.f, 0.f));
					}
					else if ((CurLogType & ELogType::StatusAdvanced) == ELogType::StatusAdvanced ||
								(CurLogType & ELogType::StatusVerbose) == ELogType::StatusVerbose)
					{
						SetStatusColor(FLinearColor(0.25f, 0.25f, 0.25f));
					}
					else
					{
						bSetTypeColor = false;
					}
				}


				FString* LogLine = NULL;
				UUnitTest* CurLogUnitTest = Cast<UUnitTest>(GActiveLogUnitTest);

				if (CurLogUnitTest != NULL)
				{
					// Store the log within the unit test
					CurLogUnitTest->StatusLogSummary.Add(MakeShareable(new FUnitStatusLog(CurLogType, FString(Data))));

					LogLine = new FString(FString::Printf(TEXT("%s: %s"), *CurLogUnitTest->GetUnitTestName(), Data));
				}
				else
				{
					LogLine = new FString(Data);
				}

				TSharedRef<FString> LogLineRef = MakeShareable(LogLine);

				LogWidget->AddLine(CurLogType, LogLineRef, StatusColor);


				if (bSetTypeColor)
				{
					SetStatusColor();
				}
			}
		}
	}
	// Unit test logs (including hooked log events) - also double check that this is not a log for the global status window
	else if (!(GActiveLogTypeFlags & (ELogType::GlobalStatus | ELogType::OriginVoid)))
	{
		// Prevent re-entrant code
		static bool bLogSingularCheck = false;

		if (!bLogSingularCheck)
		{
			bLogSingularCheck = true;


			ELogType CurLogType = ELogType::Local | GActiveLogTypeFlags;
			UUnitTest* SourceUnitTest = NULL;

			// If this log was triggered, while a unit test net connection was processing a packet, find and notify the unit test
			if (GActiveReceiveUnitConnection != NULL)
			{
				CurLogType |= ELogType::OriginNet;

				for (auto CurUnitTest : ActiveUnitTests)
				{
					UClientUnitTest* CurClientUnitTest = Cast<UClientUnitTest>(CurUnitTest);

					if (CurClientUnitTest != NULL && CurClientUnitTest->UnitConn == GActiveReceiveUnitConnection)
					{
						SourceUnitTest = CurUnitTest;
						break;
					}
				}
			}
			// If it was triggered from within a unit test log, also notify
			else if (GActiveLogUnitTest != NULL)
			{
				CurLogType |= ELogType::OriginUnitTest;
				SourceUnitTest = Cast<UUnitTest>(GActiveLogUnitTest);
			}
			// If it was triggered within an engine event, within a unit test, again notify
			else if (GActiveLogEngineEvent != NULL)
			{
				CurLogType |= ELogType::OriginEngine;
				SourceUnitTest = Cast<UUnitTest>(GActiveLogEngineEvent);
			}
			// If it was triggered during UWorld::Tick, for the world assigned to a unit test, again find and notify
			else if (GActiveLogWorld != NULL)
			{
				CurLogType |= ELogType::OriginEngine;

				for (auto CurUnitTest : ActiveUnitTests)
				{
					UClientUnitTest* CurClientUnitTest = Cast<UClientUnitTest>(CurUnitTest);

					if (CurClientUnitTest != NULL && CurClientUnitTest->UnitWorld == GActiveLogWorld)
					{
						SourceUnitTest = CurUnitTest;
						break;
					}
				}
			}

			if (SourceUnitTest != NULL && (CurLogType & ELogType::OriginMask) != ELogType::None)
			{
				SourceUnitTest->NotifyLocalLog(CurLogType, Data, Verbosity, Category);
			}


			bLogSingularCheck = false;
		}
	}
}


/**
 * Exec hook for the unit test manager (also handles creation of unit test manager)
 */

static bool UnitTestExec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bReturnVal = false;

	if (FParse::Command(&Cmd, TEXT("UnitTest")))
	{
		// Create the unit test manager, if it hasn't already been created
		if (GUnitTestManager == NULL)
		{
			UUnitTestManager::Get();
		}

		if (GUnitTestManager != NULL)
		{
			bReturnVal = GUnitTestManager->Exec(InWorld, Cmd, Ar);
		}
		else
		{
			Ar.Logf(TEXT("Failed to execute unit test command '%s', GUnitTestManager == NULL"), Cmd);
		}

		bReturnVal = true;
	}
	/**
	 * For the connection-per-unit-test code, which also creates a whole new world/netdriver etc. per unit test,
	 * you need to very carefully remove references to the world when done, outside ticking of GEngine->WorldList.
	 * This needs to be done using GEngine->TickDeferredCommands, which is what should trigger this console command.
	 */
	else if (FParse::Command(&Cmd, TEXT("CleanupUnitTestWorlds")))
	{
		NUTNet::CleanupUnitTestWorlds();

		return true;
	}

	return bReturnVal;
}

// Register the above static exec function
FStaticSelfRegisteringExec UnitTestExecRegistration(UnitTestExec);


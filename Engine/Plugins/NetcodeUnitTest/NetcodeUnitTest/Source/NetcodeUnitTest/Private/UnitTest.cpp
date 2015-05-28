// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NetcodeUnitTestPCH.h"

#include "UnitTest.h"
#include "UnitTestManager.h"

#include "SLogWidget.h"
#include "SLogWindow.h"


FUnitTestEnvironment* UUnitTest::UnitEnv = NULL;


/**
 * UUnitTestBase
 */

UUnitTestBase::UUnitTestBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UUnitTestBase::StartUnitTest()
{
	bool bReturnVal = false;

	UNIT_EVENT_BEGIN(this);

	bReturnVal = UTStartUnitTest();

	UNIT_EVENT_END;

	return bReturnVal;
}


/**
 * UUnitTest
 */

UUnitTest::UUnitTest(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, UnitTestName()
	, UnitTestType()
	, UnitTestDate(FDateTime::MinValue())
	, UnitTestBugTrackIDs()
	, UnitTestCLs()
	, bWorkInProgress(false)
	, bUnreliable(false)
	, ExpectedResult()
	, UnitTestTimeout(0)
	, PeakMemoryUsage(0)
	, TimeToPeakMem(0.0f)
	, LastExecutionTime(0.0f)
	, LastNetTick(0.0)
	, CurrentMemoryUsage(0)
	, StartTime(0.0)
	, TimeoutExpire(0.0)
	, LastTimeoutReset(0.0)
	, LastTimeoutResetEvent()
	, bDeveloperMode(false)
	, bFirstTimeStats(false)
	, bCompleted(false)
	, VerificationState(EUnitTestVerification::Unverified)
	, bVerificationLogged(false)
	, bAborted(false)
	, LogWindow()
	, LogColor(FSlateColor::UseForeground())
	, StatusLogSummary()
{
}

bool UUnitTest::ValidateUnitTestSettings(bool bCDOCheck/*=false*/)
{
	bool bSuccess = true;

	// The unit test must specify some ExpectedResult values
	UNIT_ASSERT(ExpectedResult.Num() > 0);


	TArray<EUnitTestVerification> ExpectedResultList;

	ExpectedResult.GenerateValueArray(ExpectedResultList);

	// Unit tests should not expect unreliable results, without being marked as unreliable
	UNIT_ASSERT(!ExpectedResultList.Contains(EUnitTestVerification::VerifiedUnreliable) || bUnreliable);

	// Unit tests should never expect 'needs-update' as a valid unit test result
	// @todo JohnB: It might make sense to allow this in the future, if you want to mark unit tests as 'needs-update' before running,
	//				so that you can skip running those unit tests
	UNIT_ASSERT(!ExpectedResultList.Contains(EUnitTestVerification::VerifiedNeedsUpdate));


	// Every unit test must specify a timeout value
	UNIT_ASSERT(UnitTestTimeout > 0);

	return bSuccess;
}

bool UUnitTest::UTStartUnitTest()
{
	bool bSuccess = false;

	StartTime = FPlatformTime::Seconds();

	if (bWorkInProgress)
	{
		UNIT_LOG(ELogType::StatusWarning, TEXT("WARNING: Unit test marked as 'work in progress', not included in automated tests."));
	}

	bSuccess = ExecuteUnitTest();

	if (bSuccess)
	{
		ResetTimeout(TEXT("StartUnitTest"));
	}
	else
	{
		CleanupUnitTest();
	}

	return bSuccess;
}

void UUnitTest::AbortUnitTest()
{
	bAborted = true;

	UNIT_LOG(ELogType::StatusWarning, TEXT("Aborting unit test execution"));

	EndUnitTest();
}

void UUnitTest::EndUnitTest()
{
	if (!bAborted)
	{
		// Save the time it took to execute the unit test
		LastExecutionTime = (float)(FPlatformTime::Seconds() - StartTime);
		SaveConfig();

		UNIT_LOG(ELogType::StatusImportant, TEXT("Execution time: %f seconds"), LastExecutionTime);
	}

	// If we aborted, and this is the first run, wipe any stats that were written
	if (bAborted && IsFirstTimeStats())
	{
		PeakMemoryUsage = 0;

		SaveConfig();
	}


	CleanupUnitTest();
	bCompleted = true;

	if (GUnitTestManager != NULL)
	{
		GUnitTestManager->NotifyUnitTestComplete(this, bAborted);
	}
}

void UUnitTest::CleanupUnitTest()
{
	if (GUnitTestManager != NULL)
	{
		GUnitTestManager->NotifyUnitTestCleanup(this);
	}
	else
	{
		// Mark for garbage collection
		MarkPendingKill();
	}
}

void UUnitTest::NotifyLocalLog(ELogType LogType, const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	if (LogWindow.IsValid())
	{
		TSharedPtr<SLogWidget>& LogWidget = LogWindow->LogWidget;

		ELogType LogOrigin = (LogType & ELogType::OriginMask);

		if (LogWidget.IsValid() && LogOrigin != ELogType::None && !(LogOrigin & ELogType::OriginVoid))
		{
			if (Verbosity == ELogVerbosity::SetColor)
			{
				// Unimplemented
			}
			else
			{
				FString LogLine = FOutputDevice::FormatLogLine(Verbosity, Category, Data, GPrintLogTimes);
				FSlateColor CurLogColor = FSlateColor::UseForeground();

				// Make unit test logs grey
				if (LogOrigin == ELogType::OriginUnitTest)
				{
					if (LogColor.IsColorSpecified())
					{
						CurLogColor = LogColor;
					}
					else
					{
						CurLogColor = FLinearColor(0.25f, 0.25f, 0.25f);
					}
				}
				// Prefix net logs with [NET]
				else if (LogOrigin == ELogType::OriginNet)
				{
					LogLine = FString(TEXT("[NET]")) + LogLine;
				}

				// Override log color, if a warning or error is being printed
				if (Verbosity == ELogVerbosity::Error)
				{
					CurLogColor = FLinearColor(1.f, 0.f, 0.f);
				}
				else if (Verbosity == ELogVerbosity::Warning)
				{
					CurLogColor = FLinearColor(1.f, 1.f, 0.f);
				}

				LogWidget->AddLine(LogType, MakeShareable(new FString(LogLine)), CurLogColor);
			}
		}
	}
}

void UUnitTest::PostUnitTick(float DeltaTime)
{
	if (!bDeveloperMode && UnitTestTimeout > 0 && LastTimeoutReset != 0.0 && TimeoutExpire != 0.0)
	{
		double TimeSinceLastReset = FPlatformTime::Seconds() - LastTimeoutReset;

		if ((FPlatformTime::Seconds() - TimeoutExpire) > 0.0)
		{
			FString UpdateMsg = FString::Printf(TEXT("Unit test timed out after '%f' seconds - marking unit test as needing update."),
													TimeSinceLastReset);

			UNIT_LOG(ELogType::StatusFailure | ELogType::StyleBold, TEXT("%s"), *UpdateMsg);
			UNIT_STATUS_LOG(ELogType::StatusFailure | ELogType::StatusVerbose | ELogType::StyleBold, TEXT("%s"), *UpdateMsg);


			UpdateMsg = FString::Printf(TEXT("Last event to reset the timeout timer: '%s'"), *LastTimeoutResetEvent);

			UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *UpdateMsg);
			UNIT_STATUS_LOG(ELogType::StatusImportant | ELogType::StatusVerbose, TEXT("%s"), *UpdateMsg);


			VerificationState = EUnitTestVerification::VerifiedNeedsUpdate;
		}
	}
}

void UUnitTest::TickIsComplete(float DeltaTime)
{
	// If the unit test was verified as success/fail (i.e. as completed), end it now
	if (!bCompleted && VerificationState != EUnitTestVerification::Unverified)
	{
		if (!bVerificationLogged)
		{
			if (VerificationState == EUnitTestVerification::VerifiedNotFixed)
			{
				UNIT_LOG(ELogType::StatusFailure, TEXT("Unit test verified as NOT FIXED"));
			}
			else if (VerificationState == EUnitTestVerification::VerifiedFixed)
			{
				UNIT_LOG(ELogType::StatusSuccess, TEXT("Unit test verified as FIXED"));
			}
			else if (VerificationState == EUnitTestVerification::VerifiedUnreliable)
			{
				UNIT_LOG(ELogType::StatusWarning, TEXT("Unit test verified as UNRELIABLE"));
			}
			else if (VerificationState == EUnitTestVerification::VerifiedNeedsUpdate)
			{
				UNIT_LOG(ELogType::StatusWarning | ELogType::StyleBold, TEXT("Unit test NEEDS UPDATE"));
			}
			else
			{
				// Don't forget to add new verification states
				UNIT_ASSERT(false);
			}

			bVerificationLogged = true;
		}

		if (!bDeveloperMode)
		{
			EndUnitTest();
		}
	}
}




// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealWatchdog.h"
#include "WatchdogAnalytics.h"

/**
* WinMain, called when the application is started
*/
INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	FString CommandLine;
	for (int32 Option = 1; Option < ArgC; Option++)
	{
		CommandLine += TEXT(" ");
		FString Argument(ArgV[Option]);
		if (Argument.Contains(TEXT(" ")))
		{
			if (Argument.Contains(TEXT("=")))
			{
				FString ArgName;
				FString ArgValue;
				Argument.Split(TEXT("="), &ArgName, &ArgValue);
				Argument = FString::Printf(TEXT("%s=\"%s\""), *ArgName, *ArgValue);
			}
			else
			{
				Argument = FString::Printf(TEXT("\"%s\""), *Argument);
			}
		}
		CommandLine += Argument;
	}

	// hide the console window
	ShowWindow(GetConsoleWindow(), SW_HIDE);

	// Run the app
	RunUnrealWatchdog(*CommandLine);

	return 0;
}

bool WaitForProcess(IAnalyticsProviderET& Analytics, const FWatchdogCommandLine& CommandLine, int32& OutReturnCode, bool& bOutHang, const FString& WatchdogSectionName)
{
	const int HangDetectionGraceSeconds = 10;
	const FTimespan CheckParentHeartbeatPeriod(0, 0, CommandLine.HeartbeatSeconds + HangDetectionGraceSeconds);
	const FTimespan SendWatchdogHeartbeatPeriod(0, 5, 0);
	const FTimespan CheckParentRunningPeriod(0, 0, 10);

	FDateTime NextHeartbeatSend = FDateTime::UtcNow();
	FDateTime NextHeartbeatCheck = FDateTime::UtcNow();
	bool bSuccess = false;
	bOutHang = false;
	OutReturnCode = -1;

	FProcHandle ParentProcess = FPlatformProcess::OpenProcess(CommandLine.ParentProcessId);
	if (ParentProcess.IsValid())
	{
		while (!GIsRequestingExit)
		{
			// Send heartbeat due?
			if (FDateTime::UtcNow() >= NextHeartbeatSend)
			{
				NextHeartbeatSend += SendWatchdogHeartbeatPeriod;
				SendHeartbeatEvent(Analytics, CommandLine);
			}

			// Parent application still running?
			if (!FPlatformProcess::IsApplicationRunning(CommandLine.ParentProcessId))
			{
				UE_LOG(UnrealWatchdogLog, Log, TEXT("Watchdog detected terminated process PID %u"), CommandLine.ParentProcessId);
				bSuccess = FPlatformProcess::GetProcReturnCode(ParentProcess, &OutReturnCode);
				break;
			}

			// Check parent heartbeat for hang?
			if (FDateTime::UtcNow() >= NextHeartbeatCheck)
			{
				NextHeartbeatCheck += CheckParentHeartbeatPeriod;
				bool bHangDetected = !CheckParentHeartbeat(Analytics, CommandLine, WatchdogSectionName);

				if (bHangDetected && !bOutHang)
				{
					// New hang
					SendHangDetectedEvent(Analytics, CommandLine);
					bOutHang = true;
				}
				else if (!bHangDetected && bOutHang)
				{
					// Previous hang recovered
					SendHangRecoveredEvent(Analytics, CommandLine);
					bOutHang = false;
				}
			}

			// Sleep until next event
			FDateTime EndLoopTime = FDateTime::UtcNow();
			FTimespan TimeToHeartbeatCheck = NextHeartbeatCheck - EndLoopTime;
			FTimespan TimeToHeartbeatSend = NextHeartbeatSend - EndLoopTime;
			double SleepSeconds = FMath::Min3(TimeToHeartbeatCheck.GetTotalSeconds(), TimeToHeartbeatSend.GetTotalSeconds(), CheckParentRunningPeriod.GetTotalSeconds());
			FPlatformProcess::Sleep((float)SleepSeconds);
		}

		FPlatformProcess::CloseProc(ParentProcess);
	}
	else
	{
		UE_LOG(UnrealWatchdogLog, Error, TEXT("Watchdog failed to get handle to process PID %u"), CommandLine.ParentProcessId);
	}

	return bSuccess;
}
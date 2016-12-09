// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

DECLARE_LOG_CATEGORY_EXTERN(UnrealWatchdogLog, Log, All);

struct FWatchdogCommandLine
{
	FString RunType;
	FString ProjectName;
	FString PlatformName;
	FString SessionId;
	FString EngineVersion;
	uint32 ParentProcessId;
	int32 SuccessReturnCode;
	int32 HeartbeatSeconds;
	bool bAllowDialogs;
	bool bHasProcessId;

	FWatchdogCommandLine(const TCHAR* InCommandLine)
	{
		// Read command line args
		FParse::Value(InCommandLine, TEXT("RunType="), RunType);
		FParse::Value(InCommandLine, TEXT("ProjectName="), ProjectName);
		FParse::Value(InCommandLine, TEXT("Platform="), PlatformName);
		FParse::Value(InCommandLine, TEXT("SessionId="), SessionId);
		FParse::Value(InCommandLine, TEXT("EngineVersion="), EngineVersion);
		bHasProcessId = FParse::Value(InCommandLine, TEXT("PID="), ParentProcessId);
		SuccessReturnCode = 0;
		FParse::Value(InCommandLine, TEXT("SuccessfulRtnCode="), SuccessReturnCode);
		HeartbeatSeconds = 300;
		FParse::Value(InCommandLine, TEXT("HeartbeatSeconds="), HeartbeatSeconds);
		bAllowDialogs = FParse::Param(InCommandLine, TEXT("AllowDialogs"));
	}
};

int RunUnrealWatchdog(const TCHAR* CommandLine);
bool WaitForProcess(class IAnalyticsProviderET& Analytics, const FWatchdogCommandLine& CommandLine, int32& OutReturnCode, bool& bOutHang, const FString& WatchdogSectionName);
void SendHeartbeatEvent(class IAnalyticsProviderET& Analytics, const FWatchdogCommandLine& CommandLine);
bool CheckParentHeartbeat(class IAnalyticsProviderET& Analytics, const FWatchdogCommandLine& CommandLine, const FString& WatchdogSectionName);
void SendHangDetectedEvent(class IAnalyticsProviderET& Analytics, const FWatchdogCommandLine& CommandLine);
void SendHangRecoveredEvent(class IAnalyticsProviderET& Analytics, const FWatchdogCommandLine& CommandLine);

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

enum class EEngineSessionManagerMode
{
	Editor,
	Game
};

/* Handles writing session records to platform's storage to track crashed and timed-out editor sessions */
class FEngineSessionManager
{
public:
	FEngineSessionManager(EEngineSessionManagerMode InMode)
		: Mode(InMode)
		, bInitializedRecords(false)
		, bShutdown(false)
		, HeartbeatTimeElapsed(0.0f)
	{
		CurrentSession.bCrashed = false;
		CurrentSession.bIsDeactivated = false;
		CurrentSession.bIsInBackground = false;
	}

	void Initialize();

	void Tick(float DeltaTime);

	void Shutdown();

private:
	struct FSessionRecord
	{
		FString SessionId;
		EEngineSessionManagerMode Mode;
		FString ProjectName;
		FString EngineVersion;
		FDateTime Timestamp;
		bool bCrashed;
		bool bIsDebugger;
		bool bIsDeactivated;
		bool bIsInBackground;
		FString CurrentUserActivity;
	};

private:
	void InitializeRecords(bool bFirstAttempt);
	bool BeginReadWriteRecords();
	void EndReadWriteRecords();
	void DeleteStoredRecord(const FSessionRecord& Record);
	void SendAbnormalShutdownReport(const FSessionRecord& Record);
	void CreateAndWriteRecordForSession();
	void OnCrashing();
	void OnAppReactivate();
	void OnAppDeactivate();
	void OnAppBackground();
	void OnAppForeground();
	FString GetStoreSectionString(FString InSuffix);
	void OnUserActivity(const FUserActivity& UserActivity);
	FString GetUserActivityString() const;

private:
	EEngineSessionManagerMode Mode;
	bool bInitializedRecords;
	bool bShutdown;
	float HeartbeatTimeElapsed;
	FSessionRecord CurrentSession;
	FString CurrentSessionSectionName;
	TArray<FSessionRecord> SessionRecords;
};

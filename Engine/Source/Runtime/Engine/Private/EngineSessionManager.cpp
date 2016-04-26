// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "EngineSessionManager.h"
#include "EngineAnalytics.h"
#include "IAnalyticsProvider.h"
#include "AnalyticsEventAttribute.h"
#include "GeneralProjectSettings.h"
#include "UserActivityTracking.h"

#define LOCTEXT_NAMESPACE "SessionManager"

namespace SessionManagerDefs
{
	static const FTimespan SessionRecordExpiration(30, 0, 0, 0);	// 30 days
	static const FTimespan SessionRecordTimeout(0, 30, 0);			// 30 minutes
	static const FTimespan GlobalLockWaitTimeout(0, 0, 0, 0, 500);	// 1/2 second
	static const float HeartbeatPeriodSeconds(300.0f);				// 5 minutes
	static const FString DefaultUserActivity(TEXT("Unknown"));
	static const FString StoreId(TEXT("Epic Games"));
	static const FString CrashSessionToken(TEXT("Crashed"));
	static const FString DebuggerSessionToken(TEXT("Debugger"));
	static const FString AbnormalSessionToken(TEXT("AbnormalShutdown"));
	static const FString PS4SessionToken(TEXT("AbnormalShutdownPS4"));
	static const FString SessionRecordListSection(TEXT("List"));
	static const FString EditorSessionRecordSectionPrefix(TEXT("Unreal Engine/Editor Sessions/"));
	static const FString GameSessionRecordSectionPrefix(TEXT("Unreal Engine/Game Sessions/"));
	static const FString SessionsVersionString(TEXT("1_3"));
	static const FString ModeStoreKey(TEXT("Mode"));
	static const FString ProjectNameStoreKey(TEXT("ProjectName"));
	static const FString CrashStoreKey(TEXT("IsCrash"));
	static const FString DeactivatedStoreKey(TEXT("IsDeactivated"));
	static const FString BackgroundStoreKey(TEXT("IsInBackground"));
	static const FString EngineVersionStoreKey(TEXT("EngineVersion"));
	static const FString TimestampStoreKey(TEXT("Timestamp"));
	static const FString DebuggerStoreKey(TEXT("IsDebugger"));
	static const FString UserActivityStoreKey(TEXT("CurrentUserActivity"));
	static const FString GlobalLockName(TEXT("UE4_SessionManager_Lock"));
	static const FString FalseValueString(TEXT("0"));
	static const FString TrueValueString(TEXT("1"));
	static const FString EditorValueString(TEXT("Editor"));
	static const FString GameValueString(TEXT("Game"));
	static const FString UnknownProjectValueString(TEXT("UnknownProject"));
}

namespace
{
	FString TimestampToString(FDateTime InTimestamp)
	{
		return LexicalConversion::ToString(InTimestamp.ToUnixTimestamp());
	}

	FDateTime StringToTimestamp(FString InString)
	{
		int64 TimestampUnix;
		if (LexicalConversion::TryParseString(TimestampUnix, *InString))
		{
			return FDateTime::FromUnixTimestamp(TimestampUnix);
		}
		return FDateTime::MinValue();
	}
}

/* FEngineSessionManager */

void FEngineSessionManager::Initialize()
{
	// Register for crash and app state callbacks
	FCoreDelegates::OnHandleSystemError.AddRaw(this, &FEngineSessionManager::OnCrashing);
	FCoreDelegates::ApplicationHasReactivatedDelegate.AddRaw(this, &FEngineSessionManager::OnAppReactivate);
	FCoreDelegates::ApplicationWillDeactivateDelegate.AddRaw(this, &FEngineSessionManager::OnAppDeactivate);
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FEngineSessionManager::OnAppBackground);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FEngineSessionManager::OnAppForeground);
	FUserActivityTracking::OnActivityChanged.AddRaw(this, &FEngineSessionManager::OnUserActivity);

	const bool bFirstInitAttempt = true;
	InitializeRecords(bFirstInitAttempt);
}

void FEngineSessionManager::InitializeRecords(bool bFirstAttempt)
{
	if (!FEngineAnalytics::IsAvailable())
	{
		return;
	}

	TArray<FSessionRecord> SessionRecordsToReport;

	{
		// Scoped lock
		FSystemWideCriticalSection StoredValuesLock(SessionManagerDefs::GlobalLockName, bFirstAttempt ? SessionManagerDefs::GlobalLockWaitTimeout : FTimespan::Zero());

		// Get list of sessions in storage
		if (StoredValuesLock.IsValid() && BeginReadWriteRecords())
		{
			TArray<FSessionRecord> SessionRecordsToDelete;

			// Attempt check each stored session
			for (FSessionRecord& Record : SessionRecords)
			{
				FTimespan RecordAge = FDateTime::UtcNow() - Record.Timestamp;

				if (Record.bCrashed)
				{
					// Crashed sessions
					SessionRecordsToReport.Add(Record);
					SessionRecordsToDelete.Add(Record);
				}
				else if (RecordAge > SessionManagerDefs::SessionRecordExpiration)
				{
					// Delete expired session records
					SessionRecordsToDelete.Add(Record);
				}
				else if (RecordAge > SessionManagerDefs::SessionRecordTimeout)
				{
					// Timed out sessions
					SessionRecordsToReport.Add(Record);
					SessionRecordsToDelete.Add(Record);
				}
			}

			for (FSessionRecord& DeletingRecord : SessionRecordsToDelete)
			{
				DeleteStoredRecord(DeletingRecord);
			}

			// Create a session record for this session
			CreateAndWriteRecordForSession();

			// Update and release list of sessions in storage
			EndReadWriteRecords();

			bInitializedRecords = true;
		}
	}

	for (FSessionRecord& ReportingSession : SessionRecordsToReport)
	{
		// Send error report for session that timed out or crashed
		SendAbnormalShutdownReport(ReportingSession);
	}
}

void FEngineSessionManager::Tick(float DeltaTime)
{
	HeartbeatTimeElapsed += DeltaTime;

	if (HeartbeatTimeElapsed > SessionManagerDefs::HeartbeatPeriodSeconds && !bShutdown)
	{
		HeartbeatTimeElapsed = 0.0f;

		if (!bInitializedRecords)
		{
			// Try late initialization
			const bool bFirstInitAttempt = false;
			InitializeRecords(bFirstInitAttempt);
		}

		// Update timestamp in the session record for this session 
		if (bInitializedRecords)
		{
			CurrentSession.Timestamp = FDateTime::UtcNow();

			FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::TimestampStoreKey, TimestampToString(CurrentSession.Timestamp));
		}
	}
}

void FEngineSessionManager::Shutdown()
{
	FCoreDelegates::OnHandleSystemError.RemoveAll(this);
	FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationWillDeactivateDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.RemoveAll(this);

	// Clear the session record for this session
	if (bInitializedRecords)
	{
		if (!CurrentSession.bCrashed)
		{
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::ModeStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::ProjectNameStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::CrashStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::EngineVersionStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::TimestampStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::DebuggerStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::DeactivatedStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::BackgroundStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::UserActivityStoreKey);
		}

		bInitializedRecords = false;
		bShutdown = true;
	}
}

bool FEngineSessionManager::BeginReadWriteRecords()
{
	SessionRecords.Empty();

	// Lock and read the list of sessions in storage
	FString ListSectionName = GetStoreSectionString(SessionManagerDefs::SessionRecordListSection);

	// Write list to SessionRecords member
	FString SessionListString;
	FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, ListSectionName, TEXT("SessionList"), SessionListString);

	// Parse SessionListString for session ids
	TArray<FString> SessionIds;
	SessionListString.ParseIntoArray(SessionIds, TEXT(","));

	// Retrieve all the sessions in the list from storage
	for (FString& SessionId : SessionIds)
	{
		FString SectionName = GetStoreSectionString(SessionId);

		FString IsCrashString;
		FString EngineVersionString;
		FString TimestampString;
		FString IsDebuggerString;

		// Read manditory values
		if (FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::CrashStoreKey, IsCrashString) &&
			FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::EngineVersionStoreKey, EngineVersionString) &&
			FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::TimestampStoreKey, TimestampString) &&
			FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::DebuggerStoreKey, IsDebuggerString))
		{
			// Read optional values
			FString ModeString = SessionManagerDefs::EditorValueString;
			FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::ModeStoreKey, ModeString);
			FString ProjectName = SessionManagerDefs::UnknownProjectValueString;
			FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::ProjectNameStoreKey, ProjectName);
			FString IsDeactivatedString = SessionManagerDefs::FalseValueString;
			FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::DeactivatedStoreKey, IsDeactivatedString);
			FString IsInBackgroundString = SessionManagerDefs::FalseValueString;
			FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::BackgroundStoreKey, IsInBackgroundString);
			FString UserActivityString = SessionManagerDefs::DefaultUserActivity;
			FPlatformMisc::GetStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::UserActivityStoreKey, UserActivityString);

			// Create new record from the read values
			FSessionRecord NewRecord;
			NewRecord.SessionId = SessionId;
			NewRecord.Mode = ModeString == SessionManagerDefs::EditorValueString ? EEngineSessionManagerMode::Editor : EEngineSessionManagerMode::Game;
			NewRecord.ProjectName = ProjectName;
			NewRecord.EngineVersion = EngineVersionString;
			NewRecord.Timestamp = StringToTimestamp(TimestampString);
			NewRecord.bCrashed = IsCrashString == SessionManagerDefs::TrueValueString;
			NewRecord.bIsDebugger = IsDebuggerString == SessionManagerDefs::TrueValueString;
			NewRecord.bIsDeactivated = IsDeactivatedString == SessionManagerDefs::TrueValueString;
			NewRecord.bIsInBackground = IsInBackgroundString == SessionManagerDefs::TrueValueString;
			NewRecord.CurrentUserActivity = UserActivityString;

			SessionRecords.Add(NewRecord);
		}
		else
		{
			// Clean up orphaned values, if there are any
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::ModeStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::ProjectNameStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::CrashStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::EngineVersionStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::TimestampStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::DebuggerStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::DeactivatedStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::BackgroundStoreKey);
			FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::UserActivityStoreKey);
		}
	}

	return true;
}

void FEngineSessionManager::EndReadWriteRecords()
{
	// Update the list of sessions in storage to match SessionRecords
	FString SessionListString;
	if (SessionRecords.Num() > 0)
	{
		for (FSessionRecord& Session : SessionRecords)
		{
			SessionListString.Append(Session.SessionId);
			SessionListString.Append(TEXT(","));
		}
		SessionListString.RemoveAt(SessionListString.Len() - 1);
	}

	FString ListSectionName = GetStoreSectionString(SessionManagerDefs::SessionRecordListSection);
	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, ListSectionName, TEXT("SessionList"), SessionListString);

	// Clear SessionRecords member
	SessionRecords.Empty();
}

void FEngineSessionManager::DeleteStoredRecord(const FSessionRecord& Record)
{
	// Delete the session record in storage
	FString SessionId = Record.SessionId;
	FString SectionName = GetStoreSectionString(SessionId);

	FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::ModeStoreKey);
	FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::ProjectNameStoreKey);
	FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::CrashStoreKey);
	FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::EngineVersionStoreKey);
	FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::TimestampStoreKey);
	FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::DebuggerStoreKey);
	FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::DeactivatedStoreKey);
	FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::BackgroundStoreKey);
	FPlatformMisc::DeleteStoredValue(SessionManagerDefs::StoreId, SectionName, SessionManagerDefs::UserActivityStoreKey);

	// Remove the session record from SessionRecords list
	SessionRecords.RemoveAll([&SessionId](const FSessionRecord& X){ return X.SessionId == SessionId; });
}

void FEngineSessionManager::SendAbnormalShutdownReport(const FSessionRecord& Record)
{
#if PLATFORM_WINDOWS
	const FString PlatformName(TEXT("Windows"));
#elif PLATFORM_MAC
	const FString PlatformName(TEXT("Mac"));
#elif PLATFORM_LINUX
	const FString PlatformName(TEXT("Linux"));
#elif PLATFORM_PS4
	const FString PlatformName(TEXT("PS4"));
	if (Record.bIsDeactivated && !Record.bCrashed)
	{
		// Shutting down in deactivated state on PS4 is normal - don't report it
		return;
	}
#elif PLATFORM_XBOXONE
	const FString PlatformName(TEXT("XBoxOne"));
	if (Record.bIsInBackground && !Record.bCrashed)
	{
		// Shutting down in background state on XB1 is normal - don't report it
		return;
	}
#else
	const FString PlatformName(TEXT("Unknown"));
	return; // TODO: CWood: disabled on other platforms
#endif

	FGuid SessionId;
	FString SessionIdString = Record.SessionId;
	if (FGuid::Parse(SessionIdString, SessionId))
	{
		// convert session guid to one with braces for sending to analytics
		SessionIdString = SessionId.ToString(EGuidFormats::DigitsWithHyphensInBraces);
	}

#if !PLATFORM_PS4
	FString ShutdownTypeString = Record.bCrashed ? SessionManagerDefs::CrashSessionToken :
		(Record.bIsDebugger ? SessionManagerDefs::DebuggerSessionToken : SessionManagerDefs::AbnormalSessionToken);
#else
	// PS4 cannot set the crash flag so report abnormal shutdowns with a specific token meaning "crash or abnormal shutdown".
	FString ShutdownTypeString = Record.bIsDebugger ? SessionManagerDefs::DebuggerSessionToken : SessionManagerDefs::PS4SessionToken;
#endif

	FString RunTypeString = Record.Mode == EEngineSessionManagerMode::Editor ? SessionManagerDefs::EditorValueString : SessionManagerDefs::GameValueString;

	TArray< FAnalyticsEventAttribute > AbnormalShutdownAttributes;
	AbnormalShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("RunType"), RunTypeString));
	AbnormalShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("ProjectName"), Record.ProjectName));
	AbnormalShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("Platform"), PlatformName));
	AbnormalShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("SessionId"), SessionIdString));
	AbnormalShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("EngineVersion"), Record.EngineVersion));
	AbnormalShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("ShutdownType"), ShutdownTypeString));
	AbnormalShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("Timestamp"), Record.Timestamp.ToIso8601()));
	AbnormalShutdownAttributes.Add(FAnalyticsEventAttribute(TEXT("CurrentUserActivity"), Record.CurrentUserActivity));

	FEngineAnalytics::GetProvider().RecordEvent(TEXT("Engine.AbnormalShutdown"), AbnormalShutdownAttributes);
}

void FEngineSessionManager::CreateAndWriteRecordForSession()
{
	FGuid SessionId;
	if (FGuid::Parse(FEngineAnalytics::GetProvider().GetSessionID(), SessionId))
	{
		// convert session guid to one without braces or other chars that might not be suitable for storage
		CurrentSession.SessionId = SessionId.ToString(EGuidFormats::DigitsWithHyphens);
	}
	else
	{
		CurrentSession.SessionId = FEngineAnalytics::GetProvider().GetSessionID();
	}

	const UGeneralProjectSettings& ProjectSettings = *GetDefault<UGeneralProjectSettings>();

	CurrentSession.Mode = Mode;
	CurrentSession.ProjectName = ProjectSettings.ProjectName;
	CurrentSession.EngineVersion = FEngineVersion::Current().ToString(EVersionComponent::Changelist);
	CurrentSession.Timestamp = FDateTime::UtcNow();
	CurrentSession.bIsDebugger = FPlatformMisc::IsDebuggerPresent();
	CurrentSession.CurrentUserActivity = GetUserActivityString();
	CurrentSessionSectionName = GetStoreSectionString(CurrentSession.SessionId);

	FString ModeString = CurrentSession.Mode == EEngineSessionManagerMode::Editor ? SessionManagerDefs::EditorValueString : SessionManagerDefs::GameValueString;
	FString IsDebuggerString = CurrentSession.bIsDebugger ? SessionManagerDefs::TrueValueString : SessionManagerDefs::FalseValueString;
	FString IsDeactivatedString = CurrentSession.bIsDeactivated ? SessionManagerDefs::TrueValueString : SessionManagerDefs::FalseValueString;
	FString IsInBackgroundString = CurrentSession.bIsInBackground ? SessionManagerDefs::TrueValueString : SessionManagerDefs::FalseValueString;

	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::ModeStoreKey, ModeString);
	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::ProjectNameStoreKey, CurrentSession.ProjectName);
	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::CrashStoreKey, SessionManagerDefs::FalseValueString);
	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::EngineVersionStoreKey, CurrentSession.EngineVersion);
	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::TimestampStoreKey, TimestampToString(CurrentSession.Timestamp));
	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::DebuggerStoreKey, IsDebuggerString);
	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::DeactivatedStoreKey, IsDeactivatedString);
	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::BackgroundStoreKey, IsInBackgroundString);
	FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::UserActivityStoreKey, CurrentSession.CurrentUserActivity);

	SessionRecords.Add(CurrentSession);
}

void FEngineSessionManager::OnCrashing()
{
	if (!CurrentSession.bCrashed && bInitializedRecords)
	{
		CurrentSession.bCrashed = true;
		FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::CrashStoreKey, SessionManagerDefs::TrueValueString);
	}
}

void FEngineSessionManager::OnAppReactivate()
{
	if (CurrentSession.bIsDeactivated)
	{
		CurrentSession.bIsDeactivated = false;
		FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::DeactivatedStoreKey, SessionManagerDefs::FalseValueString);
	}
}

void FEngineSessionManager::OnAppDeactivate()
{
	if (!CurrentSession.bIsDeactivated)
	{
		CurrentSession.bIsDeactivated = true;
		FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::DeactivatedStoreKey, SessionManagerDefs::TrueValueString);
	}
}

void FEngineSessionManager::OnAppBackground()
{
	if (!CurrentSession.bIsInBackground)
	{
		CurrentSession.bIsInBackground = true;
		FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::BackgroundStoreKey, SessionManagerDefs::TrueValueString);
	}
}

void FEngineSessionManager::OnAppForeground()
{
	if (CurrentSession.bIsInBackground)
	{
		CurrentSession.bIsInBackground = false;
		FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::BackgroundStoreKey, SessionManagerDefs::FalseValueString);
	}
}

FString FEngineSessionManager::GetStoreSectionString(FString InSuffix)
{
	check(Mode == EEngineSessionManagerMode::Editor || Mode == EEngineSessionManagerMode::Game)

	if (Mode == EEngineSessionManagerMode::Editor)
	{
		return FString::Printf(TEXT("%s%s/%s"), *SessionManagerDefs::EditorSessionRecordSectionPrefix, *SessionManagerDefs::SessionsVersionString, *InSuffix);
	}
	else
	{
		const UGeneralProjectSettings& ProjectSettings = *GetDefault<UGeneralProjectSettings>();
		return FString::Printf(TEXT("%s%s/%s/%s"), *SessionManagerDefs::GameSessionRecordSectionPrefix, *SessionManagerDefs::SessionsVersionString, *ProjectSettings.ProjectName, *InSuffix);
	}
}

void FEngineSessionManager::OnUserActivity(const FUserActivity& UserActivity)
{
	if (!CurrentSession.bCrashed && bInitializedRecords)
	{
		CurrentSession.CurrentUserActivity = GetUserActivityString();
		FPlatformMisc::SetStoredValue(SessionManagerDefs::StoreId, CurrentSessionSectionName, SessionManagerDefs::UserActivityStoreKey, CurrentSession.CurrentUserActivity);
	}
}

FString FEngineSessionManager::GetUserActivityString() const
{
	const FUserActivity& UserActivity = FUserActivityTracking::GetUserActivity();
	
	if (UserActivity.ActionName.IsEmpty())
	{
		return SessionManagerDefs::DefaultUserActivity;
	}

	return UserActivity.ActionName;
}

#undef LOCTEXT_NAMESPACE

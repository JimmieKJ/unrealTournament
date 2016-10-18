// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HotfixPrivatePCH.h"

#include "UpdateManager.h"
#include "OnlineHotfixManager.h"

#include "Online.h"
#include "OnlineSubsystemUtils.h"

#include "LoadTimeTracker.h"

#define UPDATE_CHECK_SECONDS 30.0

static TAutoConsoleVariable<int32> CVarDebugUpdateManager(
	TEXT("UI.DebugUpdateCheck"),
	-1,
	TEXT("Force switch between update states (-1 is off)"));

struct FLoadingScreenConfig
{
public:
	// Do we check for hotfixes in this build?
	static bool CheckForHotfixes() { return true; }

	// Do we block waiting for pending async loads to complete during the initial loading screen state?
	static bool ShouldBlockOnInitialLoad() { return (FPlatformProperties::IsServerOnly() || true); }

	// Can we preload assets used in Agora during the initial loading screen?
	static bool CanPreloadMapAssets() { return true; }

private:
	FLoadingScreenConfig() {}
};

UUpdateManager::UUpdateManager()
	: HotfixCheckCompleteDelay(0.1f)
	, UpdateCheckCompleteDelay(0.5f)
	, HotfixAvailabilityCheckCompleteDelay(0.1f)
	, UpdateCheckAvailabilityCompleteDelay(0.1f)
	, bPlatformEnvironmentDetected(false)
	, bInitialUpdateFinished(false)
	, bCheckHotfixAvailabilityOnly(false)
	, CurrentUpdateState(EUpdateState::UpdateIdle)
	, WorstNumFilesPendingLoadViewed(0)
	, LastPatchCheckResult(EPatchCheckResult::PatchCheckFailure)
	, LastHotfixResult(EHotfixResult::Failed)
	, LoadStartTime(0.0)
	, UpdateStateEnum(nullptr)
	, UpdateCompletionEnum(nullptr)
{
	LastUpdateCheck[0] = LastUpdateCheck[1] = FDateTime(0);
	LastCompletionResult[0] = LastCompletionResult[1] = EUpdateCompletionStatus::UpdateUnknown;
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		UpdateStateEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EUpdateState"));
		UpdateCompletionEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EUpdateCompletionStatus"));
	}
}

void UUpdateManager::SetPending()
{ 
	if (ChecksEnabled())
	{
		CurrentUpdateState = EUpdateState::UpdatePending;
	}
}

void UUpdateManager::Reset()
{
	LastUpdateCheck[0] = LastUpdateCheck[1] = FDateTime(0);
	SetUpdateState(EUpdateState::UpdatePending);
}

void UUpdateManager::StartCheck(bool bInCheckHotfixOnly)
{
	StartCheckInternal(bInCheckHotfixOnly);
}

UUpdateManager::EUpdateStartResult UUpdateManager::StartCheckInternal(bool bInCheckHotfixOnly)
{
	EUpdateStartResult Result = EUpdateStartResult::None;

	if (!ChecksEnabled())
	{
		UE_LOG(LogHotfixManager, Display, TEXT("Update checks disabled!"));
		bInitialUpdateFinished = true;

		auto StartDelegate = [this]()
		{
			CheckComplete(EUpdateCompletionStatus::UpdateSuccess_NoChange);
		};
		
		DelayResponse(StartDelegate, 0.1f);
		return Result;
	}

	if (CurrentUpdateState == EUpdateState::UpdateIdle ||
		CurrentUpdateState == EUpdateState::UpdatePending ||
		CurrentUpdateState == EUpdateState::UpdateComplete)
	{
		bCheckHotfixAvailabilityOnly = bInCheckHotfixOnly;

		// Immediately move into a pending state so the UI state trigger fires
		SetUpdateState(EUpdateState::UpdatePending);

		const EUpdateCompletionStatus LastResult = LastCompletionResult[bCheckHotfixAvailabilityOnly];
		const FTimespan DeltaTime = FDateTime::UtcNow() - LastUpdateCheck[bCheckHotfixAvailabilityOnly];

		const bool bForceCheck = LastResult == EUpdateCompletionStatus::UpdateUnknown ||
								 LastResult == EUpdateCompletionStatus::UpdateFailure_PatchCheck ||
								 LastResult == EUpdateCompletionStatus::UpdateFailure_HotfixCheck ||
								 LastResult == EUpdateCompletionStatus::UpdateFailure_NotLoggedIn;

		static double CacheTimer = UPDATE_CHECK_SECONDS;
		const double TimeSinceCheck = DeltaTime.GetTotalSeconds();
		if (bForceCheck || TimeSinceCheck >= CacheTimer)
		{
			auto StartDelegate = [this]()
			{
				// Check for a patch first, then hotfix application
				StartPatchCheck();
			};

			// Give the UI state widget a chance to start listening for delegates
			DelayResponse(StartDelegate, 0.2f);
			Result = EUpdateStartResult::UpdateStarted;
		}
		else
		{
			UE_LOG(LogHotfixManager, Display, TEXT("Returning cached update result %d"), (int32)LastResult);
			auto StartDelegate = [this, LastResult]()
			{
				CheckComplete(LastResult, false);
			};

			DelayResponse(StartDelegate, 0.1f);
			Result = EUpdateStartResult::UpdateCached;
		}
	}
	else
	{
		UE_LOG(LogHotfixManager, Display, TEXT("Update already in progress"));
	}

	return Result;
}

void UUpdateManager::CheckComplete(EUpdateCompletionStatus Result, bool bUpdateTimestamp)
{
	UE_LOG(LogHotfixManager, Display, TEXT("CheckComplete %s"), UpdateCompletionEnum ? *UpdateCompletionEnum->GetEnumName((int32)Result) : TEXT("Invalid"));

	UGameInstance* GameInstance = GetGameInstance();
	bool bIsServer = GameInstance->IsDedicatedServerInstance();

#if !UE_BUILD_SHIPPING
	int32 DbgVal = CVarDebugUpdateManager.GetValueOnGameThread();
	if (DbgVal >= 0 && DbgVal <= (int32)EUpdateCompletionStatus::UpdateFailure_NotLoggedIn)
	{
		Result = (EUpdateCompletionStatus)DbgVal;
		UE_LOG(LogHotfixManager, Display, TEXT("CheckComplete OVERRIDE! %s"), UpdateCompletionEnum ? *UpdateCompletionEnum->GetEnumName((int32)Result) : TEXT("Invalid"));
	}
#endif

	LastCompletionResult[bCheckHotfixAvailabilityOnly] = Result;

	const bool bSuccessResult = (Result == EUpdateCompletionStatus::UpdateSuccess ||	
								 Result == EUpdateCompletionStatus::UpdateSuccess_NoChange ||
								 Result == EUpdateCompletionStatus::UpdateSuccess_NeedsReload ||
								 Result == EUpdateCompletionStatus::UpdateSuccess_NeedsRelaunch);
	
	if (bUpdateTimestamp && bSuccessResult)
	{
		LastUpdateCheck[bCheckHotfixAvailabilityOnly] = FDateTime::UtcNow();
	}

	auto CompletionDelegate = [this, Result]()
	{
		UE_LOG(LogHotfixManager, Display, TEXT("External CheckComplete %s"), UpdateCompletionEnum ? *UpdateCompletionEnum->GetEnumName((int32)Result) : TEXT("Invalid"));
		if (!bInitialUpdateFinished)
		{
			// Prime the state so that the first "after login" check will occur
			bInitialUpdateFinished = true;
			SetUpdateState(EUpdateState::UpdatePending);
		}
		else
		{
			SetUpdateState(EUpdateState::UpdateComplete);
		}

		bCheckHotfixAvailabilityOnly = false;
		OnUpdateCheckComplete().Broadcast(Result);
	};

	// Delay completion delegate to give UI a chance to show the screen for a reasonable amount of time
	DelayResponse(CompletionDelegate, bCheckHotfixAvailabilityOnly ? UpdateCheckAvailabilityCompleteDelay : UpdateCheckCompleteDelay);
}

void UUpdateManager::StartPatchCheck()
{
	ensure(ChecksEnabled());

	bool bStarted = false;

	SetUpdateState(EUpdateState::CheckingForPatch);

	IOnlineSubsystem* OnlineSubConsole = IOnlineSubsystem::GetByPlatform();
	if (OnlineSubConsole)
	{
		IOnlineIdentityPtr OnlineIdentityConsole = OnlineSubConsole->GetIdentityInterface();
		if (OnlineIdentityConsole.IsValid())
		{
			bStarted = true;

			TSharedPtr<const FUniqueNetId> UserId = OnlineIdentityConsole->GetUniquePlayerId(0);
			OnlineIdentityConsole->GetUserPrivilege(*UserId,
				EUserPrivileges::CanPlayOnline, IOnlineIdentity::FOnGetUserPrivilegeCompleteDelegate::CreateUObject(this, &ThisClass::OnCheckForPatchComplete, true));
		}
	}
	else
	{
		UGameInstance* GameInstance = GetGameInstance();
		if (GameInstance->IsDedicatedServerInstance())
		{
			bStarted = true;
			PatchCheckComplete(EPatchCheckResult::NoPatchRequired);
		}
		else
		{
			UWorld* World = GetWorld();
			IOnlineIdentityPtr IdentityInt = Online::GetIdentityInterface(World);
			if (IdentityInt.IsValid())
			{
				bStarted = true;
				TSharedPtr<const FUniqueNetId> UserId = GameInstance->GetPrimaryPlayerUniqueId();
				if (!bInitialUpdateFinished && !UserId.IsValid())
				{
					// Invalid user for "before title/login" check, underlying code doesn't need a valid user currently
					UserId = IdentityInt->CreateUniquePlayerId(TEXT("InvalidUser"));
				}

				IdentityInt->GetUserPrivilege(*UserId,
					EUserPrivileges::CanPlayOnline, IOnlineIdentity::FOnGetUserPrivilegeCompleteDelegate::CreateUObject(this, &ThisClass::OnCheckForPatchComplete, false));
			}
		}
	}

	if (!bStarted)
	{
		PatchCheckComplete(EPatchCheckResult::PatchCheckFailure);
	}
}

bool UUpdateManager::ChecksEnabled() const
{
	return !GIsEditor;
}

bool UUpdateManager::EnvironmentWantsPatchCheck() const
{
	return false;
}

inline bool SkipPatchCheck(UUpdateManager* UpdateManager)
{
	// Does the environment care about patch checks (LIVE, STAGE, etc)
	bool bEnvironmentWantsPatchCheck = UpdateManager->EnvironmentWantsPatchCheck();

	// Can always opt in to a check
	const bool bForcePatchCheck = FParse::Param(FCommandLine::Get(), TEXT("ForcePatchCheck"));

	// Prevent a patch check on editor builds 
	const bool bSkipDueToEditor = UE_EDITOR;
	// Explicitly skipping the check
	const bool bForceSkipCheck = FParse::Param(FCommandLine::Get(), TEXT("SkipPatchCheck"));
	const bool bSkipPatchCheck = !bForcePatchCheck && (!bEnvironmentWantsPatchCheck || bSkipDueToEditor || bForceSkipCheck);

	return bSkipPatchCheck;
}

void UUpdateManager::OnCheckForPatchComplete(const FUniqueNetId& UniqueId, EUserPrivileges::Type Privilege, uint32 PrivilegeResult, bool bConsoleCheck)
{
	UE_LOG(LogHotfixManager, Verbose, TEXT("[OnCheckForPatchComplete] Privilege=%d PrivilegeResult=%d"), (uint32)Privilege, PrivilegeResult);

	EPatchCheckResult Result = EPatchCheckResult::NoPatchRequired;
	if (Privilege == EUserPrivileges::CanPlayOnline)
	{
		if (bConsoleCheck || !SkipPatchCheck(this))
		{
			if (PrivilegeResult & (uint32)IOnlineIdentity::EPrivilegeResults::RequiredSystemUpdate)
			{
				Result = EPatchCheckResult::PatchRequired;
			}
			else if (PrivilegeResult & (uint32)IOnlineIdentity::EPrivilegeResults::RequiredPatchAvailable)
			{
				Result = EPatchCheckResult::PatchRequired;
			}
			else if (PrivilegeResult & ((uint32)IOnlineIdentity::EPrivilegeResults::UserNotLoggedIn | (uint32)IOnlineIdentity::EPrivilegeResults::UserNotFound))
			{
				Result = EPatchCheckResult::NoLoggedInUser;
			}
			else if (PrivilegeResult & (uint32)IOnlineIdentity::EPrivilegeResults::GenericFailure)
			{
				Result = EPatchCheckResult::PatchCheckFailure;
			}
		}
	}

	PatchCheckComplete(Result);
}

void UUpdateManager::PatchCheckComplete(EPatchCheckResult PatchResult)
{
	LastPatchCheckResult = PatchResult;

	if (PatchResult == EPatchCheckResult::NoPatchRequired)
	{
		StartPlatformEnvironmentCheck();
	}
	else if (PatchResult == EPatchCheckResult::NoLoggedInUser)
	{
		CheckComplete(EUpdateCompletionStatus::UpdateFailure_NotLoggedIn);
	}
	else
	{
		ensure(PatchResult == EPatchCheckResult::PatchCheckFailure ||
			   PatchResult == EPatchCheckResult::PatchRequired);

		// Skip hotfix check in error states, but still preload data as if there was nothing wrong
		StartInitialPreload();
	}
}

void UUpdateManager::StartPlatformEnvironmentCheck()
{
	IOnlineSubsystem* OnlineSubConsole = IOnlineSubsystem::GetByPlatform();
	if (!bPlatformEnvironmentDetected &&
		OnlineSubConsole != nullptr &&
		OnlineSubConsole->GetIdentityInterface().IsValid())
	{
		SetUpdateState(EUpdateState::DetectingPlatformEnvironment);

		ULocalPlayer* LP = GetGameInstance()->GetFirstGamePlayer();
		if (LP != nullptr)
		{
			OnLoginConsoleCompleteHandle = OnlineSubConsole->GetIdentityInterface()->AddOnLoginCompleteDelegate_Handle(
				LP->GetControllerId(),
				FOnLoginCompleteDelegate::CreateUObject(this, &ThisClass::PlatformEnvironmentCheck_OnLoginConsoleComplete)
				);

			OnlineSubConsole->GetIdentityInterface()->Login(LP->GetControllerId(), FOnlineAccountCredentials());
		}
	}
	else
	{
		StartHotfixCheck();
	}
}

void UUpdateManager::PlatformEnvironmentCheck_OnLoginConsoleComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	if (bWasSuccessful)
	{
		bPlatformEnvironmentDetected = true;
		StartHotfixCheck();
	}
	else
	{
		UE_LOG(LogHotfixManager, Warning, TEXT("Failed to detect online environment for the platform"));
		CheckComplete(EUpdateCompletionStatus::UpdateFailure_NotLoggedIn);
	}
}

void UUpdateManager::StartHotfixCheck()
{
	if (bCheckHotfixAvailabilityOnly)
	{
		// Just check for the presence of a hotfix
		StartHotfixAvailabilityCheck();
	}
	else
	{
		SetUpdateState(EUpdateState::CheckingForHotfix);

		if (FLoadingScreenConfig::CheckForHotfixes())
		{
			UOnlineHotfixManager* HotfixManager = GetHotfixManager<UOnlineHotfixManager>();
			HotfixProgressDelegateHandle = HotfixManager->AddOnHotfixProgressDelegate_Handle(FOnHotfixProgressDelegate::CreateUObject(this, &ThisClass::OnHotfixProgress));
			HotfixProcessedFileDelegateHandle = HotfixManager->AddOnHotfixProcessedFileDelegate_Handle(FOnHotfixProcessedFileDelegate::CreateUObject(this, &ThisClass::OnHotfixProcessedFile));
			HotfixCompleteDelegateHandle = HotfixManager->AddOnHotfixCompleteDelegate_Handle(FOnHotfixCompleteDelegate::CreateUObject(this, &ThisClass::OnHotfixCheckComplete));

			HotfixManager->StartHotfixProcess();
		}
		else
		{
			OnHotfixCheckComplete(EHotfixResult::SuccessNoChange);
		}
	}
}

void UUpdateManager::OnHotfixProgress(uint32 NumDownloaded, uint32 TotalFiles, uint64 NumBytes, uint64 TotalBytes)
{
	UE_LOG(LogHotfixManager, VeryVerbose, TEXT("OnHotfixProgress %d/%d [%d/%d]"), NumDownloaded, TotalFiles, NumBytes, TotalBytes);
	OnUpdateHotfixProgress().Broadcast(NumDownloaded, TotalFiles, NumBytes, TotalBytes);
}

void UUpdateManager::OnHotfixProcessedFile(const FString& FriendlyName, const FString& CachedName)
{
	UE_LOG(LogHotfixManager, VeryVerbose, TEXT("OnHotfixProcessedFile %s"), *FriendlyName);
	OnUpdateHotfixProcessedFile().Broadcast(FriendlyName, CachedName);
}

void UUpdateManager::OnHotfixCheckComplete(EHotfixResult Result)
{
	UE_LOG(LogHotfixManager, Display, TEXT("OnHotfixCheckComplete %d"), (int32)Result);

	if (auto HotfixManager = GetHotfixManager<UOnlineHotfixManager>())
	{
		HotfixManager->ClearOnHotfixProgressDelegate_Handle(HotfixProgressDelegateHandle);
		HotfixManager->ClearOnHotfixProcessedFileDelegate_Handle(HotfixProcessedFileDelegateHandle);
		HotfixManager->ClearOnHotfixCompleteDelegate_Handle(HotfixCompleteDelegateHandle);
	}

	LastHotfixResult = Result;

	auto CompletionDelegate = [this]()
	{
		// Always preload data
		StartInitialPreload();
	};

	// Debug delay delegate firing
	DelayResponse(CompletionDelegate, HotfixCheckCompleteDelay);
}

void UUpdateManager::StartHotfixAvailabilityCheck()
{
	SetUpdateState(EUpdateState::CheckingForHotfix);

	if (FLoadingScreenConfig::CheckForHotfixes())
	{
		UOnlineHotfixManager* HotfixManager = GetHotfixManager<UOnlineHotfixManager>();

		FOnHotfixAvailableComplete CompletionDelegate;
		CompletionDelegate.BindUObject(this, &ThisClass::HotfixAvailabilityCheckComplete);
		HotfixManager->CheckAvailability(CompletionDelegate);
	}
	else
	{
		OnHotfixCheckComplete(EHotfixResult::SuccessNoChange);
	}
}

void UUpdateManager::HotfixAvailabilityCheckComplete(EHotfixResult Result)
{
	UE_LOG(LogHotfixManager, Display, TEXT("HotfixAvailabilityCheckComplete %d"), (int32)Result);

	auto CompletionDelegate = [this, Result]()
	{
		UE_LOG(LogHotfixManager, Display, TEXT("External HotfixAvailabilityCheckComplete %d"), (int32)Result);
		switch (Result)
		{
			case EHotfixResult::Success:
				CheckComplete(EUpdateCompletionStatus::UpdateSuccess);
				break;
			case EHotfixResult::SuccessNoChange:
				CheckComplete(EUpdateCompletionStatus::UpdateSuccess_NoChange);
				break;
			case EHotfixResult::Failed:
				CheckComplete(EUpdateCompletionStatus::UpdateFailure_HotfixCheck);
				break;
			default:
				ensure(0 && "No other result codes should reach here!");
				CheckComplete(EUpdateCompletionStatus::UpdateFailure_HotfixCheck);
				break;
		}
	};

	// Debug delay delegate firing
	DelayResponse(CompletionDelegate, HotfixAvailabilityCheckCompleteDelay);
}

void UUpdateManager::StartInitialPreload()
{
	SetUpdateState(EUpdateState::WaitingOnInitialLoad);

	// Start ticking
	FTicker& Ticker = FTicker::GetCoreTicker();
	FTickerDelegate TickDelegate = FTickerDelegate::CreateUObject(this, &ThisClass::Tick);
	ensure(!TickerHandle.IsValid());
	TickerHandle = Ticker.AddTicker(TickDelegate, 0.0f);

	LoadStartTime = FPlatformTime::Seconds();
	WorstNumFilesPendingLoadViewed = GetNumAsyncPackages();
}

void UUpdateManager::InitialPreloadComplete()
{
	SetUpdateState(EUpdateState::InitialLoadComplete);

	if (LastPatchCheckResult == EPatchCheckResult::PatchCheckFailure)
	{
		CheckComplete(EUpdateCompletionStatus::UpdateFailure_PatchCheck);
	}
	else if (LastPatchCheckResult == EPatchCheckResult::PatchRequired)
	{
		CheckComplete(EUpdateCompletionStatus::UpdateSuccess_NeedsPatch);
	}
	else
	{
		ensure(LastPatchCheckResult == EPatchCheckResult::NoPatchRequired);
		// Patch check success, check hotfix status
		switch (LastHotfixResult)
		{
			case EHotfixResult::Success:
				CheckComplete(EUpdateCompletionStatus::UpdateSuccess);
				break;
			case EHotfixResult::SuccessNoChange:
				CheckComplete(EUpdateCompletionStatus::UpdateSuccess_NoChange);
				break;
			case EHotfixResult::Failed:
				CheckComplete(EUpdateCompletionStatus::UpdateFailure_HotfixCheck);
				break;
			case EHotfixResult::SuccessNeedsRelaunch:
				CheckComplete(EUpdateCompletionStatus::UpdateSuccess_NeedsRelaunch);
				break;
			case EHotfixResult::SuccessNeedsReload:
				CheckComplete(EUpdateCompletionStatus::UpdateSuccess_NeedsReload);
				break;
		}
	}
}

void UUpdateManager::SetUpdateState(EUpdateState NewState)
{
	if (CurrentUpdateState != NewState)
	{
		UE_LOG(LogHotfixManager, Display, TEXT("Update State %s -> %s"), UpdateStateEnum ? *UpdateStateEnum->GetEnumName((int32)CurrentUpdateState) : TEXT("Invalid"), UpdateStateEnum ? *UpdateStateEnum->GetEnumName((int32)NewState) : TEXT("Invalid"));
		CurrentUpdateState = NewState;
		OnUpdateStatusChanged().Broadcast(NewState);
	}
}

bool UUpdateManager::Tick(float InDeltaTime)
{
	if (CurrentUpdateState == EUpdateState::WaitingOnInitialLoad)
	{
		WorstNumFilesPendingLoadViewed = FMath::Max(GetNumAsyncPackages(), WorstNumFilesPendingLoadViewed);
		//UE_LOG(LogInit, Log, TEXT("Load progress %f (%d %d)"), GetLoadProgress(), GetNumAsyncPackages(), WorstNumFilesPendingLoadViewed);

		if (!IsAsyncLoading())
		{
			const double LoadTime = FPlatformTime::Seconds() - LoadStartTime;
			UE_LOG(LogHotfixManager, Log, TEXT("Finished initial load/hotfix phase in %fs"), LoadTime);
			ACCUM_LOADTIME(TEXT("FinishedInitialLoadHotfix"), LoadTime);

			InitialPreloadComplete();

			TickerHandle.Reset();
			return false;
		}
	}

	return true;
}

float UUpdateManager::GetLoadProgress() const
{
	const int32 CurrentNumFiles = GetNumAsyncPackages();
	return (WorstNumFilesPendingLoadViewed > 0) ? FMath::Clamp((WorstNumFilesPendingLoadViewed - CurrentNumFiles) / (float)WorstNumFilesPendingLoadViewed, 0.0f, 1.0f) : 0.0f;
}

bool UUpdateManager::IsHotfixingEnabled() const
{
	if (GIsEditor)
	{
		return false;
	}

	return FLoadingScreenConfig::CheckForHotfixes();
}

bool UUpdateManager::IsBlockingForInitialLoadEnabled() const
{
	return FLoadingScreenConfig::ShouldBlockOnInitialLoad();
}

FTimerHandle UUpdateManager::DelayResponse(DelayCb&& Delegate, float Delay)
{
	FTimerHandle TimerHandle;

	UWorld* World = GetWorld();
	if (ensure(World != nullptr))
	{
		World->GetTimerManager().SetTimer(TimerHandle, MoveTemp(Delegate), Delay, false, -1.f);
	}

	return TimerHandle;
}

UWorld* UUpdateManager::GetWorld() const
{
	UGameInstance* GameInstance = GetTypedOuter<UGameInstance>();
	return GameInstance->GetWorld();
}

UGameInstance* UUpdateManager::GetGameInstance() const
{
	return GetTypedOuter<UGameInstance>();
}

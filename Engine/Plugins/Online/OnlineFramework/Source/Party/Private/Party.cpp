// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PartyPrivatePCH.h"
#include "Party.h"
#include "PartyGameState.h"
#include "Engine/GameInstance.h"

#include "Online.h"
#include "OnlineSubsystemUtils.h"

#define LOCTEXT_NAMESPACE "Parties"

UParty::UParty(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	bLeavingPersistentParty(false)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
	}
}

void UParty::Init()
{
	UGameInstance* GameInstance = GetGameInstance();
	check(GameInstance);
	GameInstance->OnNotifyPreClientTravel().AddUObject(this, &ThisClass::NotifyPreClientTravel);

	FCoreUObjectDelegates::PostLoadMap.AddUObject(this, &ThisClass::OnPostLoadMap);
}

void UParty::InitPIE()
{
	OnPostLoadMap();
}

void UParty::OnPostLoadMap()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		RegisterIdentityDelegates();
		RegisterPartyDelegates();
	}
}

void UParty::OnShutdown()
{
	for (const auto& PartyKeyValue : JoinedParties)
	{
		UPartyGameState* Party = PartyKeyValue.Value;
		if (Party)
		{
			Party->OnShutdown();
		}
	}

	JoinedParties.Empty();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		FCoreUObjectDelegates::PostLoadMap.RemoveAll(this);

		UnregisterIdentityDelegates();
		UnregisterPartyDelegates();
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		GameInstance->OnNotifyPreClientTravel().RemoveAll(this);
	}
}

void UParty::OnLogoutComplete(int32 LocalUserNum, bool bWasSuccessful)
{
	if (JoinedParties.Num())
	{
		UE_LOG(LogParty, Log, TEXT("Party cleanup on logout"));
		TArray<FOnlinePartyTypeId> PartiesToRemove;
		for (const auto& PartyKeyValue : JoinedParties)
		{
			PartiesToRemove.Add(PartyKeyValue.Key);
		}
		for (const auto& PartyKey : PartiesToRemove)
		{
			UPartyGameState** FoundParty = JoinedParties.Find(PartyKey);
			if (FoundParty)
			{
				UPartyGameState* Party = *FoundParty;
				if (Party && Party->GetPartyId().IsValid())
				{
					if (Party->GetPartyId().IsValid())
					{
						UE_LOG(LogParty, Log, TEXT("[%s] Removed"), *(Party->GetPartyId()->ToDebugString()));
					}
					else
					{
						UE_LOG(LogParty, Log, TEXT("[%s] Removed - Invalid party Id"));
					}
					Party->HandleRemovedFromParty(EMemberExitedReason::Left);
				}
				JoinedParties.Remove(PartyKey);
			}
		}

		ensure(JoinedParties.Num() == 0);
		JoinedParties.Empty();
	}
}

void UParty::OnLoginStatusChanged(int32 LocalUserNum, ELoginStatus::Type OldStatus, ELoginStatus::Type NewStatus, const FUniqueNetId& NewId)
{
	if (NewStatus == ELoginStatus::NotLoggedIn)
	{
		if (JoinedParties.Num())
		{
			UE_LOG(LogParty, Log, TEXT("Party cleanup on logout"));
			TArray<FOnlinePartyTypeId> PartiesToRemove;
			for (const auto& PartyKeyValue : JoinedParties)
			{
				PartiesToRemove.Add(PartyKeyValue.Key);
			}
			for (const auto& PartyKey : PartiesToRemove)
			{
				UPartyGameState** FoundParty = JoinedParties.Find(PartyKey);
				if (FoundParty)
				{
					UPartyGameState* Party = *FoundParty;
					if (Party)
					{
						TSharedPtr<const FOnlinePartyId> PartyId = Party->GetPartyId();
						FString PartyIdString = PartyId.IsValid() ? PartyId->ToDebugString() : TEXT("");
						UE_LOG(LogParty, Log, TEXT("[%s] Removed"), *PartyIdString);
						Party->HandleRemovedFromParty(EMemberExitedReason::Left);
					}
					JoinedParties.Remove(PartyKey);
				}
			}

			ensure(JoinedParties.Num() == 0);
			JoinedParties.Empty();
		}
	}

	ClearPendingPartyJoin();
}

void UParty::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	UParty* This = CastChecked<UParty>(InThis);
	check(This);

	TArray<UPartyGameState*> Parties;
	This->JoinedParties.GenerateValueArray(Parties);
	Collector.AddReferencedObjects(Parties);
}

void UParty::RegisterIdentityDelegates()
{
	UWorld* World = GetWorld();
	check(World);

	IOnlineIdentityPtr IdentityInt = Online::GetIdentityInterface(World);
	if (IdentityInt.IsValid())
	{
		// Unbind and then rebind
		UnregisterIdentityDelegates();

		FOnLogoutCompleteDelegate OnLogoutCompleteDelegate;
		OnLogoutCompleteDelegate.BindUObject(this, &ThisClass::OnLogoutComplete);

		FOnLoginStatusChangedDelegate OnLoginStatusChangedDelegate;
		OnLoginStatusChangedDelegate.BindUObject(this, &ThisClass::OnLoginStatusChanged);

		for (int32 LocalPlayerId = 0; LocalPlayerId < MAX_LOCAL_PLAYERS; LocalPlayerId++)
		{
			LogoutCompleteDelegateHandle[LocalPlayerId] = IdentityInt->AddOnLogoutCompleteDelegate_Handle(LocalPlayerId, OnLogoutCompleteDelegate);
			LogoutStatusChangedDelegateHandle[LocalPlayerId] = IdentityInt->AddOnLoginStatusChangedDelegate_Handle(LocalPlayerId, OnLoginStatusChangedDelegate);
		}
	}
}

void UParty::UnregisterIdentityDelegates()
{
	UWorld* World = GetWorld();
	IOnlineIdentityPtr IdentityInt = Online::GetIdentityInterface(World);
	if (IdentityInt.IsValid())
	{
		for (int32 LocalPlayerId = 0; LocalPlayerId < MAX_LOCAL_PLAYERS; LocalPlayerId++)
		{
			if (LogoutCompleteDelegateHandle[LocalPlayerId].IsValid())
			{
				IdentityInt->ClearOnLogoutCompleteDelegate_Handle(LocalPlayerId, LogoutCompleteDelegateHandle[LocalPlayerId]);
			}

			if (LogoutStatusChangedDelegateHandle[LocalPlayerId].IsValid())
			{
				IdentityInt->ClearOnLoginStatusChangedDelegate_Handle(LocalPlayerId, LogoutStatusChangedDelegateHandle[LocalPlayerId]);
			}
		}
	}
}

void UParty::RegisterPartyDelegates()
{
	UnregisterPartyDelegates();

	UWorld* World = GetWorld();
	check(World);

	IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
	if (PartyInt.IsValid())
	{
		PartyConfigChangedDelegateHandle = PartyInt->AddOnPartyConfigChangedDelegate_Handle(FOnPartyConfigChangedDelegate::CreateUObject(this, &ThisClass::PartyConfigChangedInternal));
		PartyMemberJoinedDelegateHandle = PartyInt->AddOnPartyMemberJoinedDelegate_Handle(FOnPartyMemberJoinedDelegate::CreateUObject(this, &ThisClass::PartyMemberJoinedInternal));
		PartyDataReceivedDelegateHandle = PartyInt->AddOnPartyDataReceivedDelegate_Handle(FOnPartyDataReceivedDelegate::CreateUObject(this, &ThisClass::PartyDataReceivedInternal));
		PartyMemberDataReceivedDelegateHandle = PartyInt->AddOnPartyMemberDataReceivedDelegate_Handle(FOnPartyMemberDataReceivedDelegate::CreateUObject(this, &ThisClass::PartyMemberDataReceivedInternal));
		PartyJoinRequestReceivedDelegateHandle = PartyInt->AddOnPartyJoinRequestReceivedDelegate_Handle(FOnPartyJoinRequestReceivedDelegate::CreateUObject(this, &ThisClass::PartyJoinRequestReceivedInternal));
		PartyMemberChangedDelegateHandle = PartyInt->AddOnPartyMemberChangedDelegate_Handle(FOnPartyMemberChangedDelegate::CreateUObject(this, &ThisClass::PartyMemberChangedInternal));
		PartyMemberExitedDelegateHandle = PartyInt->AddOnPartyMemberExitedDelegate_Handle(FOnPartyMemberExitedDelegate::CreateUObject(this, &ThisClass::PartyMemberExitedInternal));
		PartyPromotionLockoutChangedDelegateHandle = PartyInt->AddOnPartyPromotionLockoutChangedDelegate_Handle(FOnPartyPromotionLockoutChangedDelegate::CreateUObject(this, &ThisClass::PartyPromotionLockoutStateChangedInternal));
		PartyExitedDelegateHandle = PartyInt->AddOnPartyExitedDelegate_Handle(FOnPartyExitedDelegate::CreateUObject(this, &ThisClass::PartyExitedInternal));
	}
}

void UParty::UnregisterPartyDelegates()
{
	UWorld* World = GetWorld();
	IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
	if (PartyInt.IsValid())
	{
		PartyInt->ClearOnPartyConfigChangedDelegate_Handle(PartyConfigChangedDelegateHandle);
		PartyInt->ClearOnPartyMemberJoinedDelegate_Handle(PartyMemberJoinedDelegateHandle);
		PartyInt->ClearOnPartyDataReceivedDelegate_Handle(PartyDataReceivedDelegateHandle);
		PartyInt->ClearOnPartyMemberDataReceivedDelegate_Handle(PartyMemberDataReceivedDelegateHandle);
		PartyInt->ClearOnPartyJoinRequestReceivedDelegate_Handle(PartyJoinRequestReceivedDelegateHandle);
		PartyInt->ClearOnPartyMemberChangedDelegate_Handle(PartyMemberChangedDelegateHandle);
		PartyInt->ClearOnPartyMemberExitedDelegate_Handle(PartyMemberExitedDelegateHandle);
		PartyInt->ClearOnPartyPromotionLockoutChangedDelegate_Handle(PartyPromotionLockoutChangedDelegateHandle);
		PartyInt->ClearOnPartyExitedDelegate_Handle(PartyExitedDelegateHandle);
	}
}

void UParty::NotifyPreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{
	for (auto& JoinedParty : JoinedParties)
	{
		UPartyGameState* PartyState = JoinedParty.Value;
		PartyState->PreClientTravel();
	}
}

bool UParty::HasPendingPartyJoin()
{
	return PendingPartyJoin.IsValid();
}

UPartyGameState* UParty::GetParty(const FOnlinePartyId& InPartyId) const
{
	UPartyGameState* ResultPtr = nullptr;

	for (auto& JoinedParty : JoinedParties)
	{
		TSharedPtr<const FOnlinePartyId> PartyId = JoinedParty.Value->GetPartyId();
		if (*PartyId == InPartyId)
		{
			ResultPtr = JoinedParty.Value;
			break;
		}
	}

	return ResultPtr;
}

UPartyGameState* UParty::GetParty(const FOnlinePartyTypeId InPartyTypeId) const
{
	UPartyGameState* ResultPtr = nullptr;

	UPartyGameState* const * PartyPtr = JoinedParties.Find(InPartyTypeId);
	ResultPtr = PartyPtr ? *PartyPtr : nullptr;
	
	return ResultPtr;
}

UPartyGameState* UParty::GetPersistentParty() const
{
	UPartyGameState* ResultPtr = nullptr;

	UPartyGameState* const * PartyPtr = JoinedParties.Find(IOnlinePartySystem::GetPrimaryPartyTypeId());
	ResultPtr = PartyPtr ? *PartyPtr : nullptr;

	return ResultPtr;
}

void UParty::PartyConfigChangedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const TSharedRef<FPartyConfiguration>& InPartyConfig)
{
	UE_LOG(LogParty, Log, TEXT("[%s] Party config changed"), *InPartyId.ToString());

	UPartyGameState* PartyState = GetParty(InPartyId);
	if (PartyState)
	{
		PartyState->HandlePartyConfigChanged(InPartyConfig);
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("[%s]: Missing party state during config change"), *InPartyId.ToString());
	}
}

void UParty::PartyMemberJoinedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const FUniqueNetId& InMemberId)
{
	UE_LOG(LogParty, Log, TEXT("[%s] Player %s joined"), *InPartyId.ToString(), *InMemberId.ToString());

	UPartyGameState* PartyState = GetParty(InPartyId);
	if (PartyState)
	{
		PartyState->HandlePartyMemberJoined(InMemberId);
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("[%s]: Missing party state player: %s"), *InPartyId.ToString(), *InMemberId.ToString());
	}
}

void UParty::PartyDataReceivedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const TSharedRef<FOnlinePartyData>& InPartyData)
{
	UE_LOG(LogParty, Log, TEXT("[%s] party data received"), *InPartyId.ToString());

	UPartyGameState* PartyState = GetParty(InPartyId);
	if (PartyState)
	{
		PartyState->HandlePartyDataReceived(InPartyData);
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("[%s]: Missing party state to apply data."), *InPartyId.ToString());
	}
}

void UParty::PartyMemberDataReceivedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const FUniqueNetId& InMemberId, const TSharedRef<FOnlinePartyData>& InPartyMemberData)
{
	UE_LOG(LogParty, Log, TEXT("[%s] Player %s data received"), *InPartyId.ToString(), *InMemberId.ToString());

	UPartyGameState* PartyState = GetParty(InPartyId);
	if (PartyState)
	{
		PartyState->HandlePartyMemberDataReceived(InMemberId, InPartyMemberData);
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("[%s]: Missing party state to apply data for player %s"), *InPartyId.ToString(), *InMemberId.ToString());
	}
}

void UParty::PartyJoinRequestReceivedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const FUniqueNetId& SenderId)
{
	UPartyGameState* PartyState = GetParty(InPartyId);
	if (PartyState)
	{
		PartyState->HandlePartyJoinRequestReceived(InLocalUserId, SenderId);
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("[%s]: Missing party state to process join request."), *InPartyId.ToString());
	}
}

void UParty::PartyMemberChangedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const FUniqueNetId& InMemberId, const EMemberChangedReason InReason)
{
	if (InLocalUserId == InMemberId)
	{
		UE_LOG(LogParty, Log, TEXT("[%s] [%s] local member %s"), *InPartyId.ToString(), *InMemberId.ToString(), ToString(InReason));
	}
	else
	{
		UE_LOG(LogParty, Log, TEXT("[%s] [%s] remote member %s"), *InPartyId.ToString(), *InMemberId.ToString(), ToString(InReason));
	}

	UPartyGameState* PartyState = GetParty(InPartyId);
	if (!PartyState)
	{
		UE_LOG(LogParty, Warning, TEXT("[%s]: Missing party state during member change."), *InPartyId.ToString());
	}

	if (InReason == EMemberChangedReason::Disconnected)
	{
		// basically cleanup because they get kicked for now as long as party config says
		// PartyConfig.bShouldRemoveOnDisconnection = true
		// handled by PartyMemberLeft 
	}
	else if (InReason == EMemberChangedReason::Rejoined)
	{
		// can't happen as long as 
		// PartyConfig.bShouldRemoveOnDisconnection = true
	}
	else if (InReason == EMemberChangedReason::Promoted)
	{
		if (PartyState)
		{
			PartyState->HandlePartyMemberPromoted(InMemberId);
		}

		if (PersistentPartyId.IsValid() &&
			(InPartyId == *PersistentPartyId))
		{
			FUniqueNetIdRepl NewPartyLeader;
			NewPartyLeader.SetUniqueNetId(InMemberId.AsShared());
			UpdatePersistentPartyLeader(NewPartyLeader);
		}
	}
}

void UParty::PartyMemberExitedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const FUniqueNetId& InMemberId, const EMemberExitedReason InReason)
{
	if (InLocalUserId == InMemberId)
	{
		UE_LOG(LogParty, Log, TEXT("[%s] [%s] local member removed. Reason: %s"), *InPartyId.ToString(), *InMemberId.ToString(), ToString(InReason));
	}
	else
	{
		UE_LOG(LogParty, Log, TEXT("[%s] [%s] remote member exited. Reason: %s"), *InPartyId.ToString(), *InMemberId.ToString(), ToString(InReason));
	}

	if (InLocalUserId == InMemberId)
	{
		if (InReason == EMemberExitedReason::Left)
		{
			// EMemberExitedReason::Left -> local player chose to leave, handled by leave completion delegate
		}
		else
		{
			UPartyGameState* PartyState = GetParty(InPartyId);

			// EMemberExitedReason::Removed -> removed from other reasons beside kick (can't happen?)
			// EMemberExitedReason::Disbanded -> leader kicked everyone
			// EMemberExitedReason::Kicked -> party/leader kicked you
			// EMemberExitedReason::Unknown -> bad but try to cleanup anyway
			if (PartyState)
			{
				FOnlinePartyTypeId PartyTypeId = PartyState->GetPartyTypeId();
				PartyState->HandleRemovedFromParty(InReason);
				JoinedParties.Remove(PartyTypeId);
			}
			else
			{
				UE_LOG(LogParty, Warning, TEXT("[%s]: Missing party state during local player exit."), *InPartyId.ToString());
			}
		}

		// If the removal was the persistent party, make sure we are in a good state
		if (PersistentPartyId.IsValid() &&
			(InPartyId == *PersistentPartyId))
		{
			RestorePersistentPartyState();
		}
	}
	else
	{
		UPartyGameState* PartyState = GetParty(InPartyId);
		if (!PartyState)
		{
			UE_LOG(LogParty, Warning, TEXT("[%s]: Missing party state during remote player exit."), *InPartyId.ToString());
		}

		if (PartyState)
		{
			// EMemberExitedReason::Left -> player chose to leave
			// EMemberExitedReason::Removed -> player was removed forcibly (timeout from disconnect, etc)
			// EMemberExitedReason::Disbanded -> leader kicked everyone (only leader should get this)
			// EMemberExitedReason::Kicked -> party/leader kicked this player
			PartyState->HandlePartyMemberLeft(InMemberId, InReason);
		}
	}
}

void UParty::PartyPromotionLockoutStateChangedInternal(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId, const bool bLockoutState)
{
	UE_LOG(LogParty, Log, TEXT("[%s] party lockout state changed to %s"), *InPartyId.ToString(), bLockoutState ? TEXT("true") : TEXT("false"));

	UPartyGameState* PartyState = GetParty(InPartyId);
	if (PartyState)
	{
		PartyState->HandleLockoutPromotionStateChange(bLockoutState);
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("[%s]: Missing party state during lockout call"), *InPartyId.ToString());
	}
}

void UParty::PartyExitedInternal(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId)
{
	UE_LOG(LogParty, Log, TEXT("[%s] exited party %s"), *InPartyId.ToString(), *(InPartyId.ToDebugString()));

	UPartyGameState* PartyState = GetParty(InPartyId);
	if (PartyState != nullptr && (PartyState->OssParty->PartyTypeId == IOnlinePartySystem::GetPrimaryPartyTypeId()))
	{
		TSharedRef<FPartyConfiguration> Config = PartyState->OssParty->Config;
		PartyState->HandleLeavingParty();
		PartyState->HandleRemovedFromParty(EMemberExitedReason::Left);
		JoinedParties.Remove(IOnlinePartySystem::GetPrimaryPartyTypeId());

		UPartyDelegates::FOnCreateUPartyComplete CompletionDelegate;
		CompletionDelegate.BindUObject(this, &ThisClass::OnPersistentPartyExitedInternalCompleted);
		CreatePartyInternal(LocalUserId, IOnlinePartySystem::GetPrimaryPartyTypeId(), *Config, CompletionDelegate);
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("[%s]: Missing party state during exit"), *InPartyId.ToString());
	}
}

void UParty::OnPersistentPartyExitedInternalCompleted(const FUniqueNetId& LocalUserId, const ECreatePartyCompletionResult Result)
{
	if (Result == ECreatePartyCompletionResult::Succeeded)
	{
		OnCreatePesistentPartyCompletedCommon(LocalUserId);

		UPartyGameState** Party = JoinedParties.Find(IOnlinePartySystem::GetPrimaryPartyTypeId());

		if (Party != nullptr)
		{
			PartyResetForFrontendDelegate.Broadcast(*Party);
		}
	}

	if (Result != ECreatePartyCompletionResult::Succeeded)
	{
		UE_LOG(LogParty, Warning, TEXT("Error when attempting to recreate persistent party on reconnection error=%s"), ToString(Result));
	}
}

void UParty::CreatePartyInternal(const FUniqueNetId& InUserId, const FOnlinePartyTypeId InPartyTypeId, const FPartyConfiguration& InPartyConfig, const UPartyDelegates::FOnCreateUPartyComplete& InCompletionDelegate)
{
	ECreatePartyCompletionResult Result = ECreatePartyCompletionResult::UnknownClientFailure;
	FString ErrorMsg;

	UWorld* World = GetWorld();
	IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
	if (PartyInt.IsValid())
	{
		FOnCreatePartyComplete CompletionDelegate;
		CompletionDelegate.BindUObject(this, &ThisClass::OnCreatePartyInternalComplete, InPartyTypeId, InCompletionDelegate);
		PartyInt->CreateParty(InUserId, InPartyTypeId, InPartyConfig, CompletionDelegate);
		Result = ECreatePartyCompletionResult::Succeeded;
	}
	else
	{
		Result = ECreatePartyCompletionResult::UnknownClientFailure;
		ErrorMsg = FString::Printf(TEXT("No party interface during JoinParty()"));
	}

	if (Result != ECreatePartyCompletionResult::Succeeded)
	{
		UE_LOG(LogParty, Warning, TEXT("%s"), *ErrorMsg);
		InCompletionDelegate.ExecuteIfBound(InUserId, Result);
	}
}

void UParty::OnCreatePartyInternalComplete(const FUniqueNetId& LocalUserId, const TSharedPtr<const FOnlinePartyId>& InPartyId, const ECreatePartyCompletionResult Result, const FOnlinePartyTypeId InPartyTypeId, UPartyDelegates::FOnCreateUPartyComplete InCompletionDelegate)
{
	const FString PartyIdDebugString = InPartyId.IsValid() ? InPartyId->ToDebugString() : TEXT("Invalid");
	UE_LOG(LogParty, Display, TEXT("OnCreatePartyInternalComplete() %s %s"), *PartyIdDebugString, ToString(Result));

	ECreatePartyCompletionResult LocalResult = Result;
	if (Result == ECreatePartyCompletionResult::Succeeded)
	{
		UWorld* World = GetWorld();
		IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
		if (PartyInt.IsValid())
		{
			TSharedPtr<const FOnlineParty> Party = PartyInt->GetParty(LocalUserId, InPartyTypeId);
			if (ensure(Party.IsValid()))
			{
				TSubclassOf<UPartyGameState>* PartyGameStateClass = PartyClasses.Find(InPartyTypeId);
				if (PartyGameStateClass != nullptr)
				{
					UPartyGameState* NewParty = NewObject<UPartyGameState>(this, *PartyGameStateClass);

					// Add right away so future delegate broadcasts have this available
					JoinedParties.Add(InPartyTypeId, NewParty);

					// Initialize and trigger delegates
					NewParty->InitFromCreate(LocalUserId, Party);

					LocalResult = ECreatePartyCompletionResult::Succeeded;
				}
				else
				{
					LocalResult = ECreatePartyCompletionResult::UnknownClientFailure;
				}
			}
			else
			{
				LocalResult = ECreatePartyCompletionResult::UnknownClientFailure;
			}
		}
		else
		{
			LocalResult = ECreatePartyCompletionResult::UnknownClientFailure;
		}
	}

	if (LocalResult != ECreatePartyCompletionResult::Succeeded)
	{
		UE_LOG(LogParty, Warning, TEXT("Error when creating party %s error=%s"), *PartyIdDebugString, ToString(LocalResult));
	}

	InCompletionDelegate.ExecuteIfBound(LocalUserId, LocalResult);
}

void UParty::JoinPartyInternal(const FUniqueNetId& InUserId, const FPartyDetails& InPartyDetails, const UPartyDelegates::FOnJoinUPartyComplete& InCompletionDelegate)
{
	EJoinPartyCompletionResult Result = EJoinPartyCompletionResult::UnknownClientFailure;
	FString ErrorMsg;

	IOnlinePartyPtr PartyInt = Online::GetPartyInterface(GetWorld());
	if (PartyInt.IsValid())
	{
		if (InPartyDetails.IsValid())
		{
			const FOnlinePartyId& PartyId = *InPartyDetails.GetPartyId();
			// High level party data check
			UPartyGameState* PartyState = GetParty(PartyId);
			// Interface level party data check should not be out of sync
			TSharedPtr<const FOnlineParty> Party = PartyInt->GetParty(InUserId, PartyId);
			if (!PartyState)
			{
				if (!Party.IsValid())
				{
					FOnJoinPartyComplete CompletionDelegate;
					CompletionDelegate.BindUObject(this, &ThisClass::OnJoinPartyInternalComplete, InPartyDetails.GetPartyTypeId(), InCompletionDelegate);
					PartyInt->JoinParty(InUserId, *(InPartyDetails.PartyJoinInfo), CompletionDelegate);
					Result = EJoinPartyCompletionResult::Succeeded;
				}
				else
				{
					Result = EJoinPartyCompletionResult::AlreadyJoiningParty;
					ErrorMsg = FString::Printf(TEXT("Already joining party %s, not joining again."), *InPartyDetails.GetPartyId()->ToString());
				}
			}
			else
			{
				Result = EJoinPartyCompletionResult::AlreadyInParty;
				ErrorMsg = FString::Printf(TEXT("Already in party %s, not joining again."), *InPartyDetails.GetPartyId()->ToString());
			}
		}
		else
		{
			Result = EJoinPartyCompletionResult::JoinInfoInvalid;
			ErrorMsg = FString::Printf(TEXT("Invalid party details, cannot join. Details: %s"), *InPartyDetails.ToString());
		}
	}
	else
	{
		Result = EJoinPartyCompletionResult::UnknownClientFailure;
		ErrorMsg = FString::Printf(TEXT("No party interface during JoinParty()"));
	}

	if (Result != EJoinPartyCompletionResult::Succeeded)
	{
		UE_LOG(LogParty, Warning, TEXT("%s"), *ErrorMsg);
		InCompletionDelegate.ExecuteIfBound(InUserId, Result, 0);
	}
}

void UParty::OnJoinPartyInternalComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId, const EJoinPartyCompletionResult Result, int32 DeniedResultCode, const FOnlinePartyTypeId InPartyTypeId, UPartyDelegates::FOnJoinUPartyComplete CompletionDelegate)
{
	const FString PartyIdDebugString = InPartyId.ToDebugString();
	UE_LOG(LogParty, Display, TEXT("OnJoinPartyInternalComplete() %s %s."), *PartyIdDebugString, ToString(Result));

	EJoinPartyCompletionResult LocalResult = Result;
	if (Result == EJoinPartyCompletionResult::Succeeded)
	{
		UWorld* World = GetWorld();
		IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
		if (PartyInt.IsValid())
		{
			TSharedPtr<const FOnlineParty> Party = PartyInt->GetParty(LocalUserId, InPartyTypeId);
			if (Party.IsValid())
			{
				TSubclassOf<UPartyGameState>* PartyGameStateClass = PartyClasses.Find(InPartyTypeId);
				if (PartyGameStateClass != nullptr)
				{
					UPartyGameState* NewParty = NewObject<UPartyGameState>(this, *PartyGameStateClass);

					// Add right away so future delegate broadcasts have this available
					JoinedParties.Add(InPartyTypeId, NewParty);

					// Initialize and trigger delegates
					NewParty->InitFromJoin(LocalUserId, Party);
				}
				else
				{
					LocalResult = EJoinPartyCompletionResult::AlreadyInPartyOfSpecifiedType;
				}
			}
			else
			{
				LocalResult = EJoinPartyCompletionResult::UnknownClientFailure;
			}
		}
		else
		{
			LocalResult = EJoinPartyCompletionResult::UnknownClientFailure;
		}
	}

	if (LocalResult != EJoinPartyCompletionResult::Succeeded)
	{
		UE_LOG(LogParty, Warning, TEXT("Error when joining party %s error=%s"), *PartyIdDebugString, ToString(LocalResult));
	}
	
	int32 OutDeniedResultCode = (Result == EJoinPartyCompletionResult::NotApproved) ? DeniedResultCode : 0;
	CompletionDelegate.ExecuteIfBound(LocalUserId, LocalResult, OutDeniedResultCode);
}

void UParty::LeavePartyInternal(const FUniqueNetId& InUserId, const FOnlinePartyTypeId InPartyTypeId, const UPartyDelegates::FOnLeaveUPartyComplete& InCompletionDelegate)
{
	ELeavePartyCompletionResult Result = ELeavePartyCompletionResult::UnknownClientFailure;
	FString ErrorMsg;

	IOnlinePartyPtr PartyInt = Online::GetPartyInterface(GetWorld());
	if (PartyInt.IsValid())
	{
		// Get the party by type here (don't rely on interface structures here because they can be missing during disconnects)
		UPartyGameState* PartyState = GetParty(InPartyTypeId);
		if (PartyState)
		{
			PartyState->HandleLeavingParty();

			TSharedPtr<const FOnlinePartyId> PartyId = PartyState->GetPartyId();
			if (ensure(PartyId.IsValid()))
			{
				FOnLeavePartyComplete CompletionDelegate;
				CompletionDelegate.BindUObject(this, &ThisClass::OnLeavePartyInternalComplete, InPartyTypeId, InCompletionDelegate);
				PartyInt->LeaveParty(InUserId, *PartyId, CompletionDelegate);
				Result = ELeavePartyCompletionResult::Succeeded;
			}
			else
			{
				// Manual cleanup here because we can't call the above delegate
				PartyState->HandleRemovedFromParty(EMemberExitedReason::Left);
				JoinedParties.Remove(InPartyTypeId);
			}
		}
		else
		{
			Result = ELeavePartyCompletionResult::UnknownParty;
			ErrorMsg = FString::Printf(TEXT("Party not found in LeaveParty()"));
		}
	}
	else
	{
		Result = ELeavePartyCompletionResult::UnknownClientFailure;
		ErrorMsg = FString::Printf(TEXT("No party interface during LeaveParty()"));
	}

	if (Result != ELeavePartyCompletionResult::Succeeded)
	{
		UE_LOG(LogParty, Warning, TEXT("%s"), *ErrorMsg);
		InCompletionDelegate.ExecuteIfBound(InUserId, Result);
	}
}

void UParty::OnLeavePartyInternalComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId, const ELeavePartyCompletionResult Result, const FOnlinePartyTypeId InPartyTypeId, UPartyDelegates::FOnLeaveUPartyComplete CompletionDelegate)
{
	const FString PartyIdDebugString = InPartyId.ToDebugString();
	UE_LOG(LogParty, Display, TEXT("OnLeavePartyComplete() %s %s."), *PartyIdDebugString, ToString(Result));

	UPartyGameState* PartyState = GetParty(InPartyTypeId);
	if (PartyState)
	{
		PartyState->HandleRemovedFromParty(EMemberExitedReason::Left);
		JoinedParties.Remove(InPartyTypeId);
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("OnLeavePartyComplete: Missing party state %s"), *PartyIdDebugString);
	}

	CompletionDelegate.ExecuteIfBound(LocalUserId, Result);
}

void UParty::CreateParty(const FUniqueNetId& InUserId, const FOnlinePartyTypeId InPartyTypeId, const FPartyConfiguration& InPartyConfig, const UPartyDelegates::FOnCreateUPartyComplete& InCompletionDelegate)
{
	UPartyDelegates::FOnCreateUPartyComplete CompletionDelegate;
	CompletionDelegate.BindUObject(this, &ThisClass::OnCreatePartyComplete, InPartyTypeId, InCompletionDelegate);
	CreatePartyInternal(InUserId, InPartyTypeId, InPartyConfig, CompletionDelegate);
}

void UParty::OnCreatePartyComplete(const FUniqueNetId& LocalUserId, const ECreatePartyCompletionResult Result, const FOnlinePartyTypeId InPartyTypeId, UPartyDelegates::FOnCreateUPartyComplete InCompletionDelegate)
{
	UE_LOG(LogParty, Display, TEXT("OnCreatePartyComplete() type(0x%08x) %s"), InPartyTypeId.GetValue(), ToString(Result));

	InCompletionDelegate.ExecuteIfBound(LocalUserId, Result);
}

void UParty::JoinParty(const FUniqueNetId& InUserId, const FPartyDetails& InPartyDetails, const UPartyDelegates::FOnJoinUPartyComplete& InCompletionDelegate)
{
	UPartyDelegates::FOnJoinUPartyComplete CompletionDelegate;
	CompletionDelegate.BindUObject(this, &ThisClass::OnJoinPartyComplete, InPartyDetails.GetPartyTypeId(), InCompletionDelegate);
	JoinPartyInternal(InUserId, InPartyDetails, CompletionDelegate);
}

void UParty::OnJoinPartyComplete(const FUniqueNetId& LocalUserId, EJoinPartyCompletionResult Result, int32 DeniedResultCode, const FOnlinePartyTypeId InPartyTypeId, UPartyDelegates::FOnJoinUPartyComplete InCompletionDelegate)
{
	UE_LOG(LogParty, Display, TEXT("OnJoinPartyComplete() type(0x%08x) %s"), InPartyTypeId.GetValue(), ToString(Result));

	int32 OutDeniedResultCode = (Result == EJoinPartyCompletionResult::NotApproved) ? DeniedResultCode : 0;
	InCompletionDelegate.ExecuteIfBound(LocalUserId, Result, OutDeniedResultCode);
}

void UParty::LeaveParty(const FUniqueNetId& InUserId, const FOnlinePartyTypeId InPartyTypeId, const UPartyDelegates::FOnLeaveUPartyComplete& InCompletionDelegate)
{
	UPartyDelegates::FOnLeaveUPartyComplete CompletionDelegate;
	CompletionDelegate.BindUObject(this, &ThisClass::OnLeavePartyComplete, InPartyTypeId, InCompletionDelegate);
	LeavePartyInternal(InUserId, InPartyTypeId, CompletionDelegate);
}

void UParty::OnLeavePartyComplete(const FUniqueNetId& LocalUserId, const ELeavePartyCompletionResult Result, const FOnlinePartyTypeId InPartyTypeId, UPartyDelegates::FOnLeaveUPartyComplete InCompletionDelegate)
{
	UE_LOG(LogParty, Display, TEXT("OnLeavePartyComplete() type(0x%08x) %s"), InPartyTypeId.GetValue(), ToString(Result));

	InCompletionDelegate.ExecuteIfBound(LocalUserId, Result);
}

void UParty::GetDefaultPersistentPartySettings(EPartyType& PartyType, bool& bLeaderFriendsOnly, bool& bLeaderInvitesOnly, bool& bAllowInvites)
{
	PartyType = EPartyType::Public;
	bLeaderInvitesOnly = false;
	bLeaderFriendsOnly = false;
	bAllowInvites = true;
}

void UParty::GetPersistentPartyConfiguration(FPartyConfiguration& PartyConfig)
{
	EPartyType PartyType = EPartyType::Public;
	bool bLeaderInvitesOnly = false;
	bool bLeaderFriendsOnly = false;
	bool bAllowInvites = true;
	GetDefaultPersistentPartySettings(PartyType, bLeaderFriendsOnly, bLeaderInvitesOnly, bAllowInvites);

	bool bIsPrivate = (PartyType == EPartyType::Private);

	PartySystemPermissions::EPresencePermissions PresencePermissions;
	if (bLeaderFriendsOnly)
	{
		if (bIsPrivate)
		{
			PresencePermissions = PartySystemPermissions::FriendsInviteOnly;
		}
		else
		{
			PresencePermissions = PartySystemPermissions::FriendsOnly;
		}
	}
	else
	{
		if (bIsPrivate)
		{
			PresencePermissions = PartySystemPermissions::PublicInviteOnly;
		}
		else
		{
			PresencePermissions = PartySystemPermissions::Public;
		}
	}

	PartyConfig.JoinRequestAction = EJoinRequestAction::Manual;
	PartyConfig.bIsAcceptingMembers = bIsPrivate ? false : true;
	PartyConfig.bShouldRemoveOnDisconnection = true;
	PartyConfig.PresencePermissions = PresencePermissions;
	PartyConfig.InvitePermissions = bAllowInvites ? (bLeaderInvitesOnly ? PartySystemPermissions::EInvitePermissions::Leader : PartySystemPermissions::EInvitePermissions::Anyone) : PartySystemPermissions::EInvitePermissions::Noone;

	PartyConfig.MaxMembers = DefaultMaxPartySize;
}

void UParty::CreatePersistentParty(const FUniqueNetId& InUserId, const UPartyDelegates::FOnCreateUPartyComplete& InCompletionDelegate)
{
	if (PersistentPartyId.IsValid())
	{
		UE_LOG(LogParty, Warning, TEXT("Existing persistent party %s found when creating a new one."), *PersistentPartyId->ToString());
	}

	PersistentPartyId = nullptr;

	FPartyConfiguration PartyConfig;
	GetPersistentPartyConfiguration(PartyConfig);

	UPartyDelegates::FOnCreateUPartyComplete CompletionDelegate;
	CompletionDelegate.BindUObject(this, &ThisClass::OnCreatePersistentPartyComplete, InCompletionDelegate);
	CreatePartyInternal(InUserId, IOnlinePartySystem::GetPrimaryPartyTypeId(), PartyConfig, CompletionDelegate);
}

void UParty::OnCreatePersistentPartyComplete(const FUniqueNetId& LocalUserId, const ECreatePartyCompletionResult Result, UPartyDelegates::FOnCreateUPartyComplete CompletionDelegate)
{
	UE_LOG(LogParty, Display, TEXT("OnCreatePersistentPartyComplete() %s"), ToString(Result));

	if (Result == ECreatePartyCompletionResult::Succeeded)
	{
		OnCreatePesistentPartyCompletedCommon(LocalUserId);
	}

	CompletionDelegate.ExecuteIfBound(LocalUserId, Result);
}

void UParty::OnCreatePesistentPartyCompletedCommon(const FUniqueNetId& LocalUserId)
{
	UWorld* World = GetWorld();
	check(World);

	IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
	if (ensure(PartyInt.IsValid()))
	{
		TSharedPtr<const FOnlineParty> Party = PartyInt->GetParty(LocalUserId, IOnlinePartySystem::GetPrimaryPartyTypeId());
		if (ensure(Party.IsValid()))
		{
			PersistentPartyId = Party->PartyId;
		}
	}

	ensure(PersistentPartyId.IsValid());
	UPartyGameState* PersistentParty = GetPersistentParty();
	ensure(PersistentParty != nullptr);

	EPartyType PartyType = EPartyType::Public;
	bool bLeaderInvitesOnly = false;
	bool bLeaderFriendsOnly = false;
	bool bAllowInvites = true;
	GetDefaultPersistentPartySettings(PartyType, bLeaderFriendsOnly, bLeaderInvitesOnly, bAllowInvites);

	PersistentParty->SetPartyType(PartyType, bLeaderFriendsOnly, bLeaderInvitesOnly);
	PersistentParty->SetInvitesDisabled(!bAllowInvites);

	TSharedPtr<const FUniqueNetId> PartyLeaderPtr = PersistentParty->GetPartyLeader();
	ensure(PartyLeaderPtr.IsValid());

	FUniqueNetIdRepl PartyLeader(PartyLeaderPtr);
	UpdatePersistentPartyLeader(PartyLeader);
}

void UParty::JoinPersistentParty(const FUniqueNetId& InUserId, const FPartyDetails& InPartyDetails, const UPartyDelegates::FOnJoinUPartyComplete& InCompletionDelegate)
{
	if (PersistentPartyId.IsValid())
	{
		UE_LOG(LogParty, Warning, TEXT("Existing persistent party %s found when joining a new one."), *PersistentPartyId->ToString());
	}

	PersistentPartyId = nullptr;

	UPartyDelegates::FOnJoinUPartyComplete CompletionDelegate;
	CompletionDelegate.BindUObject(this, &ThisClass::OnJoinPersistentPartyComplete, InCompletionDelegate);
	JoinPartyInternal(InUserId, InPartyDetails, CompletionDelegate);
}

void UParty::OnJoinPersistentPartyComplete(const FUniqueNetId& LocalUserId, EJoinPartyCompletionResult Result, int32 DeniedResultCode, UPartyDelegates::FOnJoinUPartyComplete CompletionDelegate)
{
	UWorld* World = GetWorld();
	IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
	if (PartyInt.IsValid())
	{
		if (Result == EJoinPartyCompletionResult::Succeeded)
		{
			TSharedPtr<const FOnlineParty> Party = PartyInt->GetParty(LocalUserId, IOnlinePartySystem::GetPrimaryPartyTypeId());
			if (ensure(Party.IsValid()))
			{
				PersistentPartyId = Party->PartyId;
			}
		}
	}

	FString PartyIdDebugString = PersistentPartyId.IsValid() ? PersistentPartyId->ToDebugString() : TEXT("Invalid");
	UE_LOG(LogParty, Display, TEXT("OnJoinPersistentPartyComplete() %s %s"), *PartyIdDebugString, ToString(Result));

	int32 OutDeniedResultCode = (Result == EJoinPartyCompletionResult::NotApproved) ? DeniedResultCode : 0;
	CompletionDelegate.ExecuteIfBound(LocalUserId, Result, OutDeniedResultCode);

	if (Result == EJoinPartyCompletionResult::Succeeded)
	{
		ensure(PersistentPartyId.IsValid());
		UPartyGameState* PersistentParty = GetPersistentParty();
		if (PersistentParty)
		{
			TSharedPtr<const FUniqueNetId> PartyLeaderPtr = PersistentParty->GetPartyLeader();
			if (PartyLeaderPtr.IsValid())
			{
				FUniqueNetIdRepl PartyLeader(PartyLeaderPtr);
				UpdatePersistentPartyLeader(PartyLeader);
			}
			else
			{
				UE_LOG(LogParty, Warning, TEXT("OnJoinPersistentPartyComplete [%s]: Failed to update party leader"), *PersistentPartyId->ToString());
			}
		}
		else
		{
			UE_LOG(LogParty, Warning, TEXT("OnJoinPersistentPartyComplete [%s]: Failed to find party state object"), PersistentPartyId.IsValid() ? *PersistentPartyId->ToString() : TEXT("INVALID"));
		}
	}
	else
	{
		if (Result != EJoinPartyCompletionResult::AlreadyJoiningParty)
		{
			if (World)
			{
				// Try to get back to a good state
				FTimerDelegate HandleJoinPartyFailure;
				HandleJoinPartyFailure.BindUObject(this, &ThisClass::HandleJoinPersistentPartyFailure);
				World->GetTimerManager().SetTimerForNextTick(HandleJoinPartyFailure);
			}
		}
		else
		{
			UE_LOG(LogParty, Verbose, TEXT("OnJoinPersistentPartyComplete [%s]: already joining party."), PersistentPartyId.IsValid() ? *PersistentPartyId->ToString() : TEXT("INVALID"));
		}
	}
}

void UParty::HandleJoinPersistentPartyFailure()
{
	RestorePersistentPartyState();
}

void UParty::LeavePersistentParty(const FUniqueNetId& InUserId, const UPartyDelegates::FOnLeaveUPartyComplete& InCompletionDelegate)
{
	if (PersistentPartyId.IsValid())
	{
		if (!bLeavingPersistentParty)
		{
			UE_LOG(LogParty, Verbose, TEXT("LeavePersistentParty"));

			UPartyDelegates::FOnLeaveUPartyComplete CompletionDelegate;
			CompletionDelegate.BindUObject(this, &ThisClass::OnLeavePersistentPartyComplete, InCompletionDelegate);

			bLeavingPersistentParty = true;
			LeavePartyInternal(InUserId, IOnlinePartySystem::GetPrimaryPartyTypeId(), CompletionDelegate);
		}
		else
		{
			LeavePartyCompleteDelegates.Add(InCompletionDelegate);

			UE_LOG(LogParty, Verbose, TEXT("LeavePersistentParty %d extra delegates"), LeavePartyCompleteDelegates.Num());
		}
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("No party during LeavePersistentParty()"));
		InCompletionDelegate.ExecuteIfBound(InUserId, ELeavePartyCompletionResult::UnknownParty);
	}
}

void UParty::OnLeavePersistentPartyComplete(const FUniqueNetId& LocalUserId, const ELeavePartyCompletionResult Result, UPartyDelegates::FOnLeaveUPartyComplete CompletionDelegate)
{
	UE_LOG(LogParty, Display, TEXT("OnLeavePersistentPartyComplete() %s"), ToString(Result));

	ensure(bLeavingPersistentParty);
	bLeavingPersistentParty = false;
	PersistentPartyId = nullptr;

	CompletionDelegate.ExecuteIfBound(LocalUserId, Result);

	TArray<UPartyDelegates::FOnLeaveUPartyComplete> DelegatesCopy = LeavePartyCompleteDelegates;
	LeavePartyCompleteDelegates.Empty();

	// fire delegates for any/all LeavePersistentParty calls made while this one was in flight
	for (const UPartyDelegates::FOnLeaveUPartyComplete& ExtraDelegate : DelegatesCopy)
	{
		ExtraDelegate.ExecuteIfBound(LocalUserId, Result);
	}
}

void UParty::RestorePersistentPartyState()
{
	if (!GIsRequestingExit)
	{
		if (!bLeavingPersistentParty)
		{
			UWorld* World = GetWorld();
			IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
			if (PartyInt.IsValid())
			{
				UGameInstance* GameInstance = GetGameInstance();
				check(GameInstance);

				TSharedPtr<const FUniqueNetId> LocalUserId = GameInstance->GetPrimaryPlayerUniqueId();
				check(LocalUserId.IsValid() && LocalUserId->IsValid());

				UPartyGameState* PersistentParty = GetPersistentParty();

				// Check for existing party and create a new one if necessary
				bool bFoundExistingPersistentParty = PersistentParty ? true : false;
				if (bFoundExistingPersistentParty)
				{
					// In a party already, make sure the UI is aware of its state
					if (PersistentParty->ResetForFrontend())
					{
						OnPartyResetForFrontend().Broadcast(PersistentParty);
					}
					else
					{
						// There was an issue resetting the party, so leave 
						LeaveAndRestorePersistentParty();
					}
				}
				else
				{
					PersistentPartyId = nullptr;

					// Create a new party
					CreatePersistentParty(*LocalUserId);
				}
			}
		}
		else
		{
			UE_LOG(LogParty, Verbose, TEXT("Can't RestorePersistentPartyState while leaving party, ignoring"));
		}
	}
}

void UParty::UpdatePersistentPartyLeader(const FUniqueNetIdRepl& NewPartyLeader)
{
}

bool UParty::IsInParty(TSharedPtr<const FOnlinePartyId>& PartyId) const
{
	bool bFoundParty = false;

	UWorld* World = GetWorld();
	check(World);

	IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
	if (PartyInt.IsValid())
	{
		UGameInstance* GameInstance = GetGameInstance();
		check(GameInstance);

		TSharedPtr<const FUniqueNetId> LocalUserId = GameInstance->GetPrimaryPlayerUniqueId();
		if (ensure(LocalUserId.IsValid() && LocalUserId->IsValid()))
		{
			TArray<TSharedRef<const FOnlinePartyId>> LocalJoinedParties;
			PartyInt->GetJoinedParties(*LocalUserId, LocalJoinedParties);
			for (const auto& JoinedParty : LocalJoinedParties)
			{
				if (*JoinedParty == *PartyId)
				{
					bFoundParty = true;
					break;
				}
			}
		}
	}

	return bFoundParty;
}

void UParty::KickFromPersistentParty(const UPartyDelegates::FOnLeaveUPartyComplete& InCompletionDelegate)
{
	TSharedPtr<const FOnlinePartyId> LocalPersistentPartyId = GetPersistentPartyId();
	UPartyGameState* PersistentParty = GetPersistentParty();
	if (LocalPersistentPartyId.IsValid() && PersistentParty)
	{
		if (PersistentParty->GetPartySize() > 1)
		{
			UGameInstance* GameInstance = GetGameInstance();
			check(GameInstance);

			TSharedPtr<const FUniqueNetId> LocalUserId = GameInstance->GetPrimaryPlayerUniqueId();
			if (ensure(LocalUserId.IsValid() && LocalUserId->IsValid()))
			{
				// Leave the party (restored in frontend)
				LeavePersistentParty(*LocalUserId, InCompletionDelegate);
			}
		}
		else
		{
			// Just block new joining until back in the frontend
			PersistentParty->SetAcceptingMembers(false, EJoinPartyDenialReason::Busy);
		}
	}
}

void UParty::LeaveAndRestorePersistentParty()
{
	if (!bLeavingPersistentParty)
	{
		bLeavingPersistentParty = true;
		UWorld* World = GetWorld();

		FTimerDelegate LeaveAndRestorePersistentPartyNextTick;
		LeaveAndRestorePersistentPartyNextTick.BindUObject(this, &ThisClass::LeaveAndRestorePersistentPartyInternal);
		World->GetTimerManager().SetTimerForNextTick(LeaveAndRestorePersistentPartyNextTick);
	}
	else
	{
		UE_LOG(LogParty, Verbose, TEXT("Already leaving persistent party, ignoring"));
	}
}

void UParty::LeaveAndRestorePersistentPartyInternal()
{
	UGameInstance* GameInstance = GetGameInstance();
	check(GameInstance);

	TSharedPtr<const FUniqueNetId> PrimaryUserId = GameInstance->GetPrimaryPlayerUniqueId();
	check(PrimaryUserId.IsValid() && PrimaryUserId->IsValid());

	UPartyDelegates::FOnLeaveUPartyComplete CompletionDelegate;
	CompletionDelegate.BindUObject(this, &ThisClass::OnLeavePersistentPartyAndRestore);

	// Unset this here, wouldn't be here if we were unable to call LeaveAndRestorePersistentParty, it will be reset inside
	ensure(bLeavingPersistentParty);
	bLeavingPersistentParty = false;
	LeavePersistentParty(*PrimaryUserId, CompletionDelegate);
}

void UParty::OnLeavePersistentPartyAndRestore(const FUniqueNetId& LocalUserId, const ELeavePartyCompletionResult Result)
{
	UE_LOG(LogParty, Display, TEXT("OnLeavePersistentPartyAndRestore Result: %s"), ToString(Result));

	RestorePersistentPartyState();
}

void UParty::AddPendingPartyJoin(const FUniqueNetId& LocalUserId, const FPartyDetails& PartyDetails, const UPartyDelegates::FOnJoinUPartyComplete& JoinCompleteDelegate)
{
	if (LocalUserId.IsValid() && PartyDetails.IsValid())
	{
		if (!HasPendingPartyJoin())
		{
			PendingPartyJoin = MakeShareable(new FPendingPartyJoin(LocalUserId.AsShared(), PartyDetails, JoinCompleteDelegate));
		}
	}
}

void UParty::ClearPendingPartyJoin()
{
	PendingPartyJoin.Reset();
}

bool UParty::ProcessPendingPartyJoin()
{
	if (HasPendingPartyJoin())
	{
		HandlePendingJoin();
		return true;
	}

	return false;
}

UWorld* UParty::GetWorld() const
{
	UGameInstance* GameInstance = Cast<UGameInstance>(GetOuter());
	if (GameInstance)
	{
		return GameInstance->GetWorld();
	}
	
	return nullptr;
}

UGameInstance* UParty::GetGameInstance() const
{
	return Cast<UGameInstance>(GetOuter());
}

#undef LOCTEXT_NAMESPACE

// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintContext.h"

#include "UTGameInstance.h"
#include "Party.h"
#include "PartyContext.h"
#include "PartyGameState.h"
#include "OnlineSubsystemUtils.h"
#include "UTLocalPlayer.h"
#include "UTParty.h"
#include "UTLobbyGameMode.h"

#if WITH_SOCIAL
#include "Social.h"
#endif

#define LOCTEXT_NAMESPACE "UTPartyContext"

REGISTER_CONTEXT(UPartyContext);

UPartyContext::UPartyContext()
{
}

void UPartyContext::Initialize()
{
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	if (!OnlineSub)
	{
		return;
	}

	PartyInterface = OnlineSub->GetPartyInterface();
	FriendsInterface = OnlineSub->GetFriendsInterface();
	UserInfoInterface = OnlineSub->GetUserInterface();
	PresenceInterface = OnlineSub->GetPresenceInterface();

	if (!PartyInterface.IsValid() ||
		!FriendsInterface.IsValid() ||
		!UserInfoInterface.IsValid() ||
		!PresenceInterface.IsValid())
	{
		return;
	}

#if WITH_SOCIAL
	FriendsAndChatManager = ISocialModule::Get().GetFriendsAndChatManager();
	TSharedPtr<IGameAndPartyService> GameAndPartyService = FriendsAndChatManager->GetGameAndPartyService();
	GameAndPartyService->OnFriendsJoinParty().AddUObject(this, &ThisClass::OnFriendsListJoinParty);
#endif
	
	UUTGameInstance* GameInstance = GetGameInstance<UUTGameInstance>();

	UUTParty* UTParty = GameInstance->GetParties();
	if (UTParty)
	{
		UTParty->OnPartyJoined().AddUObject(this, &ThisClass::HandlePartyJoined);
		UTParty->OnPartyLeft().AddUObject(this, &ThisClass::HandlePartyLeft);
		UTParty->OnPartyMemberJoined().AddUObject(this, &ThisClass::HandlePartyMemberJoined);
		UTParty->OnPartyMemberLeft().AddUObject(this, &ThisClass::HandlePartyMemberLeft);
		UTParty->OnPartyMemberLeaving().AddUObject(this, &ThisClass::HandlePartyMemberLeaving);
		UTParty->OnPartyMemberPromoted().AddUObject(this, &ThisClass::HandlePartyMemberPromoted);
	}

	/*
	UTParty->OnPartyResetForFrontend().AddUObject(this, &ThisClass::HandlePartyResetForFrontend);
	UTParty->OnPartyMemberLeaving().AddUObject(this, &ThisClass::HandlePartyMemberLeaving);
	*/

	UUTLocalPlayer* LocalPlayer = GetOwningPlayer<UUTLocalPlayer>();
	if (LocalPlayer)
	{
		LocalPlayer->OnPlayerLoggedOut().AddUObject(this, &ThisClass::HandlePlayerLoggedOut);
	}

	/*
	LocalPlayer.OnPlayerLoggedIn().AddUObject(this, &ThisClass::OnPlayerLoggedIn);
	*/
}

void UPartyContext::HandlePlayerLoggedOut()
{
	OnPartyLeft.Broadcast();
}

void UPartyContext::HandlePartyJoined(UPartyGameState* PartyState)
{
	UUTPartyGameState* UTPartyState = Cast<UUTPartyGameState>(PartyState);
	if (UTPartyState)
	{
		OnPartyJoined.Broadcast();

		OnPartyTransitionCompleted.Broadcast(CurrentTransition);
		CurrentTransition = EUTPartyTransition::Idle;

		// If we're playing an offline game, quit back to main menu
		UUTLocalPlayer* LocalPlayer = GetOwningPlayer<UUTLocalPlayer>();
		if (LocalPlayer && !LocalPlayer->IsPartyLeader())
		{
			if (!LocalPlayer->IsMenuGame())
			{
				if (LocalPlayer->GetWorld()->GetNetMode() == NM_Standalone)
				{
					LocalPlayer->ReturnToMainMenu();
				}
				else if (LocalPlayer->GetWorld()->GetGameState() && 
					     LocalPlayer->GetWorld()->GetGameState()->GameModeClass &&
						 LocalPlayer->GetWorld()->GetGameState()->GameModeClass->IsChildOf(AUTLobbyGameMode::StaticClass()))
				{
					LocalPlayer->ReturnToMainMenu();
				}
			}
		}
	}
}

void UPartyContext::HandlePartyLeft(UPartyGameState* PartyState, EMemberExitedReason Reason)
{
	OnPartyLeft.Broadcast();

	if (PartyState && !PartyState->IsLocalPartyLeader())
	{
		UUTLocalPlayer* LocalPlayer = GetOwningPlayer<UUTLocalPlayer>();
		if (LocalPlayer)
		{
			if (Reason == EMemberExitedReason::Kicked)
			{
				LocalPlayer->ShowToast(NSLOCTEXT("UPartyContext", "PartyLeftKicked", "You have been kicked from the party"));
			}
			else
			{
				LocalPlayer->ShowToast(NSLOCTEXT("UPartyContext", "PartyLeft", "You have left the party"));
			}
		}
	}
}

void UPartyContext::HandlePartyMemberJoined(UPartyGameState* PartyState, const FUniqueNetIdRepl& UniqueId)
{
	UPartyMemberState* NewPartyMember = PartyState->GetPartyMember(UniqueId);
	UUTPartyMemberState* NewUTPartyMember = Cast<UUTPartyMemberState>(NewPartyMember);
	if (NewUTPartyMember)
	{
		UE_LOG(LogParty, Verbose, TEXT("HandlePartyMemberJoined: Registering team member!"));
		OnPlayerStateChanged.Broadcast();

		UUTLocalPlayer* LocalPlayer = GetOwningPlayer<UUTLocalPlayer>();
		if (LocalPlayer)
		{
			TSharedPtr<const FUniqueNetId> LocalUserId = LocalPlayer->GetPreferredUniqueNetId();
			if (LocalUserId.IsValid() && UniqueId.ToString() != LocalUserId->ToString())
			{
				if (IsPartyLeader(UniqueId))
				{
					FText PartyMessage = FText::Format(NSLOCTEXT("UPartyContext", "JoinedPartyMessage", "You have joined {0}'s party"), NewUTPartyMember->DisplayName);
					LocalPlayer->ShowToast(PartyMessage);
				}
				else
				{
					FText PartyMessage = FText::Format(NSLOCTEXT("UPartyContext", "JoinMessage", "{0} has joined your party"), NewUTPartyMember->DisplayName);
					LocalPlayer->ShowToast(PartyMessage);
				}
			}
		}
	}
}

void UPartyContext::HandlePartyMemberLeaving(UPartyGameState* PartyState, const FUniqueNetIdRepl& RemovedMemberId, EMemberExitedReason Reason)
{
	UPartyMemberState* LeavingPartyMember = PartyState->GetPartyMember(RemovedMemberId);
	UUTPartyMemberState* LeavingUTPartyMember = Cast<UUTPartyMemberState>(LeavingPartyMember);
	if (LeavingUTPartyMember)
	{
		UUTLocalPlayer* LocalPlayer = GetOwningPlayer<UUTLocalPlayer>();
		if (LocalPlayer)
		{
			if (Reason == EMemberExitedReason::Kicked)
			{
				FText PartyMessage = FText::Format(NSLOCTEXT("UPartyContext", "LeaveMessageKicked", "{0} was kicked from your party"), LeavingUTPartyMember->DisplayName);
				LocalPlayer->ShowToast(PartyMessage);
			}
			else
			{
				FText PartyMessage = FText::Format(NSLOCTEXT("UPartyContext", "LeaveMessage", "{0} has left your party"), LeavingUTPartyMember->DisplayName);
				LocalPlayer->ShowToast(PartyMessage);
			}
		}
	}
}

void UPartyContext::HandlePartyMemberLeft(UPartyGameState* PartyState, const FUniqueNetIdRepl& RemovedMemberId, EMemberExitedReason Reason)
{
	OnPlayerStateChanged.Broadcast();
}

void UPartyContext::HandlePartyMemberPromoted(UPartyGameState* PartyState, const FUniqueNetIdRepl& InMemberId)
{
	OnPartyMemberPromoted.Broadcast();

	UPartyMemberState* LeaderPartyMember = PartyState->GetPartyMember(InMemberId);
	UUTPartyMemberState* LeaderUTPartyMember = Cast<UUTPartyMemberState>(LeaderPartyMember);
	if (LeaderUTPartyMember)
	{
		UUTLocalPlayer* LocalPlayer = GetOwningPlayer<UUTLocalPlayer>();
		if (LocalPlayer)
		{
			FText PartyMessage = FText::Format(NSLOCTEXT("UPartyContext", "NewLeader", "{0} is now the party leader"), LeaderUTPartyMember->DisplayName);
			LocalPlayer->ShowToast(PartyMessage);
		}
	}
}

void UPartyContext::Finalize()
{
#if WITH_SOCIAL
	if (FriendsAndChatManager.IsValid())
	{
		TSharedPtr<IGameAndPartyService> GameAndPartyService = FriendsAndChatManager->GetGameAndPartyService();
		if (GameAndPartyService.IsValid())
		{
			GameAndPartyService->OnFriendsJoinParty().RemoveAll(this);
		}
	}
	FriendsAndChatManager.Reset();
#endif

	UserInfoInterface.Reset();
	PresenceInterface.Reset();
	FriendsInterface.Reset();
	PartyInterface.Reset();
}

void UPartyContext::OnFriendsListJoinParty(const FUniqueNetId& SenderId, const TSharedRef<class IOnlinePartyJoinInfo>& PartyJoinInfo, bool bIsFromInvite)
{
	const ULocalPlayer* LocalPlayer = GetOwningPlayer();
	TSharedPtr<const FUniqueNetId> LocalUserId = LocalPlayer->GetPreferredUniqueNetId();

	if (LocalUserId.IsValid())
	{
		TSharedPtr<const IOnlinePartyJoinInfo> UpdatedPartyJoinInfo;
		if (PartyInterface.IsValid())
		{
			UpdatedPartyJoinInfo = PartyInterface->GetAdvertisedParty(*LocalUserId, SenderId, IOnlinePartySystem::GetPrimaryPartyTypeId());
		}

		if (!UpdatedPartyJoinInfo.IsValid() || !UpdatedPartyJoinInfo->CanJoin())
		{
			// Prefer the advertised party information, but get the social item invite data if necessary
			UpdatedPartyJoinInfo = PartyJoinInfo;
		}

		if (UpdatedPartyJoinInfo.IsValid())
		{
			JoinPartyInternal(*LocalUserId, bIsFromInvite, UpdatedPartyJoinInfo.ToSharedRef());
		}
	}
}

void UPartyContext::JoinPartyInternal(const FUniqueNetId& LocalPlayerId, bool bIsFromInvite, const TSharedRef<const IOnlinePartyJoinInfo>& PartyJoinInfo)
{
	UUTGameInstance* UTGameInstance = GetGameInstance<UUTGameInstance>();
	check(UTGameInstance);

	UUTParty* UTParty = UTGameInstance->GetParties();
	check(UTParty);

	PendingPartyPlayerInfo = UserInfoInterface->GetUserInfo(GetOwningPlayer()->GetControllerId(), *(PartyJoinInfo->GetSourceUserId()));
	if (!ensure(PendingPartyPlayerInfo.IsValid()))
	{
		UE_LOG(LogParty, Warning, TEXT("Invalid online user info!"));
		return;
	}

	UTParty->AddPendingJoin(LocalPlayerId.AsShared(), MakeShareable(new FPartyDetails(PartyJoinInfo, true)), UPartyDelegates::FOnJoinUPartyComplete::CreateUObject(this, &ThisClass::OnJoinPartyCompleteInternal));
	
	CurrentTransition = EUTPartyTransition::Joining;
	OnPartyTransitionStarted.Broadcast(CurrentTransition);

	UTParty->HandlePendingJoin();
}

void UPartyContext::OnJoinPartyCompleteInternal(const FUniqueNetId& LocalUserId, EJoinPartyCompletionResult Result, int32 DeniedResultCode)
{
	UE_LOG(LogParty, Display, TEXT("UPartyContext::OnJoinPartyCompleteInternal Result: %s"), ToString(Result));

	if (Result != EJoinPartyCompletionResult::Succeeded)
	{
		OnPartyTransitionCompleted.Broadcast(CurrentTransition);
		CurrentTransition = EUTPartyTransition::Idle;
		
		return HandleJoinPartyFailure(Result, DeniedResultCode);
	}

	UUTGameInstance* GameInstance = GetGameInstance<UUTGameInstance>();
	check(GameInstance);

	UUTParty* Party = GameInstance->GetParties();
	UPartyGameState* PartyState = Party ? Party->GetPersistentParty() : nullptr;
	if (!PartyState)
	{
		UE_LOG(LogParty, Warning, TEXT("OnJoinPartyComplete: Invalid party state (null)"));
		return;
	}

	TSharedPtr<const FUniqueNetId> PartyLeaderId = PartyState->GetPartyLeader();
	if (!PartyLeaderId.IsValid() || *PartyLeaderId == LocalUserId)
	{
		return;
	}

	if (!PendingPartyPlayerInfo.IsValid())
	{
		UE_LOG(LogParty, Warning, TEXT("OnJoinPartyComplete: Invalid pending player info"));
		return;
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("Username"), FText::FromString(PendingPartyPlayerInfo->GetDisplayName()));
	PendingPartyPlayerInfo.Reset();
	const FText JoinedPartyMessage = FText::Format(LOCTEXT("JoinPartyToast", "You have joined {Username}'s party"), Args);
	UE_LOG(LogParty, Display, TEXT("%s"), *JoinedPartyMessage.ToString());
}

void UPartyContext::HandleJoinPartyFailure(EJoinPartyCompletionResult Result, int32 DeniedResultCode)
{
	UUTLocalPlayer* LocalPlayer = GetOwningPlayer<UUTLocalPlayer>();
	if (LocalPlayer)
	{
		LocalPlayer->ShowToast(NSLOCTEXT("UPartyContext", "FailPartyJoin", "Could not join party, it may be in matchmaking"));
	}
}

int32 UPartyContext::GetPartySize() const
{
	if (UUTGameInstance* GameInstance = GetGameInstance<UUTGameInstance>())
	{
		if (UUTParty* Parties = GameInstance->GetParties())
		{
			if (UPartyGameState* Party = Parties->GetPersistentParty())
			{
				return Party->GetPartySize();
			}
		}
	}

	return 0;
}

void UPartyContext::GetLocalPartyMemberIDs(TArray<FUniqueNetIdRepl>& PartyMemberIDs) const
{
	TArray<UPartyMemberState*> PartyMembers;

	if (UUTGameInstance* GameInstance = GetGameInstance<UUTGameInstance>())
	{
		if (UUTParty* Parties = GameInstance->GetParties())
		{
			if (UPartyGameState* Party = Parties->GetPersistentParty())
			{
				Party->GetAllPartyMembers(PartyMembers);
			}
		}
	}

	for (UPartyMemberState* PartyMember : PartyMembers)
	{	
		PartyMemberIDs.Add(PartyMember->UniqueId);
	}
}

void UPartyContext::GetLocalPartyMemberNames(TArray<FText>& PartyMemberNames) const
{
	TArray<UPartyMemberState*> PartyMembers;

	if (UUTGameInstance* GameInstance = GetGameInstance<UUTGameInstance>())
	{
		if (UUTParty* Parties = GameInstance->GetParties())
		{
			if (UPartyGameState* Party = Parties->GetPersistentParty())
			{
				Party->GetAllPartyMembers(PartyMembers);
			}
		}
	}

	for (UPartyMemberState* PartyMember : PartyMembers)
	{
		PartyMemberNames.Add(PartyMember->DisplayName);
	}
}

bool UPartyContext::IsPartyLeader(const FUniqueNetIdRepl& PartyMemberId)
{
	if (!PartyMemberId.IsValid())
	{
		UE_LOG(LogParty, Warning, TEXT("IsPartyLeader invalid: Invalid net ID for player to check for leader."));
		return false;
	}

	UUTGameInstance* GameInstance = GetGameInstance<UUTGameInstance>();
	check(GameInstance);
	UUTParty* UTParty = GameInstance->GetParties();
	check(UTParty);

	UPartyGameState* PersistentParty = UTParty->GetPersistentParty();
	if (ensure(PersistentParty))
	{
		TSharedPtr<const FUniqueNetId> PartyLeaderId = PersistentParty->GetPartyLeader();
		if (ensure(PartyLeaderId.IsValid()) && *PartyMemberId == *PartyLeaderId)
		{
			return true;
		}
	}

	return false;
}

void UPartyContext::PromotePartyMemberToLeader(const FUniqueNetIdRepl& PartyMemberId)
{
	if (!PartyMemberId.IsValid())
	{
		UE_LOG(LogParty, Warning, TEXT("Promotion failed: Invalid net ID for player to promote."));
		return;
	}

	UUTGameInstance* GameInstance = GetGameInstance<UUTGameInstance>();
	check(GameInstance);
	UUTParty* UTParty = GameInstance->GetParties();
	check(UTParty);

	UPartyGameState* PersistentParty = UTParty->GetPersistentParty();
	if (ensure(PersistentParty))
	{
		TSharedPtr<const FUniqueNetId> PartyLeaderId = PersistentParty->GetPartyLeader();
		const ULocalPlayer* LocalPlayer = GetOwningPlayer();
		TSharedPtr<const FUniqueNetId> LocalUserId = LocalPlayer->GetPreferredUniqueNetId();
		if (ensure(PartyLeaderId.IsValid()) && *LocalUserId == *PartyLeaderId)
		{
			PersistentParty->PromoteMember(PartyMemberId);
		}
		else
		{
			UE_LOG(LogParty, Warning, TEXT("Kick failed: You are not party leader. Must be party leader to kick members."));
		}
	}
}

void UPartyContext::KickPartyMember(const FUniqueNetIdRepl& PartyMemberId)
{
	if (!PartyMemberId.IsValid())
	{
		UE_LOG(LogParty, Warning, TEXT("Kick failed: Invalid net ID for player to kick."));
		return;
	}

	UUTGameInstance* GameInstance = GetGameInstance<UUTGameInstance>();
	check(GameInstance);
	UUTParty* UTParty = GameInstance->GetParties();
	check(UTParty);

	UPartyGameState* PersistentParty = UTParty->GetPersistentParty();
	if (ensure(PersistentParty))
	{
		TSharedPtr<const FUniqueNetId> PartyLeaderId = PersistentParty->GetPartyLeader();
		const ULocalPlayer* LocalPlayer = GetOwningPlayer();
		TSharedPtr<const FUniqueNetId> LocalUserId = LocalPlayer->GetPreferredUniqueNetId();
		if (ensure(PartyLeaderId.IsValid()) && *LocalUserId == *PartyLeaderId)
		{
			PersistentParty->KickMember(PartyMemberId);
		}
		else
		{
			UE_LOG(LogParty, Warning, TEXT("Kick failed: You are not party leader. Must be party leader to kick members."));
		}
	}
}

void UPartyContext::LeaveParty()
{
	UUTLocalPlayer* LocalPlayer = GetOwningPlayer<UUTLocalPlayer>();
	TSharedPtr<const FUniqueNetId> LocalUserId = LocalPlayer->GetPreferredUniqueNetId();
	if (LocalUserId.IsValid())
	{
		UUTGameInstance* GameInstance = GetGameInstance<UUTGameInstance>();
		check(GameInstance);

		UPartyDelegates::FOnLeaveUPartyComplete CompletionDelegate;

		if (LocalPlayer->IsMenuGame())
		{
			// Don't need to go anywhere afterwards
			CompletionDelegate.BindUObject(this, &ThisClass::OnLeavePartyFromMenu);
		}
		else
		{
			// Delegate should quit game afterwards
			CompletionDelegate.BindUObject(this, &ThisClass::OnLeavePartyFromGame);
		}

		// Leaving as party leader should just promote someone else in that party and we'll create a new party later
		UUTParty* Party = GameInstance->GetParties();
		check(Party);
		UPartyGameState* PersistentParty = Party->GetPersistentParty();
		if (PersistentParty)
		{
			CurrentTransition = EUTPartyTransition::Leaving;
			OnPartyTransitionStarted.Broadcast(CurrentTransition);
			Party->LeavePersistentParty(*LocalUserId, CompletionDelegate);
		}
		else
		{
			// Hail Mary here, hope the frontend can restore things
			FString ErrorStr = TEXT("No persistent party when leaving!");
			CompletionDelegate.Execute(*LocalUserId, ELeavePartyCompletionResult::UnknownParty);
		}
	}
}

void UPartyContext::OnLeavePartyFromMenu(const FUniqueNetId& LocalUserId, const ELeavePartyCompletionResult Result)
{
	UE_LOG(LogParty, Display, TEXT("OnLeavePartyFromMenu Success: %d %s"), Result == ELeavePartyCompletionResult::Succeeded, ToString(Result));

	UUTGameInstance* GameInstance = GetGameInstance<UUTGameInstance>();
	check(GameInstance);
	UUTParty* Party = GameInstance->GetParties();
	check(Party);

	Party->RestorePersistentPartyState();
}

void UPartyContext::OnLeavePartyFromGame(const FUniqueNetId& LocalUserId, const ELeavePartyCompletionResult Result)
{
	UE_LOG(LogParty, Display, TEXT("OnLeavePartyFromGame Success: %d %s"), Result == ELeavePartyCompletionResult::Succeeded, ToString(Result));

	UUTGameInstance* GameInstance = GetGameInstance<UUTGameInstance>();
	check(GameInstance);
	UUTParty* Party = GameInstance->GetParties();
	check(Party);

	// Should exit the game here?

	Party->RestorePersistentPartyState();
}

#undef LOCTEXT_NAMESPACE
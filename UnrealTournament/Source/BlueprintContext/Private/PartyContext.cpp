// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintContext.h"

#include "UTGameInstance.h"
#include "Party.h"
#include "PartyContext.h"
#include "PartyGameState.h"
#include "OnlineSubsystemUtils.h"
#include "UTLocalPlayer.h"
#include "UTParty.h"

#if WITH_SOCIAL
#include "Social.h"
#endif

#define LOCTEXT_NAMESPACE "UTPartyContext"

REGISTER_CONTEXT(UPartyContext);

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
	}

	/*
	UTParty->OnPartyResetForFrontend().AddUObject(this, &ThisClass::HandlePartyResetForFrontend);
	UTParty->OnPartyMemberLeaving().AddUObject(this, &ThisClass::HandlePartyMemberLeaving);
	UTParty->OnPartyMemberPromoted().AddUObject(this, &ThisClass::HandlePartyMemberPromoted);
	*/

	UUTLocalPlayer* LocalPlayer = GetOwningPlayer<UUTLocalPlayer>();
	/*
	LocalPlayer.OnPlayerLoggedIn().AddUObject(this, &ThisClass::OnPlayerLoggedIn);
	LocalPlayer.OnPlayerLoggedOut().AddUObject(this, &ThisClass::OnPlayerLoggedOut);
	*/
}

void UPartyContext::HandlePartyJoined(UPartyGameState* PartyState)
{
	UUTPartyGameState* UTPartyState = Cast<UUTPartyGameState>(PartyState);
	if (UTPartyState)
	{
		OnPartyJoined.Broadcast();

		OnPartyTransitionCompleted.Broadcast(CurrentTransition);
		CurrentTransition = EUTPartyTransition::Idle;
	}
}

void UPartyContext::HandlePartyLeft(UPartyGameState* PartyState, EMemberExitedReason Reason)
{
	OnPartyLeft.Broadcast();
}

void UPartyContext::HandlePartyMemberJoined(UPartyGameState* PartyState, const FUniqueNetIdRepl& UniqueId)
{
	UPartyMemberState* NewPartyMember = PartyState->GetPartyMember(UniqueId);
	UUTPartyMemberState* NewUTPartyMember = Cast<UUTPartyMemberState>(NewPartyMember);
	if (NewUTPartyMember)
	{
		UE_LOG(LogParty, Verbose, TEXT("HandlePartyMemberJoined: Registering team member!"));
		OnPlayerStateChanged.Broadcast();
	}
}

void UPartyContext::HandlePartyMemberLeft(UPartyGameState* PartyState, const FUniqueNetIdRepl& RemovedMemberId, EMemberExitedReason Reason)
{
	OnPlayerStateChanged.Broadcast();
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

	FPartyDetails PartyDetails(PartyJoinInfo, true);
	UTParty->AddPendingJoin(LocalPlayerId.AsShared(), PartyDetails, UPartyDelegates::FOnJoinUPartyComplete::CreateUObject(this, &ThisClass::OnJoinPartyCompleteInternal));
	
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
	UUTGameInstance* GameInstance = GetGameInstance<UUTGameInstance>();
	check(GameInstance);

	// Let something know we failed
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
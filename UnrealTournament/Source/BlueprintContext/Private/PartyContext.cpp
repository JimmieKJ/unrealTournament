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

UPartyContext::UPartyContext()
{
	OnPartyInviteResponseReceivedDelegate =
		FOnPartyInviteResponseReceivedDelegate::CreateUObject(this, &UPartyContext::HandlePartyInviteResponseReceived);
}

void UPartyContext::Initialize()
{
	Super::Initialize();

	// Start off in the joining state so we don't immediately get a party joined notification
	CurrentTransitionState = EUTPartyTransition::Joining;

	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	if (OnlineSub)
	{
		UserInfoInterface = OnlineSub->GetUserInterface();
		PartyInterface = OnlineSub->GetPartyInterface();
	}

	if (PartyInterface.IsValid())
	{
		PartyInviteResponseReceivedDelegateHandle =
			PartyInterface->AddOnPartyInviteResponseReceivedDelegate_Handle(OnPartyInviteResponseReceivedDelegate);
	}

	if (auto Owner = GetOwningPlayer<UUTLocalPlayer>())
	{
		Owner->OnPlayerLoggedIn().AddUObject(this, &ThisClass::HandlePlayerLoggedIn);
	}
}

void UPartyContext::PreDestructContext(UWorld* World)
{
	Super::PreDestructContext(World);

	UserInfoInterface.Reset();
	PartyInterface.Reset();
}

void UPartyContext::PostInitWorld(UWorld* World)
{
	Super::PostInitWorld(World);

	UnbindOnlineDelegates(World);
	BindOnlineDelegates(World);
}

void UPartyContext::OnWorldCleanup(UWorld* World)
{
	Super::OnWorldCleanup(World);
	UnbindOnlineDelegates(World);
}

void UPartyContext::BindOnlineDelegates(UWorld* World)
{
#if WITH_SOCIAL
	TSharedRef<IFriendsAndChatManager> FriendsAndChatManager =
		ISocialModule::Get().GetFriendsAndChatManager(Online::GetUtils()->GetOnlineIdentifier(World), true);
	TSharedPtr<IGameAndPartyService> SharedGame = FriendsAndChatManager->GetGameAndPartyService();
	TSharedPtr<IChatNotificationService> SharedNotificationService = FriendsAndChatManager->GetNotificationService();

	if (!FriendsAndChatManager->OnSendPartyInvitationComplete().IsBoundToObject(this))
	{
		FriendsAndChatManager->OnSendPartyInvitationComplete().AddUObject(this, &ThisClass::OnFriendsListSendPartyInviteComplete);
	}

	if (SharedGame.IsValid())
	{
		if (!SharedGame->OnFriendsJoinParty().IsBoundToObject(this))
		{
			SharedGame->OnFriendsJoinParty().AddUObject(this, &ThisClass::OnFriendsJoinParty);
		}
		/*
		if (!SharedGame->OnSendGameInvitationRequested().IsBoundToObject(this))
		{
			SharedGame->OnSendGameInvitationRequested().AddUObject(this, &ThisClass::OnSendGameInvitationRequested);
		}
		*/
	}

	if (SharedNotificationService.IsValid() && !SharedNotificationService->OnSendNotification().IsBoundToObject(this))
	{
		SharedNotificationService->OnSendNotification().AddUObject(this, &ThisClass::HandleFriendsActionNotification);
	}

	IOnlinePresencePtr OnlinePresence = Online::GetPresenceInterface(World);
	if (OnlinePresence.IsValid() && !PresenceReceivedDelegateHandle.IsValid())
	{
		PresenceReceivedDelegateHandle = OnlinePresence->AddOnPresenceReceivedDelegate_Handle(
			FOnPresenceReceivedDelegate::CreateUObject(this, &ThisClass::OnPresenceReceivedInternal));
	}

	IOnlinePartyPtr OnlineParty = Online::GetPartyInterface(World);
	if (OnlineParty.IsValid() && !ParyInvitesUpdatedHandle.IsValid())
	{
		ParyInvitesUpdatedHandle = OnlineParty->AddOnPartyInvitesChangedDelegate_Handle(
			FOnPartyInvitesChangedDelegate::CreateUObject(this, &ThisClass::OnPartyInvitesChangedInternal));
	}

	GameAndPartyService = SharedGame;
	NotificationService = SharedNotificationService;
#endif
}

void UPartyContext::UnbindOnlineDelegates(UWorld* World)
{
	if (ParyInvitesUpdatedHandle.IsValid())
	{
		IOnlinePartyPtr OnlineParty = Online::GetPartyInterface(World);
		if (OnlineParty.IsValid())
		{
			OnlineParty->ClearOnPartyInvitesChangedDelegate_Handle(ParyInvitesUpdatedHandle);
		}
	}

	if (PresenceReceivedDelegateHandle.IsValid())
	{
		IOnlinePresencePtr OnlinePresence = Online::GetPresenceInterface(World);
		if (OnlinePresence.IsValid())
		{
			OnlinePresence->ClearOnPresenceReceivedDelegate_Handle(PresenceReceivedDelegateHandle);
		}
	}

#if WITH_SOCIAL
	TSharedRef<IFriendsAndChatManager> FriendsAndChatManager =
		ISocialModule::Get().GetFriendsAndChatManager(Online::GetUtils()->GetOnlineIdentifier(World), true);
	FriendsAndChatManager->OnSendPartyInvitationComplete().RemoveAll(this);

	TSharedPtr<IGameAndPartyService> SharedGame = GameAndPartyService.Pin();
	if (SharedGame.IsValid())
	{
		//SharedGame->OnSendGameInvitationRequested().RemoveAll(this);

		SharedGame->OnFriendsJoinParty().RemoveAll(this);
	}

	if (NotificationService.IsValid())
	{
		TSharedPtr<IChatNotificationService> SharedNotificationService = NotificationService.Pin();
		if (SharedNotificationService.IsValid())
		{
			SharedNotificationService->OnSendNotification().RemoveAll(this);
		}
	}

	GameAndPartyService.Reset();
	NotificationService.Reset();
#endif
}

void UPartyContext::HandlePlayerLoggedIn()
{
	if (auto Owner = GetOwningPlayer<UUTLocalPlayer>())
	{
		Owner->OnPlayerLoggedIn().RemoveAll(this);
		Owner->OnPlayerLoggedOut().AddUObject(this, &ThisClass::HandlePlayerLoggedOut);
	}

	if (auto GameInstance = GetGameInstance<const UUTGameInstance>())
	{
		Party = GameInstance->GetParties();
		if (Party.IsValid())
		{
			PartyStack.Push(Party->GetPersistentParty());

			Party->OnPartyJoined().AddUObject(this, &ThisClass::OnPartyJoined);
			Party->OnPartyLeft().AddUObject(this, &ThisClass::OnPartyLeft);
			Party->OnPartyResetForFrontend().AddUObject(this, &ThisClass::OnPartyResetForFrontend);
			Party->OnPartyMemberJoined().AddUObject(this, &ThisClass::OnPartyMemberJoined);
			Party->OnPartyMemberPromoted().AddUObject(this, &ThisClass::OnPartyMemberPromoted);
			Party->OnPartyMemberLeaving().AddUObject(this, &ThisClass::OnPartyMemberLeaving);
			Party->OnPartyMemberLeft().AddUObject(this, &ThisClass::OnPartyMemberLeft);

			UUTParty* UTParty = Cast<UUTParty>(Party.Get());
			if (UTParty)
			{
				UTParty->OnPartyJoinComplete().AddUObject(this, &ThisClass::OnUTJoinPartyComplete);
			}
		}
	}

	// If a player has logged out and then logged in, they will have a new party, so the OnPartyChanged
	// event should get called.
	if (OnPartyChanged.IsBound())
	{
		OnPartyChanged.Broadcast();
	}
}

void UPartyContext::HandlePlayerLoggedOut()
{
	if (auto Owner = GetOwningPlayer<UUTLocalPlayer>())
	{
		Owner->OnPlayerLoggedOut().RemoveAll(this);
		Owner->OnPlayerLoggedIn().AddUObject(this, &ThisClass::HandlePlayerLoggedIn);
	}

	if (Party.IsValid())
	{
		Party->OnPartyJoined().RemoveAll(this);
		Party->OnPartyLeft().RemoveAll(this);
		Party->OnPartyResetForFrontend().RemoveAll(this);
		Party->OnPartyMemberJoined().RemoveAll(this);
		Party->OnPartyMemberPromoted().RemoveAll(this);
		Party->OnPartyMemberLeaving().RemoveAll(this);
		Party->OnPartyMemberLeft().RemoveAll(this);

		UUTParty* UTParty = Cast<UUTParty>(Party.Get());
		if (UTParty)
		{
			UTParty->OnPartyJoinComplete().RemoveAll(this);
		}

		Party.Reset();
	}
}

void UPartyContext::OnFriendsListSendPartyInviteComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId,
	const FUniqueNetId& RecipientId, ESendPartyInvitationCompletionResult Result)
{
}

void UPartyContext::OnPartyJoined(UPartyGameState* PartyState)
{
	UUTPartyGameState* UTPartyState = Cast<UUTPartyGameState>(PartyState);
	if (UTPartyState)
	{
		UE_LOG(LogParty, Display, TEXT("Local player joined a new party."));

		bIsLocalPlayerPartyLeader = true;

		if (!UTPartyState->OnPartyMemberDataChanged().IsBoundToObject(this))
		{
			UTPartyState->OnClientPartyStateChanged().AddUObject(this, &UPartyContext::HandleClientPartyStateChanged);
			UTPartyState->OnPartyMemberDataChanged().AddUObject(this, &UPartyContext::HandlePartyMemberStateChanged);
			UTPartyState->OnPartyTypeChanged().AddUObject(this, &UPartyContext::HandlePartyTypeChanged);
			UTPartyState->OnLeaderFriendsOnlyChanged().AddUObject(this, &UPartyContext::HandleLeaderFriendsOnlyChanged);
			UTPartyState->OnLeaderInvitesOnlyChanged().AddUObject(this, &UPartyContext::HandleLeaderInvitesOnlyChanged);

			UTPartyState->OnPartyDataChanged().AddUObject(this, &UPartyContext::HandlePartyDataChanged);
		}
		
		// This needs to be done for the local player since they may have joined a party and we
		// want to update the join and invite statuses on friends who may already be in the same 
		// party, or if the local player was kicked and we want to update the join/invite status.
		//UpdateFriendsJoinInviteStatus();

		if (OnPartyChanged.IsBound())
		{
			OnPartyChanged.Broadcast();
		}

		OnPartyTransitionCompleted.Broadcast(CurrentTransitionState);
		CurrentTransitionState = EUTPartyTransition::Idle;
	}

	// Toggle party chat tab if appropriate
	//TogglePartyChatTab(PartyState);
}


void UPartyContext::OnPartyLeft(UPartyGameState* PartyState, EMemberExitedReason Reason)
{
	// This is called more often than it might seem at first glance.
	// Whenever the player leaves a game once Hero Select is reached, they also leave a party, even if they're alone (i.e. in a party of one)
	// This also happens when a player is kicked from a party (which will also kick from the lobby or zone, if in one).
	// Therefore, all info about any team is now invalid and we start from scratch (the player will join a new party shortly)

	UWorld* World = GetWorld();
	if (World)
	{
		// Protecting against possibility that this is received during shutdown and the world is null
		UUTGameInstance* GameInstance = Cast<UUTGameInstance>(World->GetGameInstance());
		if (GameInstance)
		{
			UE_LOG(LogParty, Display, TEXT("Local player left party."));
			/*
			if (UUTMatchmakingContext* MatchmakingContext = UBlueprintContextLibrary::GetTypedContext<UUTMatchmakingContext>(GameInstance))
			{
				MatchmakingContext->SetConnectedToGameFalse();
			}*/
		}
		else
		{
			UE_LOG(LogParty, Warning, TEXT("HandlePartyLeft: Null GameInstance."));
		}
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("HandlePartyLeft: Null World."));
	}

	UUTPartyGameState* UTPartyState = Cast<UUTPartyGameState>(PartyState);
	if (UTPartyState)
	{
		UTPartyState->OnClientPartyStateChanged().RemoveAll(this);
		UTPartyState->OnPartyMemberDataChanged().RemoveAll(this);
		UTPartyState->OnPartyTypeChanged().RemoveAll(this);
		UTPartyState->OnLeaderFriendsOnlyChanged().RemoveAll(this);
		UTPartyState->OnLeaderInvitesOnlyChanged().RemoveAll(this);
	}

	if (OnPartyChanged.IsBound())
	{
		OnPartyChanged.Broadcast();
	}

	/*

	if (Reason == EMemberExitedReason::Kicked)
	{
		SendNotification(LOCTEXT("LocalPlayerKickedToast", "You have been removed from the party"));
	}

	// Toggle party chat tab if appropriate
	TogglePartyChatTab(PartyState);
	*/
}

void UPartyContext::HandleClientPartyStateChanged(EUTPartyState PartyState)
{
	OnClientPartyStateChanged.Broadcast(PartyState);
}

void UPartyContext::HandlePartyTypeChanged(EPartyType PartyType)
{
	OnPartyTypeChanged.Broadcast(PartyType);
}

void UPartyContext::OnPartyResetForFrontend(UPartyGameState* PersistentParty)
{
	if (UUTPartyGameState* UTPartyState = Cast<UUTPartyGameState>(PersistentParty))
	{
		CurrentTransitionState = EUTPartyTransition::Idle;
	}
}

void UPartyContext::OnPartyMemberJoined(UPartyGameState* PartyState, const FUniqueNetIdRepl& MemberId)
{
	if (UUTPartyGameState* UTPartyState = Cast<UUTPartyGameState>(PartyState))
	{
		// Don't notify about each member member joining initially and never notify that we've joined our own team
		if (CurrentTransitionState != EUTPartyTransition::Joining /*&& GetOwningPlayer<UUTLocalPlayer>()->GetGameAccountId() != MemberId*/)
		{
			if (UPartyMemberState* MemberState = PartyState->GetPartyMember(MemberId))
			{
				/*
				TNotification<ENotificationType::Information>::Display(
					GetWorld(), FText::FormatNamed(PartyMemberJoined, TEXT("Player"), MemberState->DisplayName),
					FText::FormatNamed(PartySizeBody, TEXT("Current"), NumPartyMembers(), TEXT("Max"), MaxPartyMembers()));
					*/
			}
		}

		//TogglePartyChatTab();

		if (NumPartyMembers() > 1)
		{
			OnPartyStateChanged.Broadcast(EPartyMemberState::InParty);
		}

		if (OnPartyChanged.IsBound())
		{
			OnPartyChanged.Broadcast();
		}
	}
}

void UPartyContext::OnPartyMemberPromoted(UPartyGameState* PartyState, const FUniqueNetIdRepl& MemberId)
{
	if (UUTPartyGameState* UTPartyState = Cast<UUTPartyGameState>(PartyState))
	{
		FText LeaderSubjectText;
		/*
		if (MemberId == GetOwningPlayer<UUTLocalPlayer>()->GetGameAccountId())
		{
			// The local player was promoted
			LeaderSubjectText = LOCTEXT("YouAre", "You are");
		}
		else*/ if (UPartyMemberState* PartyMember = PartyState->GetPartyMember(MemberId))
		{
			// Another player in the party was promoted
			LeaderSubjectText = FText::Format(LOCTEXT("PlayerIs", "{0} is"), PartyMember->DisplayName);
		}

		if (!LeaderSubjectText.IsEmpty())
		{/*
			TNotification<ENotificationType::Information>::Display(
				GetWorld(), PartyMemberPromoted,
				FText::Format(LOCTEXT("PartyMemberPromotedBody", "{0} now the party leader"), LeaderSubjectText));
				*/
		}

		if (OnPartyChanged.IsBound())
		{
			OnPartyChanged.Broadcast();
		}

		bIsLocalPlayerPartyLeader = false; // MemberId == GetOwningPlayer<UUTLocalPlayer>().GetGameAccountId();
		if (bIsLocalPlayerPartyLeader)
		{
		}
	}
}

void UPartyContext::OnPartyMemberLeft(UPartyGameState* PartyState, const FUniqueNetIdRepl& MemberId, EMemberExitedReason ExitReason)
{
	if (UUTPartyGameState* UTPartyState = Cast<UUTPartyGameState>(PartyState))
	{
		//TogglePartyChatTab();

		if (OnPartyChanged.IsBound())
		{
			OnPartyChanged.Broadcast();
		}

		if (NumPartyMembers() <= 1)
		{
			OnPartyStateChanged.Broadcast(EPartyMemberState::Solo);
		}
	}
}

void UPartyContext::OnPartyMemberLeaving(UPartyGameState* PartyState, const FUniqueNetIdRepl& MemberId, EMemberExitedReason ExitReason)
{
	if (UUTPartyGameState* UTPartyState = Cast<UUTPartyGameState>(PartyState))
	{
		if (UPartyMemberState* MemberState = PartyState->GetPartyMember(MemberId))
		{
			/*
			// If they're all alone, say so (otherwise say the new number of party members)
			const int32 NewPartySize = NumPartyMembers() - 1;
			FText MessageBody = NewPartySize > 1 ?
				FText::FormatNamed(PartySizeBody, TEXT("Current"), NumPartyMembers() - 1, TEXT("Max"), MaxPartyMembers()) :
				PartyLeftMessage;

			TNotification<ENotificationType::Information>::Display(
				GetWorld(), FText::FormatNamed(PartyMemberLeft, TEXT("Player"), MemberState->DisplayName), MessageBody);
				*/
		}
	}
}

int32 UPartyContext::NumPartyMembers() const
{
	TWeakObjectPtr<UPartyGameState> PartyState = GetActivePartyState();
	if (PartyState.IsValid())
	{
		return PartyState->GetPartySize();
	}

	return 0;
}

int32 UPartyContext::MaxPartyMembers() const
{
	TWeakObjectPtr<UPartyGameState> PartyState = GetActivePartyState();
	if (PartyState.IsValid())
	{
		return PartyState->GetPartyMaxSize();
	}

	return 0;
}

void UPartyContext::OnUTJoinPartyComplete(const FUniqueNetId& LocalUserId, EJoinPartyCompletionResult Result, int32 DeniedResultCode)
{
	// Display a notification if this didn't succeed
	if (Result != EJoinPartyCompletionResult::Succeeded)
	{

	}
}

void UPartyContext::OnFriendsJoinParty(const FUniqueNetId& SenderId, const TSharedRef<IOnlinePartyJoinInfo>& PartyJoinInfo, bool bIsFromInvite)
{
	if (auto Owner = GetOwningPlayer())
	{
		TSharedPtr<const FUniqueNetId> LocalUserId = Owner->GetPreferredUniqueNetId();

		if (LocalUserId.IsValid() && Party.IsValid())
		{
			TSharedPtr<const IOnlinePartyJoinInfo> UpdatedPartyJoinInfo;

			UWorld* World = GetWorld();
			IOnlinePartyPtr OnlineParty = Online::GetPartyInterface(World);
			if (OnlineParty.IsValid())
			{
				UpdatedPartyJoinInfo = OnlineParty->GetAdvertisedParty(*LocalUserId, SenderId, IOnlinePartySystem::GetPrimaryPartyTypeId());
			}

			if (!UpdatedPartyJoinInfo.IsValid() || !UpdatedPartyJoinInfo->CanJoin())
			{
				// Prefer the advertised party information, but get the social item invite data if necessary
				UpdatedPartyJoinInfo = PartyJoinInfo;
			}

			if (UpdatedPartyJoinInfo.IsValid())
			{
				CurrentTransitionState = EUTPartyTransition::Joining;
				Party->AddPendingPartyJoin(*LocalUserId, FPartyDetails(UpdatedPartyJoinInfo.ToSharedRef(), bIsFromInvite),
					UPartyDelegates::FOnJoinUPartyComplete::CreateUObject(this, &ThisClass::OnJoinPartyComplete));
				Party->ProcessPendingPartyJoin();
			}
			else
			{
				// invalid join information
				JoinPartyCompleteInternal(EJoinPartyCompletionResult::JoinInfoInvalid, 0);
			}
		}
		else
		{
			// No Valid user
			JoinPartyCompleteInternal(EJoinPartyCompletionResult::UnknownClientFailure, 0);
		}
	}
	else
	{
		// No valid local player
		JoinPartyCompleteInternal(EJoinPartyCompletionResult::UnknownClientFailure, 0);
	}
}

void UPartyContext::OnJoinPartyComplete(const FUniqueNetId& LocalUserId, EJoinPartyCompletionResult Result, int32 DeniedResultCode)
{
	if (Result != EJoinPartyCompletionResult::Succeeded)
	{
		CurrentTransitionState = EUTPartyTransition::Idle;
	}
	JoinPartyCompleteInternal(Result, DeniedResultCode);
}

void UPartyContext::JoinPartyCompleteInternal(EJoinPartyCompletionResult Result, int32 DeniedResultCode)
{
	LOG(LogBlueprintContext, Display, TEXT("UPartyContext::OnJoinPartyComplete - %s / %d"), ToString(Result), DeniedResultCode);

	if (auto PersonalPartyEntry = GetMyPersistentPartyEntry())
	{
	}

	if (Result != EJoinPartyCompletionResult::Succeeded)
	{
		//DisplayFailedToJoinNotification(Result, DeniedResultCode);
	}
}

UUTPartyMemberState* UPartyContext::GetMyPersistentPartyEntry() const
{
	TWeakObjectPtr<UPartyGameState> PartyState = GetActivePartyState();
	if (PartyState.IsValid())
	{
		return Cast<UUTPartyMemberState>(PartyState->GetPartyMember(GetOwningPlayer()->GetPreferredUniqueNetId()));
	}

	LOG(LogBlueprintContext, Warning, TEXT("You don't have a valid party state."));
	return nullptr;
}

void UPartyContext::HandlePartyDataChanged(const FPartyState& PartyData)
{
	OnPartyDataChanged.Broadcast(PartyData);
}

void UPartyContext::HandlePartyMemberStateChanged(const FUniqueNetIdRepl& UniqueId, const UPartyMemberState* PartyMember)
{

}

void UPartyContext::HandleLeaderFriendsOnlyChanged(bool bLeaderFriendsOnly)
{
	OnLeaderFriendsOnlyChanged.Broadcast(bLeaderFriendsOnly);
}

void UPartyContext::HandleLeaderInvitesOnlyChanged(bool bLeaderInviteOnly)
{
	OnLeaderInvitesOnlyChanged.Broadcast(bLeaderInviteOnly);
}

void UPartyContext::OnPresenceReceivedInternal(const FUniqueNetId& UserId, const TSharedRef<FOnlineUserPresence>& NewPresence)
{
}

void UPartyContext::HandleFriendsActionNotification(TSharedRef<FFriendsAndChatMessage> FriendsNotification)
{
}

void UPartyContext::OnPartyInvitesChangedInternal(const FUniqueNetId& LocalUserId)
{
}

void UPartyContext::HandlePartyInviteResponseReceived(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& SenderId,
	const EInvitationResponse Response)
{
	if (Response == EInvitationResponse::Rejected && UserInfoInterface.IsValid())
	{
	}
}

TWeakObjectPtr<UPartyGameState> UPartyContext::GetActivePartyState() const
{
	TWeakObjectPtr<UPartyGameState> PartyGameState = nullptr;
	if (PartyStack.Num())
	{
		PartyGameState = PartyStack.Top();
	}
	return PartyGameState;
}

#undef LOCTEXT_NAMESPACE
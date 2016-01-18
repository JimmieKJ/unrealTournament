// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "GameAndPartyService.h"

// Services
#include "OSSScheduler.h"
#include "ChatNotificationService.h"

// Models
#include "FriendPartyInviteItem.h"
#include "FriendGameInviteItem.h"
#include "FriendsService.h"

#include "FriendsAndChatAnalytics.h"

#define LOCTEXT_NAMESPACE ""

/**
* Game invite that needs to be processed before being displayed
*/
class FReceivedGameInvite
{
public:
	FReceivedGameInvite(
		const TSharedRef<const FUniqueNetId>& InFromId,
		const FOnlineSessionSearchResult& InInviteResult,
		const FString& InAppId)
		: FromId(InFromId)
		, InviteResult(new FOnlineSessionSearchResult(InInviteResult))
		, AppId(InAppId)
	{}
	// who sent the invite (could be non-friend)
	TSharedRef<const FUniqueNetId> FromId;
	// session info needed to join the invite
	TSharedRef<FOnlineSessionSearchResult> InviteResult;
	// Client id for user that sent the invite
	FString AppId;
	// equality check
	bool operator==(const FReceivedGameInvite& Other) const
	{
		return *Other.FromId == *FromId && Other.AppId == AppId;
	}
};

/**
* Party invite that needs to be processed before being displayed
*/
class FReceivedPartyInvite
{
public:
	FReceivedPartyInvite(
		const TSharedRef<const FUniqueNetId>& InFromId,
		const TSharedRef<IOnlinePartyJoinInfo>& InPartyJoinInfo)
		: FromId(InFromId)
		, PartyJoinInfo(InPartyJoinInfo)
	{}
	// who sent the invite (could be non-friend)
	TSharedRef<const FUniqueNetId> FromId;
	// info needed to join the party
	TSharedRef<IOnlinePartyJoinInfo> PartyJoinInfo;
	// equality check
	bool operator==(const FReceivedPartyInvite& Other) const
	{
		return *Other.FromId == *FromId || *Other.PartyJoinInfo->GetPartyId() == *PartyJoinInfo->GetPartyId();
	}
};

class FGameAndPartyServiceImpl:
	public FGameAndPartyService
{
public:

	virtual void Login() override
	{
		Logout();
		OnPresenceReceivedCompleteDelegate = FOnPresenceReceivedDelegate::CreateSP(this, &FGameAndPartyServiceImpl::OnPresenceReceived);
		OnQueryUserInfoCompleteDelegate = FOnQueryUserInfoCompleteDelegate::CreateSP(this, &FGameAndPartyServiceImpl::OnQueryUserInfoComplete);
		OnGameInviteReceivedDelegate = FOnSessionInviteReceivedDelegate::CreateSP(this, &FGameAndPartyServiceImpl::OnGameInviteReceived);
		OnPartyInviteReceivedDelegate = FOnPartyInviteReceivedDelegate::CreateSP(this, &FGameAndPartyServiceImpl::OnPartyInviteReceived);
		
		OnGameInviteResponseDelegate = IChatNotificationService::FOnNotificationResponseDelegate::CreateSP(this, &FGameAndPartyServiceImpl::OnHandleGameInviteResponse);
		OnPresenceReceivedCompleteDelegateHandle = OSSScheduler->GetPresenceInterface()->AddOnPresenceReceivedDelegate_Handle(OnPresenceReceivedCompleteDelegate);
		OnQueryUserInfoCompleteDelegateHandle = OSSScheduler->GetUserInterface()->AddOnQueryUserInfoCompleteDelegate_Handle(LocalControllerIndex, OnQueryUserInfoCompleteDelegate);
		OnGameInviteReceivedDelegateHandle = OSSScheduler->GetSessionInterface()->AddOnSessionInviteReceivedDelegate_Handle(OnGameInviteReceivedDelegate);
		OnPartyInviteReceivedDelegateHandle = OSSScheduler->GetPartyInterface()->AddOnPartyInviteReceivedDelegate_Handle(OnPartyInviteReceivedDelegate);

		OnRequestOSSAcceptedDelegate = FOSSScheduler::FOnRequestOSSAccepted::CreateSP(this, &FGameAndPartyServiceImpl::OnRequestOSSAccepted);


		if (UpdatePartyTickerDelegate.IsBound() == false)
		{
			UpdatePartyTickerDelegate = FTickerDelegate::CreateSP(this, &FGameAndPartyServiceImpl::Tick);
		}

		UpdatePartyTickerDelegateHandle = FTicker::GetCoreTicker().AddTicker(UpdatePartyTickerDelegate);
	}

	virtual void Logout() override
	{
		PendingGameInvitesList.Empty();
		if (OSSScheduler.IsValid())
		{
			if (OSSScheduler->GetUserInterface().IsValid())
			{
				OSSScheduler->GetUserInterface()->ClearOnQueryUserInfoCompleteDelegate_Handle(LocalControllerIndex, OnQueryUserInfoCompleteDelegateHandle);
			}

			if (OSSScheduler->GetSessionInterface().IsValid())
			{
				OSSScheduler->GetSessionInterface()->ClearOnSessionInviteReceivedDelegate_Handle(OnGameInviteReceivedDelegateHandle);
			}

			if (OSSScheduler->GetPartyInterface().IsValid())
			{
				OSSScheduler->GetPartyInterface()->ClearOnPartyInviteReceivedDelegate_Handle(OnPartyInviteReceivedDelegateHandle);
			}

			if (OSSScheduler->GetPresenceInterface().IsValid())
			{
				OSSScheduler->GetPresenceInterface()->ClearOnPresenceReceivedDelegate_Handle(OnPresenceReceivedCompleteDelegateHandle);
			}
			if (OSSScheduler->GetUserInterface().IsValid())
			{
				OSSScheduler->GetUserInterface()->ClearOnQueryUserInfoCompleteDelegate_Handle(LocalControllerIndex, OnQueryUserInfoCompleteDelegateHandle);
			}
		}

		if (UpdatePartyTickerDelegate.IsBound())
		{
			FTicker::GetCoreTicker().RemoveTicker(UpdatePartyTickerDelegateHandle);
		}
	}

private:

	void OnPresenceReceived(const FUniqueNetId& UserId, const TSharedRef<FOnlineUserPresence>& NewPresence)
	{
		// Check for out of date invites
		if (OSSScheduler.IsValid() && OSSScheduler->GetOnlineIdentity().IsValid())
		{
			TSharedPtr<const FUniqueNetId> PlayerUserId = OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(0);
			if (PlayerUserId.IsValid())
			{
				TSharedPtr<FOnlineUserPresence> CurrentPresence;
				TArray<FString> InvitesToRemove;
				OSSScheduler->GetPresenceInterface()->GetCachedPresence(*PlayerUserId, CurrentPresence);
				for (auto It = PendingGameInvitesList.CreateConstIterator(); It; ++It)
				{
					if(It.Value()->GetPartyJoinInfo().IsValid())
					{
						if(IsFriendInSameParty(It.Value()))
						{
							InvitesToRemove.Add(It.Key());
						}
					}
					else
					{
						FString CurrentSessionID = CurrentPresence->SessionId.IsValid() ? CurrentPresence->SessionId->ToString() : TEXT("");
						FString InviteSessionID = It.Value()->GetGameSessionId().IsValid() ? It.Value()->GetGameSessionId()->ToString() : TEXT("");
						if ((CurrentSessionID == InviteSessionID) || (GetOnlineStatus() != EOnlinePresenceState::Offline && !It.Value()->IsOnline()))
						{
							if (!CurrentSessionID.IsEmpty())
							{
								// Already in the same session so remove
								InvitesToRemove.Add(It.Key());
							}
						}
					}
				}
				for (auto It = InvitesToRemove.CreateConstIterator(); It; ++It)
				{
					TSharedPtr<IFriendItem>* Existing = PendingGameInvitesList.Find(*It);
					if (Existing != nullptr)
					{
						(*Existing)->SetPendingDelete();
						PendingGameInvitesList.Remove(*It);
					}
					OnGameInvitesUpdated().Broadcast();
				}
			}
		}
	}

	virtual int32 GetFilteredGameInviteList(TArray< TSharedPtr< IFriendItem > >& OutFriendsList) const override
	{
		for (auto It = PendingGameInvitesList.CreateConstIterator(); It; ++It)
		{
			OutFriendsList.Add(It.Value());
		}
		return OutFriendsList.Num();
	}

	virtual TSharedPtr<const FUniqueNetId> GetGameSessionId() const override
	{
		return OSSScheduler->GetGameSessionId();
	}

	virtual TSharedPtr<const FUniqueNetId> GetGameSessionId(FString SessionID) const override
	{
		return OSSScheduler->GetOnlineIdentity()->CreateUniquePlayerId(SessionID);
	}

	virtual bool IsInGameSession() const override
	{
		if (OSSScheduler.IsValid() &&
			OSSScheduler->GetOnlineIdentity().IsValid() &&
			OSSScheduler->GetSessionInterface().IsValid() &&
			OSSScheduler->GetSessionInterface()->GetNamedSession(GameSessionName) != nullptr)
		{
			return true;
		}
		return false;
	}

	virtual bool ShouldUseGameChatForPartyOutput() override
	{
		if (IsInGameChat() && ShouldCombineGameChatIntoPartyTab())
		{
			return true;
		}
		return false;
	}

	virtual void SetShouldCombineGameChatIntoPartyTab(bool bShouldCombine) override
	{
		bIsCombiningGameAndPartyChat = bShouldCombine;
	}

	virtual bool ShouldCombineGameChatIntoPartyTab() const override
	{
		return bIsCombiningGameAndPartyChat;
	}

	virtual bool IsInPartyChat() const override
	{
		return bPartyChatIsEnabled;
	}

	virtual void SetTeamChatEnabled(bool bEnable) override
	{
		bIsTeamChatEnabled = bEnable;
	}

	virtual bool IsTeamChatEnabled() const override
	{
		return bIsTeamChatEnabled;
	}

	virtual bool IsInTeamChat() const override
	{
		return bIsInTeamChat;
	}

	virtual bool IsLiveStreaming() const override
	{
		return bIsLiveStreaming;
	}

	virtual bool IsInGameChat() const override
	{
		return bGameChatIsEnabled;
	}

	virtual void EnablePartyChat(FChatRoomId ChatRoomID)
	{
		PartyChatRoomID = ChatRoomID;
		bool bPartyChatWasOff = bPartyChatIsEnabled == false;
		bPartyChatIsEnabled = true;
		if (bPartyChatWasOff)
		{
			// Broadcast out, eg. also bring the tab to front
			OnPartyChatChanged().Broadcast();
		}
	}

	virtual void DisablePartyChat()
	{
		PartyChatRoomID.Empty();
		bool bPartyChatWasOn = bPartyChatIsEnabled == true;
		bPartyChatIsEnabled = false;
		if (bPartyChatWasOn)
		{
			// Broadcast out, eg. change the selected view channel if it was this one
			OnPartyChatChanged().Broadcast();
		}
	}

	virtual void EnableGameChat()
	{
		bool bGameChatWasOff = bGameChatIsEnabled == false;
		bGameChatIsEnabled = true;
		if (bGameChatWasOff)
		{
			// Broadcast out, eg. also bring the tab to front
			OnGameChatChanged().Broadcast();
		}
	}

	virtual void DisableGameChat()
	{
		bool bGameChatWasOn = bGameChatIsEnabled == true;
		bGameChatIsEnabled = false;
		if (bGameChatWasOn)
		{
			// Broadcast out, eg. change the selected view channel if it was this one
			OnGameChatChanged().Broadcast();
		}
	}

	virtual void EnableLiveStreaming() override
	{
		bIsLiveStreaming = true;
	}
	
	virtual void DisableLiveStreaming() override
	{
		bIsLiveStreaming = false;
	}

	virtual void EnableTeamChat(FChatRoomId ChatRoomID) override
	{
		TeamChatRoomID = ChatRoomID;
		if(!bIsInTeamChat)
		{
			bIsInTeamChat = true;
			OnTeamChatChanged().Broadcast(true);
		}
	}

	virtual void DisableTeamChat() override
	{
		bIsInTeamChat = false;
		OnTeamChatChanged().Broadcast(false);
	}

	virtual bool IsLocalPlayerInActiveParty() const override
	{
		if (OSSScheduler.IsValid())
		{
			if (OSSScheduler->GetOnlineIdentity().IsValid())
			{
				TSharedPtr<const FUniqueNetId> LocalUserId = OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex);
				if (LocalUserId.IsValid())
				{
					if (OSSScheduler->GetPartyInterface().IsValid())
					{
						TSharedPtr<const FOnlinePartyId> PartyId = GetActivePartyId();
						if (PartyId.IsValid())
						{
							TSharedPtr<const FOnlineParty> Party = OSSScheduler->GetPartyInterface()->GetParty(*LocalUserId, *PartyId);
							if (Party.IsValid())
							{
								return true;
							}
						}
					}
				}
			}
		}

		return false;
	}

	virtual bool IsLocalPlayerOnPCPlatform() const override
	{
		if (OSSScheduler->GetPlatformAuthType() == "")
		{
			return true;
		}
		return false;
	}

	virtual bool IsFriendInSameSession(const TSharedPtr< const IFriendItem >& FriendItem) const override
	{
		return OSSScheduler->IsFriendInSameSession(FriendItem);
	}

	virtual bool IsFriendInSameParty(const TSharedPtr< const IFriendItem >& FriendItem) const override
	{
		return OSSScheduler->IsFriendInSameParty(FriendItem);
	}

	virtual TSharedPtr<IOnlinePartyJoinInfo> GetPartyJoinInfo(const TSharedPtr<const IFriendItem>& FriendItem) const override
	{
		return OSSScheduler->GetPartyJoinInfo(FriendItem);
	}

	virtual TSharedPtr<const FOnlinePartyId> GetActivePartyId() const override
	{
		TSharedPtr<const FOnlinePartyId> ActivePartyId = nullptr;

		if (OSSScheduler.IsValid() &&
			OSSScheduler->GetOnlineIdentity().IsValid() &&
			OSSScheduler->GetPartyInterface().IsValid())
		{
			TSharedPtr<const FUniqueNetId> LocalUserId = OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex);
			if (LocalUserId.IsValid())
			{
				IOnlinePartyPtr PartyInt = OSSScheduler->GetPartyInterface();
				if (PartyInt.IsValid())
				{
					TSharedPtr<const FOnlineParty> Party = PartyInt->GetParty(*LocalUserId, IOnlinePartySystem::GetPrimaryPartyTypeId());
					if (Party.IsValid())
					{
						ActivePartyId = Party->PartyId;
					}
				}
			}
		}

		return ActivePartyId;
	}

	virtual bool CanInviteToParty() const override
	{
		bool bResult = false;

		if (OSSScheduler.IsValid() &&
			OSSScheduler->GetOnlineIdentity().IsValid() &&
			OSSScheduler->GetPartyInterface().IsValid())
		{
			TSharedPtr<const FUniqueNetId> LocalUserId = OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex);
			if (LocalUserId.IsValid())
			{
				IOnlinePartyPtr PartyInt = OSSScheduler->GetPartyInterface();
				if (PartyInt.IsValid())
				{
					TSharedPtr<const FOnlinePartyId> PartyId = GetActivePartyId();
					TSharedPtr<const FOnlineParty> Party = PartyInt->GetParty(*LocalUserId, *PartyId);
					if (Party.IsValid())
					{
						bResult = Party->CanLocalUserInvite(*LocalUserId);
					}
				}
			}
		}

		return bResult;
	}

	virtual bool IsInJoinableParty() const override
	{
		bool bIsJoinable = false;

		if (OSSScheduler.IsValid() &&
			OSSScheduler->GetOnlineIdentity().IsValid() &&
			OSSScheduler->GetPartyInterface().IsValid())
		{
			TSharedPtr<const FUniqueNetId> LocalUserId = OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex);
			if (LocalUserId.IsValid())
			{
				IOnlinePartyPtr PartyInt = OSSScheduler->GetPartyInterface();
				if (PartyInt.IsValid())
				{
					TSharedPtr<const FOnlinePartyId> PartyId = GetActivePartyId();
					if (PartyId.IsValid())
					{
						TSharedPtr<const FOnlineParty> Party = PartyInt->GetParty(*LocalUserId, *PartyId);
						if (Party.IsValid())
						{
							bIsJoinable = Party->IsJoinable();
						}
					}
				}
			}
		}

		return bIsJoinable;
	}

	virtual bool IsInJoinableGameSession() const override
	{
		bool bIsJoinable = false;

		if (OSSScheduler.IsValid())
		{
			IOnlineSessionPtr SessionInt = OSSScheduler->GetSessionInterface();
			if (SessionInt.IsValid())
			{
				FNamedOnlineSession* Session = SessionInt->GetNamedSession(GameSessionName);
				if (Session)
				{
					bool bPublicJoinable = false;
					bool bFriendJoinable = false;
					bool bInviteOnly = false;
					bool bAllowInvites = false;
					if (Session->GetJoinability(bPublicJoinable, bFriendJoinable, bInviteOnly, bAllowInvites))
					{
						// User's game is joinable in some way if any of this is true (context needs to be handled outside this function)
						bIsJoinable = bPublicJoinable || bFriendJoinable || bInviteOnly;
					}
				}
			}
		}

		return bIsJoinable;
	}

	virtual bool JoinGameAllowed(const FString& AppId) override
	{
		bool bJoinGameAllowed = true;

		if (bIsInGame)
		{
			bJoinGameAllowed = true;
		}
		else
		{
			TSharedPtr<IFriendsApplicationViewModel>* FriendsApplicationViewModel = ApplicationViewModels.Find(AppId);
			if (FriendsApplicationViewModel != nullptr &&
				(*FriendsApplicationViewModel).IsValid())
			{
				bJoinGameAllowed = (*FriendsApplicationViewModel)->IsAppJoinable();
			}
		}

		return bJoinGameAllowed;
	}

	virtual bool IsInLauncher() const override
	{
		return !bIsInGame;
	}

	virtual void NotifyJoinPartyFail() override
	{
		static const FText JoinFailedMessage = LOCTEXT("JoinPartyFailedMessage", "Join Party Failed");
		NotificationService->SendNotification(JoinFailedMessage.ToString(), EMessageType::JoinGame);
	}

	EOnlinePresenceState::Type GetOnlineStatus()
	{
		return OSSScheduler->GetOnlineStatus();
	}

	virtual FString GetUserAppId() const override
	{
		return OSSScheduler->GetUserAppId();
	}

	virtual FChatRoomId GetPartyChatRoomId() const override
	{
		return PartyChatRoomID;
	}

	virtual FChatRoomId GetTeamChatRoomId() const override
	{
		return TeamChatRoomID;
	}

	virtual void RejectGameInvite(const TSharedPtr<IFriendItem>& FriendItem) override
	{
		TSharedPtr<IFriendItem>* Existing = PendingGameInvitesList.Find(FriendItem->GetUniqueID()->ToString());
		if (Existing != nullptr)
		{
			(*Existing)->SetPendingDelete();
			PendingGameInvitesList.Remove(FriendItem->GetUniqueID()->ToString());
		}
		// update game invite UI
		OnGameInvitesUpdated().Broadcast();

		// reject party invite
		TSharedPtr<IOnlinePartyJoinInfo> PartyJoinInfo = FriendItem->GetPartyJoinInfo();
		if (PartyJoinInfo.IsValid())
		{
			TSharedPtr<const FUniqueNetId> UserId = OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex);
			if (UserId.IsValid())
			{
				OSSScheduler->GetPartyInterface()->RejectInvitation(*UserId, *(FriendItem->GetUniqueID()));
			}
		}

		if (OSSScheduler->GetOnlineIdentity().IsValid() &&
			OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex).IsValid())
		{
			OSSScheduler->GetAnalytics()->FireEvent_RejectGameInvite(*OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex), *FriendItem->GetUniqueID());
		}
		NotificationService->OnNotificationsAvailable().Broadcast(false);
	}

	virtual void AcceptGameInvite(const TSharedPtr<IFriendItem>& FriendItem) override
	{
		TSharedPtr<IFriendItem>* Existing = PendingGameInvitesList.Find(FriendItem->GetUniqueID()->ToString());
		if (Existing != nullptr)
		{
			(*Existing)->SetPendingDelete();
			PendingGameInvitesList.Remove(FriendItem->GetUniqueID()->ToString());
		}
		// update game invite UI
		OnGameInvitesUpdated().Broadcast();

		FString AdditionalCommandline;

		TSharedPtr<IOnlinePartyJoinInfo> PartyJoinInfo = FriendItem->GetPartyJoinInfo();
		if (PartyJoinInfo.IsValid())
		{
			// notify for further processing of join party request 
			OnFriendsJoinParty().Broadcast(*FriendItem->GetUniqueID(), PartyJoinInfo.ToSharedRef(), true);

			AdditionalCommandline = OSSScheduler->GetPartyInterface()->MakeTokenFromJoinInfo(*PartyJoinInfo);
		}
		else if(FriendItem.IsValid() && FriendItem->GetGameSessionId().IsValid())
		{
			AdditionalCommandline =
				TEXT("-invitesession=") + FriendItem->GetGameSessionId()->ToString() +
				TEXT(" -invitefrom=") + FriendItem->GetUniqueID()->ToString();

			NotificationService->OnJoinGame().Broadcast(*FriendItem->GetUniqueID(), *FriendItem->GetGameSessionId());
		}

		// launch game with invite cmd line parameters
		TSharedPtr<IFriendsApplicationViewModel>* FriendsApplicationViewModel = ApplicationViewModels.Find(FriendItem->GetAppId());
		if (FriendsApplicationViewModel != nullptr &&
			(*FriendsApplicationViewModel).IsValid())
		{
			(*FriendsApplicationViewModel)->LaunchFriendApp(AdditionalCommandline);
		}

		if (OSSScheduler->GetOnlineIdentity().IsValid() &&
			OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex).IsValid())
		{
			OSSScheduler->GetAnalytics()->FireEvent_AcceptGameInvite(*OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex), *FriendItem->GetUniqueID());
		}
		NotificationService->OnNotificationsAvailable().Broadcast(false);
	}

	virtual void SendGameInvite(const TSharedPtr<IFriendItem>& FriendItem) override
	{
		SendGameInvite(*FriendItem->GetUniqueID());
	}

	virtual bool SendPartyInvite(const FUniqueNetId& ToUser) override
	{
		bool bResult = false;

		// In a joinable party implies in a party at all 
		if (IsInJoinableParty() && CanInviteToParty())
		{
			IOnlineIdentityPtr IdentityInt = OSSScheduler->GetOnlineIdentity();
			if (IdentityInt.IsValid())
			{
				TSharedPtr<const FUniqueNetId> UserId = IdentityInt->GetUniquePlayerId(LocalControllerIndex);
				if (UserId.IsValid())
				{
					IOnlinePartyPtr PartyInt = OSSScheduler->GetPartyInterface();
					// party invite if available
					if (PartyInt.IsValid())
					{
						TSharedPtr<const FOnlinePartyId> PartyId = GetActivePartyId();
						if (PartyId.IsValid())
						{
							bResult = PartyInt->SendInvitation(*UserId, *PartyId, ToUser);
						}
					}
				}
			}
		}

		return bResult;
	}

	virtual void SendGameInvite(const FUniqueNetId& ToUser) override
	{
		if (OSSScheduler.IsValid() &&
			OSSScheduler->GetOnlineIdentity().IsValid())
		{
			TSharedPtr<const FUniqueNetId> UserId = OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(0);
			if (UserId.IsValid())
			{
				// party invite if available
				if (!SendPartyInvite(ToUser))
				{
					// just a game session invite
					if (OSSScheduler->GetSessionInterface().IsValid() &&
						OSSScheduler->GetSessionInterface()->GetNamedSession(GameSessionName) != nullptr)
					{
						OSSScheduler->GetSessionInterface()->SendSessionInviteToFriend(*UserId, GameSessionName, ToUser);
					}
				}

				OSSScheduler->GetAnalytics()->FireEvent_SendGameInvite(*OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex), ToUser);
			}
		}
	}

	virtual void AddApplicationViewModel(const FString AppId, TSharedPtr<IFriendsApplicationViewModel> InApplicationViewModel) override
	{
		ApplicationViewModels.Add(AppId, InApplicationViewModel);
	}

	virtual void ClearApplicationViewModels() override
	{
		ApplicationViewModels.Empty();
	}

	void OnQueryUserInfoComplete(int32 LocalPlayer, bool bWasSuccessful, const TArray< TSharedRef<const FUniqueNetId> >& UserIds, const FString& ErrorStr)
	{
		if (bHasOSS)
		{
			ProcessReceivedGameInvites();
			ReceivedGameInvites.Empty();
			ReceivedPartyInvites.Empty();
			bHasOSS = false;
			OSSScheduler->ReleaseOSS();
		}
	}

	void RequestOSS()
	{
		if (ReceivedGameInvites.Num() == 1 || ReceivedPartyInvites.Num() == 1)
		{
			OSSScheduler->RequestOSS(OnRequestOSSAcceptedDelegate);
		}
	}

	void OnRequestOSSAccepted()
	{
		bHasOSS = true;
		ProcessReceivedGameInvites();
		if (!RequestGameInviteUserInfo())
		{
			bHasOSS = false;
			OSSScheduler->ReleaseOSS();
		}
	}

	bool RequestGameInviteUserInfo()
	{
		bool bPending = false;

		// query for user ids that are not already cached from game invites
		IOnlineUserPtr UserInterface = OSSScheduler->GetUserInterface();
		IOnlineIdentityPtr IdentityInterface = OSSScheduler->GetOnlineIdentity();
		if (UserInterface.IsValid() &&
			IdentityInterface.IsValid())
		{
			TArray<TSharedRef<const FUniqueNetId>> GameInviteUserIds;
			for (auto GameInvite : ReceivedGameInvites)
			{
				GameInviteUserIds.Add(GameInvite.FromId);
			}
			for (auto PartyInvite : ReceivedPartyInvites)
			{
				GameInviteUserIds.Add(PartyInvite.FromId);
			}
			if (GameInviteUserIds.Num() > 0)
			{
				UserInterface->QueryUserInfo(LocalControllerIndex, GameInviteUserIds);
				bPending = true;
			}
		}

		return bPending;
	}

	void OnGameInviteReceived(const FUniqueNetId& UserId, const FUniqueNetId& FromId, const FString& AppId, const FOnlineSessionSearchResult& InviteResult)
	{
		if (OSSScheduler->GetOnlineIdentity().IsValid())
		{
			TSharedPtr<const FUniqueNetId> FromIdPtr = OSSScheduler->GetOnlineIdentity()->CreateUniquePlayerId(FromId.ToString());

			// Check we have entitlement for this game
			TSharedPtr<IFriendsApplicationViewModel>* FriendsApplicationViewModel = ApplicationViewModels.Find(AppId);
			if (AppId == GetUserAppId() ||
				(FriendsApplicationViewModel != nullptr && (*FriendsApplicationViewModel).IsValid() && (*FriendsApplicationViewModel)->IsAppEntitlementGranted()))
			{
				if (FromIdPtr.IsValid())
				{
					ReceivedGameInvites.AddUnique(FReceivedGameInvite(FromIdPtr.ToSharedRef(), InviteResult, AppId));
					RequestOSS();
				}
			}
		}
	}

	void OnPartyInviteReceived(const FUniqueNetId& RecipientId, const FOnlinePartyId& PartyId, const FUniqueNetId& SenderId)
	{
		if (OSSScheduler->GetPartyInterface().IsValid())
		{
			TArray<TSharedRef<IOnlinePartyJoinInfo>> PartyInvites;
			if (OSSScheduler->GetPartyInterface()->GetPendingInvites(RecipientId, PartyInvites))
			{
				for (auto PartyInvite : PartyInvites)
				{
					TSharedPtr<IFriendsApplicationViewModel>* FriendsApplicationViewModel = ApplicationViewModels.Find(PartyInvite->GetAppId());

					const bool bAppIdsMatch = PartyInvite->GetAppId() == GetUserAppId();
					const bool bAppEntitlementsGranted = (FriendsApplicationViewModel != nullptr && (*FriendsApplicationViewModel).IsValid() && (*FriendsApplicationViewModel)->IsAppEntitlementGranted());
					const bool bPersistentPartyType = PartyInvite->GetPartyTypeId() == IOnlinePartySystem::GetPrimaryPartyTypeId();
					if ((bAppIdsMatch || bAppEntitlementsGranted) && bPersistentPartyType)
					{
						if (*PartyInvite->GetSourceUserId() == SenderId)
						{
							TSharedPtr<const FUniqueNetId> SenderIdPtr = OSSScheduler->GetOnlineIdentity()->CreateUniquePlayerId(SenderId.ToString());
							ReceivedPartyInvites.AddUnique(FReceivedPartyInvite(SenderIdPtr.ToSharedRef(), PartyInvite));
							RequestOSS();
							break;
						}
					}
				}
			}
		}
	}

	void ProcessReceivedGameInvites()
	{
		if (OSSScheduler->GetUserInterface().IsValid())
		{
			// list of newly added game invites
			TArray<TSharedPtr<IFriendItem>> UpdatedGameInvites;
			// received game invites waiting to be processed
			for (int32 Idx = 0; Idx < ReceivedGameInvites.Num(); Idx++)
			{
				const FReceivedGameInvite& Invite = ReceivedGameInvites[Idx];

				TSharedPtr<const FUniqueNetId> MySessionId = GetGameSessionId();
				bool bMySessionValid = MySessionId.IsValid() && MySessionId->IsValid();

				if (!Invite.InviteResult->Session.SessionInfo.IsValid() ||
					(bMySessionValid && Invite.InviteResult->Session.SessionInfo->GetSessionId().ToString() == MySessionId->ToString()))
				{
					// remove invites if user is already in the game session
					ReceivedGameInvites.RemoveAt(Idx--);
				}
				else
				{
					// add to list of pending invites to accept
					TSharedPtr<FOnlineUser> UserInfo;
					TSharedPtr<IFriendItem> Friend = FriendsService->FindUser(*Invite.FromId);
					TSharedPtr<FOnlineFriend> OnlineFriend;
					if (Friend.IsValid())
					{
						UserInfo = Friend->GetOnlineUser();
						OnlineFriend = Friend->GetOnlineFriend();
					}
					if (!UserInfo.IsValid())
					{
						UserInfo = OSSScheduler->GetUserInterface()->GetUserInfo(LocalControllerIndex, *Invite.FromId);
					}
					if (UserInfo.IsValid() && OnlineFriend.IsValid())
					{
						TSharedPtr<FFriendGameInviteItem> GameInvite = MakeShareable(
							new FFriendGameInviteItem(UserInfo.ToSharedRef(), Invite.InviteResult, Invite.AppId, OnlineFriend.ToSharedRef(), SharedThis(this))
							);
						PendingGameInvitesList.Add(Invite.FromId->ToString(), GameInvite);
						UpdatedGameInvites.Add(GameInvite);
						ReceivedGameInvites.RemoveAt(Idx--);
					}
				}
			}

			// received party invites waiting to be processed
			for (int32 Idx = 0; Idx < ReceivedPartyInvites.Num(); Idx++)
			{
				const FReceivedPartyInvite& Invite = ReceivedPartyInvites[Idx];
				// user info should already be queried by now
				TSharedPtr<FOnlineUser> UserInfo;
				UserInfo = OSSScheduler->GetUserInterface()->GetUserInfo(LocalControllerIndex, *Invite.FromId);
				if (UserInfo.IsValid())
				{
					TSharedPtr<FFriendPartyInviteItem> PartyInvite = MakeShareable(
						new FFriendPartyInviteItem(UserInfo.ToSharedRef(), Invite.PartyJoinInfo->GetAppId(), Invite.PartyJoinInfo, SharedThis(this))
						);
					PendingGameInvitesList.Add(Invite.FromId->ToString(), PartyInvite);
					UpdatedGameInvites.Add(PartyInvite);
					ReceivedPartyInvites.RemoveAt(Idx--);

					if (!AutoAcceptPartyId.IsEmpty() && (AutoAcceptPartyId == PartyInvite->GetPartyJoinInfo()->GetPartyId()->ToString()))
					{
						AutoAcceptPartyId.Empty();
						AcceptGameInvite(PartyInvite);
					}
				}
			}

			// notifications for any newly added invtes
			if (UpdatedGameInvites.Num() > 0)
			{
				OnGameInvitesUpdated().Broadcast();
			}
			for (auto NewInvite : UpdatedGameInvites)
			{
				NotificationService->SendGameInviteNotification(NewInvite, OnGameInviteResponseDelegate);
			}
		}
	}

	void OnHandleGameInviteResponse(TSharedPtr< FFriendsAndChatMessage > MessageNotification, EResponseType::Type ResponseType)
	{
		TSharedPtr< IFriendItem > User = FriendsService->FindUser(MessageNotification->GetUniqueID().Get());
		if (User.IsValid())
		{
			if (ResponseType == EResponseType::Response_Accept)
			{
				AcceptGameInvite(User);
			}
			else
			{
				RejectGameInvite(User);
			}
		}
	}

	bool Tick(float Delta)
	{
		ProcessCommandLineInvites();
		return true;
	}

	void ProcessCommandLineInvites()
	{
		if (OSSScheduler->GetPartyInterface().IsValid())
		{
			TSharedPtr<IOnlinePartyJoinInfo> AutoAcceptJoinInfo = OSSScheduler->GetPartyInterface()->ConsumePendingCommandLineInvite();
			if (AutoAcceptJoinInfo.IsValid())
			{
				ReceivedPartyInvites.Add(FReceivedPartyInvite(AutoAcceptJoinInfo->GetLeaderId(), AutoAcceptJoinInfo.ToSharedRef()));
				AutoAcceptPartyId = AutoAcceptJoinInfo->GetPartyId()->ToString();
				RequestOSS();
			}
		}
	}

	// External events
	DECLARE_DERIVED_EVENT(FGameAndPartyService, FGameAndPartyService::FOnGameInvitesUpdated, FOnGameInvitesUpdated)
	virtual FOnGameInvitesUpdated& OnGameInvitesUpdated()
	{
		return OnGameInvitesUpdatedDelegate;
	}

	DECLARE_DERIVED_EVENT(FGameAndPartyService, IGameAndPartyService::FOnFriendsJoinPartyEvent, FOnFriendsJoinPartyEvent)
	virtual FOnFriendsJoinPartyEvent& OnFriendsJoinParty() override
	{
		return FriendsJoinPartyEvent;
	}

	DECLARE_DERIVED_EVENT(FGameAndPartyService, FGameAndPartyService::FOnPartyChatChangedEvent, FOnPartyChatChangedEvent)
	virtual FOnPartyChatChangedEvent& OnPartyChatChanged() override
	{
		return PartyChatChangedEvent;
	}

	DECLARE_DERIVED_EVENT(FGameAndPartyService, FGameAndPartyService::FOnGameChatChangedEvent, FOnGameChatChangedEvent)
	virtual FOnGameChatChangedEvent& OnGameChatChanged() override
	{
		return GameChatChangedEvent;
	}

	DECLARE_DERIVED_EVENT(FGameAndPartyService, FGameAndPartyService::FOnTeamChatChangedEvent, FOnTeamChatChangedEvent)
	virtual FOnTeamChatChangedEvent& OnTeamChatChanged() override
	{
		return TeamChatChangedEvent;
	}

private:

	FGameAndPartyServiceImpl(const TSharedRef<FOSSScheduler>& InOSSScheduler, const TSharedRef<FFriendsService>& InFriendsService, const TSharedRef< FChatNotificationService>& InNotificationService, bool bInIsInGame)
		: OSSScheduler(InOSSScheduler)
		, FriendsService(InFriendsService)
		, NotificationService(InNotificationService)
		, bIsInGame(bInIsInGame)
		, bGameChatIsEnabled(false)
		, bPartyChatIsEnabled(false)
		, bIsTeamChatEnabled(false)
		, bIsInTeamChat(false)
		, bIsLiveStreaming(false)
		, bIsCombiningGameAndPartyChat(false)
		, bShouldPreferGameOutputForPartyChat(false)
		, LocalControllerIndex(0)
		, bHasOSS(false)
	{
	}

	// Holds the list of incoming game invites that need to be responded to
	TMap< FString, TSharedPtr< IFriendItem > > PendingGameInvitesList;
	// List of game invites that need to be processed
	TArray<FReceivedGameInvite> ReceivedGameInvites;
	// List of party invites that need to be processed
	TArray<FReceivedPartyInvite> ReceivedPartyInvites;
	/** Party that may have been passed on command line to auto-join */
	FString AutoAcceptPartyId;
	// Holds the application view models - used for launching and querying the games
	TMap<FString, TSharedPtr<IFriendsApplicationViewModel>> ApplicationViewModels;
	

	// Holds the party chat changed delegate
	FOnPartyChatChangedEvent PartyChatChangedEvent;
	// Holds the game chat changed delegate
	FOnGameChatChangedEvent GameChatChangedEvent;
	// Holds the team chat changed delegate
	FOnTeamChatChangedEvent TeamChatChangedEvent;

	// Holds the join party request delegate
	FOnFriendsJoinPartyEvent FriendsJoinPartyEvent;
	// Delegate for a game invite received
	FOnSessionInviteReceivedDelegate OnGameInviteReceivedDelegate;
	// Delegate for a party invite received
	FOnPartyInviteReceivedDelegate OnPartyInviteReceivedDelegate;
	// Delegate for game invites updating
	FOnGameInvitesUpdated OnGameInvitesUpdatedDelegate;
	// Delegate for querying user id from a name string
	IOnlineUser::FOnQueryUserMappingComplete OnQueryUserIdMappingCompleteDelegate;
	// Delegate to use for querying user info list
	FOnQueryUserInfoCompleteDelegate OnQueryUserInfoCompleteDelegate;
	// Delegate for Game Invite Notification responses
	IChatNotificationService::FOnNotificationResponseDelegate OnGameInviteResponseDelegate;
	// Delegate for the presence received
	FOnPresenceReceivedDelegate OnPresenceReceivedCompleteDelegate;
	// Delegate for when its our turn for OSS
	FOSSScheduler::FOnRequestOSSAccepted OnRequestOSSAcceptedDelegate;

	/** Handle to various registered delegates */
	FDelegateHandle OnGameInviteReceivedDelegateHandle;
	FDelegateHandle OnPartyChatChangedDelegateHandle;
	FDelegateHandle OnGameChatChangedDelegateHandle;
	FDelegateHandle OnPartyInviteReceivedDelegateHandle;
	FDelegateHandle OnPartyMemberJoinedDelegateHandle;
	FDelegateHandle OnPartyMemberExitedDelegateHandle;
	FDelegateHandle OnPresenceReceivedCompleteDelegateHandle;
	FDelegateHandle OnQueryUserInfoCompleteDelegateHandle;

	// Holds the OSS Scheduler
	TSharedPtr<FOSSScheduler> OSSScheduler;
	// Holds the Friends Service
	TSharedRef<FFriendsService> FriendsService;
	// Manages notification services
	TSharedPtr<class FChatNotificationService> NotificationService;
	// Lets us know if we are in game for invites / join game sessions
	bool bIsInGame;
	// Lets us know whether to allow game chat over the lobby/server (using party tab if combining)
	bool bGameChatIsEnabled;
	// Party ChatRoom ID
	FChatRoomId PartyChatRoomID;
	// Lets us know whether to show party tab & allow party chat over xmpp
	bool bPartyChatIsEnabled;
	// Team ChatRoom ID
	FChatRoomId TeamChatRoomID;
	// Lets us know whether to team chat is enabled
	bool bIsTeamChatEnabled;
	// Lets us know whether we are in team chat
	bool bIsInTeamChat;
	// Lets us know whether to show live streaming tab
	bool bIsLiveStreaming;
	// Direct game chat through the party chat tab
	bool bIsCombiningGameAndPartyChat;
	// Whether to use game server output whenever possible when sending party chat messages
	bool bShouldPreferGameOutputForPartyChat;
	// Controller index
	int32 LocalControllerIndex;

	bool bHasOSS;

	// Holds the ticker delegate
	FTickerDelegate UpdatePartyTickerDelegate;
	FDelegateHandle UpdatePartyTickerDelegateHandle;

	friend FGameAndPartyServiceFactory;
};

TSharedRef< FGameAndPartyService > FGameAndPartyServiceFactory::Create(const TSharedRef<FOSSScheduler>& OSSScheduler, const TSharedRef<FFriendsService>& FriendsService, const TSharedRef<FChatNotificationService>& NotificationService, bool bIsInGame)
{
	TSharedRef< FGameAndPartyServiceImpl > GamePartyService(new FGameAndPartyServiceImpl(OSSScheduler, FriendsService, NotificationService, bIsInGame));
	return GamePartyService;
}

#undef LOCTEXT_NAMESPACE
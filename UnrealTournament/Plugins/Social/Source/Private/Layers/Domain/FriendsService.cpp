// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "FriendsService.h"

#include "OSSScheduler.h"
#include "FriendRecentPlayerItems.h"
#include "FriendBlockedPlayerItem.h"
#include "ChatNotificationService.h"
#include "FriendsAndChatAnalytics.h"
#include "FriendFriendItem.h"
#include "WebImageService.h"
#include "FriendImportPlayerItem.h"

// Enum holding the state of the Friends manager
namespace EFriendsAndManagerState
{
	enum Type
	{
		Idle,								// Idle - can accept requests
		RequestingFriendsList,				// Requesting a list refresh
		RequestFriendsListRefresh,			// List request in progress
		RequestingRecentPlayersIDs,			// Requesting recent player ids
		RequestingBlockedPlayersIDs,		// Requesting blocked player ids
		RequestRecentPlayersListRefresh,	// Recent players request in progress
		RequestBlockedPlayersListRefresh,	// Blocked players request in progress
		ProcessFriendsList,					// Process the Friends List after a list refresh
		RequestingFriendName,				// Requesting a friend add
		RequestingImportableFriendNames,	// Requesting importable friend names
		DeletingFriends,					// Deleting a friend
		BlockingPlayer,						// Blocking a player
		UnblockingPlayer,					// Unblocking a player
		AcceptingFriendRequest,				// Accepting a friend request
		OffLine,							// No logged in
	};
};

#define LOCTEXT_NAMESPACE ""

class FFriendsServiceImpl :
	public FFriendsService
{
public:

	virtual void Login() override
	{
		// Clear existing delegates
		Logout();

		OnQueryRecentPlayersCompleteDelegate = FOnQueryRecentPlayersCompleteDelegate::CreateRaw(this, &FFriendsServiceImpl::OnQueryRecentPlayersComplete);
		OnQueryBlockedPlayersCompleteDelegate = FOnQueryBlockedPlayersCompleteDelegate::CreateRaw(this, &FFriendsServiceImpl::OnQueryBlockedPlayersComplete);
		OnFriendsListChangedDelegate = FOnFriendsChangeDelegate::CreateSP(this, &FFriendsServiceImpl::OnFriendsListChanged);
		OnDeleteFriendCompleteDelegate = FOnDeleteFriendCompleteDelegate::CreateSP(this, &FFriendsServiceImpl::OnDeleteFriendComplete);
		OnQueryUserIdMappingCompleteDelegate = IOnlineUser::FOnQueryUserMappingComplete::CreateSP(this, &FFriendsServiceImpl::OnQueryUserIdMappingComplete);
		OnQueryUserInfoCompleteDelegate = FOnQueryUserInfoCompleteDelegate::CreateSP(this, &FFriendsServiceImpl::OnQueryUserInfoComplete);
		OnFriendInviteReceivedDelegate = FOnInviteReceivedDelegate::CreateSP(this, &FFriendsServiceImpl::OnFriendInviteReceived);
		OnFriendRemovedDelegate = FOnFriendRemovedDelegate::CreateSP(this, &FFriendsServiceImpl::OnFriendRemoved);
		OnBlockedPlayerCompleteDelegate = FOnBlockedPlayerCompleteDelegate::CreateSP(this, &FFriendsServiceImpl::OnBlockedPlayerComplete);
		OnUnblockedPlayerCompleteDelegate = FOnUnblockedPlayerCompleteDelegate::CreateSP(this, &FFriendsServiceImpl::OnUnblockedPlayerComplete);
		OnFriendInviteRejectedDelegate = FOnInviteRejectedDelegate::CreateSP(this, &FFriendsServiceImpl::OnInviteRejected);
		OnFriendInviteAcceptedDelegate = FOnInviteAcceptedDelegate::CreateSP(this, &FFriendsServiceImpl::OnInviteAccepted);
		OnFriendInviteResponseDelegate = IChatNotificationService::FOnNotificationResponseDelegate::CreateSP(this, &FFriendsServiceImpl::OnHandleFriendInviteResponse);
		OnPresenceReceivedCompleteDelegate = FOnPresenceReceivedDelegate::CreateSP(this, &FFriendsServiceImpl::OnPresenceReceived);
		OnDestroySessionCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateSP(this, &FFriendsServiceImpl::OnGameDestroyed);
		OnPartyMemberExitedDelegate = FOnPartyMemberExitedDelegate::CreateSP(this, &FFriendsServiceImpl::OnPartyMemberExited);
		OnQueryExternalIdMappingsCompleteDelegate = IOnlineUser::FOnQueryExternalIdMappingsComplete::CreateSP(this, &FFriendsServiceImpl::OnQueryExternalIdMappingsComplete);

		OnRequestOSSAcceptedDelegate = FOSSScheduler::FOnRequestOSSAccepted::CreateSP(this, &FFriendsServiceImpl::OnRequestOSSAccepted);

		OnQueryRecentPlayersCompleteDelegateHandle = OSSScheduler->GetFriendsInterface()->AddOnQueryRecentPlayersCompleteDelegate_Handle(OnQueryRecentPlayersCompleteDelegate);
		OnBlockedPlayerListCompleteDelegateHandle = OSSScheduler->GetFriendsInterface()->AddOnQueryBlockedPlayersCompleteDelegate_Handle(OnQueryBlockedPlayersCompleteDelegate);
		OnFriendsListChangedDelegateHandle = OSSScheduler->GetFriendsInterface()->AddOnFriendsChangeDelegate_Handle(LocalControllerIndex, OnFriendsListChangedDelegate);
		OnFriendInviteReceivedDelegateHandle = OSSScheduler->GetFriendsInterface()->AddOnInviteReceivedDelegate_Handle(OnFriendInviteReceivedDelegate);
		OnFriendRemovedDelegateHandle = OSSScheduler->GetFriendsInterface()->AddOnFriendRemovedDelegate_Handle(OnFriendRemovedDelegate);
		OnPlayerBlockedCompleteDelegateHandle = OSSScheduler->GetFriendsInterface()->AddOnBlockedPlayerCompleteDelegate_Handle(LocalControllerIndex, OnBlockedPlayerCompleteDelegate);
		OnPlayerUnblockedCompleteDelegateHandle = OSSScheduler->GetFriendsInterface()->AddOnUnblockedPlayerCompleteDelegate_Handle(LocalControllerIndex, OnUnblockedPlayerCompleteDelegate);
		OnFriendInviteRejectedDelegateHandle = OSSScheduler->GetFriendsInterface()->AddOnInviteRejectedDelegate_Handle(OnFriendInviteRejectedDelegate);
		OnFriendInviteAcceptedDelegateHandle = OSSScheduler->GetFriendsInterface()->AddOnInviteAcceptedDelegate_Handle(OnFriendInviteAcceptedDelegate);
		OnDeleteFriendCompleteDelegateHandle = OSSScheduler->GetFriendsInterface()->AddOnDeleteFriendCompleteDelegate_Handle(LocalControllerIndex, OnDeleteFriendCompleteDelegate);
		OnQueryUserInfoCompleteDelegateHandle = OSSScheduler->GetUserInterface()->AddOnQueryUserInfoCompleteDelegate_Handle(LocalControllerIndex, OnQueryUserInfoCompleteDelegate);
		OnPresenceReceivedCompleteDelegateHandle = OSSScheduler->GetPresenceInterface()->AddOnPresenceReceivedDelegate_Handle(OnPresenceReceivedCompleteDelegate);
		OnDestroySessionCompleteDelegateHandle = OSSScheduler->GetSessionInterface()->AddOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegate);
		OnPartyMemberExitedDelegateHandle = OSSScheduler->GetPartyInterface()->AddOnPartyMemberExitedDelegate_Handle(OnPartyMemberExitedDelegate);
	
		FriendsList.Empty();
		PendingFriendsList.Empty();

		ManagerState = EFriendsAndManagerState::Idle;
		RequestListRefresh();
		RequestRecentPlayersListRefresh();
		RequestBlockedPlayersListRefresh();
	}

	virtual void Logout() override
	{
		if (OSSScheduler.IsValid())
		{
			IOnlineFriendsPtr OnlineFriendsInterface = OSSScheduler->GetFriendsInterface();
			if (OnlineFriendsInterface.IsValid())
			{
				OnlineFriendsInterface->ClearOnQueryRecentPlayersCompleteDelegate_Handle(OnQueryRecentPlayersCompleteDelegateHandle);
				OnlineFriendsInterface->ClearOnFriendsChangeDelegate_Handle(LocalControllerIndex, OnFriendsListChangedDelegateHandle);
				OnlineFriendsInterface->ClearOnInviteReceivedDelegate_Handle(OnFriendInviteReceivedDelegateHandle);
				OnlineFriendsInterface->ClearOnFriendRemovedDelegate_Handle(OnFriendRemovedDelegateHandle);
				OnlineFriendsInterface->ClearOnInviteRejectedDelegate_Handle(OnFriendInviteRejectedDelegateHandle);
				OnlineFriendsInterface->ClearOnInviteAcceptedDelegate_Handle(OnFriendInviteAcceptedDelegateHandle);
				OnlineFriendsInterface->ClearOnDeleteFriendCompleteDelegate_Handle(LocalControllerIndex, OnDeleteFriendCompleteDelegateHandle);
				OnlineFriendsInterface->ClearOnBlockedPlayerCompleteDelegate_Handle(LocalControllerIndex, OnPlayerBlockedCompleteDelegateHandle);
				OnlineFriendsInterface->ClearOnUnblockedPlayerCompleteDelegate_Handle(LocalControllerIndex, OnPlayerUnblockedCompleteDelegateHandle);
				OnlineFriendsInterface->ClearOnQueryBlockedPlayersCompleteDelegate_Handle(OnBlockedPlayerListCompleteDelegateHandle);
			}
			if (OSSScheduler->GetPresenceInterface().IsValid())
			{
				OSSScheduler->GetPresenceInterface()->ClearOnPresenceReceivedDelegate_Handle(OnPresenceReceivedCompleteDelegateHandle);
			}
			if (OSSScheduler->GetUserInterface().IsValid())
			{
				OSSScheduler->GetUserInterface()->ClearOnQueryUserInfoCompleteDelegate_Handle(LocalControllerIndex, OnQueryUserInfoCompleteDelegateHandle);
			}
			if (OSSScheduler->GetSessionInterface().IsValid())
			{
				OSSScheduler->GetSessionInterface()->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegateHandle);
			}
			if (OSSScheduler->GetPartyInterface().IsValid())
			{
				OSSScheduler->GetPartyInterface()->ClearOnPartyMemberExitedDelegate_Handle(OnPartyMemberExitedDelegateHandle);
			}
		}
		FilteredFriendsList.Empty();
		PendingOutgoingDeleteFriendRequests.Empty();
		PendingOutgoingAcceptFriendRequests.Empty();
		bRequiresListRefresh = false;
	}

private:

	virtual void RequestFriendByName(const FText& FriendName) override
	{
		if (!FriendName.IsEmpty())
		{
			FriendByNameRequests.AddUnique(*FriendName.ToString());
			RequestFriendRequestRefresh();
		}
	}

	virtual void RequestFriendById(const TSharedRef<const FUniqueNetId>& FriendId, const FString& DisplayName) override
	{
		if (!OSSScheduler->GetUserInterface().IsValid())
		{
			UE_LOG(LogOnline, Warning, TEXT("Failed to request friend by online user because user interface is not valid"));
			return;
		}

		EFindFriendResult::Type FindFriendResult = EFindFriendResult::NotFound;

		TSharedPtr<const FUniqueNetId> LocalUserId = OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(0);
		TSharedPtr<IFriendItem> ExistingFriend = FindUser(FriendId);
		if (ExistingFriend.IsValid())
		{
			if (ExistingFriend->GetInviteStatus() == EInviteStatus::Accepted)
			{
				OnAddToast().Broadcast(FText::FromString("Already friends"));
				FindFriendResult = EFindFriendResult::AlreadyFriends;
			}
			else
			{
				OnAddToast().Broadcast(FText::FromString("Friend already requested"));

				FindFriendResult = EFindFriendResult::FriendsPending;
			}
		}
		else if (LocalUserId.IsValid() && FriendId == LocalUserId)
		{
			OnAddToast().Broadcast(FText::FromString("Can't friend yourself"));

			FindFriendResult = EFindFriendResult::AddingSelfFail;
		}
		else
		{
			FindFriendResult = EFindFriendResult::Success;
		}

		bool bRecentPlayer = false;
		if (FindFriendResult == EFindFriendResult::Success)
		{
			bRecentPlayer = FindRecentPlayer(*FriendId).IsValid();
		}

		if (OSSScheduler->GetOnlineIdentity().IsValid() &&
			OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex).IsValid())
		{
			OSSScheduler->GetAnalytics()->FireEvent_AddFriend(*OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex),
				DisplayName,
				*FriendId,
				FindFriendResult,
				bRecentPlayer);
		}

		FOnSendInviteComplete Delegate = FOnSendInviteComplete::CreateSP(this, &FFriendsServiceImpl::OnSendInviteByOnlineUserComplete);
		OSSScheduler->GetFriendsInterface()->SendInvite(LocalControllerIndex, *FriendId, EFriendsLists::ToString(EFriendsLists::Default), Delegate);
		OnAddToast().Broadcast(LOCTEXT("FriendRequestSent", "Request Sent"));

		NotificationService->SendFriendInviteSentNotification(DisplayName);
	}

	virtual void AddRecentPlayerNamespace(const FString& Namespace) override
	{
		RecentPlayersNamespaces.AddUnique(Namespace);
	}

	virtual int32 GetFilteredFriendsList(TArray< TSharedPtr< IFriendItem > >& OutFriendsList) override
	{
		OutFriendsList = FilteredFriendsList;
		return OutFriendsList.Num();
	}

	TSharedPtr<const FUniqueNetId > FindUserID(const FString& InUsername)
	{
		for (int32 Index = 0; Index < FriendsList.Num(); Index++)
		{
			if (FriendsList[Index]->GetOnlineUser()->GetDisplayName() == InUsername)
			{
				return FriendsList[Index]->GetUniqueID();
			}
		}
		return nullptr;
	}

	virtual TSharedPtr< IFriendItem > FindUser(const FUniqueNetId& InUserID) override
	{
		for (const auto& Friend : FriendsList)
		{
			if (Friend->GetUniqueID().Get() == InUserID)
			{
				return Friend;
			}
		}

		return nullptr;
	}

	virtual TSharedPtr<const FUniqueNetId> CreateUniquePlayerId(const FString InUserID) override
	{
		return OSSScheduler->GetOnlineIdentity()->CreateUniquePlayerId(InUserID);
	}

	virtual TSharedPtr< IFriendItem > FindUser(const TSharedRef<const FUniqueNetId>& InUserID) override
	{
		return FindUser(InUserID.Get());
	}

	virtual void QueryImportableFriends(IOnlineSubsystem* ExternalSubSystem)
	{
		if (ExternalSubSystem->GetFriendsInterface().IsValid() &&
			ExternalSubSystem->GetIdentityInterface().IsValid())
		{
			// Begin read now
			FOnReadFriendsListComplete OnReadFriendsListCompleteExternalDelegate = FOnReadFriendsListComplete::CreateSP(this, &FFriendsServiceImpl::OnReadFriendslistCompleteExternal, ExternalSubSystem);

			if (ExternalSubSystem->GetFriendsInterface()->ReadFriendsList(LocalControllerIndex, EFriendsLists::ToString(EFriendsLists::Default), OnReadFriendsListCompleteExternalDelegate))
			{
			}
			else
			{
				UE_LOG(LogOnline, Warning, TEXT("Unable to load importable friends: Read friends list call failed"));
			}
		}
		else
		{
			UE_LOG(LogOnline, Warning, TEXT("Unable to load importable friends: Online subsystem missing friends interface or identity interface"));
		}
	}

	void OnReadFriendslistCompleteExternal(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr, IOnlineSubsystem* ExternalSubSystem)
	{
		if (bWasSuccessful)
		{
			IOnlineUserPtr UserInterface = OSSScheduler->GetUserInterface();
			if (UserInterface.IsValid())
			{
				TSharedPtr<const FUniqueNetId> UserId = OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(0);
				if (UserId.IsValid())
				{
					TArray<TSharedRef<FOnlineFriend>> ExternalFriendsList;
					if (ExternalSubSystem->GetFriendsInterface()->GetFriendsList(LocalUserNum, ListName, ExternalFriendsList) &&
						ExternalFriendsList.Num() > 0)
					{
						TArray<FString> ExternalIds;
						ExternalIds.Reserve(ExternalFriendsList.Num());
						for (auto Friend : ExternalFriendsList)
						{
							FString ExternalId;
							if (Friend->GetUserAttribute(TEXT("id"), ExternalId))
							{
								ExternalIds.Add(ExternalId);
							}
						}

						if (!UserInterface->QueryExternalIdMappings(*UserId, ExternalSubSystem->GetIdentityInterface()->GetAuthType(), ExternalIds, OnQueryExternalIdMappingsCompleteDelegate))
						{
							UE_LOG(LogOnline, Warning, TEXT("Unable to load importable friends: Failed to query external id mappings"));
						}
					}
				}
			}
		}
		else
		{
			UE_LOG(LogOnline, Warning, TEXT("Unable to load importable friends: Read friends list failed with error %s"), *ErrorStr);
		}
	}

	void OnQueryExternalIdMappingsComplete(bool bWasSuccessful, const FUniqueNetId& LocalUserId, const FString& AuthType, const TArray<FString>& ExternalIds, const FString& Error)
	{
		if (bWasSuccessful)
		{
			// Query the mcp user info for the resulting ids
			auto& ImportableIds = QueryImportableUserIds.FindOrAdd(AuthType);
			ImportableIds.Reserve(ImportableIds.Num() + ExternalIds.Num());
			for (auto ExternalId : ExternalIds)
			{
				const TSharedPtr<const FUniqueNetId> UserId = OSSScheduler->GetUserInterface()->GetExternalIdMapping(AuthType, ExternalId);
				if (UserId.IsValid())
				{
					ImportableIds.AddUnique(UserId.ToSharedRef());
				}
			}
			if (ImportableIds.Num() > 0)
			{
				RequestImportableFriendQuery();
			}
			else
			{
				QueryImportableUserIds.Remove(AuthType);
			}
		}
		else
		{
			UE_LOG(LogOnline, Warning, TEXT("Unable to load importable friends: Query external id mappings failed with error %s"), *Error);
		}
	}
	
	virtual TSharedPtr< IFriendItem > FindRecentPlayer(const FUniqueNetId& InUserID) override
	{
		for (const auto& Friend : RecentPlayersList)
		{
			if (Friend->GetUniqueID().Get() == InUserID)
			{
				return Friend;
			}
		}

		return nullptr;
	}

	virtual TSharedPtr< IFriendItem > FindBlockedPlayer(const FUniqueNetId& InUserID) override
	{
		for (const auto& Friend : BlockedPlayersList)
		{
			if (Friend->GetUniqueID().Get() == InUserID)
			{
				return Friend;
			}
		}

		return nullptr;
	}

	virtual void AcceptFriend(TSharedPtr< IFriendItem > FriendItem) override
	{
		PendingOutgoingAcceptFriendRequests.Add(FriendItem->GetOnlineUser()->GetUserId()->ToString());
		FriendItem->SetPendingAccept();
		NotificationService->OnNotificationsAvailable().Broadcast(false);
		if (OSSScheduler->GetOnlineIdentity().IsValid() &&
			OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex).IsValid())
		{
			OSSScheduler->GetAnalytics()->FireEvent_FriendRequestAccepted(*OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex), *FriendItem);
		}
		RequestFriendAcceptRefresh();
	}

	virtual void RejectFriend(TSharedPtr< IFriendItem > FriendItem) override
	{
		NotifiedRequest.Remove(FriendItem->GetOnlineUser()->GetUserId());
		PendingOutgoingDeleteFriendRequests.Add(FriendItem->GetOnlineUser()->GetUserId().Get().ToString());
		FriendsList.Remove(FriendItem);
		NotificationService->OnNotificationsAvailable().Broadcast(false);

		if (OSSScheduler->GetOnlineIdentity().IsValid() &&
			OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex).IsValid())
		{
			OSSScheduler->GetAnalytics()->FireEvent_FriendRemoved(*OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex), *FriendItem, EFriendActionType::ToAnalyticString(EFriendActionType::RejectFriendRequest));
		}
		RequestFriendDeleteRefresh();
	}

	virtual void DeleteFriend(TSharedPtr< IFriendItem > FriendItem, EFriendActionType::Type Reason) override
	{
		TSharedRef<const FUniqueNetId> UserID = FriendItem->GetOnlineFriend()->GetUserId();
		NotifiedRequest.Remove(UserID);
		PendingOutgoingDeleteFriendRequests.Add(UserID.Get().ToString());
		FriendsList.Remove(FriendItem);
		FriendItem->SetPendingDelete();
		RefreshList();
		// Clear notifications
		NotificationService->OnNotificationsAvailable().Broadcast(false);

		if (OSSScheduler->GetOnlineIdentity().IsValid() &&
			OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex).IsValid())
		{
			OSSScheduler->GetAnalytics()->FireEvent_FriendRemoved(*OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex), *FriendItem, EFriendActionType::ToAnalyticString(Reason));
		}
		RequestFriendDeleteRefresh();
	}

	virtual void BlockPlayer(TSharedPtr< IFriendItem > FriendItem) override
	{
		TSharedRef<const FUniqueNetId> UserID = FriendItem->GetUniqueID();
		PendingOutgoingBlockPlayerRequests.Add(UserID.Get().ToString());
		FriendsList.Remove(FriendItem);
		FriendItem->SetPendingDelete();
		RefreshList();
		// Clear notifications
		NotificationService->OnNotificationsAvailable().Broadcast(false);

		if (OSSScheduler->GetOnlineIdentity().IsValid() &&
			OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex).IsValid())
		{
			OSSScheduler->GetAnalytics()->FireEvent_PlayerBlocked(*OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex), *FriendItem);
		}
		RequestBlockPlayerRefresh();
	}

	virtual void UnblockPlayer(TSharedPtr< IFriendItem > FriendItem) override
	{
		TSharedRef<const FUniqueNetId> UserID = FriendItem->GetOnlineUser()->GetUserId();
		PendingOutgoingUnblockPlayerRequests.Add(UserID.Get().ToString());
		BlockedPlayersList.Remove(FriendItem);

		RefreshList();
		// Clear notifications
		NotificationService->OnNotificationsAvailable().Broadcast(false);

		if (OSSScheduler->GetOnlineIdentity().IsValid() &&
			OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex).IsValid())
		{
			OSSScheduler->GetAnalytics()->FireEvent_PlayerUnblocked(*OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex), *FriendItem);
		}

		RequestPlayerUnblockRefresh();
	}

	virtual bool IsPlayerBlocked(const TSharedRef<const FUniqueNetId>& UserId) override
	{
		for(const auto& BlockedPlayer: BlockedPlayersList)
		{
			if(*BlockedPlayer->GetUniqueID() == *UserId)
			{
				return true;
			}
		}
		return false;
	}

	virtual void MutePlayer(const TSharedRef<const FUniqueNetId>& UserId) override
	{
		MuteList.AddUnique(UserId.Get().ToString());
	}

	virtual void UnmutePlayer(const TSharedRef<const FUniqueNetId>& UserId) override
	{
		MuteList.Remove(UserId.Get().ToString());
	}

	virtual bool IsPlayerMuted(const TSharedRef<const FUniqueNetId>& UserId) override
	{
		return MuteList.Contains(UserId.Get().ToString());
	}

	virtual TArray< TSharedPtr< IFriendItem > >& GetRecentPlayerList() override
	{
		return RecentPlayersList;
	}

	virtual TArray< TSharedPtr< IFriendItem > >& GetBlockedPlayerList() override
	{
		return BlockedPlayersList;
	}

	virtual TArray< TSharedPtr< IFriendItem > >& GetExternalPlayerList() override
	{
		return ExternalPlayersList;
	}

	int32 GetFilteredOutgoingFriendsList(TArray< TSharedPtr< IFriendItem > >& OutFriendsList)
	{
		OutFriendsList = FilteredOutgoingList;
		return OutFriendsList.Num();
	}

	virtual FString GetUserAppId() const override
	{
		return OSSScheduler->GetUserAppId();
	}

	virtual EOnlinePresenceState::Type GetOnlineStatus() override
	{
		return OSSScheduler->GetOnlineStatus();
	}

	virtual void SetUserIsOnline(EOnlinePresenceState::Type OnlineState) override
	{
		return OSSScheduler->SetUserIsOnline(OnlineState);
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

	virtual FString GetUserNickname() const override
	{
		FString Result;
		if (OSSScheduler.IsValid() &&
			OSSScheduler->GetOnlineIdentity().IsValid())
		{
			TSharedPtr<FOnlineUserPresence> Presence;
			TSharedPtr<const FUniqueNetId> UserId = OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(0);
			if (UserId.IsValid())
			{
				Result = OSSScheduler->GetOnlineIdentity()->GetPlayerNickname(*UserId);
			}
		}
		return Result;
	}

	// List processing

	void RequestListRefresh()
	{
		if (!bRequiresListRefresh)
		{
			bRequiresListRefresh = true;
			OSSScheduler->RequestOSS(OnRequestOSSAcceptedDelegate);
		}
	}

	void RequestRecentPlayersListRefresh()
	{
		if (!bRequiresRecentPlayersRefresh)
		{
			bRequiresRecentPlayersRefresh = true;
			OSSScheduler->RequestOSS(OnRequestOSSAcceptedDelegate);
		}
	}

	void RequestFriendRequestRefresh()
	{
		if (!bRequiresRequestFriendRefresh)
		{
			bRequiresRequestFriendRefresh = true;
			OSSScheduler->RequestOSS(OnRequestOSSAcceptedDelegate);
		}
	}

	void RequestImportableFriendQuery()
	{
		if (!bRequiresImportableFriendQuery)
		{
			bRequiresImportableFriendQuery = true;
			OSSScheduler->RequestOSS(OnRequestOSSAcceptedDelegate);
		}
	}

	void RequestFriendDeleteRefresh()
	{
		if (!bRequiresDeleteFriendRefresh)
		{
			bRequiresDeleteFriendRefresh = true;
			OSSScheduler->RequestOSS(OnRequestOSSAcceptedDelegate);
		}
	}

	void RequestBlockPlayerRefresh()
	{
		if (!bRequiresBlockPlayerRefresh)
		{
			bRequiresBlockPlayerRefresh = true;
			OSSScheduler->RequestOSS(OnRequestOSSAcceptedDelegate);
		}
	}

	void RequestPlayerUnblockRefresh()
	{
		if (!bRequiresUnblockPlayerRefresh)
		{
			bRequiresUnblockPlayerRefresh = true;
			OSSScheduler->RequestOSS(OnRequestOSSAcceptedDelegate);
		}
	}

	void RequestBlockedPlayersListRefresh()
	{
		if (!bRequiresBlockedPlayerListRefresh)
		{
			bRequiresBlockedPlayerListRefresh = true;
			OSSScheduler->RequestOSS(OnRequestOSSAcceptedDelegate);
		}
	}

	void RequestFriendAcceptRefresh()
	{
		if (!bRequiresAcceptFriendRefresh)
		{
			bRequiresAcceptFriendRefresh = true;
			OSSScheduler->RequestOSS(OnRequestOSSAcceptedDelegate);
		}
	}

	void OnReadFriendsListComplete(int32 LocalPlayer, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr)
	{
		PreProcessList(ListName);
	}

	void OnQueryRecentPlayersComplete(const FUniqueNetId& UserId, const FString& Namespace, bool bWasSuccessful, const FString& ErrorStr)
	{
		bool bFoundAllIds = true;

		// keep querying until done with all namespaces
		FString RecentPlayersNamespace = PendingRecentPlayersQueries.Num() > 0 ? PendingRecentPlayersQueries.Pop() : FString();
		if (!RecentPlayersNamespace.IsEmpty())
		{
			OSSScheduler->GetFriendsInterface()->QueryRecentPlayers(UserId, RecentPlayersNamespace);
		}
		else
		{
			RecentPlayersList.Empty();
			TArray< TSharedRef<FOnlineRecentPlayer> > Players;
			if (OSSScheduler->GetFriendsInterface()->GetRecentPlayers(UserId, FString(), Players))
			{
				for (const auto& RecentPlayer : Players)
				{
					if (RecentPlayer->GetDisplayName().IsEmpty())
					{
						QueryUserIds.Add(RecentPlayer->GetUserId());
					}
					else
					{
						RecentPlayersList.Add(MakeShareable(new FFriendRecentPlayerItem(RecentPlayer)));
					}
				}

				if (QueryUserIds.Num())
				{
					check(OSSScheduler->GetUserInterface().IsValid());
					IOnlineUserPtr UserInterface = OSSScheduler->GetUserInterface();
					UserInterface->QueryUserInfo(LocalControllerIndex, QueryUserIds);
					bFoundAllIds = false;
				}
				else
				{
					OnFriendsListUpdated().Broadcast();
				}
			}

			if (bFoundAllIds)
			{
				SetState(EFriendsAndManagerState::Idle);
			}
		}
	}

	void OnQueryBlockedPlayersComplete(const FUniqueNetId& UserId, bool bWasSuccessful, const FString& ErrorStr)
	{
		bool bFoundAllIds = true;

		BlockedPlayersList.Empty();
		TArray< TSharedRef<FOnlineBlockedPlayer> > Players;
		if (OSSScheduler->GetFriendsInterface()->GetBlockedPlayers(UserId, Players))
		{
			for (const auto& BlockedPlayer : Players)
			{
				if (BlockedPlayer->GetDisplayName().IsEmpty())
				{
					QueryUserIds.Add(BlockedPlayer->GetUserId());
				}
				else
				{
					BlockedPlayersList.Add(MakeShareable(new FFriendBlockedPlayerItem(BlockedPlayer)));
				}
			}

			if (QueryUserIds.Num())
			{
				check(OSSScheduler->GetUserInterface().IsValid());
				IOnlineUserPtr UserInterface = OSSScheduler->GetUserInterface();
				UserInterface->QueryUserInfo(LocalControllerIndex, QueryUserIds);
				bFoundAllIds = false;
			}
			else
			{
				OnFriendsListUpdated().Broadcast();
			}
		}

		if (bFoundAllIds)
		{
			SetState(EFriendsAndManagerState::Idle);
		}
	}

	void PreProcessList(const FString& ListName)
	{
		TArray< TSharedRef<FOnlineFriend> > Friends;
		bool bReadyToChangeState = true;

		if (OSSScheduler.IsValid() && OSSScheduler->IsLoggedIn() && OSSScheduler->GetFriendsInterface()->GetFriendsList(LocalControllerIndex, ListName, Friends))
		{
			if (Friends.Num() > 0)
			{
				for (const auto& Friend : Friends)
				{
					TSharedPtr< IFriendItem > ExistingFriend = FindUser(Friend->GetUserId().Get());
					if (ExistingFriend.IsValid())
					{
						if (Friend->GetInviteStatus() != ExistingFriend->GetOnlineFriend()->GetInviteStatus() || (ExistingFriend->IsPendingAccepted() && Friend->GetInviteStatus() == EInviteStatus::Accepted))
						{
							ExistingFriend->SetOnlineFriend(Friend);
						}
						PendingFriendsList.AddUnique(ExistingFriend);
					}
					else
					{
						QueryUserIds.Add(Friend->GetUserId());
					}
				}
			}

			check(OSSScheduler->GetUserInterface().IsValid());
			IOnlineUserPtr UserInterface = OSSScheduler->GetUserInterface();

			if (QueryUserIds.Num() > 0)
			{
				UserInterface->QueryUserInfo(LocalControllerIndex, QueryUserIds);
				// OnQueryUserInfoComplete will handle state change
				bReadyToChangeState = false;
			}
		}
		else
		{
			UE_LOG(LogOnline, Warning, TEXT("Failed to obtain Friends List %s"), *ListName);
		}

		if (bReadyToChangeState)
		{
			SetState(EFriendsAndManagerState::ProcessFriendsList);
		}
	}

	void OnQueryUserInfoComplete(int32 LocalPlayer, bool bWasSuccessful, const TArray< TSharedRef<const FUniqueNetId> >& UserIds, const FString& ErrorStr)
	{
		if (ManagerState == EFriendsAndManagerState::RequestingFriendsList)
		{
			check(OSSScheduler->GetUserInterface().IsValid());
			IOnlineUserPtr UserInterface = OSSScheduler->GetUserInterface();

			for (int32 UserIdx = 0; UserIdx < UserIds.Num(); UserIdx++)
			{
				TSharedPtr<FOnlineFriend> OnlineFriend = OSSScheduler->GetFriendsInterface()->GetFriend(0, *UserIds[UserIdx], EFriendsLists::ToString(EFriendsLists::Default));
				TSharedPtr<FOnlineUser> OnlineUser = OSSScheduler->GetUserInterface()->GetUserInfo(LocalPlayer, *UserIds[UserIdx]);
				if (OnlineFriend.IsValid() && OnlineUser.IsValid())
				{
					TSharedPtr<IFriendItem> Existing;
					for (auto FriendEntry : PendingFriendsList)
					{
						if (*FriendEntry->GetUniqueID() == *UserIds[UserIdx])
						{
							Existing = FriendEntry;
							break;
						}
					}
					if (Existing.IsValid())
					{
						Existing->SetOnlineUser(OnlineUser);
						Existing->SetOnlineFriend(OnlineFriend);
					}
					else
					{
						TSharedPtr< FFriendFriendItem > FriendItem = MakeShareable(new FFriendFriendItem(OnlineFriend, OnlineUser, EFriendsDisplayLists::DefaultDisplay, SharedThis(this)));
						PendingFriendsList.Add(FriendItem);
						FriendItem->SetChachedOnline(FriendItem->IsOnline());
					}
				}
				else
				{
					UE_LOG(LogOnline, Verbose, TEXT("PlayerId=%s not found"), *UserIds[UserIdx]->ToDebugString());
				}
			}

			QueryUserIds.Empty();

			SetState(EFriendsAndManagerState::ProcessFriendsList);
		}
		else if (ManagerState == EFriendsAndManagerState::RequestingRecentPlayersIDs)
		{
			RecentPlayersList.Empty();
			TSharedPtr<const FUniqueNetId> UserId = OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(0);
			if (UserId.IsValid())
			{
				TArray< TSharedRef<FOnlineRecentPlayer> > Players;
				if (OSSScheduler->GetFriendsInterface()->GetRecentPlayers(*UserId, FString(), Players))
				{
					for (const auto& RecentPlayer : Players)
					{
						TSharedRef<FFriendRecentPlayerItem> RecentPlayerItem = MakeShareable(new FFriendRecentPlayerItem(RecentPlayer));
						TSharedPtr<FOnlineUser> OnlineUser = OSSScheduler->GetUserInterface()->GetUserInfo(LocalPlayer, *RecentPlayer->GetUserId());
						// Invalid OnlineUser can happen if user disabled their account, but are still on someone's recent players list.  Skip including those users.
						if (OnlineUser.IsValid())
						{
							RecentPlayerItem->SetOnlineUser(OnlineUser);
							RecentPlayersList.Add(RecentPlayerItem);
						}
						else
						{
							FString InvalidUserId = RecentPlayerItem->GetUniqueID()->ToString();
							UE_LOG(LogOnline, VeryVerbose, TEXT("Hiding recent player that we could not obtain info for (eg. disabled player), id: %s"), *InvalidUserId);
						}
					}
				}
			}
			OnFriendsListUpdated().Broadcast();
			SetState(EFriendsAndManagerState::Idle);
		}
		else if (ManagerState == EFriendsAndManagerState::RequestingBlockedPlayersIDs)
		{
			BlockedPlayersList.Empty();
			TSharedPtr<const FUniqueNetId> UserId = OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(0);
			if (UserId.IsValid())
			{
				TArray< TSharedRef<FOnlineBlockedPlayer> > Players;
				if (OSSScheduler->GetFriendsInterface()->GetBlockedPlayers(*UserId, Players))
				{
					for (const auto& BlockedPlayer : Players)
					{
						TSharedRef<FFriendBlockedPlayerItem> BlockedPlayerItem = MakeShareable(new FFriendBlockedPlayerItem(BlockedPlayer));
						TSharedPtr<FOnlineUser> OnlineUser = OSSScheduler->GetUserInterface()->GetUserInfo(LocalPlayer, *BlockedPlayer->GetUserId());
						// Invalid OnlineUser can happen if user disabled their account, but are still on someone's recent players list.  Skip including those users.
						if (OnlineUser.IsValid())
						{
							BlockedPlayerItem->SetOnlineUser(OnlineUser);
							BlockedPlayersList.Add(BlockedPlayerItem);
						}
						else
						{
							FString InvalidUserId = BlockedPlayerItem->GetUniqueID()->ToString();
							UE_LOG(LogOnline, VeryVerbose, TEXT("Hiding blocked player that we could not obtain info for (eg. disabled player), id: %s"), *InvalidUserId);
						}
					}
				}
			}
			OnFriendsListUpdated().Broadcast();
			SetState(EFriendsAndManagerState::Idle);
		}
		else if (ManagerState == EFriendsAndManagerState::RequestingImportableFriendNames)
		{
			check(OSSScheduler->GetUserInterface().IsValid());
			IOnlineUserPtr UserInterface = OSSScheduler->GetUserInterface();

			for (auto UserId : UserIds)
			{
				TSharedPtr<FOnlineUser> OnlineUser = OSSScheduler->GetUserInterface()->GetUserInfo(LocalPlayer, *UserId);
				if (OnlineUser.IsValid())
				{
					// Check to see if they are already in the list
					auto FriendCheckFn = [UserId](const TSharedPtr< IFriendItem >& FriendItem)
					{
						return *FriendItem->GetUniqueID() == *UserId;
					};
					if (!ExternalPlayersList.FindByPredicate(FriendCheckFn) &&
						!FriendsList.FindByPredicate(FriendCheckFn))
					{
						ExternalPlayersList.Add(MakeShareable(new FFriendImportPlayerItem(OnlineUser.ToSharedRef(), CurrentQueryImportableUserIdsAuthType)));
					}
				}
			}
			CurrentQueryImportableUserIdsAuthType.Empty();
			OnFriendsListUpdated().Broadcast();

			if (QueryImportableUserIds.Num() > 0)
			{
				RequestImportableFriendQuery();
			}
			SetState(EFriendsAndManagerState::Idle);
		}
	}

	bool ProcessFriendsList()
	{
		/** Functor for comparing friends list */
		struct FCompareGroupByName
		{
			FORCEINLINE bool operator()(const TSharedPtr< IFriendItem > A, const TSharedPtr< IFriendItem > B) const
			{
				check(A.IsValid());
				check(B.IsValid());
				return (A->GetName() > B->GetName());
			}
		};

		bool bChanged = false;
		// Early check if list has changed
		if (PendingFriendsList.Num() != FriendsList.Num())
		{
			bChanged = true;
		}
		else
		{
			// Need to check each item
			FriendsList.Sort(FCompareGroupByName());
			PendingFriendsList.Sort(FCompareGroupByName());
			for (int32 Index = 0; Index < FriendsList.Num(); Index++)
			{
				if (PendingFriendsList[Index]->IsUpdated() || FriendsList[Index]->GetUniqueID() != PendingFriendsList[Index]->GetUniqueID())
				{
					bChanged = true;
					break;
				}
			}
		}

		if (bChanged)
		{
			for (int32 Index = 0; Index < PendingFriendsList.Num(); Index++)
			{
				PendingFriendsList[Index]->ClearUpdated();
				EInviteStatus::Type FriendStatus = PendingFriendsList[Index].Get()->GetOnlineFriend()->GetInviteStatus();
				if (FriendStatus == EInviteStatus::PendingInbound)
				{
					if (NotifiedRequest.Contains(PendingFriendsList[Index].Get()->GetUniqueID()) == false)
					{
						NotificationService->SendFriendInviteNotification(PendingFriendsList[Index], OnFriendInviteResponseDelegate);
						NotifiedRequest.Add(PendingFriendsList[Index]->GetUniqueID());
					}
				}
			}
			FriendByNameInvites.Empty();
			FriendsList = PendingFriendsList;
		}
		PendingFriendsList.Empty();
		return bChanged;
	}

	void RefreshList()
	{
		FilteredFriendsList.Empty();

		for (const auto& Friend : FriendsList)
		{
			if (!Friend->IsPendingDelete())
			{
				FilteredFriendsList.Add(Friend);
			}
		}

		OnFriendsListUpdated().Broadcast();
	}

	void SendFriendRequests()
	{
		// Invite Friends
		IOnlineUserPtr UserInterface = OSSScheduler->GetUserInterface();
		if (UserInterface.IsValid())
		{
			TSharedPtr<const FUniqueNetId> UserId = OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(0);
			if (UserId.IsValid())
			{
				for (int32 Index = 0; Index < FriendByNameRequests.Num(); Index++)
				{
					UserInterface->QueryUserIdMapping(*UserId, FriendByNameRequests[Index], OnQueryUserIdMappingCompleteDelegate);
				}
			}
		}
	}

	void SendImportableFriendNameRequests()
	{
		IOnlineUserPtr UserInterface = OSSScheduler->GetUserInterface();
		if (UserInterface.IsValid())
		{
			TSharedPtr<const FUniqueNetId> UserId = OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(0);
			if (UserId.IsValid())
			{
				bool Successful = false;
				if (QueryImportableUserIds.Num() > 0)
				{
					auto FirstIt = QueryImportableUserIds.CreateIterator();
					if (FirstIt)
					{
						FString AuthType = FirstIt.Key();
						TArray<TSharedRef<const FUniqueNetId>> Ids;
						QueryImportableUserIds.RemoveAndCopyValue(AuthType, Ids);

						if (UserInterface->QueryUserInfo(LocalControllerIndex, Ids))
						{
							CurrentQueryImportableUserIdsAuthType = AuthType;
							Successful = true;
						}
					}
				}
				if (!Successful)
				{
					if (QueryImportableUserIds.Num() > 0)
					{
						RequestImportableFriendQuery();
					}
					SetState(EFriendsAndManagerState::Idle);
				}
			}
		}
	}

	void OnQueryUserIdMappingComplete(bool bWasSuccessful, const FUniqueNetId& RequestingUserId, const FString& DisplayName, const FUniqueNetId& IdentifiedUserId, const FString& Error)
	{
		check(OSSScheduler->GetUserInterface().IsValid());

		EFindFriendResult::Type FindFriendResult = EFindFriendResult::NotFound;

		if (bWasSuccessful && IdentifiedUserId.IsValid())
		{
			TSharedPtr<IFriendItem> ExistingFriend = FindUser(IdentifiedUserId);
			if (ExistingFriend.IsValid())
			{
				if (ExistingFriend->GetInviteStatus() == EInviteStatus::Accepted)
				{
					OnAddToast().Broadcast(FText::FromString("Already friends"));
					FindFriendResult = EFindFriendResult::AlreadyFriends;
				}
				else
				{
					OnAddToast().Broadcast(FText::FromString("Friend already requested"));

					FindFriendResult = EFindFriendResult::FriendsPending;
				}
			}
			else if (IdentifiedUserId == RequestingUserId)
			{
				OnAddToast().Broadcast(FText::FromString("Can't friend yourself"));

				FindFriendResult = EFindFriendResult::AddingSelfFail;
			}
			else
			{
				TSharedPtr<const FUniqueNetId> FriendId = OSSScheduler->GetOnlineIdentity()->CreateUniquePlayerId(IdentifiedUserId.ToString());
				PendingOutgoingFriendRequests.Add(FriendId.ToSharedRef());
				FriendByNameInvites.AddUnique(DisplayName);

				FindFriendResult = EFindFriendResult::Success;
			}
		}
		else
		{
			const FString DiplayMessage = DisplayName + TEXT(" not found");
			OnAddToast().Broadcast(FText::FromString(DiplayMessage));
		}

		bool bRecentPlayer = false;
		if (FindFriendResult == EFindFriendResult::Success)
		{
			bRecentPlayer = FindRecentPlayer(IdentifiedUserId).IsValid();
		}

		if (OSSScheduler->GetOnlineIdentity().IsValid() &&
			OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex).IsValid())
		{
			OSSScheduler->GetAnalytics()->FireEvent_AddFriend(*OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex),
				DisplayName,
				IdentifiedUserId,
				FindFriendResult,
				bRecentPlayer);
		}

		FriendByNameRequests.Remove(DisplayName);
		if (FriendByNameRequests.Num() == 0)
		{
			if (PendingOutgoingFriendRequests.Num() > 0)
			{
				for (int32 Index = 0; Index < PendingOutgoingFriendRequests.Num(); Index++)
				{
					FOnSendInviteComplete Delegate = FOnSendInviteComplete::CreateSP(this, &FFriendsServiceImpl::OnSendInviteComplete);
					OSSScheduler->GetFriendsInterface()->SendInvite(LocalControllerIndex, PendingOutgoingFriendRequests[Index].Get(), EFriendsLists::ToString(EFriendsLists::Default), Delegate);
					OnAddToast().Broadcast(LOCTEXT("FriendRequestSent", "Request Sent"));

					NotificationService->SendFriendInviteSentNotification(DisplayName);
				}
			}
			else
			{
				SetState(EFriendsAndManagerState::Idle);
			}
		}
	}

	void OnSendInviteComplete(int32 LocalPlayer, bool bWasSuccessful, const FUniqueNetId& FriendId, const FString& ListName, const FString& ErrorStr)
	{
		PendingOutgoingFriendRequests.RemoveAt(0);

		if (PendingOutgoingFriendRequests.Num() == 0)
		{
			RequestListRefresh();
			SetState(EFriendsAndManagerState::Idle);
		}
	}

	void OnSendInviteByOnlineUserComplete(int32 LocalPlayer, bool bWasSuccessful, const FUniqueNetId& FriendId, const FString& ListName, const FString& ErrorStr)
	{
		RequestListRefresh();
	}

	void OnFriendsListChanged()
	{
		if (ManagerState == EFriendsAndManagerState::Idle)
		{
			// Request called from outside our expected actions. e.g. Friend canceled their friend request
			RequestListRefresh();
		}
	}

	void OnPresenceReceived(const FUniqueNetId& UserId, const TSharedRef<FOnlineUserPresence>& NewPresence)
	{
		// Compare to previous presence for this friend, display a toast if a friend has came online or joined a game
		TSharedPtr<const FUniqueNetId> SelfId = OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(0);
		bool bIsSelf = (SelfId.IsValid() && UserId == *SelfId);
		bool bFriendNotificationsBound = NotificationService->OnSendNotification().IsBound();
		// Don't show notifications if we're building the friends list presences for the first time.
		// Guess at this using the size of the saved presence list. OK to show the last 1 or 2, but avoid spamming dozens of notifications at startup
		int32 OnlineFriendCount = 0;
		for (auto Friend : FriendsList)
		{
			if (Friend->IsOnline())
			{
				OnlineFriendCount++;
			}
		}

		TSharedPtr<IFriendItem> PresenceFriend = FindUser(UserId);
		if (PresenceFriend.IsValid())
		{
			if (PresenceFriend->IsCachedOnline() != PresenceFriend->IsOnline())
			{
				RefreshList();
			}
			PresenceFriend->SetChachedOnline(PresenceFriend->IsOnline());
		}

		// Make sure we have the icon downloaded for this game
		FString Result;
		const FVariantData* AppId = NewPresence->Status.Properties.Find(DefaultAppIdKey);
		const FVariantData* OverrideAppId = NewPresence->Status.Properties.Find(OverrideClientIdKey);
		const FVariantData* LegacyId = NewPresence->Status.Properties.Find(TEXT("ClientId"));
		if (OverrideAppId != nullptr && OverrideAppId->GetType() == EOnlineKeyValuePairDataType::String)
		{
			OverrideAppId->GetValue(Result);
		}
		else if (AppId != nullptr && AppId->GetType() == EOnlineKeyValuePairDataType::String)
		{
			AppId->GetValue(Result);
		}
		else if (LegacyId != nullptr && LegacyId->GetType() == EOnlineKeyValuePairDataType::String)
		{
			LegacyId->GetValue(Result);
		}
		WebImageService->RequestTitleIcon(Result);
	}

	void OnFriendInviteReceived(const FUniqueNetId& UserId, const FUniqueNetId& FriendId)
	{
		NotificationService->OnNotificationsAvailable().Broadcast(true);
		RequestListRefresh();
	}

	void OnFriendRemoved(const FUniqueNetId& UserId, const FUniqueNetId& FriendId)
	{
		RequestListRefresh();
	}

	void OnInviteRejected(const FUniqueNetId& UserId, const FUniqueNetId& FriendId)
	{
		RequestListRefresh();
	}

	void OnInviteAccepted(const FUniqueNetId& UserId, const FUniqueNetId& FriendId)
	{
		TSharedPtr< IFriendItem > Friend = FindUser(FriendId);
		if (Friend.IsValid())
		{
			NotificationService->SendAcceptInviteNotification(Friend);
			Friend->SetPendingAccept();
		}
		RequestListRefresh();
	}

	void OnAcceptInviteComplete(int32 LocalPlayer, bool bWasSuccessful, const FUniqueNetId& FriendId, const FString& ListName, const FString& ErrorStr)
	{
		PendingOutgoingAcceptFriendRequests.Remove(FriendId.ToString());

		// Do something with an accepted invite
		if (PendingOutgoingAcceptFriendRequests.Num() > 0)
		{
			SetState(EFriendsAndManagerState::AcceptingFriendRequest);
		}
		else
		{
			RequestListRefresh();
			SetState(EFriendsAndManagerState::Idle);
		}
	}

	void OnDeleteFriendComplete(int32 LocalPlayer, bool bWasSuccessful, const FUniqueNetId& DeletedFriendID, const FString& ListName, const FString& ErrorStr)
	{
		PendingOutgoingDeleteFriendRequests.Remove(DeletedFriendID.ToString());

		if (PendingOutgoingDeleteFriendRequests.Num() > 0)
		{
			SetState(EFriendsAndManagerState::DeletingFriends);
		}
		else
		{
			SetState(EFriendsAndManagerState::Idle);
		}
	}

	void OnBlockedPlayerComplete(int32 LocalPlayer, bool bWasSuccessful, const FUniqueNetId& BlockedPlayerID, const FString& ListName, const FString& ErrorStr)
	{
		PendingOutgoingBlockPlayerRequests.Remove(BlockedPlayerID.ToString());

		if (PendingOutgoingBlockPlayerRequests.Num() > 0)
		{
			SetState(EFriendsAndManagerState::BlockingPlayer);
		}
		else
		{
			SetState(EFriendsAndManagerState::Idle);
			RequestBlockedPlayersListRefresh();
		}
	}

	void OnUnblockedPlayerComplete(int32 LocalPlayer, bool bWasSuccessful, const FUniqueNetId& UnblockedPlayerID, const FString& ListName, const FString& ErrorStr)
	{
		PendingOutgoingUnblockPlayerRequests.Remove(UnblockedPlayerID.ToString());

		if (PendingOutgoingUnblockPlayerRequests.Num() > 0)
		{
			SetState(EFriendsAndManagerState::UnblockingPlayer);
		}
		else
		{
			SetState(EFriendsAndManagerState::Idle);
		}
	}

	void OnHandleFriendInviteResponse(TSharedPtr< FFriendsAndChatMessage > ChatMessage, EResponseType::Type ResponseType)
	{
		switch (ResponseType)
		{
		case EResponseType::Response_Accept:
		{
			PendingOutgoingAcceptFriendRequests.Add(ChatMessage->GetUniqueID()->ToString());
			TSharedPtr< IFriendItem > User = FindUser(ChatMessage->GetUniqueID().Get());
			if (User.IsValid())
			{
				AcceptFriend(User);
			}
			RequestFriendAcceptRefresh();
		}
		break;
		case EResponseType::Response_Reject:
		{
			NotifiedRequest.Remove(ChatMessage->GetUniqueID());
			TSharedPtr< IFriendItem > User = FindUser(ChatMessage->GetUniqueID().Get());
			if (User.IsValid())
			{
				RejectFriend(User);
			}
		}
		break;
		case EResponseType::Response_Ignore:
		{
			NotifiedRequest.Remove(ChatMessage->GetUniqueID());
			TSharedPtr< IFriendItem > User = FindUser(ChatMessage->GetUniqueID().Get());
			if (User.IsValid())
			{
				DeleteFriend(User, EFriendActionType::IgnoreFriendRequest);
			}
		}
		break;
		}
	}

	void OnGameDestroyed(const FName SessionName, bool bWasSuccessful)
	{
		if (SessionName == GameSessionName)
		{
			RequestRecentPlayersListRefresh();
		}
	}

	void OnPartyMemberExited(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& MemberId, const EMemberExitedReason Reason)
	{
		RequestRecentPlayersListRefresh();
	}

	void OnRequestOSSAccepted()
	{
		if (ManagerState == EFriendsAndManagerState::Idle)
		{
			if (bRequiresRequestFriendRefresh)
			{
				bRequiresRequestFriendRefresh = false;
				SetState(EFriendsAndManagerState::RequestingFriendName);
			}
			else if (bRequiresImportableFriendQuery)
			{
				bRequiresImportableFriendQuery = false;
				SetState(EFriendsAndManagerState::RequestingImportableFriendNames);
			}
			else if (bRequiresDeleteFriendRefresh)
			{
				bRequiresDeleteFriendRefresh = false;
				SetState(EFriendsAndManagerState::DeletingFriends);
			}
			else if (bRequiresBlockPlayerRefresh)
			{
				bRequiresBlockPlayerRefresh = false;
				SetState(EFriendsAndManagerState::BlockingPlayer);
			}
			else if (bRequiresUnblockPlayerRefresh)
			{
				bRequiresUnblockPlayerRefresh = false;
				SetState(EFriendsAndManagerState::UnblockingPlayer);
			}
			else if (bRequiresAcceptFriendRefresh)
			{
				bRequiresAcceptFriendRefresh = false;
				SetState(EFriendsAndManagerState::AcceptingFriendRequest);
			}
			else if (bRequiresListRefresh)
			{
				bRequiresListRefresh = false;
				SetState(EFriendsAndManagerState::RequestFriendsListRefresh);
			}
			else if (bRequiresRecentPlayersRefresh)
			{
				bRequiresRecentPlayersRefresh = false;

				// reset list of namespaces to query
				PendingRecentPlayersQueries = RecentPlayersNamespaces;
				if (PendingRecentPlayersQueries.Num() > 0)
				{
					SetState(EFriendsAndManagerState::RequestRecentPlayersListRefresh);
				}
				else
				{
					SetState(EFriendsAndManagerState::Idle);
				}
			}
			else if (bRequiresBlockedPlayerListRefresh)
			{
				bRequiresBlockedPlayerListRefresh = false;
				SetState(EFriendsAndManagerState::RequestBlockedPlayersListRefresh);
			}
			else
			{
				SetState(EFriendsAndManagerState::Idle);
			}
		}
	}

	void SetState(EFriendsAndManagerState::Type NewState)
	{
		ManagerState = NewState;

		switch (NewState)
		{
		case EFriendsAndManagerState::Idle:
		{
			OSSScheduler->ReleaseOSS();
		}
		break;
		case EFriendsAndManagerState::RequestFriendsListRefresh:
		{
			TSharedPtr<const FUniqueNetId> UserId = OSSScheduler->GetOnlineIdentity().IsValid() ? OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex) : nullptr;
			if (UserId.IsValid())
			{
				FOnReadFriendsListComplete Delegate = FOnReadFriendsListComplete::CreateSP(this, &FFriendsServiceImpl::OnReadFriendsListComplete);
				if (OSSScheduler->GetFriendsInterface()->ReadFriendsList(LocalControllerIndex, EFriendsLists::ToString(EFriendsLists::Default), Delegate))
				{
					SetState(EFriendsAndManagerState::RequestingFriendsList);
				}
				else
				{
					SetState(EFriendsAndManagerState::Idle);
					RequestListRefresh();
				}
			}
			else
			{
				// Not logged in
				SetState(EFriendsAndManagerState::Idle);
			}
		}
		break;
		case EFriendsAndManagerState::RequestRecentPlayersListRefresh:
		{
			FString RecentPlayersNamespace = PendingRecentPlayersQueries.Num() > 0 ? PendingRecentPlayersQueries.Pop() : FString();

			bool bRetry = false;
			if (!RecentPlayersNamespace.IsEmpty())
			{
				TSharedPtr<const FUniqueNetId> UserId = OSSScheduler->GetOnlineIdentity().IsValid() ? OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex) : nullptr;
				if (UserId.IsValid())
				{
					if (OSSScheduler->GetFriendsInterface()->QueryRecentPlayers(*UserId, RecentPlayersNamespace))
					{
						SetState(EFriendsAndManagerState::RequestingRecentPlayersIDs);
					}
					else
					{
						bRetry = true;
					}
				}
			}

			if (bRetry)
			{
				SetState(EFriendsAndManagerState::Idle);
				RequestRecentPlayersListRefresh();
			}
		}
		break;
		case EFriendsAndManagerState::RequestBlockedPlayersListRefresh:
		{
			bool bRetry = false;

			TSharedPtr<const FUniqueNetId> UserId = OSSScheduler->GetOnlineIdentity().IsValid() ? OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex) : nullptr;
			if (UserId.IsValid())
			{
				if (OSSScheduler->GetFriendsInterface()->QueryBlockedPlayers(*UserId))
				{
					SetState(EFriendsAndManagerState::RequestingBlockedPlayersIDs);
				}
				else
				{
					bRetry = true;
				}
			}

			if (bRetry)
			{
				SetState(EFriendsAndManagerState::Idle);
				RequestBlockedPlayersListRefresh();
			}
		}
		break;
		case EFriendsAndManagerState::ProcessFriendsList:
		{
			if (ProcessFriendsList())
			{
				RefreshList();
			}
			SetState(EFriendsAndManagerState::Idle);
		}
		break;
		case EFriendsAndManagerState::RequestingFriendName:
		{
			SendFriendRequests();
		}
		break;
		case EFriendsAndManagerState::RequestingImportableFriendNames:
		{
			SendImportableFriendNameRequests();
		}
		break;
		case EFriendsAndManagerState::DeletingFriends:
		{
			if (OSSScheduler->GetFriendsInterface().IsValid())
			{
				if (PendingOutgoingDeleteFriendRequests.Num() > 0)
				{ 
					TSharedPtr<const FUniqueNetId> PlayerId = OSSScheduler->GetOnlineIdentity()->CreateUniquePlayerId(PendingOutgoingDeleteFriendRequests[0]);
					OSSScheduler->GetFriendsInterface()->DeleteFriend(0, *PlayerId, EFriendsLists::ToString(EFriendsLists::Default));
				}
				else
				{
					SetState(EFriendsAndManagerState::Idle);
				}
			}
		}
		break;
		case EFriendsAndManagerState::BlockingPlayer:
		{
			if (OSSScheduler->GetFriendsInterface().IsValid())
			{
				if (PendingOutgoingBlockPlayerRequests.Num() > 0)
				{ 
					TSharedPtr<const FUniqueNetId> PlayerId = OSSScheduler->GetOnlineIdentity()->CreateUniquePlayerId(PendingOutgoingBlockPlayerRequests[0]);
					OSSScheduler->GetFriendsInterface()->BlockPlayer(0, *PlayerId);
				}
				else
				{
					SetState(EFriendsAndManagerState::Idle);
				}
			}
		}
		break;
		case EFriendsAndManagerState::UnblockingPlayer:
		{
			if (OSSScheduler->GetFriendsInterface().IsValid())
			{
				if (PendingOutgoingUnblockPlayerRequests.Num() > 0)
				{ 
					TSharedPtr<const FUniqueNetId> PlayerId = OSSScheduler->GetOnlineIdentity()->CreateUniquePlayerId(PendingOutgoingUnblockPlayerRequests[0]);
					OSSScheduler->GetFriendsInterface()->UnblockPlayer(0, *PlayerId);
				}
				else
				{
					SetState(EFriendsAndManagerState::Idle);
				}
			}
		}
		break;
		case EFriendsAndManagerState::AcceptingFriendRequest:
		{
			if (OSSScheduler->GetFriendsInterface().IsValid())
			{
				if (PendingOutgoingAcceptFriendRequests.Num() > 0)
				{
					TSharedPtr<const FUniqueNetId> PlayerId = OSSScheduler->GetOnlineIdentity()->CreateUniquePlayerId(PendingOutgoingAcceptFriendRequests[0]);
					FOnAcceptInviteComplete Delegate = FOnAcceptInviteComplete::CreateSP(this, &FFriendsServiceImpl::OnAcceptInviteComplete);
					OSSScheduler->GetFriendsInterface()->AcceptInvite(0, *PlayerId, EFriendsLists::ToString(EFriendsLists::Default), Delegate);
				}
				else
				{
					SetState(EFriendsAndManagerState::Idle);
				}
			}
		}
		break;
		default:
			break;
		}
	}


	// Internal events
	DECLARE_DERIVED_EVENT(FFriendsService, FFriendsService::FOnFriendsUpdated, FOnFriendsUpdated)
	virtual FOnFriendsUpdated& OnFriendsListUpdated()
	{
		return OnFriendsListUpdatedDelegate;
	}

	DECLARE_DERIVED_EVENT(FFriendsService, FFriendsService::FOnAddToast, FOnAddToast)
	virtual FOnAddToast& OnAddToast()
	{
		return OnAddToastDelegate;
	}

private:

	FFriendsServiceImpl(const TSharedRef<FOSSScheduler>& InOSSScheduler, const TSharedRef<class FChatNotificationService>& InNotificationService, const TSharedRef<class FWebImageService>& InWebImageService)
		: ManagerState(EFriendsAndManagerState::OffLine)
		, bRequiresListRefresh(false)
		, bRequiresRecentPlayersRefresh(false)
		, bRequiresAcceptFriendRefresh(false)
		, bRequiresDeleteFriendRefresh(false)
		, bRequiresBlockPlayerRefresh(false)
		, bRequiresUnblockPlayerRefresh(false)
		, bRequiresBlockedPlayerListRefresh(false)
		, bRequiresRequestFriendRefresh(false)
		, bRequiresImportableFriendQuery(false)
		, LocalControllerIndex(0)
	{
		OSSScheduler = InOSSScheduler;
		NotificationService = InNotificationService;
		WebImageService = InWebImageService;

		FriendByNameRequests.Empty();
	}

	// Holds the manager state
	EFriendsAndManagerState::Type ManagerState;
	// Holds if we need a list refresh
	bool bRequiresListRefresh;
	// Holds if we need a recent player list refresh
	bool bRequiresRecentPlayersRefresh;
	// Holds if we need a accept friend refresh
	bool bRequiresAcceptFriendRefresh;
	// Holds if we need a delete friend refresh
	bool bRequiresDeleteFriendRefresh;
	// Holds if we need a block player refresh
	bool bRequiresBlockPlayerRefresh;
	// Holds if we need a unblock player friend refresh
	bool bRequiresUnblockPlayerRefresh;
	// Holds if we need a block player friend refresh
	bool bRequiresBlockedPlayerListRefresh;
	// Holds if we need a friend name refresh
	bool bRequiresRequestFriendRefresh;
	// Holds if we need an importable friend query
	bool bRequiresImportableFriendQuery;
	// Holds the index for the local controller
	int32 LocalControllerIndex;

	// Holds the list of user names to send invites to. 
	TArray< FString > FriendByNameInvites;
	// Holds the list of user names to query add as friends 
	TArray< FString > FriendByNameRequests;
	// Holds an array of outgoing invite friend requests
	TArray< TSharedRef< const FUniqueNetId > > PendingOutgoingFriendRequests;
	// Holds the unprocessed friends list generated from a friends request update
	TArray< TSharedPtr< IFriendItem > > PendingFriendsList;
	// Holds the filtered friends list used in the UI
	TArray< TSharedPtr< IFriendItem > > FilteredFriendsList;
	// Holds the outgoing friend request list used in the UI
	TArray< TSharedPtr< IFriendItem > > FilteredOutgoingList;
	// Holds the full friends list used to build the UI
	TArray< TSharedPtr< IFriendItem > > FriendsList;
	// Holds the recent players list
	TArray< TSharedPtr< IFriendItem > > RecentPlayersList;
	// Holds the blocked players list
	TArray< TSharedPtr< IFriendItem > > BlockedPlayersList;
	// Holds the mute list
	TArray< FString > MuteList;
	// Holds the external players list
	TArray< TSharedPtr< IFriendItem > > ExternalPlayersList;
	/** Recent players namespace to query */
	TArray<FString> RecentPlayersNamespaces;
	TArray<FString> PendingRecentPlayersQueries;

	// Holds the list of Unique Ids found for user names to add as friends
	TArray< TSharedRef< const FUniqueNetId > > QueryUserIds;
	// Holds a map of auth type to list of Unique Ids found for importable friends that we need to perform an additional query for
	TMap< FString, TArray< TSharedRef< const FUniqueNetId > > > QueryImportableUserIds;
	// Holds the auth type that we are currently querying for importable user ids (if we are currently querying)
	FString CurrentQueryImportableUserIdsAuthType;
	// Holds the list of old presence data for comparison to new presence updates
	TMap< FString, FOnlineUserPresence > OldUserPresenceMap;

	// Holds the list of invites we have already responded to
	TArray< TSharedPtr< const FUniqueNetId > > NotifiedRequest;
	// Holds an array of outgoing delete friend requests
	TArray< FString > PendingOutgoingDeleteFriendRequests;
	// Holds an array of outgoing accept friend requests
	TArray< FString > PendingOutgoingAcceptFriendRequests;
	// Holds an array of outgoing block players requests
	TArray< FString > PendingOutgoingBlockPlayerRequests;
	// Holds an array of outgoing unblock players requests
	TArray< FString > PendingOutgoingUnblockPlayerRequests;

	FOnAddToast OnAddToastDelegate;
	// Holds the delegate to call when the friends list gets updated - refresh the UI
	FOnFriendsUpdated OnFriendsListUpdatedDelegate;
	// Delegate to use for querying for recent players 
	FOnQueryRecentPlayersCompleteDelegate OnQueryRecentPlayersCompleteDelegate;
	// Delegate to use for querying for blocked players 
	FOnQueryBlockedPlayersCompleteDelegate OnQueryBlockedPlayersCompleteDelegate;
	// Delegate to use for deleting a friend
	FOnDeleteFriendCompleteDelegate OnDeleteFriendCompleteDelegate;
	// Delegate to use for blocking a player
	FOnBlockedPlayerCompleteDelegate OnBlockedPlayerCompleteDelegate;
	// Delegate to use for blocking a player
	FOnUnblockedPlayerCompleteDelegate OnUnblockedPlayerCompleteDelegate;
	// Delegate for querying user id from a name string
	IOnlineUser::FOnQueryUserMappingComplete OnQueryUserIdMappingCompleteDelegate;
	// Delegate to use for querying user info list
	FOnQueryUserInfoCompleteDelegate OnQueryUserInfoCompleteDelegate;
	// Delegate for friends list changing
	FOnFriendsChangeDelegate OnFriendsListChangedDelegate;
	// Delegate for an invite received
	FOnInviteReceivedDelegate OnFriendInviteReceivedDelegate;
	// Delegate for friend removed
	FOnFriendRemovedDelegate OnFriendRemovedDelegate;
	// Delegate for friend invite rejected
	FOnInviteRejectedDelegate	OnFriendInviteRejectedDelegate;
	// Delegate for friend invite accepted
	FOnInviteAcceptedDelegate	OnFriendInviteAcceptedDelegate;
	// Delegate for Friend Invite Notification responses
	IChatNotificationService::FOnNotificationResponseDelegate OnFriendInviteResponseDelegate;
	// Delegate for the presence received
	FOnPresenceReceivedDelegate OnPresenceReceivedCompleteDelegate;
	// Delegate for when session is destroyed
	FOnDestroySessionCompleteDelegate OnDestroySessionCompleteDelegate;
	// Delegate for when a party member exits
	FOnPartyMemberExitedDelegate OnPartyMemberExitedDelegate;
	// Delegate for when external id mappings querying is complete
	IOnlineUser::FOnQueryExternalIdMappingsComplete OnQueryExternalIdMappingsCompleteDelegate;

	FOSSScheduler::FOnRequestOSSAccepted OnRequestOSSAcceptedDelegate;

	/** Handle to various registered delegates */
	FDelegateHandle OnQueryRecentPlayersCompleteDelegateHandle;
	FDelegateHandle OnBlockedPlayerListCompleteDelegateHandle;
	FDelegateHandle OnFriendsListChangedDelegateHandle;
	FDelegateHandle OnFriendInviteReceivedDelegateHandle;
	FDelegateHandle OnFriendRemovedDelegateHandle;
	FDelegateHandle OnPlayerBlockedCompleteDelegateHandle;
	FDelegateHandle OnPlayerUnblockedCompleteDelegateHandle;
	FDelegateHandle OnFriendInviteRejectedDelegateHandle;
	FDelegateHandle OnFriendInviteAcceptedDelegateHandle;
	FDelegateHandle OnReadFriendsCompleteDelegateHandle;
	FDelegateHandle OnDeleteFriendCompleteDelegateHandle;
	FDelegateHandle OnQueryUserInfoCompleteDelegateHandle;
	FDelegateHandle OnPresenceReceivedCompleteDelegateHandle;
	FDelegateHandle OnDestroySessionCompleteDelegateHandle;
	FDelegateHandle OnPartyMemberExitedDelegateHandle;

	// Manages notification services
	TSharedPtr<class FChatNotificationService> NotificationService;
	
	// Holds the OSS Scheduler
	TSharedPtr<FOSSScheduler> OSSScheduler;

	// Holds the OSS Scheduler
	TSharedPtr<FWebImageService> WebImageService;

	friend FFriendsServiceFactory;
};

TSharedRef< FFriendsService > FFriendsServiceFactory::Create(const TSharedRef<FOSSScheduler>& OSSScheduler, const TSharedRef<class FChatNotificationService>& NotificationService, const TSharedRef<class FWebImageService>& WebImageService)
{
	TSharedRef< FFriendsServiceImpl > FriendsService(new FFriendsServiceImpl(OSSScheduler, NotificationService, WebImageService));
	return FriendsService;
}

#undef LOCTEXT_NAMESPACE
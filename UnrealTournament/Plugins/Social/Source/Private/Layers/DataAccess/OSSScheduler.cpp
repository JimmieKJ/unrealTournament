// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "OSSScheduler.h"
#include "OnlineSubsystemNames.h"

#include "FriendsAndChatAnalytics.h"

const float CHAT_ANALYTICS_INTERVAL = 5 * 60.0f;  // 5 min

class FOSSSchedulerImpl
	: public FOSSScheduler
{
public:

	virtual void Login(IOnlineSubsystem* InOnlineSub, bool bInIsGame, int32 LocalUserID) override
	{
		// Clear existing data
		Logout();
		
		LocalControllerIndex = LocalUserID;
		
		if (InOnlineSub)
		{
			OnlineSub = InOnlineSub;
		}
		else
		{
			OnlineSub = IOnlineSubsystem::Get(MCP_SUBSYSTEM);
		}

		if (OnlineSub != nullptr &&
			OnlineSub->GetUserInterface().IsValid() &&
			OnlineSub->GetIdentityInterface().IsValid())
		{
			OnlineIdentity = OnlineSub->GetIdentityInterface();
			OnPresenceUpdatedCompleteDelegate = IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateSP(this, &FOSSSchedulerImpl::OnPresenceUpdated);
		}

		if (UpdateFriendsTickerDelegate.IsBound() == false)
		{
			UpdateFriendsTickerDelegate = FTickerDelegate::CreateSP(this, &FOSSSchedulerImpl::Tick);
		}

		UpdateFriendsTickerDelegateHandle = FTicker::GetCoreTicker().AddTicker(UpdateFriendsTickerDelegate);

		// Check we can chat online

		IOnlineIdentityPtr PlatformIdentity = nullptr;
		IOnlineSubsystem* OnlineSubConsole = IOnlineSubsystem::GetByPlatform();
		if (OnlineSubConsole)
		{
			PlatformIdentity = OnlineSubConsole->GetIdentityInterface();
		}
		if (!PlatformIdentity.IsValid())
		{
			PlatformIdentity = OnlineIdentity;
		}

		if (PlatformIdentity.IsValid())
		{
			TSharedPtr<const FUniqueNetId> UserId = PlatformIdentity->GetUniquePlayerId(LocalControllerIndex);
			if (UserId.IsValid())
			{
				PlatformIdentity->GetUserPrivilege(*UserId, EUserPrivileges::CanCommunicateOnline, IOnlineIdentity::FOnGetUserPrivilegeCompleteDelegate::CreateSP(this, &FOSSSchedulerImpl::OnUserPrivilege));
			}
		}
	}

	virtual void Logout() override
	{
		OnlineSub = nullptr;
		OnlineIdentity = nullptr;
		// flush before removing the analytics provider
		Analytics->FireEvent_FlushChatStats();

		bOSSInUse = false;
		RequestDelegates.Empty();
		

		if (UpdateFriendsTickerDelegate.IsBound())
		{
			FTicker::GetCoreTicker().RemoveTicker(UpdateFriendsTickerDelegateHandle);
		}
	}

	bool IsLoggedIn() const override
	{
		return OnlineSub != nullptr && OnlineIdentity.IsValid();
	}

	void SetOnline() override
	{
		SetUserIsOnline(EOnlinePresenceState::Online);
	}

	void SetAway() override
	{
		SetUserIsOnline(EOnlinePresenceState::Away);
	}

	bool CanChatOnline() override
	{
		return bCanChatOnline;
	}

	virtual EOnlinePresenceState::Type GetOnlineStatus() override
	{
		if (OnlineSub != nullptr &&
			OnlineIdentity.IsValid())
		{
			TSharedPtr<FOnlineUserPresence> Presence;
			TSharedPtr<const FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
			if (UserId.IsValid())
			{
				OnlineSub->GetPresenceInterface()->GetCachedPresence(*UserId, Presence);
				if (Presence.IsValid())
				{
					return Presence->Status.State;
				}
			}
		}
		return EOnlinePresenceState::Offline;
	}

	virtual FString GetUserAppId() const override
	{
		FString Result;
		if (OnlineSub != nullptr &&
			OnlineIdentity.IsValid())
		{
			TSharedPtr<FOnlineUserPresence> Presence;
			TSharedPtr<const FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
			if (UserId.IsValid())
			{
				GetPresenceInterface()->GetCachedPresence(*UserId, Presence);
				if (Presence.IsValid())
				{
					const FVariantData* AppId = Presence->Status.Properties.Find(DefaultAppIdKey);
					const FVariantData* OverrideAppId = Presence->Status.Properties.Find(OverrideClientIdKey);
					if (OverrideAppId != nullptr && OverrideAppId->GetType() == EOnlineKeyValuePairDataType::String)
					{
						OverrideAppId->GetValue(Result);
					}
					else if (AppId != nullptr && AppId->GetType() == EOnlineKeyValuePairDataType::String)
					{
						AppId->GetValue(Result);
					}
				}
			}
		}
		return Result;
	}

	virtual FString GetPlayerNickname(const FUniqueNetId& LocalUserId) const override
	{
		// Prefer console display name if on console
		IOnlineSubsystem* PlatformSubsystem = IOnlineSubsystem::GetByPlatform(false);
		if (PlatformSubsystem)
		{
			// Try to get local player's index
			FPlatformUserId PlatformUserId = GetOnlineIdentity()->GetPlatformUserIdFromUniqueNetId(LocalUserId);
			if (PlatformUserId != PLATFORMUSERID_NONE)
			{
				IOnlineIdentityPtr OnlineIdentityInt = PlatformSubsystem->GetIdentityInterface();
				if (OnlineIdentityInt.IsValid())
				{
					FString PlayerNickname = OnlineIdentityInt->GetPlayerNickname(PlatformUserId);
					if (!PlayerNickname.IsEmpty())
					{
						return PlayerNickname;
					}
				}
			}
		}
		return GetOnlineIdentity()->GetPlayerNickname(LocalControllerIndex);
	}

	virtual FString GetPlatformAuthType() const override
	{
		FString AuthName = "";
		IOnlineSubsystem* PlatformOSS = IOnlineSubsystem::GetByPlatform(false);
		if (PlatformOSS)
		{
			IOnlineIdentityPtr IdentityInt = PlatformOSS->GetIdentityInterface();
			if (IdentityInt.IsValid())
			{
				AuthName = IdentityInt->GetAuthType();
			}
		}
		return AuthName;
	}

	virtual TSharedPtr<class FFriendsAndChatAnalytics> GetAnalytics() const override
	{
		return Analytics;
	}

	virtual IOnlineFriendsPtr GetFriendsInterface() const override
	{
		return OnlineSub ? OnlineSub->GetFriendsInterface() : nullptr;
	}

	virtual IOnlineUserPtr GetUserInterface() const override
	{
		return OnlineSub ? OnlineSub->GetUserInterface() : nullptr;
	}

	virtual IOnlineIdentityPtr GetOnlineIdentity() const override
	{
		return OnlineIdentity;
	}

	virtual IOnlinePartyPtr GetPartyInterface() const override
	{
		return OnlineSub ? OnlineSub->GetPartyInterface() : nullptr;
	}

	virtual IOnlineSessionPtr GetSessionInterface() const override
	{
		return OnlineSub ? OnlineSub->GetSessionInterface() : nullptr;
	}

	virtual IOnlinePresencePtr GetPresenceInterface() const override
	{
		return OnlineSub ? OnlineSub->GetPresenceInterface() : nullptr;
	}

	virtual IOnlineChatPtr GetChatInterface() const override
	{
		return OnlineSub ? OnlineSub->GetChatInterface() : nullptr;
	}

	virtual void SetUserIsOnline(EOnlinePresenceState::Type OnlineState) override
	{
		if (OnlineSub != nullptr &&
			OnlineIdentity.IsValid())
		{
			TSharedPtr<const FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
			if (UserId.IsValid())
			{
				TSharedPtr<FOnlineUserPresence> CurrentPresence;
				OnlineSub->GetPresenceInterface()->GetCachedPresence(*UserId, CurrentPresence);
				FOnlineUserPresenceStatus NewStatus;
				if (CurrentPresence.IsValid())
				{
					NewStatus = CurrentPresence->Status;
				}
				NewStatus.State = OnlineState;
				OnlineSub->GetPresenceInterface()->SetPresence(*UserId, NewStatus, OnPresenceUpdatedCompleteDelegate);
			}
		}
	}

	virtual void SetOverridePresence(const FString& PresenceID) override
	{
		if (OnlineSub != nullptr &&
			OnlineIdentity.IsValid())
		{
			TSharedPtr<const FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
			if (UserId.IsValid())
			{
				TSharedPtr<FOnlineUserPresence> CurrentPresence;
				OnlineSub->GetPresenceInterface()->GetCachedPresence(*UserId, CurrentPresence);
				FOnlineUserPresenceStatus NewStatus;
				if (CurrentPresence.IsValid())
				{
					NewStatus = CurrentPresence->Status;
				}

				NewStatus.State = EOnlinePresenceState::Online;

				FVariantData ClientIdProp;
				ClientIdProp.SetValue(PresenceID);
				NewStatus.Properties.Add(OverrideClientIdKey, ClientIdProp);
				OnlineSub->GetPresenceInterface()->SetPresence(*UserId, NewStatus, OnPresenceUpdatedCompleteDelegate);
			}
		}
	}

	void ClearOverridePresence() override
	{
		if (OnlineSub != nullptr &&
			OnlineIdentity.IsValid())
		{
			TSharedPtr<const FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
			if (UserId.IsValid())
			{
				TSharedPtr<FOnlineUserPresence> CurrentPresence;
				OnlineSub->GetPresenceInterface()->GetCachedPresence(*UserId, CurrentPresence);
				FOnlineUserPresenceStatus NewStatus;
				if (CurrentPresence.IsValid())
				{
					CurrentPresence->Status.Properties.Remove(OverrideClientIdKey);
					NewStatus = CurrentPresence->Status;
				}

				NewStatus.State = EOnlinePresenceState::Online;
				OnlineSub->GetPresenceInterface()->SetPresence(*UserId, NewStatus, OnPresenceUpdatedCompleteDelegate);
			}
		}
	}

	virtual TSharedPtr<const FUniqueNetId> GetGameSessionId() const override
	{
		if (GetOnlineIdentity().IsValid() && GetSessionInterface().IsValid())
		{
			const FNamedOnlineSession* GameSession = GetSessionInterface()->GetNamedSession(GameSessionName);
			if (GameSession != nullptr)
			{
				TSharedPtr<FOnlineSessionInfo> UserSessionInfo = GameSession->SessionInfo;
				if (UserSessionInfo.IsValid())
				{
					return GetOnlineIdentity()->CreateUniquePlayerId(UserSessionInfo->GetSessionId().ToString());
				}
			}
		}
		return nullptr;
	}

	virtual bool IsFriendInSameSession(const TSharedPtr< const IFriendItem >& FriendItem) const override
	{
		TSharedPtr<const FUniqueNetId> MySessionId = GetGameSessionId();
		bool bMySessionValid = MySessionId.IsValid() && MySessionId->IsValid();

		TSharedPtr<const FUniqueNetId> FriendSessionId = FriendItem->GetGameSessionId();
		bool bFriendSessionValid = FriendSessionId.IsValid() && FriendSessionId->IsValid();

		return (bMySessionValid && bFriendSessionValid && (*FriendSessionId == *MySessionId));
	}

	virtual bool IsFriendInSameParty(const TSharedPtr< const IFriendItem >& FriendItem) const override
	{
		if (GetOnlineIdentity().IsValid() && GetPartyInterface().IsValid())
		{
			TSharedPtr<const FUniqueNetId> LocalUserId = GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex);
			if (LocalUserId.IsValid())
			{
				IOnlinePartyPtr PartyInt = GetPartyInterface();
				if (PartyInt.IsValid())
				{
					TArray<TSharedRef<const FOnlinePartyId>> JoinedParties;
					if (PartyInt->GetJoinedParties(*LocalUserId, JoinedParties))
					{
						if (JoinedParties.Num() > 0)
						{
							TSharedPtr<IOnlinePartyJoinInfo> FriendPartyJoinInfo = GetPartyJoinInfo(FriendItem);
							if (FriendPartyJoinInfo.IsValid())
							{
								for (auto PartyId : JoinedParties)
								{
									if (*PartyId == *FriendPartyJoinInfo->GetPartyId())
									{
										return true;
									}
								}
							}
							else
							{
								// Party might be private, search manually
								TSharedRef<const FUniqueNetId> FriendUserId = FriendItem->GetUniqueID();
								for (auto PartyId : JoinedParties)
								{
									TSharedPtr<FOnlinePartyMember> PartyMemberId = PartyInt->GetPartyMember(*LocalUserId, *PartyId, *FriendUserId);
									if (PartyMemberId.IsValid())
									{
										return true;
									}
								}
							}
						}
					}
				}
			}
		}

		return false;
	}

	virtual TSharedPtr<IOnlinePartyJoinInfo> GetPartyJoinInfo(const TSharedPtr<const IFriendItem>& FriendItem) const override
	{
		TSharedPtr<IOnlinePartyJoinInfo> Result;

		if (GetOnlineIdentity().IsValid() && GetPartyInterface().IsValid())
		{
			TSharedPtr<const FUniqueNetId> UserId = GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex);
			if (UserId.IsValid())
			{
				TArray<TSharedRef<IOnlinePartyJoinInfo>> PartyInvites;
				GetPartyInterface()->GetPendingInvites(*UserId, PartyInvites);
				for (const auto& PartyInvite : PartyInvites)
				{
					if (*PartyInvite->GetSourceUserId() == *FriendItem->GetUniqueID())
					{
						Result = PartyInvite;
						break;
					}
				}

				if (!Result.IsValid())
				{
					Result = GetPartyInterface()->GetAdvertisedParty(*UserId, *FriendItem->GetUniqueID(), IOnlinePartySystem::GetPrimaryPartyTypeId());
				}
			}
		}
		return Result;
	}

	void OnPresenceUpdated(const FUniqueNetId& UserId, const bool bWasSuccessful)
	{
	}

	virtual void RequestOSS(FOnRequestOSSAccepted OnRequestOSSAcceptedDelegate) override
	{
		RequestDelegates.Add(OnRequestOSSAcceptedDelegate);
	}

	virtual void ReleaseOSS() override
	{
		bOSSInUse = false;
	}

	bool Tick(float Delta)
	{
		FlushChatAnalyticsCountdown -= Delta;
		if (FlushChatAnalyticsCountdown <= 0)
		{
			Analytics->FireEvent_FlushChatStats();
			// Reset countdown for new update
			FlushChatAnalyticsCountdown = CHAT_ANALYTICS_INTERVAL;
		}
		ProcessOSSRequests();
		return true;
	}

	void ProcessOSSRequests()
	{
		if (!bOSSInUse && RequestDelegates.Num() > 0)
		{
			bOSSInUse = true;
			RequestDelegates[0].ExecuteIfBound();
			RequestDelegates.RemoveAt(0);
		}
	}

private:

	void OnUserPrivilege(const FUniqueNetId& UniqueId, EUserPrivileges::Type Privilege, uint32 PrivilegeResult)
	{
		TSharedPtr<const FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
		if (Privilege == EUserPrivileges::CanCommunicateOnline && PrivilegeResult == 0)
		{
			bCanChatOnline = true;
		}
	}

	FOSSSchedulerImpl(TSharedPtr<FFriendsAndChatAnalytics> InAnalytics)
		: OnlineIdentity(nullptr)
		, OnlineSub(nullptr)
		, Analytics(InAnalytics)
		, bOSSInUse(false)
		, FlushChatAnalyticsCountdown(CHAT_ANALYTICS_INTERVAL)
		, bCanChatOnline(false)
	{
	}

	// Holds the Online identity
	IOnlineIdentityPtr OnlineIdentity;
	// Holds the Online Subsystem
	IOnlineSubsystem* OnlineSub;

	// Delegate to owner presence updated
	IOnlinePresence::FOnPresenceTaskCompleteDelegate OnPresenceUpdatedCompleteDelegate;

	TSharedPtr<FFriendsAndChatAnalytics> Analytics;

	int32 LocalControllerIndex;

	TArray<FOnRequestOSSAccepted> RequestDelegates;
	bool bOSSInUse;

	// Holds the ticker delegate
	FTickerDelegate UpdateFriendsTickerDelegate;
	FDelegateHandle UpdateFriendsTickerDelegateHandle;
	float FlushChatAnalyticsCountdown;
	bool bCanChatOnline;

	friend FOSSSchedulerFactory;
};

TSharedRef< FOSSScheduler > FOSSSchedulerFactory::Create(TSharedPtr<FFriendsAndChatAnalytics> Analytics)
{
	TSharedRef< FOSSSchedulerImpl > OSSScheduler(new FOSSSchedulerImpl(Analytics));
	return OSSScheduler;
}
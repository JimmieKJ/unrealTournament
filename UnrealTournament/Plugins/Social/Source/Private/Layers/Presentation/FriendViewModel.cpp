// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "FriendViewModel.h"
#include "FriendsNavigationService.h"
#include "GameAndPartyService.h"
#include "FriendsService.h"
#include "WebImageService.h"

#define LOCTEXT_NAMESPACE "FriendsAndChat"

class FFriendViewModelFactoryFactory;
class FFriendViewModelFactoryImpl;

class FFriendViewModelImpl
	: public FFriendViewModel
{
public:

	virtual void EnumerateActions(TArray<EFriendActionType::Type>& Actions, bool bFromChat = false, bool DisplayChatOption = true) override
	{
		for (uint8 ActionIndex = 0; ActionIndex < EFriendActionType::MAX_None; ActionIndex++)
		{
			EFriendActionType::Type ActionType = EFriendActionType::Type(ActionIndex);

			bool bShowAction = CanPerformAction(ActionType);

			if (ActionType == EFriendActionType::AcceptFriendRequest ||
				ActionType == EFriendActionType::IgnoreFriendRequest ||
				ActionType == EFriendActionType::CancelFriendRequest)
			{
				if (bFromChat)
				{
					bShowAction = false;
				}
			}
			if (ActionType == EFriendActionType::Chat)
			{
				if (bFromChat || !DisplayChatOption)
				{
					bShowAction = false;
				}
			}
			if (bShowAction)
			{
				Actions.Add(ActionType);
			}
		}
	}
	
	virtual const bool HasChatAction() const override
	{
		bool bIsFriendInSameSession = GamePartyService->IsFriendInSameSession(FriendItem);

		return FriendItem->GetInviteStatus() != EInviteStatus::Accepted
			|| FriendItem->IsGameJoinable()
			|| (GamePartyService->IsInJoinableGameSession() && !bIsFriendInSameSession);
	}

	virtual void PerformAction(const EFriendActionType::Type ActionType) override
	{
		switch(ActionType)
		{
			case EFriendActionType::AcceptFriendRequest : 
			{
				AcceptFriend();
				break;
			}
			case EFriendActionType::RemoveFriend :
			case EFriendActionType::IgnoreFriendRequest :
			case EFriendActionType::RejectFriendRequest:
			case EFriendActionType::CancelFriendRequest:
			{
				RemoveFriend(ActionType);
				break;
			}
			case EFriendActionType::BlockPlayerRequest :
			{
				BlockPlayer();
				break;
			}
			case EFriendActionType::UnblockPlayerRequest :
			{
				UnblockPlayer();
				break;
			}
			case EFriendActionType::MutePlayerRequest :
			{
				MutePlayer();
				break;
			}
			case EFriendActionType::UnmutePlayerRequest :
			{
				UnmutePlayer();
				break;
			}
			case EFriendActionType::SendFriendRequest : 
			{
				SendFriendRequest();
				break;
			}
			case EFriendActionType::InviteToGame : 
			{
				InviteToGame();
				break;
			}
			case EFriendActionType::JoinGame : 
			{
				JoinGame();
				break;
			}
			case EFriendActionType::RejectGame:
			{
				RejectGame();
				break;
			}
			case EFriendActionType::Chat:
			{
				StartChat();
				break;
			}
			case EFriendActionType::Import:
			{
				Import();
				break;
			}
		}
		SetPendingAction(EFriendActionType::MAX_None);
	}

	virtual void SetPendingAction(EFriendActionType::Type PendingAction) override
	{
		FriendItem->SetPendingAction(PendingAction);
	}

	virtual EFriendActionType::Type GetPendingAction() override
	{
		return FriendItem->GetPendingAction();
	}

	virtual FReply ConfirmAction() override
	{
		PerformAction(GetPendingAction());
		return FReply::Handled();
	}

	virtual FReply CancelAction() override
	{
		SetPendingAction(EFriendActionType::MAX_None);
		return FReply::Handled();
	}

	virtual bool CanPerformAction(const EFriendActionType::Type ActionType) override
	{
		switch (ActionType)
		{
			case EFriendActionType::JoinGame:
			{
				if (FriendItem->IsGameRequest())
				{
					if (FriendItem->IsGameJoinable())
					{
						return true;
					}
				}
				else if (FriendItem->GetInviteStatus() == EInviteStatus::Accepted && GamePartyService->JoinGameAllowed(GetAppId()))
				{
					if (FriendItem->IsOnline())
					{
						if (FriendItem->IsInParty())
						{
							// Party rules apply (if also in a game then party needs to reflect game state)
							return FriendItem->CanJoinParty();
						}
						if (FriendItem->IsGameJoinable())
						{ 
							// Game rules apply
							return true;
						}
					}
				}
				return false;
			}
			case EFriendActionType::InviteToGame:
			{
				if (FriendItem->GetInviteStatus() == EInviteStatus::Accepted && FriendItem->IsOnline() && FriendItem->CanInvite() && !FriendItem->IsGameRequest())
				{
					if (IsLocalPlayerInActiveParty())
					{
						// Party rules apply (if also in a game then party needs to reflect game state)
						// CanInviteToParty implies in a valid party, with enough space, and proper permissions
						const bool bIsInInvitableParty = GamePartyService->CanInviteToParty() && !GamePartyService->IsFriendInSameParty(FriendItem);
						return bIsInInvitableParty;
					}
					else
					{
						// Game rules apply
						const bool bIsJoinableGame = GamePartyService->IsInJoinableGameSession() && !GamePartyService->IsFriendInSameSession(FriendItem);
						return bIsJoinableGame;
					}
				}
				return false;
			}
			case EFriendActionType::RejectGame:
			{
				if (FriendItem->IsGameRequest())
				{
					return true;
				}
				return false;
			}
			case EFriendActionType::Updating:
			{
				if (FriendItem->IsPendingAccepted())
				{
					return true;
				}
				return false;
			}
			case EFriendActionType::SendFriendRequest:
			{
				if (FriendItem->GetListType() == EFriendsDisplayLists::RecentPlayersDisplay)
				{
					TSharedPtr<IFriendItem> ExistingFriend = FriendsService->FindUser(*FriendItem->GetUniqueID());
					if (!ExistingFriend.IsValid())
					{
						return true;
					}
				}
				return false;
			}
			case EFriendActionType::Chat:
			{
				if (FriendItem->GetInviteStatus() == EInviteStatus::Accepted && FriendItem->IsOnline() && !FriendItem->IsGameRequest())
				{
					return true;
				}
				return false;
			}
			case EFriendActionType::RemoveFriend:
			{
				if (FriendItem->GetInviteStatus() == EInviteStatus::Accepted && !FriendItem->IsGameRequest())
				{
					return true;
				}
				return false;
			}
			case EFriendActionType::AcceptFriendRequest:
			case EFriendActionType::IgnoreFriendRequest:
			case EFriendActionType::RejectFriendRequest:
			{
				if (FriendItem->GetInviteStatus() == EInviteStatus::PendingInbound)
				{
					return true;
				}
				return false;
			}
			case EFriendActionType::CancelFriendRequest:
			{
				if (FriendItem->GetInviteStatus() == EInviteStatus::PendingOutbound)
				{
					return true;
				}
				return false;
			}
			case EFriendActionType::Import:
			{
				if (FriendItem->GetListType() == EFriendsDisplayLists::SuggestedFriendsDisplay)
				{
					return true;
				}
				return false;
			}
			case EFriendActionType::BlockPlayerRequest:
			{
				if(FriendItem->GetInviteStatus() != EInviteStatus::Blocked)
				{
					return true;
				}
				return false;
			}
			case EFriendActionType::UnblockPlayerRequest:
			{
				if (FriendItem->GetListType() == EFriendsDisplayLists::BlockedPlayersDisplay)
				{
					return true;
				}
				return false;
			}
			case EFriendActionType::MutePlayerRequest:
			{
				return !FriendsService->IsPlayerMuted(FriendItem->GetUniqueID()) && !FriendItem->IsGameRequest();
			}
			case EFriendActionType::UnmutePlayerRequest:
			{
				return FriendsService->IsPlayerMuted(FriendItem->GetUniqueID()) && !FriendItem->IsGameRequest();
			}
			default:
			{
				return true;
			}
		}
	}

	virtual FText GetJoinGameDisallowReason() const override
	{
		static const FText InLauncher = LOCTEXT("GameJoinFail_InLauncher", "Please ensure Fortnite is installed and up to date");
		static const FText InSession = LOCTEXT("GameJoinFail_InSession", "Quit to the Main Menu to join a friend's game");

		if (GamePartyService->IsInLauncher())
		{
			return InLauncher;
		}
		return InSession;
	}

	~FFriendViewModelImpl()
	{
		Uninitialize();
	}


	/**
	 * Get the display name from the Epic account.  This can be blank if this is a headless accounts
	 *
	 * @return The display name from the Epic account, or a blank name for headless accounts
	 */
	const FString GetEpicName() const
	{
		return FriendItem->GetName();
	}

	/**
	 * Get all available names associated with this friend
	 *
	 * @param OutNames List of names associated with this friend sorted by order they should be displayed in
	 */
	void GetAllAvailableNames(TArray<FString>& OutNames) const
	{
		// Get names by order of preference for displaying
		FString ActivePlatformName = FFriendItem::GetNameByActivePlatform(FriendItem->GetOnlineFriend(), FriendItem->GetOnlineUser());
		if (!ActivePlatformName.IsEmpty())
		{
			OutNames.AddUnique(ActivePlatformName);
		}

		FString EpicName = GetEpicName();
		if (!EpicName.IsEmpty())
		{
			OutNames.AddUnique(EpicName);
		}

		// Add other platform names
		const TSharedPtr<FOnlineUser> OnlineUser = FriendItem->GetOnlineUser();
		if (OnlineUser.IsValid())
		{
			FString PSNName;
			if (OnlineUser->GetUserAttribute(TEXT("psn:displayname"), PSNName) &&
				!PSNName.IsEmpty())
			{
				OutNames.AddUnique(PSNName);
			}
		}
	}

	virtual const FString GetName() const override
	{
		TArray<FString> AvailableNames;
		GetAllAvailableNames(AvailableNames);
		if (AvailableNames.Num() > 0)
		{
			return AvailableNames[0];
		}
		return FString();
	}

	virtual const FString GetAlternateName() const override
	{
		// Return the name from GetAllAvailableNames() that comes after the name from GetName()
		TArray<FString> AvailableNames;
		GetAllAvailableNames(AvailableNames);
		if (AvailableNames.Num() > 1)
		{
			return AvailableNames[1];
		}
		return FString();
	}

	virtual const FString GetNameNoSpaces() const override
	{
		return NameNoSpaces;
	}

	virtual FText GetFriendLocation() const override
	{
		return FriendItem->GetFriendLocation();
	}

	virtual bool IsOnline() const override
	{
		return FriendItem->IsOnline();
	}

	virtual bool IsInGameSession() const override
	{
		return GamePartyService->IsInGameSession();
	}

	virtual bool IsLocalPlayerInActiveParty() const override
	{
		return GamePartyService->IsLocalPlayerInActiveParty();
	}

	virtual bool IsInPartyChat() const override
	{
		return GamePartyService->IsInPartyChat();
	}

	virtual const FSlateBrush* GetPresenceBrush() const override
	{
		return WebImageService->GetBrush(GetAppId());
	}

	virtual const EOnlinePresenceState::Type GetOnlineStatus() const override
	{
		return FriendItem->GetOnlineStatus();
	}

	virtual const FString GetAppId() const override
	{
		return FriendItem->GetAppId();
	}

	virtual TSharedRef<IFriendItem> GetFriendItem() const override
	{
		return FriendItem;
	}

	virtual bool DisplayPresenceScroller() const override
	{
		return FriendItem->IsGameRequest() ? false : true;
	}

	virtual bool DisplayPCIcon() const override
	{
		if (!GamePartyService->IsLocalPlayerOnPCPlatform())
		{
			return FriendItem->IsOnline() && FriendItem->IsPCPlatform();
		}
		return false;
	}

private:

	void RemoveFriend(EFriendActionType::Type Reason) const
	{
		FriendsService->DeleteFriend(FriendItem, Reason);
	}

	void BlockPlayer() const
	{
		FriendsService->BlockPlayer(FriendItem);
	}

	void UnblockPlayer() const
	{
		FriendsService->UnblockPlayer(FriendItem);
	}

	void MutePlayer() const
	{
		if(GamePartyService->IsTeamChatEnabled())
		{
			FriendsService->MutePlayer(FriendItem->GetUniqueID());
		}
	}

	void UnmutePlayer() const
	{
		if(GamePartyService->IsTeamChatEnabled())
		{
			FriendsService->UnmutePlayer(FriendItem->GetUniqueID());
		}
	}

	void AcceptFriend() const
	{
		FriendsService->AcceptFriend(FriendItem);
	}

	void SendFriendRequest() const
	{
		FriendsService->RequestFriendById(FriendItem->GetUniqueID(), FriendItem->GetName());
	}

	void Import() const
	{
		FriendsService->RequestFriendById(FriendItem->GetUniqueID(), FriendItem->GetName());
	}

	void InviteToGame()
	{
		GamePartyService->SendGameInvite(FriendItem);
	}

	void JoinGame()
	{
		if (FriendItem->IsGameRequest() || FriendItem->IsGameJoinable())
		{
			GamePartyService->AcceptGameInvite(FriendItem);
		}
		else if (GamePartyService->JoinGameAllowed(GetAppId()) && FriendItem->CanJoinParty())
		{
			GamePartyService->AcceptGameInvite(FriendItem);
		}
		else
		{
			GamePartyService->NotifyJoinPartyFail();
		}
	}

	void RejectGame()
	{
		if (FriendItem->IsGameRequest())
		{
			GamePartyService->RejectGameInvite(FriendItem);
		}
	}

	void StartChat()
	{
		if (FriendItem->IsOnline())
		{
			// Inform navigation system
			NavigationService->SetOutgoingChatFriend(FriendItem);
		}
	}

private:
	void Initialize()
	{
		NameNoSpaces = FriendItem->GetName().Replace(TEXT(" "), TEXT(""));
		WebImageService->RequestTitleIcon(GetAppId());
	}

	void Uninitialize()
	{
	}

	FFriendViewModelImpl(
		const TSharedRef<IFriendItem>& InFriendItem,
		const TSharedRef<FFriendsNavigationService>& InNavigationService,
		const TSharedRef<FFriendsService>& InFriendsService,
		const TSharedRef<FGameAndPartyService>& InGamePartyService,
		const TSharedRef<FWebImageService>& InWebImageService
		)
		: FriendItem(InFriendItem)
		, NavigationService(InNavigationService)
		, FriendsService(InFriendsService)
		, GamePartyService(InGamePartyService)
		, WebImageService(InWebImageService)
	{
	}

private:
	FString NameNoSpaces;
	TSharedRef<IFriendItem> FriendItem;
	TSharedRef<FFriendsNavigationService> NavigationService;
	TSharedRef<FFriendsService> FriendsService;
	TSharedRef<FGameAndPartyService> GamePartyService;
	TSharedRef<FWebImageService> WebImageService;

private:
	friend FFriendViewModelFactoryImpl;
};

class FFriendViewModelFactoryImpl
	: public IFriendViewModelFactory
{
public:
	virtual TSharedRef<FFriendViewModel> Create(const TSharedRef<IFriendItem>& FriendItem) override
	{
		FriendItem->SetWebImageService(WebImageService);
		TSharedRef<FFriendViewModelImpl> ViewModel = MakeShareable(
			new FFriendViewModelImpl(FriendItem, NavigationService, FriendsService, GamePartyService, WebImageService));
		ViewModel->Initialize();
		return ViewModel;
	}

	virtual ~FFriendViewModelFactoryImpl(){}

private:

	FFriendViewModelFactoryImpl(
	const TSharedRef<FFriendsNavigationService>& InNavigationService,
	const TSharedRef<FFriendsService>& InFriendsService,
	const TSharedRef<FGameAndPartyService>& InGamePartyService,
	const TSharedRef<class FWebImageService>& InWebImageService
	)
		: NavigationService(InNavigationService)
		, FriendsService(InFriendsService)
		, GamePartyService(InGamePartyService)
		, WebImageService(InWebImageService)
	{ }

private:

	const TSharedRef<FFriendsNavigationService> NavigationService;
	const TSharedRef<FFriendsService> FriendsService;
	const TSharedRef<FGameAndPartyService> GamePartyService;
	const TSharedRef<FWebImageService> WebImageService;
	friend FFriendViewModelFactoryFactory;
};

TSharedRef<IFriendViewModelFactory> FFriendViewModelFactoryFactory::Create(
	const TSharedRef<FFriendsNavigationService>& NavigationService,
	const TSharedRef<FFriendsService>& FriendsService,
	const TSharedRef<FGameAndPartyService>& GamePartyService,
	const TSharedRef<class FWebImageService>& WebImageService
	)
{
	TSharedRef<FFriendViewModelFactoryImpl> Factory = MakeShareable(
		new FFriendViewModelFactoryImpl(NavigationService, FriendsService, GamePartyService, WebImageService));

	return Factory;
}

#undef LOCTEXT_NAMESPACE

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "SFriendsContainer.h"
#include "SChatWindow.h"
#include "FriendsViewModel.h"
#include "FriendViewModel.h"
#include "FriendsStatusViewModel.h"
#include "ChatViewModel.h"
#include "SNotificationList.h"
#include "SWindowTitleBar.h"
#include "SFriendsStatus.h"
#include "FriendRecentPlayerItems.h"
#include "FriendGameInviteItem.h"

#define LOCTEXT_NAMESPACE "FriendsAndChatManager"


/* FFriendsAndChatManager structors
 *****************************************************************************/

FFriendsAndChatManager::FFriendsAndChatManager( )
	: OnlineSubMcp(nullptr)
	, MessageManager(FFriendsMessageManagerFactory::Create())
	, ManagerState ( EFriendsAndManagerState::OffLine )
	, bIsInited( false )
	, bRequiresListRefresh(false)
	, bRequiresRecentPlayersRefresh(false)
	, bCreateChatWindow(false)
{
}


FFriendsAndChatManager::~FFriendsAndChatManager( )
{ }


/* IFriendsAndChatManager interface
 *****************************************************************************/

void FFriendsAndChatManager::Login()
{
	// Clear existing data
	Logout();

	bIsInited = false;

	OnlineSubMcp = static_cast< FOnlineSubsystemMcp* >( IOnlineSubsystem::Get( TEXT( "MCP" ) ) );

	if (OnlineSubMcp != nullptr &&
		OnlineSubMcp->GetMcpAccountMappingService().IsValid() &&
		OnlineSubMcp->GetIdentityInterface().IsValid())
	{
		OnlineIdentity = OnlineSubMcp->GetIdentityInterface();

		if(OnlineIdentity->GetUniquePlayerId(0).IsValid())
		{
			IOnlineUserPtr UserInterface = OnlineSubMcp->GetUserInterface();
			check(UserInterface.IsValid());

			FriendsInterface = OnlineSubMcp->GetFriendsInterface();
			check( FriendsInterface.IsValid() )

			// Create delegates for list refreshes
			OnQueryRecentPlayersCompleteDelegate = FOnQueryRecentPlayersCompleteDelegate::CreateRaw(this, &FFriendsAndChatManager::OnQueryRecentPlayersComplete);
			OnFriendsListChangedDelegate = FOnFriendsChangeDelegate::CreateSP(this, &FFriendsAndChatManager::OnFriendsListChanged);
			OnDeleteFriendCompleteDelegate = FOnDeleteFriendCompleteDelegate::CreateSP( this, &FFriendsAndChatManager::OnDeleteFriendComplete );
			OnQueryUserIdMappingCompleteDelegate = FOnQueryUserIdMappingCompleteDelegate::CreateSP( this, &FFriendsAndChatManager::OnQueryUserIdMappingComplete );
			OnQueryUserInfoCompleteDelegate = FOnQueryUserInfoCompleteDelegate::CreateSP( this, &FFriendsAndChatManager::OnQueryUserInfoComplete );
			OnPresenceReceivedCompleteDelegate = FOnPresenceReceivedDelegate::CreateSP(this, &FFriendsAndChatManager::OnPresenceReceived);
			OnPresenceUpdatedCompleteDelegate = IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateSP(this, &FFriendsAndChatManager::OnPresenceUpdated);
			OnFriendInviteReceivedDelegate = FOnInviteReceivedDelegate::CreateSP(this, &FFriendsAndChatManager::OnFriendInviteReceived);
			OnFriendRemovedDelegate = FOnFriendRemovedDelegate::CreateSP(this, &FFriendsAndChatManager::OnFriendRemoved);
			OnFriendInviteRejected = FOnInviteRejectedDelegate::CreateSP(this, &FFriendsAndChatManager::OnInviteRejected);
			OnFriendInviteAccepted = FOnInviteAcceptedDelegate::CreateSP(this, &FFriendsAndChatManager::OnInviteAccepted);
			OnGameInviteReceivedDelegate = FOnSessionInviteReceivedDelegate::CreateSP(this, &FFriendsAndChatManager::OnGameInviteReceived);
			OnDestroySessionCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateSP(this, &FFriendsAndChatManager::OnGameDestroyed);

			FriendsInterface->AddOnQueryRecentPlayersCompleteDelegate(OnQueryRecentPlayersCompleteDelegate);
			FriendsInterface->AddOnFriendsChangeDelegate(0, OnFriendsListChangedDelegate);
			FriendsInterface->AddOnInviteReceivedDelegate(OnFriendInviteReceivedDelegate);
			FriendsInterface->AddOnFriendRemovedDelegate(OnFriendRemovedDelegate);
			FriendsInterface->AddOnInviteRejectedDelegate(OnFriendInviteRejected);
			FriendsInterface->AddOnInviteAcceptedDelegate(OnFriendInviteAccepted);
			FriendsInterface->AddOnDeleteFriendCompleteDelegate( 0, OnDeleteFriendCompleteDelegate );
			UserInterface->AddOnQueryUserInfoCompleteDelegate(0, OnQueryUserInfoCompleteDelegate);
			OnlineSubMcp->GetPresenceInterface()->AddOnPresenceReceivedDelegate(OnPresenceReceivedCompleteDelegate);
			OnlineSubMcp->GetSessionInterface()->AddOnSessionInviteReceivedDelegate(OnGameInviteReceivedDelegate);
			OnlineSubMcp->GetSessionInterface()->AddOnDestroySessionCompleteDelegate(OnDestroySessionCompleteDelegate);
			
			FOnlinePersonaMcpPtr OnlinePersonaMcp = OnlineSubMcp->GetMcpPersonaService();
			OnlinePersonaMcp->AddOnQueryUserIdMappingCompleteDelegate(OnQueryUserIdMappingCompleteDelegate);

			ManagerState = EFriendsAndManagerState::Idle;

			FriendsList.Empty();
			PendingFriendsList.Empty();

			if ( UpdateFriendsTickerDelegate.IsBound() == false )
			{
				UpdateFriendsTickerDelegate = FTickerDelegate::CreateSP( this, &FFriendsAndChatManager::Tick );
			}

			FTicker::GetCoreTicker().AddTicker( UpdateFriendsTickerDelegate );

			SetState(EFriendsAndManagerState::RequestFriendsListRefresh);
			RequestRecentPlayersListRefresh();

			MessageManager->LogIn();
			for (auto RoomName : ChatRoomstoJoin)
			{
				MessageManager->JoinPublicRoom(RoomName);
			}
		}
		else
		{
			SetState(EFriendsAndManagerState::OffLine);
		}
	}
}

void FFriendsAndChatManager::Logout()
{
	if (OnlineSubMcp != nullptr)
	{
		if (OnlineSubMcp->GetFriendsInterface().IsValid())
		{
			OnlineSubMcp->GetFriendsInterface()->ClearOnQueryRecentPlayersCompleteDelegate(OnQueryRecentPlayersCompleteDelegate);
			OnlineSubMcp->GetFriendsInterface()->ClearOnFriendsChangeDelegate(0, OnFriendsListChangedDelegate);
			OnlineSubMcp->GetFriendsInterface()->ClearOnInviteReceivedDelegate(OnFriendInviteReceivedDelegate);
			OnlineSubMcp->GetFriendsInterface()->ClearOnFriendRemovedDelegate(OnFriendRemovedDelegate);
			OnlineSubMcp->GetFriendsInterface()->ClearOnInviteRejectedDelegate(OnFriendInviteRejected);
			OnlineSubMcp->GetFriendsInterface()->ClearOnInviteAcceptedDelegate(OnFriendInviteAccepted);
			OnlineSubMcp->GetFriendsInterface()->ClearOnDeleteFriendCompleteDelegate(0, OnDeleteFriendCompleteDelegate);
		}
		if (OnlineSubMcp->GetPresenceInterface().IsValid())
		{
			OnlineSubMcp->GetPresenceInterface()->ClearOnPresenceReceivedDelegate(OnPresenceReceivedCompleteDelegate);
		}
		if (OnlineSubMcp->GetSessionInterface().IsValid())
		{
			OnlineSubMcp->GetSessionInterface()->ClearOnSessionInviteReceivedDelegate(OnGameInviteReceivedDelegate);
			OnlineSubMcp->GetSessionInterface()->ClearOnDestroySessionCompleteDelegate(OnDestroySessionCompleteDelegate);
		}
		if (OnlineSubMcp->GetUserInterface().IsValid())
		{
			OnlineSubMcp->GetUserInterface()->ClearOnQueryUserInfoCompleteDelegate(0, OnQueryUserInfoCompleteDelegate);
		}
		if (OnlineSubMcp->GetMcpPersonaService().IsValid())
		{
			OnlineSubMcp->GetMcpPersonaService()->ClearOnQueryUserIdMappingCompleteDelegate(OnQueryUserIdMappingCompleteDelegate);
		}
	}

	FriendsList.Empty();
	PendingFriendsList.Empty();
	FriendByNameRequests.Empty();
	FilteredFriendsList.Empty();
	PendingOutgoingDeleteFriendRequests.Empty();
	PendingOutgoingAcceptFriendRequests.Empty();
	PendingIncomingInvitesList.Empty();
	PendingGameInvitesList.Empty();
	NotifiedRequest.Empty();

	if ( FriendWindow.IsValid() )
	{
		FriendWindow->RequestDestroyWindow();
		FriendWindow = nullptr;
	}

	if ( ChatWindow.IsValid() )
	{
		Analytics.FlushChatStats();

		ChatWindow->RequestDestroyWindow();
		ChatWindow = nullptr;
	}

	MessageManager->LogOut();

	OnlineSubMcp = nullptr;
	if ( UpdateFriendsTickerDelegate.IsBound() )
	{
		FTicker::GetCoreTicker().RemoveTicker( UpdateFriendsTickerDelegate );
	}

	SetState(EFriendsAndManagerState::OffLine);
}

bool FFriendsAndChatManager::IsLoggedIn()
{
	return ManagerState != EFriendsAndManagerState::OffLine;
}

void FFriendsAndChatManager::SetApplicationViewModel(TSharedPtr<IFriendsApplicationViewModel> InApplicationViewModel)
{
	ApplicationViewModel = InApplicationViewModel;
}

void FFriendsAndChatManager::SetUserSettings(const FFriendsAndChatSettings& UserSettings)
{
	this->UserSettings = UserSettings;
}

void FFriendsAndChatManager::SetAnalyticsProvider(const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider)
{
	Analytics.SetProvider(AnalyticsProvider);
}

void FFriendsAndChatManager::InsertNetworkChatMessage(const FString& InMessage)
{
	MessageManager->InsertNetworkMessage(InMessage);
}

void FFriendsAndChatManager::JoinPublicChatRoom(const FString& RoomName)
{
	if (!RoomName.IsEmpty())
	{
		ChatRoomstoJoin.AddUnique(RoomName);
		if (MessageManager.IsValid())
		{
			MessageManager->JoinPublicRoom(RoomName);
		}
	}
}

// UI Creation

void FFriendsAndChatManager::CreateFriendsListWindow(const FFriendsAndChatStyle* InStyle)
{
	const FVector2D DEFAULT_WINDOW_SIZE = FVector2D(400, 458);

	Style = *InStyle;
	FFriendsAndChatModuleStyle::Initialize(Style);

	if (!FriendWindow.IsValid())
	{
		FriendWindow = SNew( SWindow )
		.Title(LOCTEXT("FFriendsAndChatManager_FriendsTitle", "Friends List"))
		.Style(&Style.WindowStyle)
		.ClientSize(DEFAULT_WINDOW_SIZE)
		.ScreenPosition(FVector2D(100, 100))
		.AutoCenter( EAutoCenter::None )
		.SizingRule( ESizingRule::FixedSize )
		.SupportsMaximize(false)
		.SupportsMinimize(true)
		.bDragAnywhere(true)
		.CreateTitleBar(false);

		FriendWindow->SetOnWindowClosed(FOnWindowClosed::CreateRaw(this, &FFriendsAndChatManager::HandleFriendsWindowClosed));

		BuildFriendsUI();
		FriendWindow = FSlateApplication::Get().AddWindow(FriendWindow.ToSharedRef());
	}
	else if(FriendWindow->IsWindowMinimized())
	{
		FriendWindow->Restore();
		BuildFriendsUI();
	}

	FriendWindow->BringToFront(true);

	// Clear notifications
	OnFriendsNotification().Broadcast(false);
}

void FFriendsAndChatManager::HandleFriendsWindowClosed(const TSharedRef<SWindow>& InWindow)
{
	FriendWindow.Reset();
}

void FFriendsAndChatManager::BuildFriendsUI()
{
	check(FriendWindow.IsValid());

	TSharedPtr<SWindowTitleBar> TitleBar;
	FriendWindow->SetContent(
		SNew(SBorder)
		.BorderImage( &Style.Background )
		.VAlign( VAlign_Fill )
		.HAlign( HAlign_Fill )
		.Padding(0)
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(TitleBar, SWindowTitleBar, FriendWindow.ToSharedRef(), nullptr, HAlign_Center)
					.Style(&Style.WindowStyle)
					.ShowAppIcon(false)
					.Title(FText::GetEmpty())
				]
				+ SVerticalBox::Slot()
				[
					SNew(SFriendsContainer, FFriendsViewModelFactory::Create(SharedThis(this)))
					.FriendStyle( &Style )
					.Method(EPopupMethod::CreateNewWindow)
				]
			]
			+SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Bottom)
			[
				SAssignNew(FriendsNotificationBox, SNotificationList)
			]
		]
	);
}


TSharedPtr< SWidget > FFriendsAndChatManager::GenerateFriendsListWidget( const FFriendsAndChatStyle* InStyle )
{
	if ( !FriendListWidget.IsValid() )
	{
		Style = *InStyle;
		FFriendsAndChatModuleStyle::Initialize(Style);
		SAssignNew(FriendListWidget, SOverlay)
		+SOverlay::Slot()
		[
			 SNew(SFriendsContainer, FFriendsViewModelFactory::Create(SharedThis(this)))
			.FriendStyle( &Style )
			.Method(EPopupMethod::UseCurrentWindow)
		]
		+SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Bottom)
		[
			SAssignNew(FriendsNotificationBox, SNotificationList)
		];
	}

	// Clear notifications
	OnFriendsNotification().Broadcast(false);

	return FriendListWidget;
}


TSharedPtr< SWidget > FFriendsAndChatManager::GenerateChatWidget(const FFriendsAndChatStyle* InStyle, TSharedRef<IChatViewModel> ViewModel)
{
	// todo - NDavies = find a better way to do this
	TSharedRef<FChatViewModel> ChatViewModel = StaticCastSharedRef<FChatViewModel>(ViewModel);
	ChatViewModel->SetInGameUI(true);
	ChatViewModel->SetCaptureFocus(true);

	TSharedPtr<SChatWindow> ChatWidget;
	Style = *InStyle;
	SAssignNew(ChatWidget, SChatWindow, ChatViewModel)
	.FriendStyle(&Style)
	.Method(EPopupMethod::UseCurrentWindow);
	return ChatWidget;
}

TSharedPtr<IChatViewModel> FFriendsAndChatManager::GetChatViewModel()
{
	return FChatViewModelFactory::Create(MessageManager.ToSharedRef());
}

void FFriendsAndChatManager::CreateChatWindow(const struct FFriendsAndChatStyle* InStyle, TSharedPtr<SWindow> Parent )
{
	const FVector2D DEFAULT_WINDOW_SIZE = FVector2D(420, 500);
	ParentWidget = Parent;
	check(MessageManager.IsValid());
	bCreateChatWindow = true;

	if (!ChatWindow.IsValid())
	{
		Style = *InStyle;
		FFriendsAndChatModuleStyle::Initialize(Style);

		ChatWindow = SNew( SWindow )
		.Title( LOCTEXT( "FriendsAndChatManager_ChatTitle", "Chat Window") )
		.Style(&Style.WindowStyle)
		.ClientSize( DEFAULT_WINDOW_SIZE )
		.ScreenPosition( FVector2D( 200, 100 ) )
		.AutoCenter( EAutoCenter::None )
		.SupportsMaximize( false )
		.SupportsMinimize( true )
		.CreateTitleBar( false )
		.SizingRule( ESizingRule::FixedSize );

		ChatWindow->SetOnWindowClosed(FOnWindowClosed::CreateRaw(this, &FFriendsAndChatManager::HandleChatWindowClosed));

		SetChatWindowContents();
		ChatWindow = FSlateApplication::Get().AddWindowAsNativeChild(ChatWindow.ToSharedRef(), Parent.ToSharedRef());
	}
	else if(ChatWindow->IsWindowMinimized())
	{
		ChatWindow->Restore();
		SetChatWindowContents();
	}
	ChatWindow->BringToFront(true);
}

void FFriendsAndChatManager::SetChatFriend( TSharedPtr< IFriendItem > FriendItem )
{
	OnChatFriendSelected().Broadcast(FriendItem);

	if(bCreateChatWindow && ParentWidget.IsValid())
	{
		CreateChatWindow(&Style, ParentWidget.Pin());
	}
}

void FFriendsAndChatManager::HandleChatWindowClosed(const TSharedRef<SWindow>& InWindow)
{
	ChatWindow.Reset();

	Analytics.FlushChatStats();
}

void FFriendsAndChatManager::SetChatWindowContents()
{
	TSharedPtr<SWindowTitleBar> TitleBar;
	TSharedPtr<FChatViewModel> ChatViewModel = FChatViewModelFactory::Create(MessageManager.ToSharedRef());
	ChatViewModel->SetInGameUI(false);
	ChatViewModel->SetCaptureFocus(false);

	TSharedRef< FFriendsStatusViewModel > StatusViewModel = FFriendsStatusViewModelFactory::Create(SharedThis(this));

	ChatWindow->SetContent(
		SNew( SBorder )
		.VAlign( VAlign_Fill )
		.HAlign( HAlign_Fill )
		.BorderImage( &Style.Background )
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(TitleBar, SWindowTitleBar, ChatWindow.ToSharedRef(), nullptr, HAlign_Center)
				.Style(&Style.WindowStyle)
				.ShowAppIcon(false)
				.Title(FText::GetEmpty())
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(20.0f, 0.0f)
			.VAlign(VAlign_Top)
			[
				SNew(SFriendsStatus, StatusViewModel)
				.FriendStyle(&Style)
				.Method(EPopupMethod::UseCurrentWindow)
			]
			+ SVerticalBox::Slot()
			[
				SNew(SChatWindow, ChatViewModel.ToSharedRef())
				.FriendStyle( &Style )
			]
		]);
}
// Actions

void FFriendsAndChatManager::SetUserIsOnline(EOnlinePresenceState::Type OnlineState)
{
	if ( OnlineSubMcp != nullptr )
	{
		TSharedPtr<FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
		if (UserId.IsValid())
		{
			TSharedPtr<FOnlineUserPresence> CurrentPresence;
			OnlineSubMcp->GetPresenceInterface()->GetCachedPresence(*UserId, CurrentPresence);
			FOnlineUserPresenceStatus NewStatus;
			if (CurrentPresence.IsValid())
			{
				NewStatus = CurrentPresence->Status;
			}
			NewStatus.State = OnlineState;
			OnlineSubMcp->GetPresenceInterface()->SetPresence(*UserId, NewStatus, OnPresenceUpdatedCompleteDelegate);
		}
	}
}

void FFriendsAndChatManager::AcceptFriend( TSharedPtr< IFriendItem > FriendItem )
{
	PendingOutgoingAcceptFriendRequests.Add(FUniqueNetIdString(FriendItem->GetOnlineUser()->GetUserId()->ToString()));
	FriendItem->SetPendingAccept();
	RefreshList();
	OnFriendsNotification().Broadcast(false);

	Analytics.RecordFriendAction(*FriendItem, TEXT("Social.FriendAction.Accept"));
}


void FFriendsAndChatManager::RejectFriend( TSharedPtr< IFriendItem > FriendItem )
{
	NotifiedRequest.Remove( FriendItem->GetOnlineUser()->GetUserId() );
	PendingOutgoingDeleteFriendRequests.Add(FUniqueNetIdString(FriendItem->GetOnlineUser()->GetUserId().Get().ToString()));
	FriendsList.Remove( FriendItem );
	RefreshList();
	OnFriendsNotification().Broadcast(false);

	Analytics.RecordFriendAction(*FriendItem, TEXT("Social.FriendAction.Reject"));
}

void FFriendsAndChatManager::DeleteFriend( TSharedPtr< IFriendItem > FriendItem )
{
	TSharedRef<FUniqueNetId> UserID = FriendItem->GetOnlineFriend()->GetUserId();
	NotifiedRequest.Remove( UserID );
	PendingOutgoingDeleteFriendRequests.Add(FUniqueNetIdString(UserID.Get().ToString()));
	FriendsList.Remove( FriendItem );
	FriendItem->SetPendingDelete();
	RefreshList();
	// Clear notifications
	OnFriendsNotification().Broadcast(false);

	Analytics.RecordFriendAction(*FriendItem, TEXT("Social.FriendAction.Delete"));
}

void FFriendsAndChatManager::RequestFriend( const FText& FriendName )
{
	if ( !FriendName.IsEmpty() )
	{
		FriendByNameRequests.AddUnique(*FriendName.ToString());
	}
}

// Process action responses

FReply FFriendsAndChatManager::HandleMessageAccepted( TSharedPtr< FFriendsAndChatMessage > ChatMessage, EFriendsResponseType::Type ResponseType )
{
	switch ( ResponseType )
	{
	case EFriendsResponseType::Response_Accept:
		{
			PendingOutgoingAcceptFriendRequests.Add(FUniqueNetIdString(ChatMessage->GetUniqueID()->ToString()));
			TSharedPtr< IFriendItem > User = FindUser(ChatMessage->GetUniqueID().Get());
			if ( User.IsValid() )
			{
				AcceptFriend(User);
			}
		}
		break;
	case EFriendsResponseType::Response_Reject:
		{
			NotifiedRequest.Remove( ChatMessage->GetUniqueID() );
			TSharedPtr< IFriendItem > User = FindUser( ChatMessage->GetUniqueID().Get());
			if ( User.IsValid() )
			{
				RejectFriend(User);
			}
		}
		break;
	}

	NotificationMessages.Remove( ChatMessage );

	return FReply::Handled();
}

// Getters

int32 FFriendsAndChatManager::GetFilteredFriendsList( TArray< TSharedPtr< IFriendItem > >& OutFriendsList )
{
	OutFriendsList = FilteredFriendsList;
	return OutFriendsList.Num();
}

TArray< TSharedPtr< IFriendItem > >& FFriendsAndChatManager::GetrecentPlayerList()
{
	return RecentPlayersList;
}

int32 FFriendsAndChatManager::GetFilteredOutgoingFriendsList( TArray< TSharedPtr< IFriendItem > >& OutFriendsList )
{
	OutFriendsList = FilteredOutgoingList;
	return OutFriendsList.Num();
}

int32 FFriendsAndChatManager::GetFilteredGameInviteList(TArray< TSharedPtr< IFriendItem > >& OutFriendsList)
{
	for (auto It = PendingGameInvitesList.CreateConstIterator(); It; ++It)
	{
		OutFriendsList.Add(It.Value());
	}
	return OutFriendsList.Num();
}

FString FFriendsAndChatManager::GetGameSessionId() const
{
	FString Result;
	if (OnlineSubMcp != nullptr &&
		OnlineIdentity.IsValid() &&
		OnlineSubMcp->GetSessionInterface().IsValid())
	{
		const FNamedOnlineSession* GameSession = OnlineSubMcp->GetSessionInterface()->GetNamedSession(GameSessionName);
		if (GameSession != nullptr)
		{
			TSharedPtr<FOnlineSessionInfo> UserSessionInfo = GameSession->SessionInfo;
			if (UserSessionInfo.IsValid())
			{
				Result = UserSessionInfo->GetSessionId().ToString();
			}
		}
	}
	return Result;
}

bool FFriendsAndChatManager::IsInGameSession() const
{	
	if (OnlineSubMcp != nullptr &&
		OnlineIdentity.IsValid() &&
		OnlineSubMcp->GetSessionInterface().IsValid() &&
		OnlineSubMcp->GetSessionInterface()->GetNamedSession(GameSessionName) != nullptr)
	{
		return true;
	}
	return false;
}

bool FFriendsAndChatManager::IsInJoinableGameSession() const
{
	bool bIsGameJoinable = false;

	if (OnlineSubMcp != nullptr &&
		OnlineSubMcp->GetIdentityInterface().IsValid() &&
		OnlineSubMcp->GetPresenceInterface().IsValid())
	{
		TSharedPtr<FUniqueNetId> CurrentUserId = OnlineSubMcp->GetIdentityInterface()->GetUniquePlayerId(0);
		TSharedPtr<FOnlineUserPresence> Presence;
		OnlineSubMcp->GetPresenceInterface()->GetCachedPresence(*CurrentUserId, Presence);
		if (Presence.IsValid())
		{
			bIsGameJoinable = Presence->bIsJoinable;
		}
	}

	return bIsGameJoinable && IsInGameSession();
}

bool FFriendsAndChatManager::JoinGameAllowed()
{
	if (AllowFriendsJoinGame().IsBound())
	{
		return AllowFriendsJoinGame().Execute();
	}
	else if (ApplicationViewModel.IsValid())
	{
		return ApplicationViewModel->IsAppJoinable();
	}
	return false;
}

EOnlinePresenceState::Type FFriendsAndChatManager::GetUserIsOnline()
{
	if (OnlineSubMcp != nullptr)
	{
		TSharedPtr<FOnlineUserPresence> Presence;
		TSharedPtr<FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
		if(UserId.IsValid())
		{
			OnlineSubMcp->GetPresenceInterface()->GetCachedPresence(*UserId, Presence);
			if(Presence.IsValid())
			{
				return Presence->Status.State;
			}
		}
	}
	return EOnlinePresenceState::Offline;
}

// List processing

void FFriendsAndChatManager::RequestListRefresh()
{
	bRequiresListRefresh = true;
}

void FFriendsAndChatManager::RequestRecentPlayersListRefresh()
{
	bRequiresRecentPlayersRefresh = true;
}

bool FFriendsAndChatManager::Tick( float Delta )
{
	if ( ManagerState == EFriendsAndManagerState::Idle )
	{
		if ( FriendByNameRequests.Num() > 0 )
		{
			SetState(EFriendsAndManagerState::RequestingFriendName );
		}
		else if ( PendingOutgoingDeleteFriendRequests.Num() > 0 )
		{
			SetState( EFriendsAndManagerState::DeletingFriends );
		}
		else if ( PendingOutgoingAcceptFriendRequests.Num() > 0 )
		{
			SetState( EFriendsAndManagerState::AcceptingFriendRequest );
		}
		else if ( PendingIncomingInvitesList.Num() > 0 )
		{
			SendFriendInviteNotification();
		}
		else if (bRequiresListRefresh)
		{
			bRequiresListRefresh = false;
			SetState( EFriendsAndManagerState::RequestFriendsListRefresh );
		}
		else if (bRequiresRecentPlayersRefresh)
		{
			bRequiresRecentPlayersRefresh = false;
			SetState( EFriendsAndManagerState::RequestRecentPlayersListRefresh );
		}
		else if (ReceivedGameInvites.Num() > 0)
		{
			SetState(EFriendsAndManagerState::RequestGameInviteRefresh);
		}
	}
	return true;
}

void FFriendsAndChatManager::SetState( EFriendsAndManagerState::Type NewState )
{
	ManagerState = NewState;

	switch ( NewState )
	{
	case EFriendsAndManagerState::Idle:
		{
			// Do nothing in this state
		}
		break;
	case EFriendsAndManagerState::RequestFriendsListRefresh:
		{
			FOnReadFriendsListComplete Delegate = FOnReadFriendsListComplete::CreateSP(this, &FFriendsAndChatManager::OnReadFriendsListComplete);
			if (FriendsInterface->ReadFriendsList(0, EFriendsLists::ToString(EFriendsLists::Default), Delegate))
			{
				SetState(EFriendsAndManagerState::RequestingFriendsList);
			}
			else
			{
				SetState(EFriendsAndManagerState::Idle);
				RequestListRefresh();
			}
		}
		break;
	case EFriendsAndManagerState::RequestRecentPlayersListRefresh:
		{
			TSharedPtr<FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
			if (UserId.IsValid() &&
				FriendsInterface->QueryRecentPlayers(*UserId))
			{
				SetState(EFriendsAndManagerState::RequestingRecentPlayersIDs);
			}
			else
			{
				SetState(EFriendsAndManagerState::Idle);
				RequestRecentPlayersListRefresh();
			}
		}
		break;
	case EFriendsAndManagerState::RequestingFriendsList:
	case EFriendsAndManagerState::RequestingRecentPlayersIDs:
		{
			// Do Nothing
		}
		break;
	case EFriendsAndManagerState::ProcessFriendsList:
		{
			if ( ProcessFriendsList() )
			{
				RefreshList();
			}
			SetState( EFriendsAndManagerState::Idle );
		}
		break;
	case EFriendsAndManagerState::RequestingFriendName:
		{
			SendFriendRequests();
		}
		break;
	case EFriendsAndManagerState::DeletingFriends:
		{
			FriendsInterface->DeleteFriend( 0, PendingOutgoingDeleteFriendRequests[0], EFriendsLists::ToString( EFriendsLists::Default ) );
		}
		break;
	case EFriendsAndManagerState::AcceptingFriendRequest:
		{
			FOnAcceptInviteComplete Delegate = FOnAcceptInviteComplete::CreateSP(this, &FFriendsAndChatManager::OnAcceptInviteComplete);
			FriendsInterface->AcceptInvite( 0, PendingOutgoingAcceptFriendRequests[0], EFriendsLists::ToString( EFriendsLists::Default ), Delegate );
		}
		break;
	case EFriendsAndManagerState::RequestGameInviteRefresh:
		{	
			// process invites and remove entries that are completed
			ProcessReceivedGameInvites();
			if (!RequestGameInviteUserInfo())
			{
				SetState(EFriendsAndManagerState::Idle);
			}			
		}
		break;
	default:
		break;
	}
}

void FFriendsAndChatManager::OnReadFriendsListComplete( int32 LocalPlayer, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr )
{
	PreProcessList(ListName);
}

void FFriendsAndChatManager::OnQueryRecentPlayersComplete(const FUniqueNetId& UserId, bool bWasSuccessful, const FString& ErrorStr)
{
	bool bFoundAllIds = true;

	if (bWasSuccessful)
	{
		RecentPlayersList.Empty();
		TArray< TSharedRef<FOnlineRecentPlayer> > Players;
		if (FriendsInterface->GetRecentPlayers(UserId, Players))
		{
			for (const auto& RecentPlayer : Players)
			{
				if(RecentPlayer->GetDisplayName().IsEmpty())
				{
					QueryUserIds.Add(RecentPlayer->GetUserId());
				}
				else
				{
					RecentPlayersList.Add(MakeShareable(new FFriendRecentPlayerItem(RecentPlayer)));
				}
			}
			
			if(QueryUserIds.Num())
			{
				check(OnlineSubMcp != nullptr && OnlineSubMcp->GetMcpPersonaService().IsValid());
				IOnlineUserPtr UserInterface = OnlineSubMcp->GetUserInterface();
				UserInterface->QueryUserInfo(0, QueryUserIds);
				bFoundAllIds = false;
			}
			else
			{
				OnFriendsListUpdated().Broadcast();
			}
		}
	}

	if(bFoundAllIds)
	{
		SetState(EFriendsAndManagerState::Idle);
	}
}

void FFriendsAndChatManager::PreProcessList(const FString& ListName)
{
	TArray< TSharedRef<FOnlineFriend> > Friends;
	bool bReadyToChangeState = true;

	if ( FriendsInterface->GetFriendsList(0, ListName, Friends) )
	{
		if (Friends.Num() > 0)
		{
			for ( const auto& Friend : Friends)
			{
				TSharedPtr< IFriendItem > ExistingFriend = FindUser(Friend->GetUserId().Get());
				if ( ExistingFriend.IsValid() )
				{
					if (Friend->GetInviteStatus() != ExistingFriend->GetOnlineFriend()->GetInviteStatus() || (ExistingFriend->IsPendingAccepted() && Friend->GetInviteStatus() == EInviteStatus::Accepted))
					{
						ExistingFriend->SetOnlineFriend(Friend);
					}
					PendingFriendsList.Add(ExistingFriend);
				}
				else
				{
					QueryUserIds.Add(Friend->GetUserId());
				}
			}
		}

		check(OnlineSubMcp != nullptr && OnlineSubMcp->GetMcpPersonaService().IsValid());
		IOnlineUserPtr UserInterface = OnlineSubMcp->GetUserInterface();

		if ( QueryUserIds.Num() > 0 )
		{
			UserInterface->QueryUserInfo(0, QueryUserIds);
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

void FFriendsAndChatManager::OnQueryUserInfoComplete( int32 LocalPlayer, bool bWasSuccessful, const TArray< TSharedRef<class FUniqueNetId> >& UserIds, const FString& ErrorStr )
{
	if( ManagerState == EFriendsAndManagerState::RequestingFriendsList)
	{
		check(OnlineSubMcp != nullptr && OnlineSubMcp->GetMcpPersonaService().IsValid());
		IOnlineUserPtr UserInterface = OnlineSubMcp->GetUserInterface();

		for ( int32 UserIdx=0; UserIdx < UserIds.Num(); UserIdx++ )
		{
			TSharedPtr<FOnlineFriend> OnlineFriend = FriendsInterface->GetFriend( 0, *UserIds[UserIdx], EFriendsLists::ToString( EFriendsLists::Default ) );
			TSharedPtr<FOnlineUser> OnlineUser = OnlineSubMcp->GetUserInterface()->GetUserInfo( LocalPlayer, *UserIds[UserIdx] );
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
					TSharedPtr< FFriendItem > FriendItem = MakeShareable(new FFriendItem(OnlineFriend, OnlineUser, EFriendsDisplayLists::DefaultDisplay));
					PendingFriendsList.Add(FriendItem);
				}
			}
			else
			{
				UE_LOG(LogOnline, Log, TEXT("PlayerId=%s not found"), *UserIds[UserIdx]->ToDebugString());
			}
		}

		QueryUserIds.Empty();

		SetState( EFriendsAndManagerState::ProcessFriendsList );
	}
	else if(ManagerState == EFriendsAndManagerState::RequestingRecentPlayersIDs)
	{
		RecentPlayersList.Empty();
		TSharedPtr<FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
		if (UserId.IsValid())
		{
			TArray< TSharedRef<FOnlineRecentPlayer> > Players;
			if (FriendsInterface->GetRecentPlayers(*UserId, Players))
			{
				for (const auto& RecentPlayer : Players)
				{
					TSharedRef<FFriendRecentPlayerItem> RecentPlayerItem = MakeShareable(new FFriendRecentPlayerItem(RecentPlayer));
					TSharedPtr<FOnlineUser> OnlineUser = OnlineSubMcp->GetUserInterface()->GetUserInfo(LocalPlayer, *RecentPlayer->GetUserId());
					RecentPlayerItem->SetOnlineUser(OnlineUser);
					RecentPlayersList.Add(RecentPlayerItem);
				}
			}
		}
		OnFriendsListUpdated().Broadcast();
		SetState(EFriendsAndManagerState::Idle);
	}
	else if (ManagerState == EFriendsAndManagerState::RequestGameInviteRefresh)
	{
		ProcessReceivedGameInvites();
		ReceivedGameInvites.Empty();
		SetState(EFriendsAndManagerState::Idle);
	}
}

bool FFriendsAndChatManager::ProcessFriendsList()
{
	/** Functor for comparing friends list */
	struct FCompareGroupByName
	{
		FORCEINLINE bool operator()( const TSharedPtr< IFriendItem > A, const TSharedPtr< IFriendItem > B ) const
		{
			check( A.IsValid() );
			check ( B.IsValid() );
			return ( A->GetName() > B->GetName() );
		}
	};

	bool bChanged = false;
	// Early check if list has changed
	if ( PendingFriendsList.Num() != FriendsList.Num() )
	{
		bChanged = true;
	}
	else
	{
		// Need to check each item
		FriendsList.Sort( FCompareGroupByName() );
		PendingFriendsList.Sort( FCompareGroupByName() );
		for ( int32 Index = 0; Index < FriendsList.Num(); Index++ )
		{
			if (PendingFriendsList[Index]->IsUpdated() || FriendsList[Index]->GetUniqueID() != PendingFriendsList[Index]->GetUniqueID())
			{
				bChanged = true;
				break;
			}
		}
	}

	if ( bChanged )
	{
		PendingIncomingInvitesList.Empty();

		for ( int32 Index = 0; Index < PendingFriendsList.Num(); Index++ )
		{
			PendingFriendsList[Index]->ClearUpdated();
			EInviteStatus::Type FriendStatus = PendingFriendsList[ Index ].Get()->GetOnlineFriend()->GetInviteStatus();
			if ( FriendStatus == EInviteStatus::PendingInbound )
			{
				if ( NotifiedRequest.Contains( PendingFriendsList[ Index ].Get()->GetUniqueID() ) == false )
				{
					PendingIncomingInvitesList.Add( PendingFriendsList[ Index ] );
					NotifiedRequest.Add( PendingFriendsList[ Index ]->GetUniqueID() );
				}
			}
		}
		FriendByNameInvites.Empty();
		FriendsList = PendingFriendsList;
	}

	PendingFriendsList.Empty();

	return bChanged;
}

void FFriendsAndChatManager::RefreshList()
{
	FilteredFriendsList.Empty();

	for( const auto& Friend : FriendsList)
	{
		if( !Friend->IsPendingDelete())
		{
			FilteredFriendsList.Add( Friend );
		}
	}

	OnFriendsListUpdated().Broadcast();
}

void FFriendsAndChatManager::SendFriendRequests()
{
	// Invite Friends
	FOnlinePersonaMcpPtr OnlinePersonaMcp = OnlineSubMcp->GetMcpPersonaService();
	TSharedPtr<FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
	if (UserId.IsValid())
	{
		for (int32 Index = 0; Index < FriendByNameRequests.Num(); Index++)
		{
			OnlinePersonaMcp->QueryUserIdMapping(*UserId, FriendByNameRequests[Index]);
		}
	}
}

TSharedPtr< FUniqueNetId > FFriendsAndChatManager::FindUserID( const FString& InUsername )
{
	for ( int32 Index = 0; Index < FriendsList.Num(); Index++ )
	{
		if ( FriendsList[ Index ]->GetOnlineUser()->GetDisplayName() == InUsername )
		{
			return FriendsList[ Index ]->GetUniqueID();
		}
	}
	return nullptr;
}

TSharedPtr< IFriendItem > FFriendsAndChatManager::FindUser(const FUniqueNetId& InUserID)
{
	for ( const auto& Friend : FriendsList)
	{
		if (Friend->GetUniqueID().Get() == InUserID)
		{
			return Friend;
		}
	}

	return nullptr;
}

TSharedPtr< IFriendItem > FFriendsAndChatManager::FindRecentPlayer(const FUniqueNetId& InUserID)
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

TSharedPtr<FFriendViewModel> FFriendsAndChatManager::GetFriendViewModel(const FUniqueNetId& InUserID)
{
	TSharedPtr<IFriendItem> FoundFriend = FindUser(InUserID);
	if(FoundFriend.IsValid())
	{
		return FFriendViewModelFactory::Create(FoundFriend.ToSharedRef());
	}
	return nullptr;
}

void FFriendsAndChatManager::SendFriendInviteNotification()
{
	for( const auto& FriendRequest : PendingIncomingInvitesList)
	{
		if(OnFriendsActionNotification().IsBound())
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Username"), FText::FromString(FriendRequest->GetName()));
			const FText FriendRequestMessage = FText::Format(LOCTEXT("FFriendsAndChatManager_AddedYou", "Friend request from {Username}"), Args);

			TSharedPtr< FFriendsAndChatMessage > NotificationMessage = MakeShareable(new FFriendsAndChatMessage(FriendRequestMessage.ToString(), FriendRequest->GetUniqueID()));
			NotificationMessage->SetButtonCallback( FOnClicked::CreateSP(this, &FFriendsAndChatManager::HandleMessageAccepted, NotificationMessage, EFriendsResponseType::Response_Accept));
			NotificationMessage->SetButtonCallback( FOnClicked::CreateSP(this, &FFriendsAndChatManager::HandleMessageAccepted, NotificationMessage, EFriendsResponseType::Response_Reject));
			NotificationMessage->SetButtonDescription(LOCTEXT("FFriendsAndChatManager_Accept", "Accept"));
			NotificationMessage->SetButtonDescription(LOCTEXT("FFriendsAndChatManager_Reject", "Reject"));
			NotificationMessage->SetMessageType(EFriendsRequestType::FriendInvite);
			OnFriendsActionNotification().Broadcast(NotificationMessage.ToSharedRef());
		}
	}

	PendingIncomingInvitesList.Empty();
	FriendsListNotificationDelegate.Broadcast(true);
}

void FFriendsAndChatManager::SendInviteAcceptedNotification(const TSharedPtr< IFriendItem > Friend)
{
	if(OnFriendsActionNotification().IsBound())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Username"), FText::FromString(Friend->GetName()));
		const FText FriendRequestMessage = FText::Format(LOCTEXT("FFriendsAndChatManager_Accepted", "{Username} accepted your request"), Args);

		TSharedPtr< FFriendsAndChatMessage > NotificationMessage = MakeShareable(new FFriendsAndChatMessage(FriendRequestMessage.ToString()));
		NotificationMessage->SetMessageType(EFriendsRequestType::FriendAccepted);
		OnFriendsActionNotification().Broadcast(NotificationMessage.ToSharedRef());
	}
}

void FFriendsAndChatManager::OnQueryUserIdMappingComplete(bool bWasSuccessful, const FUniqueNetId& RequestingUserId, const FString& DisplayName, const FUniqueNetId& IdentifiedUserId, const FString& Error)
{
	check( OnlineSubMcp != nullptr && OnlineSubMcp->GetMcpPersonaService().IsValid() );

	EFindFriendResult::Type FindFriendResult = EFindFriendResult::NotFound;

	if ( bWasSuccessful && IdentifiedUserId.IsValid() )
	{
		TSharedPtr<IFriendItem> ExistingFriend = FindUser(IdentifiedUserId);
		if (ExistingFriend.IsValid())
		{
			if (ExistingFriend->GetInviteStatus() == EInviteStatus::Accepted)
			{
				AddFriendsToast(FText::FromString("Already friends"));

				FindFriendResult = EFindFriendResult::AlreadyFriends;
			}
			else
			{
				AddFriendsToast(FText::FromString("Friend already requested"));

				FindFriendResult = EFindFriendResult::FriendsPending;
			}
		}
		else if (IdentifiedUserId == RequestingUserId)
		{
			AddFriendsToast(FText::FromString("Can't friend yourself"));

			FindFriendResult = EFindFriendResult::AddingSelfFail;
		}
		else
		{
			TSharedPtr<FUniqueNetId> FriendId = OnlineIdentity->CreateUniquePlayerId(IdentifiedUserId.ToString());
			PendingOutgoingFriendRequests.Add(FriendId.ToSharedRef());
			FriendByNameInvites.AddUnique(DisplayName);

			FindFriendResult = EFindFriendResult::Success;
		}
	}
	else
	{
		const FString DiplayMessage = DisplayName +  TEXT(" not found");
		AddFriendsToast(FText::FromString(DiplayMessage));
	}

	bool bRecentPlayer = false;
	if (FindFriendResult == EFindFriendResult::Success)
	{
		bRecentPlayer = FindRecentPlayer(IdentifiedUserId).IsValid();
	}
	Analytics.RecordAddFriend(DisplayName, IdentifiedUserId, FindFriendResult, bRecentPlayer, TEXT("Social.AddFriend"));

	FriendByNameRequests.Remove( DisplayName );
	if ( FriendByNameRequests.Num() == 0 )
	{
		if ( PendingOutgoingFriendRequests.Num() > 0 )
		{
			for ( int32 Index = 0; Index < PendingOutgoingFriendRequests.Num(); Index++ )
			{
				FOnSendInviteComplete Delegate = FOnSendInviteComplete::CreateSP(this, &FFriendsAndChatManager::OnSendInviteComplete);
				FriendsInterface->SendInvite(0, PendingOutgoingFriendRequests[Index].Get(), EFriendsLists::ToString( EFriendsLists::Default ), Delegate);
				AddFriendsToast(FText::FromString("Request Sent"));
			}
		}
		else
		{
			RefreshList();
			SetState(EFriendsAndManagerState::Idle);
		}
	}
}


void FFriendsAndChatManager::OnSendInviteComplete( int32 LocalPlayer, bool bWasSuccessful, const FUniqueNetId& FriendId, const FString& ListName, const FString& ErrorStr )
{
	PendingOutgoingFriendRequests.RemoveAt( 0 );

	if ( PendingOutgoingFriendRequests.Num() == 0 )
	{
		RefreshList();
		RequestListRefresh();
		SetState(EFriendsAndManagerState::Idle);
	}
}

void FFriendsAndChatManager::OnPresenceReceived( const FUniqueNetId& UserId, const TSharedRef<FOnlineUserPresence>& Presence)
{
	RefreshList();
}

void FFriendsAndChatManager::OnPresenceUpdated(const FUniqueNetId& UserId, const bool bWasSuccessful)
{
	RefreshList();
}

void FFriendsAndChatManager::OnFriendsListChanged()
{
	// TODO: May need to do something here
}

void FFriendsAndChatManager::OnFriendInviteReceived(const FUniqueNetId& UserId, const FUniqueNetId& FriendId)
{
	RequestListRefresh();
}

void FFriendsAndChatManager::OnGameInviteReceived(const FUniqueNetId& UserId, const FUniqueNetId& FromId, const FOnlineSessionSearchResult& InviteResult)
{
	if (OnlineSubMcp != NULL &&
		OnlineSubMcp->GetIdentityInterface().IsValid())
	{
		TSharedPtr<FUniqueNetId> FromIdPtr = OnlineSubMcp->GetIdentityInterface()->CreateUniquePlayerId(FromId.ToString());
		if (FromIdPtr.IsValid())
		{
			ReceivedGameInvites.AddUnique(FReceivedGameInvite(FromIdPtr.ToSharedRef(), InviteResult));
		}
	}
}

void FFriendsAndChatManager::ProcessReceivedGameInvites()
{
	if (OnlineSubMcp != NULL &&
		OnlineSubMcp->GetUserInterface().IsValid())
	{
		for (int32 Idx = 0; Idx < ReceivedGameInvites.Num(); Idx++)
		{
			const FReceivedGameInvite& Invite = ReceivedGameInvites[Idx];

			TSharedPtr<FOnlineUser> UserInfo;
			TSharedPtr<IFriendItem> Friend = FindUser(*Invite.FromId);
			if (Friend.IsValid())
			{
				UserInfo = Friend->GetOnlineUser();
			}
			if (!UserInfo.IsValid())
			{
				UserInfo = OnlineSubMcp->GetUserInterface()->GetUserInfo(0, *Invite.FromId);
			}
			if (UserInfo.IsValid())
			{
				TSharedPtr<FFriendGameInviteItem> GameInvite = MakeShareable(
					new FFriendGameInviteItem(UserInfo.ToSharedRef(), Invite.InviteResult)
					);

				PendingGameInvitesList.Add(Invite.FromId->ToString(), GameInvite);

				OnGameInvitesUpdated().Broadcast();
				SendGameInviteNotification(GameInvite);

				ReceivedGameInvites.RemoveAt(Idx--);
			}
		}
	}
}

bool FFriendsAndChatManager::RequestGameInviteUserInfo()
{
	bool bPending = false;

	// query for user ids that are not already cached from game invites
	IOnlineUserPtr UserInterface = OnlineSubMcp->GetUserInterface();
	IOnlineIdentityPtr IdentityInterface = OnlineSubMcp->GetIdentityInterface();
	if (UserInterface.IsValid() &&
		IdentityInterface.IsValid())
	{
		TArray<TSharedRef<FUniqueNetId>> GameInviteUserIds;
		for (auto GameInvite : ReceivedGameInvites)
		{
			GameInviteUserIds.Add(GameInvite.FromId);
		}
		if (GameInviteUserIds.Num() > 0)
		{
			UserInterface->QueryUserInfo(0, GameInviteUserIds);
			bPending = true;
		}
	}

	return bPending;
}

void FFriendsAndChatManager::SendGameInviteNotification(const TSharedPtr<IFriendItem>& FriendItem)
{
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Username"), FText::FromString(FriendItem->GetName()));
		const FText FriendRequestMessage = FText::Format(LOCTEXT("FFriendsAndChatManager_GameInvite", "Game invite from {Username}"), Args);

		TSharedPtr< FFriendsAndChatMessage > NotificationMessage = MakeShareable(new FFriendsAndChatMessage(FriendRequestMessage.ToString()));
		NotificationMessage->SetMessageType(EFriendsRequestType::GameInvite);
		OnFriendsActionNotification().Broadcast(NotificationMessage.ToSharedRef());
	}
}

void FFriendsAndChatManager::OnGameDestroyed(const FName SessionName, bool bWasSuccessful)
{
	if (SessionName == GameSessionName)
	{
		RequestRecentPlayersListRefresh();
	}
}

void FFriendsAndChatManager::RejectGameInvite(const TSharedPtr<IFriendItem>& FriendItem)
{
	TSharedPtr<IFriendItem>* Existing = PendingGameInvitesList.Find(FriendItem->GetUniqueID()->ToString());
	if (Existing != nullptr)
	{
		(*Existing)->SetPendingDelete();
		PendingGameInvitesList.Remove(FriendItem->GetUniqueID()->ToString());
	}
	// update game invite UI
	OnGameInvitesUpdated().Broadcast();

	Analytics.RecordGameInvite(*FriendItem->GetUniqueID(), TEXT("Social.GameInvite.Reject"));
}

void FFriendsAndChatManager::AcceptGameInvite(const TSharedPtr<IFriendItem>& FriendItem)
{
	TSharedPtr<IFriendItem>* Existing = PendingGameInvitesList.Find(FriendItem->GetUniqueID()->ToString());
	if (Existing != nullptr)
	{
		(*Existing)->SetPendingDelete();
		PendingGameInvitesList.Remove(FriendItem->GetUniqueID()->ToString());
	}
	// update game invite UI
	OnGameInvitesUpdated().Broadcast();
	// notify for further processing of join game request 
	OnFriendsJoinGame().Broadcast(*FriendItem->GetUniqueID(), FriendItem->GetGameSessionId());

	if(ApplicationViewModel.IsValid())
	{
		const FString AdditionalCommandline = TEXT("-invitesession=") + FriendItem->GetGameSessionId() + TEXT(" -invitefrom=") + FriendItem->GetUniqueID()->ToString();
		ApplicationViewModel->LaunchFriendApp(AdditionalCommandline);
	}

	Analytics.RecordGameInvite(*FriendItem->GetUniqueID(), TEXT("Social.GameInvite.Accept"));
}

void FFriendsAndChatManager::SendGameInvite(const TSharedPtr<IFriendItem>& FriendItem)
{
	SendGameInvite(*FriendItem->GetUniqueID());
}

void FFriendsAndChatManager::SendGameInvite(const FUniqueNetId& ToUser)
{
	if (OnlineSubMcp != nullptr &&
		OnlineIdentity.IsValid() &&
		OnlineSubMcp->GetSessionInterface().IsValid() &&
		OnlineSubMcp->GetSessionInterface()->GetNamedSession(GameSessionName) != nullptr)
	{
		TSharedPtr<FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
		if (UserId.IsValid())
		{
			OnlineSubMcp->GetSessionInterface()->SendSessionInviteToFriend(*UserId, GameSessionName, ToUser);

			Analytics.RecordGameInvite(ToUser, TEXT("Social.GameInvite.Send"));
		}
	}
}

void FFriendsAndChatManager::OnFriendRemoved(const FUniqueNetId& UserId, const FUniqueNetId& FriendId)
{
	RequestListRefresh();
}

void FFriendsAndChatManager::OnInviteRejected(const FUniqueNetId& UserId, const FUniqueNetId& FriendId)
{
	RequestListRefresh();
}

void FFriendsAndChatManager::OnInviteAccepted(const FUniqueNetId& UserId, const FUniqueNetId& FriendId)
{
	TSharedPtr< IFriendItem > Friend = FindUser(FriendId);
	if(Friend.IsValid())
	{
		Friend->SetPendingAccept();
		SendInviteAcceptedNotification(Friend);
	}
	RefreshList();
	RequestListRefresh();
}

void FFriendsAndChatManager::OnAcceptInviteComplete( int32 LocalPlayer, bool bWasSuccessful, const FUniqueNetId& FriendId, const FString& ListName, const FString& ErrorStr )
{
	PendingOutgoingAcceptFriendRequests.Remove(FUniqueNetIdString(FriendId.ToString()));

	// Do something with an accepted invite
	if ( PendingOutgoingAcceptFriendRequests.Num() > 0 )
	{
		SetState( EFriendsAndManagerState::AcceptingFriendRequest );
	}
	else
	{
		RefreshList();
		RequestListRefresh();
		SetState( EFriendsAndManagerState::Idle );
	}
}

void FFriendsAndChatManager::OnDeleteFriendComplete( int32 LocalPlayer, bool bWasSuccessful, const FUniqueNetId& DeletedFriendID, const FString& ListName, const FString& ErrorStr )
{
	PendingOutgoingDeleteFriendRequests.Remove(FUniqueNetIdString(DeletedFriendID.ToString()));

	RefreshList();

	if ( PendingOutgoingDeleteFriendRequests.Num() > 0 )
	{
		SetState( EFriendsAndManagerState::DeletingFriends );
	}
	else
	{
		SetState(EFriendsAndManagerState::Idle);
	}
}

void FFriendsAndChatManager::AddFriendsToast(const FText Message)
{
	if( FriendsNotificationBox.IsValid())
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 2.0f;
		Info.bUseLargeFont = false;
		FriendsNotificationBox->AddNotification(Info);
	}
}

// Analytics

void FFriendsAndChatAnalytics::RecordGameInvite(const FUniqueNetId& ToUser, const FString& EventStr) const
{
	if (Provider.IsValid())
	{
		IOnlineIdentityPtr OnlineIdentity = Online::GetIdentityInterface(MCP_SUBSYSTEM);
		if (OnlineIdentity.IsValid())
		{
			TSharedPtr<FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
			if (UserId.IsValid())
			{
				TArray<FAnalyticsEventAttribute> Attributes;
				Attributes.Add(FAnalyticsEventAttribute(TEXT("User"), UserId->ToString()));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("Friend"), ToUser.ToString()));
				AddPresenceAttributes(*UserId, Attributes);
				Provider->RecordEvent(EventStr, Attributes);
			}
		}
	}
}

void FFriendsAndChatAnalytics::RecordFriendAction(const IFriendItem& Friend, const FString& EventStr) const
{
	if (Provider.IsValid())
	{
		IOnlineIdentityPtr OnlineIdentity = Online::GetIdentityInterface(MCP_SUBSYSTEM);
		if (OnlineIdentity.IsValid())
		{
			TSharedPtr<FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
			if (UserId.IsValid())
			{
				TArray<FAnalyticsEventAttribute> Attributes;
				Attributes.Add(FAnalyticsEventAttribute(TEXT("User"), UserId->ToString()));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("Friend"), Friend.GetUniqueID()->ToString()));
				AddPresenceAttributes(*UserId, Attributes);
				Provider->RecordEvent(EventStr, Attributes);
			}
		}
	}
}

void FFriendsAndChatAnalytics::RecordAddFriend(const FString& FriendName, const FUniqueNetId& FriendId, EFindFriendResult::Type Result, bool bRecentPlayer, const FString& EventStr) const
{
	if (Provider.IsValid())
	{
		IOnlineIdentityPtr OnlineIdentity = Online::GetIdentityInterface(MCP_SUBSYSTEM);
		if (OnlineIdentity.IsValid())
		{
			TSharedPtr<FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
			if (UserId.IsValid())
			{
				TArray<FAnalyticsEventAttribute> Attributes;
				Attributes.Add(FAnalyticsEventAttribute(TEXT("User"), UserId->ToString()));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("Friend"), FriendId.ToString()));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("FriendName"), FriendName));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("Result"), EFindFriendResult::ToString(Result)));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("bRecentPlayer"), bRecentPlayer));
				AddPresenceAttributes(*UserId, Attributes);
				Provider->RecordEvent(EventStr, Attributes);
			}
		}
	}
}

void FFriendsAndChatAnalytics::RecordToggleChat(const FString& Channel, bool bEnabled, const FString& EventStr) const
{
	if (Provider.IsValid())
	{
		IOnlineIdentityPtr OnlineIdentity = Online::GetIdentityInterface(MCP_SUBSYSTEM);
		if (OnlineIdentity.IsValid())
		{
			TSharedPtr<FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
			if (UserId.IsValid())
			{
				TArray<FAnalyticsEventAttribute> Attributes;
				Attributes.Add(FAnalyticsEventAttribute(TEXT("User"), UserId->ToString()));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("Channel"), Channel));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("bEnabled"), bEnabled));
				AddPresenceAttributes(*UserId, Attributes);
				Provider->RecordEvent(EventStr, Attributes);
			}
		}
	}
}

void FFriendsAndChatAnalytics::RecordPrivateChat(const FString& ToUser)
{
	int32& Count = ChatCounts.FindOrAdd(ToUser);
	Count += 1;
	int32& TotalCount = ChatCounts.FindOrAdd(TEXT("TotalPrivate"));
	TotalCount += 1;
}

void FFriendsAndChatAnalytics::RecordChannelChat(const FString& ToChannel)
{
	int32& Count = ChatCounts.FindOrAdd(ToChannel);
	Count += 1;
}

void FFriendsAndChatAnalytics::FlushChatStats()
{
	if (Provider.IsValid())
	{
		IOnlineIdentityPtr OnlineIdentity = Online::GetIdentityInterface(MCP_SUBSYSTEM);
		if (OnlineIdentity.IsValid())
		{
			TSharedPtr<FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
			if (UserId.IsValid())
			{
				TArray<FAnalyticsEventAttribute> Attributes;
				for (auto It = ChatCounts.CreateConstIterator(); It; ++It)
				{
					Attributes.Add(FAnalyticsEventAttribute(It.Key(), It.Value()));
				}
				Provider->RecordEvent(TEXT("Social.Chat.Counts"), Attributes);
			}
		}
	}
	ChatCounts.Empty();
}

void FFriendsAndChatAnalytics::AddPresenceAttributes(const FUniqueNetId& UserId, TArray<FAnalyticsEventAttribute>& Attributes) const
{
	IOnlinePresencePtr OnlinePresence = Online::GetPresenceInterface(MCP_SUBSYSTEM);
	if (OnlinePresence.IsValid())
	{
		TSharedPtr<FOnlineUserPresence> Presence;
		OnlinePresence->GetCachedPresence(UserId, Presence);
		if (Presence.IsValid())
		{
			FVariantData* ClientIdData = Presence->Status.Properties.Find(DefaultClientIdKey);
			if (ClientIdData != nullptr)
			{
				Attributes.Add(FAnalyticsEventAttribute(TEXT("ClientId"), ClientIdData->ToString()));
			}
			Attributes.Add(FAnalyticsEventAttribute(TEXT("Status"), Presence->Status.StatusStr));
		}
	}
}

/* FFriendsAndChatManager system singletons
*****************************************************************************/

TSharedPtr< FFriendsAndChatManager > FFriendsAndChatManager::SingletonInstance = nullptr;

TSharedRef< FFriendsAndChatManager > FFriendsAndChatManager::Get()
{
	if ( !SingletonInstance.IsValid() )
	{
		SingletonInstance = MakeShareable( new FFriendsAndChatManager() );
	}
	return SingletonInstance.ToSharedRef();
}


void FFriendsAndChatManager::Shutdown()
{
	SingletonInstance.Reset();
}


#undef LOCTEXT_NAMESPACE

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "SFriendsContainer.h"
#include "SFriendUserHeader.h"
#include "SFriendsStatusCombo.h"
#include "SChatWindow.h"
#include "FriendsViewModel.h"
#include "FriendViewModel.h"
#include "FriendsStatusViewModel.h"
#include "FriendsUserViewModel.h"
#include "ChatViewModel.h"
#include "SNotificationList.h"
#include "SWindowTitleBar.h"
#include "FriendRecentPlayerItems.h"
#include "FriendGameInviteItem.h"
#include "ChatDisplayOptionsViewModel.h"
#include "SFriendUserHeader.h"
#include "ClanRepository.h"
#include "IFriendList.h"

#define LOCTEXT_NAMESPACE "FriendsAndChatManager"

const float CHAT_ANALYTICS_INTERVAL = 5 * 60.0f;  // 5 min

namespace FriendsAndChatManagerDefs
{
	static const FVector2D FriendsListWindowPosition(100, 100);
	static const FVector2D FriendsListWindowSize(350, 550);
	static const FVector2D FriendsListWindowMinimumSize(300, 300);
	static const FVector2D ChatWindowSize(350, 500);
	static const FVector2D ChatWindowMinimumSize(300, 250);
	static const float FriendsAndChatWindowGap = 20.0f;
}

/* FFriendsAndChatManager structors
 *****************************************************************************/

FFriendsAndChatManager::FFriendsAndChatManager( )
	: OnlineSub(nullptr)
	, MessageManager(FFriendsMessageManagerFactory::Create())
	, bJoinedGlobalChat(false)
	, ChatWindowMode(EChatWindowMode::MultiWindow)
	, ManagerState ( EFriendsAndManagerState::OffLine )
	, bIsInited( false )
	, bRequiresListRefresh(false)
	, bRequiresRecentPlayersRefresh(false)
	, FlushChatAnalyticsCountdown(CHAT_ANALYTICS_INTERVAL)
{
}


FFriendsAndChatManager::~FFriendsAndChatManager( )
{
	Analytics.FlushChatStats();
}


/* IFriendsAndChatManager interface
 *****************************************************************************/

void FFriendsAndChatManager::Login()
{
	// Clear existing data
	Logout();

	bIsInited = false;

	OnlineSub = IOnlineSubsystem::Get(TEXT("MCP"));

	if (OnlineSub != nullptr &&
		OnlineSub->GetUserInterface().IsValid() &&
		OnlineSub->GetIdentityInterface().IsValid())
	{
		OnlineIdentity = OnlineSub->GetIdentityInterface();

		// ToDo - Inject these. Also, create FriendsListViewModelFactory here: Nick Davies 30th March 2015
		ClanRepository = FClanRepositoryFactory::Create();
		FriendsListFactory = FFriendListFactoryFactory::Create();

		if(OnlineIdentity->GetUniquePlayerId(0).IsValid())
		{
			IOnlineUserPtr UserInterface = OnlineSub->GetUserInterface();
			check(UserInterface.IsValid());

			FriendsInterface = OnlineSub->GetFriendsInterface();
			check( FriendsInterface.IsValid() )

			// Create delegates for list refreshes
			OnQueryRecentPlayersCompleteDelegate = FOnQueryRecentPlayersCompleteDelegate           ::CreateRaw(this, &FFriendsAndChatManager::OnQueryRecentPlayersComplete);
			OnFriendsListChangedDelegate         = FOnFriendsChangeDelegate                        ::CreateSP (this, &FFriendsAndChatManager::OnFriendsListChanged);
			OnDeleteFriendCompleteDelegate       = FOnDeleteFriendCompleteDelegate                 ::CreateSP (this, &FFriendsAndChatManager::OnDeleteFriendComplete);
			OnQueryUserIdMappingCompleteDelegate = IOnlineUser::FOnQueryUserMappingComplete::CreateSP(this, &FFriendsAndChatManager::OnQueryUserIdMappingComplete);
			OnQueryUserInfoCompleteDelegate      = FOnQueryUserInfoCompleteDelegate                ::CreateSP (this, &FFriendsAndChatManager::OnQueryUserInfoComplete);
			OnPresenceReceivedCompleteDelegate   = FOnPresenceReceivedDelegate                     ::CreateSP (this, &FFriendsAndChatManager::OnPresenceReceived);
			OnPresenceUpdatedCompleteDelegate    = IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateSP (this, &FFriendsAndChatManager::OnPresenceUpdated);
			OnFriendInviteReceivedDelegate       = FOnInviteReceivedDelegate                       ::CreateSP (this, &FFriendsAndChatManager::OnFriendInviteReceived);
			OnFriendRemovedDelegate              = FOnFriendRemovedDelegate                        ::CreateSP (this, &FFriendsAndChatManager::OnFriendRemoved);
			OnFriendInviteRejected               = FOnInviteRejectedDelegate                       ::CreateSP (this, &FFriendsAndChatManager::OnInviteRejected);
			OnFriendInviteAccepted               = FOnInviteAcceptedDelegate                       ::CreateSP (this, &FFriendsAndChatManager::OnInviteAccepted);
			OnGameInviteReceivedDelegate         = FOnSessionInviteReceivedDelegate                ::CreateSP (this, &FFriendsAndChatManager::OnGameInviteReceived);
			OnDestroySessionCompleteDelegate     = FOnDestroySessionCompleteDelegate               ::CreateSP (this, &FFriendsAndChatManager::OnGameDestroyed);

			OnQueryRecentPlayersCompleteDelegateHandle = FriendsInterface->AddOnQueryRecentPlayersCompleteDelegate_Handle(   OnQueryRecentPlayersCompleteDelegate);
			OnFriendsListChangedDelegateHandle         = FriendsInterface->AddOnFriendsChangeDelegate_Handle             (0, OnFriendsListChangedDelegate);
			OnFriendInviteReceivedDelegateHandle       = FriendsInterface->AddOnInviteReceivedDelegate_Handle            (   OnFriendInviteReceivedDelegate);
			OnFriendRemovedDelegateHandle              = FriendsInterface->AddOnFriendRemovedDelegate_Handle             (   OnFriendRemovedDelegate);
			OnFriendInviteRejectedHandle               = FriendsInterface->AddOnInviteRejectedDelegate_Handle            (   OnFriendInviteRejected);
			OnFriendInviteAcceptedHandle               = FriendsInterface->AddOnInviteAcceptedDelegate_Handle            (   OnFriendInviteAccepted);
			OnDeleteFriendCompleteDelegateHandle       = FriendsInterface->AddOnDeleteFriendCompleteDelegate_Handle      (0, OnDeleteFriendCompleteDelegate);

			OnQueryUserInfoCompleteDelegateHandle = UserInterface->AddOnQueryUserInfoCompleteDelegate_Handle(0, OnQueryUserInfoCompleteDelegate);

			OnPresenceReceivedCompleteDelegateHandle = OnlineSub->GetPresenceInterface()->AddOnPresenceReceivedDelegate_Handle      (OnPresenceReceivedCompleteDelegate);
			OnGameInviteReceivedDelegateHandle       = OnlineSub->GetSessionInterface ()->AddOnSessionInviteReceivedDelegate_Handle (OnGameInviteReceivedDelegate);
			OnDestroySessionCompleteDelegateHandle   = OnlineSub->GetSessionInterface ()->AddOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegate);

			ManagerState = EFriendsAndManagerState::Idle;

			FriendsList.Empty();
			PendingFriendsList.Empty();
			OldUserPresenceMap.Empty();

			if ( UpdateFriendsTickerDelegate.IsBound() == false )
			{
				UpdateFriendsTickerDelegate = FTickerDelegate::CreateSP( this, &FFriendsAndChatManager::Tick );
			}

			UpdateFriendsTickerDelegateHandle = FTicker::GetCoreTicker().AddTicker( UpdateFriendsTickerDelegate );

			SetState(EFriendsAndManagerState::RequestFriendsListRefresh);
			RequestRecentPlayersListRefresh();

			MessageManager->LogIn();
			MessageManager->OnChatPublicRoomJoined().AddSP(this, &FFriendsAndChatManager::OnChatPublicRoomJoined);
			for (auto RoomName : ChatRoomstoJoin)
			{
				MessageManager->JoinPublicRoom(RoomName);
			}

			SetUserIsOnline(EOnlinePresenceState::Online);
		}
		else
		{
			SetState(EFriendsAndManagerState::OffLine);
		}
	}
}

void FFriendsAndChatManager::Logout()
{
	if (OnlineSub != nullptr)
	{
		if (OnlineSub->GetFriendsInterface().IsValid())
		{
			OnlineSub->GetFriendsInterface()->ClearOnQueryRecentPlayersCompleteDelegate_Handle(   OnQueryRecentPlayersCompleteDelegateHandle);
			OnlineSub->GetFriendsInterface()->ClearOnFriendsChangeDelegate_Handle             (0, OnFriendsListChangedDelegateHandle);
			OnlineSub->GetFriendsInterface()->ClearOnInviteReceivedDelegate_Handle            (   OnFriendInviteReceivedDelegateHandle);
			OnlineSub->GetFriendsInterface()->ClearOnFriendRemovedDelegate_Handle             (   OnFriendRemovedDelegateHandle);
			OnlineSub->GetFriendsInterface()->ClearOnInviteRejectedDelegate_Handle            (   OnFriendInviteRejectedHandle);
			OnlineSub->GetFriendsInterface()->ClearOnInviteAcceptedDelegate_Handle            (   OnFriendInviteAcceptedHandle);
			OnlineSub->GetFriendsInterface()->ClearOnDeleteFriendCompleteDelegate_Handle      (0, OnDeleteFriendCompleteDelegateHandle);
		}
		if (OnlineSub->GetPresenceInterface().IsValid())
		{
			OnlineSub->GetPresenceInterface()->ClearOnPresenceReceivedDelegate_Handle(OnPresenceReceivedCompleteDelegateHandle);
		}
		if (OnlineSub->GetSessionInterface().IsValid())
		{
			OnlineSub->GetSessionInterface()->ClearOnSessionInviteReceivedDelegate_Handle (OnGameInviteReceivedDelegateHandle);
			OnlineSub->GetSessionInterface()->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegateHandle);
		}
		if (OnlineSub->GetUserInterface().IsValid())
		{
			OnlineSub->GetUserInterface()->ClearOnQueryUserInfoCompleteDelegate_Handle(0, OnQueryUserInfoCompleteDelegateHandle);
		}
	}

	FriendsList.Empty();
	RecentPlayersList.Empty();
	PendingFriendsList.Empty();
	FriendByNameRequests.Empty();
	FilteredFriendsList.Empty();
	PendingOutgoingDeleteFriendRequests.Empty();
	PendingOutgoingAcceptFriendRequests.Empty();
	PendingIncomingInvitesList.Empty();
	PendingGameInvitesList.Empty();
	NotifiedRequest.Empty();
	ChatRoomstoJoin.Empty();

	if ( FriendWindow.IsValid() )
	{
		FriendWindow->RequestDestroyWindow();
		FriendWindow = nullptr;
	}

	if ( GlobalChatWindow.IsValid() )
	{
		Analytics.FlushChatStats();

		GlobalChatWindow->RequestDestroyWindow();
		GlobalChatWindow = nullptr;
	}

	for (int32 WindowIndex = WhisperChatWindows.Num()-1; WindowIndex >= 0; --WindowIndex)
	{
		WhisperChatWindows[WindowIndex].ChatWindow->RequestDestroyWindow();
	}
	WhisperChatWindows.Empty();

	MessageManager->LogOut();

	// Prevent shared pointers lingering after OSS has shutdown
	OnlineIdentity = nullptr;
	FriendsInterface = nullptr;
	OnlineSub = nullptr;
	// Prevent a shared pointer issue with stale analytics providers
    // @todo do this as a failsafe, but unsafe now until FTickerObjectBase issues are resolved
	SetAnalyticsProvider(nullptr);

	if ( UpdateFriendsTickerDelegate.IsBound() )
	{
		FTicker::GetCoreTicker().RemoveTicker( UpdateFriendsTickerDelegateHandle );
	}

	MessageManager->OnChatPublicRoomJoined().RemoveAll(this);

	SetState(EFriendsAndManagerState::OffLine);
}

bool FFriendsAndChatManager::IsLoggedIn()
{
	return ManagerState != EFriendsAndManagerState::OffLine;
}

void FFriendsAndChatManager::SetOnline()
{
	SetUserIsOnline(EOnlinePresenceState::Online);
}

void FFriendsAndChatManager::SetAway()
{
	SetUserIsOnline(EOnlinePresenceState::Away);
}

void FFriendsAndChatManager::AddApplicationViewModel(const FString ClientID, TSharedPtr<IFriendsApplicationViewModel> InApplicationViewModel)
{
	ApplicationViewModels.Add(ClientID, InApplicationViewModel);
}

void FFriendsAndChatManager::SetUserSettings(const FFriendsAndChatSettings& InUserSettings)
{
	UserSettings = InUserSettings;
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
	if (!RoomName.IsEmpty() && bJoinedGlobalChat == false)
	{
		ChatRoomstoJoin.AddUnique(RoomName);
		if (MessageManager.IsValid())
		{
			MessageManager->JoinPublicRoom(RoomName);
		}
	}
}

void FFriendsAndChatManager::OnChatPublicRoomJoined(const FString& ChatRoomID)
{
	if (ChatRoomstoJoin.Contains(ChatRoomID))
	{
		bJoinedGlobalChat = true;
	}
}

bool FFriendsAndChatManager::IsInGlobalChat() const
{
	return bJoinedGlobalChat;
}

// UI Creation

void FFriendsAndChatManager::CreateFriendsListWindow(const FFriendsAndChatStyle* InStyle)
{
	Style = *InStyle;
	FFriendsAndChatModuleStyle::Initialize(Style);

	if (!FriendWindow.IsValid())
	{
		FriendWindow = SNew( SWindow )
		.Title(LOCTEXT("FFriendsAndChatManager_FriendsTitle", "Friends List"))
		.Style(&Style.WindowStyle)
		.ClientSize(FriendsAndChatManagerDefs::FriendsListWindowSize)
		.MinWidth(FriendsAndChatManagerDefs::FriendsListWindowMinimumSize.X)
		.MinHeight(FriendsAndChatManagerDefs::FriendsListWindowMinimumSize.Y)
		.ScreenPosition(FriendsAndChatManagerDefs::FriendsListWindowPosition)
		.AutoCenter( EAutoCenter::None )
		.SizingRule(ESizingRule::UserSized)
		.SupportsMaximize(true)
		.SupportsMinimize(true)
		.bDragAnywhere(true)
		.CreateTitleBar(false)
		.LayoutBorder(FMargin(0));

		FriendWindow->SetOnWindowClosed(FOnWindowClosed::CreateRaw(this, &FFriendsAndChatManager::HandleFriendsWindowClosed));

		BuildFriendsUI(FriendWindow);
		FriendWindow->AddOverlaySlot()
		[
			SNew(SBorder).Padding(0).BorderImage(&Style.WindowEdgingBrush).Visibility(EVisibility::HitTestInvisible)
		];
		FriendWindow = FSlateApplication::Get().AddWindow(FriendWindow.ToSharedRef());
	}
	else if(FriendWindow->IsWindowMinimized())
	{
		FriendWindow->Restore();
		BuildFriendsUI(FriendWindow);
	}

	// Bring Other Chat Windows to the front
	if (GlobalChatWindow.IsValid() && !GlobalChatWindow->IsWindowMinimized())
	{		
		GlobalChatWindow->BringToFront();
	}
	for (const auto& WhisperChatWindow : WhisperChatWindows)
	{
		// Cascade off last created chat window
		if (!WhisperChatWindow.ChatWindow->IsWindowMinimized())
		{ 
			WhisperChatWindow.ChatWindow->BringToFront();
		}		
	}


	FriendWindow->BringToFront(true);

	// Clear notifications
	OnFriendsNotification().Broadcast(false);
}

void FFriendsAndChatManager::HandleFriendsWindowClosed(const TSharedRef<SWindow>& InWindow)
{
	FriendWindow.Reset();
}

void FFriendsAndChatManager::BuildFriendsUI(TSharedPtr< SWindow > WindowPtr)
{
	check(WindowPtr.IsValid() && ClanRepository.IsValid());

	TSharedRef<FFriendsViewModel> FriendsViewModel = FFriendsViewModelFactory::Create(SharedThis(this), ClanRepository.ToSharedRef(), FriendsListFactory.ToSharedRef());

	TSharedPtr<SWidget> HeaderWidget = SNew(SBox)
	.WidthOverride(Style.FriendsListWidth)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SFriendsStatusCombo, FriendsViewModel->GetStatusViewModel(), FriendsViewModel->GetUserViewModel())
			.FriendStyle(&Style)
		]
	];

	TSharedPtr<SWindowTitleBar> TitleBar;
	WindowPtr->SetContent(
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
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SAssignNew(TitleBar, SWindowTitleBar, WindowPtr.ToSharedRef(), nullptr, HAlign_Center)
						.Style(&Style.WindowStyle)
						.ShowAppIcon(false)
						.Title(FText::GetEmpty())
					]
					+ SOverlay::Slot()
					.Padding(Style.UserHeaderPadding)
					[
						HeaderWidget.ToSharedRef()
					]
				]
				+ SVerticalBox::Slot()
				[
					SNew(SFriendsContainer, FriendsViewModel)
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
		if (!ClanRepository.IsValid())
		{
			ClanRepository = FClanRepositoryFactory::Create();
		}
		check(ClanRepository.IsValid())

		if (!FriendsListFactory.IsValid())
		{
			FriendsListFactory = FFriendListFactoryFactory::Create();
		}
		check(FriendsListFactory.IsValid());

		Style = *InStyle;
		FFriendsAndChatModuleStyle::Initialize(Style);
		SAssignNew(FriendListWidget, SOverlay)
		+SOverlay::Slot()
		[
			 SNew(SFriendsContainer, FFriendsViewModelFactory::Create(SharedThis(this), ClanRepository.ToSharedRef(), FriendsListFactory.ToSharedRef()))
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

TSharedPtr< SWidget > FFriendsAndChatManager::GenerateStatusWidget( const FFriendsAndChatStyle* InStyle )
{
	return SNew(SFriendUserHeader, FFriendsUserViewModelFactory::Create(SharedThis(this)))
		.FriendStyle( InStyle );
}

TSharedPtr< SWidget > FFriendsAndChatManager::GenerateChatWidget(const FFriendsAndChatStyle* InStyle, TSharedRef<IChatViewModel> ViewModel, TAttribute<FText> ActivationHintDelegate)
{
	ChatWindowMode = EChatWindowMode::Widget;
	if(!ChatViewModel.IsValid())
	{
		ChatViewModel = FChatViewModelFactory::Create(MessageManager.ToSharedRef());
	}

	// todo - NDavies = find a better way to do this
	TSharedRef<FChatDisplayOptionsViewModel> ChatDisplayOptionsViewModel = StaticCastSharedRef<FChatDisplayOptionsViewModel>(ViewModel);
	ChatDisplayOptionsViewModel->GetChatViewModel()->SetDisplayGlobalChat(true);
	ChatDisplayOptionsViewModel->SetCaptureFocus(true);

	TSharedPtr<SChatWindow> ChatWidget;
	Style = *InStyle;
	SAssignNew(ChatWidget, SChatWindow, ChatDisplayOptionsViewModel)
	.FriendStyle(&Style)
	.Method(EPopupMethod::UseCurrentWindow)
	.ActivationHintText(ActivationHintDelegate);
	return ChatWidget;
}

TSharedPtr<IChatViewModel> FFriendsAndChatManager::GetChatViewModel()
{
	if(!ChatViewModel.IsValid())
	{
		ChatViewModel = FChatViewModelFactory::Create(MessageManager.ToSharedRef());
	}
	return FChatDisplayOptionsViewModelFactory::Create(ChatViewModel.ToSharedRef());
}

FVector2D FFriendsAndChatManager::GetWindowSpawnPosition() const
{
	FVector2D WindowPosition
	(
		FriendsAndChatManagerDefs::FriendsListWindowPosition.X + FriendsAndChatManagerDefs::FriendsListWindowSize.X + FriendsAndChatManagerDefs::FriendsAndChatWindowGap,
		FriendsAndChatManagerDefs::FriendsListWindowPosition.Y
	);

	if (GlobalChatWindow.IsValid())
	{
		WindowPosition = GlobalChatWindow->GetPositionInScreen() + FriendsAndChatManagerDefs::FriendsAndChatWindowGap;
	}

	for (const auto& WhisperChatWindow : WhisperChatWindows)
	{
		// Cascade off last created chat window
		WindowPosition = WhisperChatWindow.ChatWindow->GetPositionInScreen() + FriendsAndChatManagerDefs::FriendsAndChatWindowGap;
	}

	// Get Screen space to make sure we do not create chat windows off screen
	FDisplayMetrics Metrics;
	FSlateApplication::Get().GetDisplayMetrics(Metrics);
	FPlatformRect Display = Metrics.VirtualDisplayRect;
	FVector2D ScreenResolution = FVector2D(Display.Right - Display.Left, Display.Top - Display.Bottom);

	// Check Chat Window position against screen space
	if (WindowPosition.X + FriendsAndChatManagerDefs::ChatWindowSize.X > Display.Right ||
		WindowPosition.Y + FriendsAndChatManagerDefs::ChatWindowSize.Y > Display.Bottom)
	{
		// Spawn Top Left of monitor as we have cascaded off screen
		WindowPosition = FVector2D(20, 20);
	}
	return WindowPosition;
}

void FFriendsAndChatManager::OpenWhisperChatWindows(const FFriendsAndChatStyle* InStyle)
{
	if (MessageManager.IsValid())
	{
		TArray<TSharedRef<FFriendChatMessage>> Messages = MessageManager->GetMessages();
		TArray<TSharedPtr<FUniqueNetId>> UniqueIDs;
		for (auto It = Messages.CreateConstIterator(); It; ++It)
		{
			if (It->Get().MessageType == EChatMessageType::Whisper && !UniqueIDs.Contains(It->Get().SenderId))
			{
				TSharedPtr<IFriendItem> FoundFriend = FindUser(It->Get().SenderId.ToSharedRef());
				CreateChatWindow(InStyle, EChatMessageType::Whisper, FoundFriend, true);
				UniqueIDs.Add(It->Get().SenderId);
			}
		}
	}
}

void FFriendsAndChatManager::CreateChatWindow(const struct FFriendsAndChatStyle* InStyle, EChatMessageType::Type ChatType, TSharedPtr< IFriendItem > FriendItem, bool BringToFront)
{	
	check(MessageManager.IsValid());	

	// Look up if window has already been created
	TSharedPtr<SWindow> ChatWindow;
	if (ChatType == EChatMessageType::Whisper && FriendItem.IsValid() && ChatWindowMode == EChatWindowMode::MultiWindow)
	{
		for (const auto& WhisperChatWindow : WhisperChatWindows)
		{
			if (WhisperChatWindow.FriendItem->GetUniqueID() == FriendItem->GetUniqueID())
			{
				ChatWindow = WhisperChatWindow.ChatWindow;
				break;
			}
		}
	}	
	else
	{
		ChatWindow = GlobalChatWindow;
	}

	

	if (!ChatWindow.IsValid())
	{
		// Choose Window Name
		FText WindowName = LOCTEXT("FriendsAndChatManager_ChatTitle", "Chat Window");

		if (ChatWindowMode != EChatWindowMode::MultiWindow)
		{
			if (ChatType == EChatMessageType::Global)
			{
				WindowName = LOCTEXT("FriendsAndChatManager_GlobalChatTitle", "Global Chat");
			}
			else if (ChatType == EChatMessageType::Whisper && FriendItem.IsValid())
			{
				WindowName = FText::FromString(FriendItem->GetName());
			}
		}

		// Create Window
		Style = *InStyle;
		FFriendsAndChatModuleStyle::Initialize(Style);

		ChatWindow = SNew( SWindow )
		.Title(WindowName)
		.Style(&Style.WindowStyle)
		.ClientSize(FriendsAndChatManagerDefs::ChatWindowSize)
		.MinWidth(FriendsAndChatManagerDefs::ChatWindowMinimumSize.X)
		.MinHeight(FriendsAndChatManagerDefs::ChatWindowMinimumSize.Y)
		.ScreenPosition(GetWindowSpawnPosition())
		.AutoCenter( EAutoCenter::None )
		.SupportsMaximize( true )
		.SupportsMinimize( true )
		.CreateTitleBar( false )
		.SizingRule(ESizingRule::UserSized)
		.LayoutBorder(FMargin(0));

		ChatWindow->SetOnWindowClosed(FOnWindowClosed::CreateRaw(this, &FFriendsAndChatManager::HandleChatWindowClosed));

		SetChatWindowContents(ChatWindow, FriendItem, Style);
		ChatWindow->AddOverlaySlot()
		[
			SNew(SBorder).Padding(0).BorderImage(&Style.WindowEdgingBrush).Visibility(EVisibility::HitTestInvisible)
		];
		ChatWindow = FSlateApplication::Get().AddWindow(ChatWindow.ToSharedRef());

		// Store window ptr
		if (ChatType == EChatMessageType::Whisper && FriendItem.IsValid() && ChatWindowMode == EChatWindowMode::MultiWindow)
		{
			WhisperChat NewWhisperChat;
			NewWhisperChat.FriendItem = FriendItem;
			NewWhisperChat.ChatWindow = ChatWindow;
			WhisperChatWindows.Add(NewWhisperChat);
		}
		else if (ChatType == EChatMessageType::Global || ChatWindowMode != EChatWindowMode::MultiWindow)
		{
			GlobalChatWindow = ChatWindow;
		}
	}
	else if(ChatWindow->IsWindowMinimized())
	{
		if (GetOnlineStatus() == EOnlinePresenceState::Online)
		{
			ChatWindow->Restore();
		}
		if (ChatWindowMode != EChatWindowMode::MultiWindow)
		{
			SetChatWindowContents(ChatWindow, nullptr, Style);
		}		
	}

	if (GetOnlineStatus() == EOnlinePresenceState::Online)
	{
		ChatWindow->BringToFront(true);
	}
	
	OnChatFriendSelected().Broadcast(FriendItem);
}

void FFriendsAndChatManager::SetChatFriend( TSharedPtr< IFriendItem > FriendItem )
{
	if (ChatWindowMode != EChatWindowMode::Widget)
	{
		CreateChatWindow(&Style, EChatMessageType::Whisper, FriendItem, true);
	}

	if (ChatWindowMode != EChatWindowMode::MultiWindow)
	{
		OnChatFriendSelected().Broadcast(FriendItem);
	}
}

void FFriendsAndChatManager::OpenGlobalChat()
{
	if (IsInGlobalChat())
	{
		FFriendsAndChatManager::Get()->GetAnalytics().RecordToggleChat(TEXT("Global"), true, TEXT("Social.Chat.Toggle"));
		CreateChatWindow(&Style, EChatMessageType::Global, nullptr, true);
	}
}

void FFriendsAndChatManager::HandleChatWindowClosed(const TSharedRef<SWindow>& InWindow)
{
	if (InWindow == GlobalChatWindow)
	{
		FFriendsAndChatManager::Get()->GetAnalytics().RecordToggleChat(TEXT("Global"), false, TEXT("Social.Chat.Toggle"));
		GlobalChatWindow.Reset();
	}	
	else
	{
		for (int WhisperChatIndex = 0; WhisperChatIndex < WhisperChatWindows.Num(); ++WhisperChatIndex)
		{
			if (WhisperChatWindows[WhisperChatIndex].ChatWindow == InWindow)
			{
				WhisperChatWindows.RemoveAt(WhisperChatIndex);
				break;
			}
		}
	}

	Analytics.FlushChatStats();
}


class SLATE_API SWindowZoneBox 
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SWindowZoneBox)
		: _Content()
		, _HAlign(HAlign_Fill)
		, _VAlign(VAlign_Fill)
		, _Padding(FMargin(0))
		, _WindowZone(EWindowZone::Unspecified)
	{}
		SLATE_DEFAULT_SLOT(FArguments, Content)

		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		SLATE_ATTRIBUTE(FMargin, Padding)

		SLATE_ATTRIBUTE(EWindowZone::Type, WindowZone)
	SLATE_END_ARGS()

	/**
	 * Default constructor.
	 */
	 SWindowZoneBox() {}

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct(const SWindowZoneBox::FArguments& InArgs)
	{
		WindowZone = InArgs._WindowZone;

		ChildSlot
		.HAlign(InArgs._HAlign)
		.VAlign(InArgs._VAlign)
		.Padding(InArgs._Padding)
		[
			InArgs._Content.Widget
		];
	}

	/**
	* Sets the content for this box
	*
	* @param	InContent	The widget to use as content for the box
	*/
	void SetContent(TSharedRef< SWidget > InContent)
	{
		ChildSlot
		[
			InContent
		];
	}

	const TSharedRef< SWidget >& GetContent() const
	{
		return ChildSlot.GetWidget();
	}

	/** Clears out the content for the box */
	void ClearContent()
	{
		ChildSlot.DetachWidget();
	}

public:
	// SWidget interface
	int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
	{
		bool bEnabled = false;
		bEnabled = ShouldBeEnabled(bParentEnabled);

		FWidgetStyle CompoundedWidgetStyle = FWidgetStyle(InWidgetStyle)
			.BlendColorAndOpacityTint(ColorAndOpacity.Get())
			.SetForegroundColor(ForegroundColor.Get());

		return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect.IntersectionWith(AllottedGeometry.GetClippingRect()), OutDrawElements, LayerId, CompoundedWidgetStyle, bEnabled);
	}

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
	{
		return SCompoundWidget::ComputeDesiredSize(LayoutScaleMultiplier);
	}

	virtual EWindowZone::Type GetWindowZoneOverride() const override
	{
		return WindowZone.Get();
	}
	// End of SWidget interface

private:

	TAttribute<EWindowZone::Type> WindowZone;
};


void FFriendsAndChatManager::SetChatWindowContents(TSharedPtr<SWindow> Window, TSharedPtr< IFriendItem > FriendItem, const FFriendsAndChatStyle& FriendStyle)
{
	TSharedPtr<SWindowTitleBar> TitleBar;
	if(!ChatViewModel.IsValid() || ChatWindowMode == EChatWindowMode::MultiWindow)
	{
		ChatViewModel = FChatViewModelFactory::Create(MessageManager.ToSharedRef());
	}

	TSharedRef<FChatDisplayOptionsViewModel> DisplayViewModel = FChatDisplayOptionsViewModelFactory::Create(ChatViewModel.ToSharedRef());
	DisplayViewModel->SetInGameUI(false);
	DisplayViewModel->SetCaptureFocus(false);
	DisplayViewModel->EnableGlobalChat(true);

	TSharedPtr<SWidget> HeaderWidget = SNew(SSpacer);

	if (FriendItem.IsValid())
	{
		TSharedPtr<FSelectedFriend> NewFriend;
		NewFriend = MakeShareable(new FSelectedFriend());
		NewFriend->DisplayName = FText::FromString(FriendItem->GetName());
		NewFriend->UserID = FriendItem->GetUniqueID();
		DisplayViewModel->GetChatViewModel()->SetWhisperChannel(NewFriend);

		HeaderWidget = SNew(SFriendUserHeader, FriendItem.ToSharedRef()).FriendStyle(&Style).ShowFriendName(true).Visibility(EVisibility::HitTestInvisible);
	}
	else if (ChatViewModel->IsGlobalChatEnabled())
	{
		HeaderWidget =
			SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Visibility(EVisibility::HitTestInvisible)
			[
				SNew(SImage)
				.Image(&FriendStyle.GlobalChatHeaderBrush)
			];
	}

	// Lock the channel if not using unified chat
	if (ChatWindowMode == EChatWindowMode::MultiWindow)
	{
		DisplayViewModel->GetChatViewModel()->LockChatChannel(true);

		// @todo - Antony Need a better way for checking for multi window global chat
		if (!FriendItem.IsValid() && bJoinedGlobalChat)
		{
			DisplayViewModel->GetChatViewModel()->SetDisplayGlobalChat(true);
		}
	}
	else if (bJoinedGlobalChat)
	{
		// If Unified Window we will need to turn on Global Chat
		DisplayViewModel->GetChatViewModel()->SetDisplayGlobalChat(true);
	}

	TSharedRef< FFriendsStatusViewModel > StatusViewModel = FFriendsStatusViewModelFactory::Create(SharedThis(this));
	TSharedPtr<SChatWindow> ChatWindow;

	TSharedRef<SWidget> Content = SNew( SBorder )
		.VAlign( VAlign_Fill )
		.HAlign( HAlign_Fill )
		.BorderImage( &Style.Background )
		.Padding(0)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Top)
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(&FriendStyle.FriendUserHeaderBackground)
				.Padding(0)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SAssignNew(TitleBar, SWindowTitleBar, Window.ToSharedRef(), nullptr, HAlign_Left)
						.Style(&Style.WindowStyle)
						.ShowAppIcon(false)
						.Title(FText::GetEmpty())
					]
					+ SOverlay::Slot()
					.Padding(FriendStyle.UserHeaderPadding)
					[
						HeaderWidget.ToSharedRef()
					]

				]
			]
			+ SVerticalBox::Slot()
			[
				SAssignNew(ChatWindow, SChatWindow, DisplayViewModel)
				.FriendStyle( &Style )
			]
		];

	Window->SetContent(Content);
	Window->SetOnWindowActivated(FOnWindowActivated::CreateSP(ChatWindow.Get(), &SChatWindow::HandleWindowActivated));
}
// Actions

void FFriendsAndChatManager::SetUserIsOnline(EOnlinePresenceState::Type OnlineState)
{
	if (OnlineSub != nullptr &&
		OnlineIdentity.IsValid())
	{
		TSharedPtr<FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
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

void FFriendsAndChatManager::AcceptFriend( TSharedPtr< IFriendItem > FriendItem )
{
	PendingOutgoingAcceptFriendRequests.Add(FUniqueNetIdString(FriendItem->GetOnlineUser()->GetUserId()->ToString()));
	FriendItem->SetPendingAccept();
	RefreshList();
	OnFriendsNotification().Broadcast(false);

	Analytics.RecordFriendAction(*FriendItem, TEXT("Social.FriendAction.Accept"));
}


void FFriendsAndChatManager::RejectFriend(TSharedPtr< IFriendItem > FriendItem)
{
	NotifiedRequest.Remove( FriendItem->GetOnlineUser()->GetUserId() );
	PendingOutgoingDeleteFriendRequests.Add(FUniqueNetIdString(FriendItem->GetOnlineUser()->GetUserId().Get().ToString()));
	FriendsList.Remove( FriendItem );
	RefreshList();
	OnFriendsNotification().Broadcast(false);

	Analytics.RecordFriendAction(*FriendItem, TEXT("Social.FriendAction.Reject"));
}

void FFriendsAndChatManager::DeleteFriend(TSharedPtr< IFriendItem > FriendItem, const FString& Action)
{
	TSharedRef<FUniqueNetId> UserID = FriendItem->GetOnlineFriend()->GetUserId();
	NotifiedRequest.Remove( UserID );
	PendingOutgoingDeleteFriendRequests.Add(FUniqueNetIdString(UserID.Get().ToString()));
	FriendsList.Remove( FriendItem );
	FriendItem->SetPendingDelete();
	RefreshList();
	// Clear notifications
	OnFriendsNotification().Broadcast(false);

	Analytics.RecordFriendAction(*FriendItem, TEXT("Social.FriendAction.") + Action);
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
	case EFriendsResponseType::Response_Ignore:
		{
			NotifiedRequest.Remove(ChatMessage->GetUniqueID());
			TSharedPtr< IFriendItem > User = FindUser(ChatMessage->GetUniqueID().Get());
			if (User.IsValid())
			{
				DeleteFriend(User, EFriendActionType::ToText(EFriendActionType::IgnoreFriendRequest).ToString());
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

TArray< TSharedPtr< IFriendItem > >& FFriendsAndChatManager::GetRecentPlayerList()
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

TSharedPtr<FUniqueNetId> FFriendsAndChatManager::GetGameSessionId() const
{
	if (OnlineSub != nullptr &&
		OnlineIdentity.IsValid() &&
		OnlineSub->GetSessionInterface().IsValid())
	{
		const FNamedOnlineSession* GameSession = OnlineSub->GetSessionInterface()->GetNamedSession(GameSessionName);
		if (GameSession != nullptr)
		{
			TSharedPtr<FOnlineSessionInfo> UserSessionInfo = GameSession->SessionInfo;
			if (UserSessionInfo.IsValid())
			{
				return MakeShareable(new FUniqueNetIdString(UserSessionInfo->GetSessionId()));
			}
		}
	}
	return nullptr;
}

bool FFriendsAndChatManager::IsInGameSession() const
{	
	if (OnlineSub != nullptr &&
		OnlineIdentity.IsValid() &&
		OnlineSub->GetSessionInterface().IsValid() &&
		OnlineSub->GetSessionInterface()->GetNamedSession(GameSessionName) != nullptr)
	{
		return true;
	}
	return false;
}

bool FFriendsAndChatManager::IsFriendInSameSession(const TSharedPtr< const IFriendItem >& FriendItem) const
{
	TSharedPtr<FUniqueNetId> MySessionId = GetGameSessionId();
	bool bMySessionValid = MySessionId.IsValid() && MySessionId->IsValid();

	TSharedPtr<FUniqueNetId> FriendSessionId = FriendItem->GetSessionId();
	bool bFriendSessionValid = FriendSessionId.IsValid() && FriendSessionId->IsValid();

	return (bMySessionValid && bFriendSessionValid && (*FriendSessionId == *MySessionId));
}

bool FFriendsAndChatManager::IsInJoinableGameSession() const
{
	bool bIsJoinable = false;

	if (OnlineSub != nullptr &&
		OnlineIdentity.IsValid())
	{
		IOnlineSessionPtr SessionInt = OnlineSub->GetSessionInterface();
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

bool FFriendsAndChatManager::JoinGameAllowed(FString ClientID)
{
	if (AllowFriendsJoinGame().IsBound())
	{
		return AllowFriendsJoinGame().Execute();
	}
	else
	{
		TSharedPtr<IFriendsApplicationViewModel>* FriendsApplicationViewModel = ApplicationViewModels.Find(ClientID);
		if (FriendsApplicationViewModel != nullptr &&
			(*FriendsApplicationViewModel).IsValid())
		{
			return (*FriendsApplicationViewModel)->IsAppJoinable();
		}
	}
	return false;
}

const bool FFriendsAndChatManager::IsInLauncher() const
{
	// ToDo NDavies - Find a better way to identify if we are in game
	return !AllowFriendsJoinGameDelegate.IsBound();
}

EOnlinePresenceState::Type FFriendsAndChatManager::GetOnlineStatus()
{
	if (OnlineSub != nullptr &&
		OnlineIdentity.IsValid())
	{
		TSharedPtr<FOnlineUserPresence> Presence;
		TSharedPtr<FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
		if(UserId.IsValid())
		{
			OnlineSub->GetPresenceInterface()->GetCachedPresence(*UserId, Presence);
			if(Presence.IsValid())
			{
				return Presence->Status.State;
			}
		}
	}
	return EOnlinePresenceState::Offline;
}

FString FFriendsAndChatManager::GetUserClientId() const
{
	FString Result;
	if (OnlineSub != nullptr &&
		OnlineIdentity.IsValid())
	{
		TSharedPtr<FOnlineUserPresence> Presence;
		TSharedPtr<FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
		if (UserId.IsValid())
		{
			OnlineSub->GetPresenceInterface()->GetCachedPresence(*UserId, Presence);
			if (Presence.IsValid())
			{
				const FVariantData* ClientId = Presence->Status.Properties.Find(DefaultClientIdKey);
				if (ClientId != nullptr && ClientId->GetType() == EOnlineKeyValuePairDataType::String)
				{
					ClientId->GetValue(Result);
				}
			}
		}
	}
	return Result;
}

FString FFriendsAndChatManager::GetUserNickname() const
{
	FString Result;
	if (OnlineSub != nullptr &&
		OnlineIdentity.IsValid())
	{
		TSharedPtr<FOnlineUserPresence> Presence;
		TSharedPtr<FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
		if (UserId.IsValid())
		{
			Result = OnlineIdentity->GetPlayerNickname(*UserId);
		}
	}
	return Result;
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

	FlushChatAnalyticsCountdown -= Delta;
	if (FlushChatAnalyticsCountdown <= 0)
	{
		Analytics.FlushChatStats();
		// Reset countdown for new update
		FlushChatAnalyticsCountdown = CHAT_ANALYTICS_INTERVAL;
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
			if (FriendsInterface.IsValid() && FriendsInterface->ReadFriendsList(0, EFriendsLists::ToString(EFriendsLists::Default), Delegate))
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
			TSharedPtr<FUniqueNetId> UserId = OnlineIdentity.IsValid() ? OnlineIdentity->GetUniquePlayerId(0) : nullptr;
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
			if (FriendsInterface.IsValid())
			{
				FriendsInterface->DeleteFriend(0, PendingOutgoingDeleteFriendRequests[0], EFriendsLists::ToString(EFriendsLists::Default));
			}			
		}
		break;
	case EFriendsAndManagerState::AcceptingFriendRequest:
		{
			if (FriendsInterface.IsValid())
			{
				FOnAcceptInviteComplete Delegate = FOnAcceptInviteComplete::CreateSP(this, &FFriendsAndChatManager::OnAcceptInviteComplete);
				FriendsInterface->AcceptInvite(0, PendingOutgoingAcceptFriendRequests[0], EFriendsLists::ToString(EFriendsLists::Default), Delegate);
			}
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
				check(OnlineSub != nullptr && OnlineSub->GetUserInterface().IsValid());
				IOnlineUserPtr UserInterface = OnlineSub->GetUserInterface();
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

	if ( FriendsInterface.IsValid() && FriendsInterface->GetFriendsList(0, ListName, Friends) )
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

		check(OnlineSub != nullptr && OnlineSub->GetUserInterface().IsValid());
		IOnlineUserPtr UserInterface = OnlineSub->GetUserInterface();

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
		check(OnlineSub != nullptr && OnlineSub->GetUserInterface().IsValid());
		IOnlineUserPtr UserInterface = OnlineSub->GetUserInterface();

		for ( int32 UserIdx=0; UserIdx < UserIds.Num(); UserIdx++ )
		{
			TSharedPtr<FOnlineFriend> OnlineFriend = FriendsInterface->GetFriend( 0, *UserIds[UserIdx], EFriendsLists::ToString( EFriendsLists::Default ) );
			TSharedPtr<FOnlineUser> OnlineUser = OnlineSub->GetUserInterface()->GetUserInfo( LocalPlayer, *UserIds[UserIdx] );
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
					TSharedPtr<FOnlineUser> OnlineUser = OnlineSub->GetUserInterface()->GetUserInfo(LocalPlayer, *RecentPlayer->GetUserId());
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
	IOnlineUserPtr UserInterface = OnlineSub->GetUserInterface();
	if (UserInterface.IsValid())
	{
	TSharedPtr<FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
	if (UserId.IsValid())
	{
		for (int32 Index = 0; Index < FriendByNameRequests.Num(); Index++)
		{
				UserInterface->QueryUserIdMapping(*UserId, FriendByNameRequests[Index], OnQueryUserIdMappingCompleteDelegate);
			}
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

TSharedPtr< IFriendItem > FFriendsAndChatManager::FindUser(const TSharedRef<FUniqueNetId>& InUserID)
{
	return FindUser(InUserID.Get());
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
			NotificationMessage->SetButtonCallback( FOnClicked::CreateSP(this, &FFriendsAndChatManager::HandleMessageAccepted, NotificationMessage, EFriendsResponseType::Response_Ignore));
			NotificationMessage->SetButtonDescription(LOCTEXT("FFriendsAndChatManager_Accept", "Accept"));
			NotificationMessage->SetButtonDescription(LOCTEXT("FFriendsAndChatManager_Ignore", "Ignore"));
			NotificationMessage->SetButtonStyle(TEXT("FriendsListEmphasisButton"));
			NotificationMessage->SetButtonStyle(TEXT("FriendsListCriticalButton"));
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
		const FText FriendRequestMessage = GetInviteNotificationText(Friend);
		TSharedPtr< FFriendsAndChatMessage > NotificationMessage = MakeShareable(new FFriendsAndChatMessage(FriendRequestMessage.ToString()));
		NotificationMessage->SetMessageType(EFriendsRequestType::FriendAccepted);
		OnFriendsActionNotification().Broadcast(NotificationMessage.ToSharedRef());
	}
}

const FText FFriendsAndChatManager::GetInviteNotificationText(TSharedPtr< IFriendItem > Friend) const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("Username"), FText::FromString(Friend->GetName()));

	if(Friend->IsPendingAccepted())
	{
		return FText::Format(LOCTEXT("FriendAddedToast", "{Username} added as a friend"), Args);
	}
	return FText::Format(LOCTEXT("FriendAcceptedToast", "{Username} accepted your request"), Args);
}

void FFriendsAndChatManager::OnQueryUserIdMappingComplete(bool bWasSuccessful, const FUniqueNetId& RequestingUserId, const FString& DisplayName, const FUniqueNetId& IdentifiedUserId, const FString& Error)
{
	check( OnlineSub != nullptr && OnlineSub->GetUserInterface().IsValid() );

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
				AddFriendsToast(LOCTEXT("FFriendsAndChatManager_FriendRequestSent", "Request Sent"));
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

void FFriendsAndChatManager::OnPresenceReceived(const FUniqueNetId& UserId, const TSharedRef<FOnlineUserPresence>& NewPresence)
{
	RefreshList();

	// Compare to previous presence for this friend, display a toast if a friend has came online or joined a game
	TSharedPtr<FUniqueNetId> SelfId = OnlineIdentity->GetUniquePlayerId(0);
	bool bIsSelf = (SelfId.IsValid() && UserId == *SelfId);
	TSharedPtr<IFriendItem> PresenceFriend = FindUser(UserId);
	bool bFoundFriend = PresenceFriend.IsValid();
	bool bFriendNotificationsBound = OnFriendsActionNotification().IsBound();
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

	// Check for out of date invites
	if (OnlineSub != nullptr && OnlineIdentity.IsValid())
	{
		TSharedPtr<FUniqueNetId> UserId0 = OnlineIdentity->GetUniquePlayerId(0);
		if (UserId0.IsValid())
		{
			TSharedPtr<FOnlineUserPresence> CurrentPresence;
			TArray<FString> InvitesToRemove;
			OnlineSub->GetPresenceInterface()->GetCachedPresence(*UserId0, CurrentPresence);
			for (auto It = PendingGameInvitesList.CreateConstIterator(); It; ++It)
			{
				FString CurrentSessionID = CurrentPresence->SessionId.IsValid() ? CurrentPresence->SessionId->ToString() : TEXT("");
				FString InviteSessionID = It.Value()->GetSessionId().IsValid() ? It.Value()->GetSessionId()->ToString() : TEXT("");
				if ((CurrentSessionID == InviteSessionID) || (GetOnlineStatus() != EOnlinePresenceState::Offline && !It.Value()->IsOnline()))
				{
					// Already in the same session so remove
					InvitesToRemove.Add(It.Key());
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

// @todo Antony.Carter Disabled for GDC as its a bit spammy
#if 0
	// When a new friend comes online, we should see eg. OnlineFriedCount = 3, OldUserPresence = 2.  Only assume we're starting up if there's a difference of 2 or more
	bool bJustStartedUp = (OnlineFriendCount - 1 > OldUserPresenceMap.Num());

	// Skip notifications for various reasons
	if (!bIsSelf)
	{
		UE_LOG(LogOnline, Verbose, TEXT("Checking friend presence change for notification %s"), *UserId.ToString());
		if (!bJustStartedUp && bFoundFriend && bFriendNotificationsBound)
		{
			const FString& FriendName = PresenceFriend->GetName();
			FFormatNamedArguments Args;
			Args.Add(TEXT("FriendName"), FText::FromString(FriendName));

			const FOnlineUserPresence* OldPresencePtr = OldUserPresenceMap.Find(FUniqueNetIdString(UserId.ToString()));
			if (OldPresencePtr == nullptr)
			{
				if ( NewPresence->bIsOnline == true)
				{
					// Had no previous presence, if the new one is online then they just logged on
					const FText PresenceChangeText = FText::Format(LOCTEXT("FriendPresenceChange_Online", "{FriendName} Is Now Online"), Args);
					TSharedPtr< FFriendsAndChatMessage > NotificationMessage = MakeShareable(new FFriendsAndChatMessage(PresenceChangeText.ToString()));
					NotificationMessage->SetMessageType(EFriendsRequestType::PresenceChange);
					OnFriendsActionNotification().Broadcast(NotificationMessage.ToSharedRef());
					UE_LOG(LogOnline, Verbose, TEXT("Notifying friend came online %s %s"), *FriendName, *UserId.ToString());
				}
				else
				{
					// This probably shouldn't be possible but can demote from warning if this turns out to be a valid use case
					UE_LOG(LogOnline, Warning, TEXT("Had no cached presence for user, then received an Offline presence. ??? %s %s"), *FriendName, *UserId.ToString());
				}
			}
			else
			{
				// Have a previous presence, see what changed
				const FOnlineUserPresence& OldPresence = *OldPresencePtr;

				if (NewPresence->bIsPlayingThisGame == true
					&& OldPresence.SessionId != NewPresence->SessionId
					&& NewPresence->SessionId.IsValid()
					&& NewPresence->SessionId->IsValid())
				{
					const FText PresenceChangeText = FText::Format(LOCTEXT("FriendPresenceChange_Online", "{FriendName} Is Now In A Game"), Args);
					TSharedPtr< FFriendsAndChatMessage > NotificationMessage = MakeShareable(new FFriendsAndChatMessage(PresenceChangeText.ToString()));
					NotificationMessage->SetMessageType(EFriendsRequestType::PresenceChange);
					OnFriendsActionNotification().Broadcast(NotificationMessage.ToSharedRef());
					UE_LOG(LogOnline, Verbose, TEXT("Notifying friend playing this game AND sessionId changed %s %s"), *FriendName, *UserId.ToString());
				}
				else if (OldPresence.bIsPlayingThisGame == false && NewPresence->bIsPlayingThisGame == true)
				{
					// could limit notifications to same game only by removing isPlaying check above
					Args.Add(TEXT("GameName"), FText::FromString(PresenceFriend->GetClientName()));
					const FText PresenceChangeText = FText::Format(LOCTEXT("FriendPresenceChange_Online", "{FriendName} Is Now Playing {GameName}"), Args);
					TSharedPtr< FFriendsAndChatMessage > NotificationMessage = MakeShareable(new FFriendsAndChatMessage(PresenceChangeText.ToString()));
					NotificationMessage->SetMessageType(EFriendsRequestType::PresenceChange);
					OnFriendsActionNotification().Broadcast(NotificationMessage.ToSharedRef());
					UE_LOG(LogOnline, Verbose, TEXT("Notifying friend isPlayingThisGame %s %s"), *FriendName, *UserId.ToString());
				}
				else if (OldPresence.bIsPlaying == false && NewPresence->bIsPlaying == true)
				{
					Args.Add(TEXT("GameName"), FText::FromString(PresenceFriend->GetClientName()));
					const FText PresenceChangeText = FText::Format(LOCTEXT("FriendPresenceChange_Online", "{FriendName} Is Now Playing {GameName}"), Args);
					TSharedPtr< FFriendsAndChatMessage > NotificationMessage = MakeShareable(new FFriendsAndChatMessage(PresenceChangeText.ToString()));
					NotificationMessage->SetMessageType(EFriendsRequestType::PresenceChange);
					OnFriendsActionNotification().Broadcast(NotificationMessage.ToSharedRef());
					UE_LOG(LogOnline, Verbose, TEXT("Notifying friend isPlaying %s %s"), *FriendName, *UserId.ToString());
				}
			}
		}

		// Make a copy of new presence to backup
		if (NewPresence->bIsOnline)
		{
			OldUserPresenceMap.Add(FUniqueNetIdString(UserId.ToString()), NewPresence.Get());
			UE_LOG(LogOnline, Verbose, TEXT("Added friend presence to oldpresence map %s"), *UserId.ToString());
		}
		else
		{
			// Or remove the presence if they went offline
			OldUserPresenceMap.Remove(FUniqueNetIdString(UserId.ToString()));
			UE_LOG(LogOnline, Verbose, TEXT("Removed offline friend presence from oldpresence map %s"), *UserId.ToString());
		}
	}
#endif
}

void FFriendsAndChatManager::OnPresenceUpdated(const FUniqueNetId& UserId, const bool bWasSuccessful)
{
	RefreshList();
}

void FFriendsAndChatManager::OnFriendsListChanged()
{
	if(ManagerState == EFriendsAndManagerState::Idle)
	{
		// Request called from outside our expected actions. e.g. Friend canceled their friend request
		RequestListRefresh();
	}
}

void FFriendsAndChatManager::OnFriendInviteReceived(const FUniqueNetId& UserId, const FUniqueNetId& FriendId)
{
	RequestListRefresh();
}

void FFriendsAndChatManager::OnGameInviteReceived(const FUniqueNetId& UserId, const FUniqueNetId& FromId, const FString& ClientId, const FOnlineSessionSearchResult& InviteResult)
{
	if (OnlineSub != NULL &&
		OnlineSub->GetIdentityInterface().IsValid())
	{
		TSharedPtr<FUniqueNetId> FromIdPtr = OnlineSub->GetIdentityInterface()->CreateUniquePlayerId(FromId.ToString());
		if (FromIdPtr.IsValid())
		{
			ReceivedGameInvites.AddUnique(FReceivedGameInvite(FromIdPtr.ToSharedRef(), InviteResult, ClientId));
		}
	}
}

void FFriendsAndChatManager::ProcessReceivedGameInvites()
{
	if (OnlineSub != NULL &&
		OnlineSub->GetUserInterface().IsValid())
	{
		for (int32 Idx = 0; Idx < ReceivedGameInvites.Num(); Idx++)
		{
			const FReceivedGameInvite& Invite = ReceivedGameInvites[Idx];

			if (!Invite.InviteResult->Session.SessionInfo.IsValid() ||
				Invite.InviteResult->Session.SessionInfo->GetSessionId().ToString() == GetGameSessionId()->ToString())
			{
				// remove invites if user is already in the game session
				ReceivedGameInvites.RemoveAt(Idx--);
			}
			else
			{
				// add to list of pending invites to accept
			TSharedPtr<FOnlineUser> UserInfo;
			TSharedPtr<IFriendItem> Friend = FindUser(*Invite.FromId);
			TSharedPtr<FOnlineFriend> OnlineFriend;
			if (Friend.IsValid())
			{
				UserInfo = Friend->GetOnlineUser();
				OnlineFriend = Friend->GetOnlineFriend();
			}
			if (!UserInfo.IsValid())
			{
				UserInfo = OnlineSub->GetUserInterface()->GetUserInfo(0, *Invite.FromId);
			}
			if (UserInfo.IsValid() && OnlineFriend.IsValid())
			{
				TSharedPtr<FFriendGameInviteItem> GameInvite = MakeShareable(
					new FFriendGameInviteItem(UserInfo.ToSharedRef(), Invite.InviteResult, Invite.ClientId, OnlineFriend.ToSharedRef())
					);

				PendingGameInvitesList.Add(Invite.FromId->ToString(), GameInvite);

				OnGameInvitesUpdated().Broadcast();
				SendGameInviteNotification(GameInvite);

				ReceivedGameInvites.RemoveAt(Idx--);
			}
		}
	}
}
}

bool FFriendsAndChatManager::RequestGameInviteUserInfo()
{
	bool bPending = false;

	// query for user ids that are not already cached from game invites
	IOnlineUserPtr UserInterface = OnlineSub->GetUserInterface();
	IOnlineIdentityPtr IdentityInterface = OnlineSub->GetIdentityInterface();
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

	FriendsListNotificationDelegate.Broadcast(true);
}

void FFriendsAndChatManager::SendChatMessageReceivedEvent(EChatMessageType::Type ChatType, TSharedPtr<IFriendItem> FriendItem)
{
	OnChatMessageRecieved().Broadcast(ChatType, FriendItem);
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
	OnFriendsJoinGame().Broadcast(*FriendItem->GetUniqueID(), *FriendItem->GetGameSessionId());

	TSharedPtr<IFriendsApplicationViewModel>* FriendsApplicationViewModel = ApplicationViewModels.Find(FriendItem->GetClientId());
	if (FriendsApplicationViewModel != nullptr &&
		(*FriendsApplicationViewModel).IsValid())
	{
		const FString AdditionalCommandline = TEXT("-invitesession=") + FriendItem->GetGameSessionId()->ToString() + TEXT(" -invitefrom=") + FriendItem->GetUniqueID()->ToString();
		(*FriendsApplicationViewModel)->LaunchFriendApp(AdditionalCommandline);
	}

	Analytics.RecordGameInvite(*FriendItem->GetUniqueID(), TEXT("Social.GameInvite.Accept"));
}

void FFriendsAndChatManager::SendGameInvite(const TSharedPtr<IFriendItem>& FriendItem)
{
	SendGameInvite(*FriendItem->GetUniqueID());
}

void FFriendsAndChatManager::SendGameInvite(const FUniqueNetId& ToUser)
{
	if (OnlineSub != nullptr &&
		OnlineIdentity.IsValid() &&
		OnlineSub->GetSessionInterface().IsValid() &&
		OnlineSub->GetSessionInterface()->GetNamedSession(GameSessionName) != nullptr)
	{
		TSharedPtr<FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
		if (UserId.IsValid())
		{
			OnlineSub->GetSessionInterface()->SendSessionInviteToFriend(*UserId, GameSessionName, ToUser);

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
		SendInviteAcceptedNotification(Friend);
		Friend->SetPendingAccept();
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
		IOnlineIdentityPtr OnlineIdentity = Online::GetIdentityInterface(TEXT("MCP"));
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
		IOnlineIdentityPtr OnlineIdentity = Online::GetIdentityInterface(TEXT("MCP"));
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
		IOnlineIdentityPtr OnlineIdentity = Online::GetIdentityInterface(TEXT("MCP"));
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
		IOnlineIdentityPtr OnlineIdentity = Online::GetIdentityInterface(TEXT("MCP"));
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
	int32& Count = PrivateChatCounts.FindOrAdd(ToUser);
	Count += 1;
}

void FFriendsAndChatAnalytics::RecordChannelChat(const FString& ToChannel)
{
	int32& Count = ChannelChatCounts.FindOrAdd(ToChannel);
	Count += 1;
}

void FFriendsAndChatAnalytics::FlushChatStats()
{
	if (Provider.IsValid())
	{
		IOnlineIdentityPtr OnlineIdentity = Online::GetIdentityInterface(TEXT("MCP"));
		if (OnlineIdentity.IsValid())
		{
			TSharedPtr<FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
			if (UserId.IsValid())
			{
				auto RecordSocialChatCountsEvents = [=](const TMap<FString, int32>& ChatCounts, const FString& ChatType)
				{
					if (ChatCounts.Num())
					{
						TArray<FAnalyticsEventAttribute> Attributes;
						for (const auto& Pair : ChannelChatCounts)
						{
							Attributes.Empty(3);
							Attributes.Emplace(TEXT("Name"), Pair.Key);
							Attributes.Emplace(TEXT("Type"), ChatType);
							Attributes.Emplace(TEXT("Count"), Pair.Value);
							Provider->RecordEvent("Social.Chat.Counts.2", Attributes);
						}
					}
				};

				RecordSocialChatCountsEvents(ChannelChatCounts, TEXT("Channel"));
				RecordSocialChatCountsEvents(PrivateChatCounts, TEXT("Private"));
			}
		}
	}
	ChannelChatCounts.Empty();
	PrivateChatCounts.Empty();
}

void FFriendsAndChatAnalytics::AddPresenceAttributes(const FUniqueNetId& UserId, TArray<FAnalyticsEventAttribute>& Attributes) const
{
	IOnlinePresencePtr OnlinePresence = Online::GetPresenceInterface(TEXT("MCP"));
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

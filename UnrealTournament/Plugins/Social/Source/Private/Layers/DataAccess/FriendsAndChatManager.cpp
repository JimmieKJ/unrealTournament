// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SFriendsContainer.h"
#include "SFriendUserHeader.h"
#include "SChatWindow.h"
#include "SInGameChat.h"
#include "SChatEntryWidget.h"
#include "SChatChrome.h"
#include "SNotificationList.h"
#include "SWindowTitleBar.h"
#include "SFriendUserHeader.h"

#include "ChatChromeViewModel.h"
#include "ChatChromeTabViewModel.h"
#include "FriendContainerViewModel.h"
#include "FriendViewModel.h"
#include "FriendsStatusViewModel.h"
#include "FriendsUserViewModel.h"
#include "ChatViewModel.h"
#include "ChatEntryViewModel.h"
#include "FriendRecentPlayerItems.h"
#include "FriendGameInviteItem.h"
#include "FriendPartyInviteItem.h"
#include "ClanRepository.h"
#include "IFriendList.h"
#include "FriendsNavigationService.h"
#include "ChatNotificationService.h"
#include "FriendsChatMarkupService.h"
#include "ChatDisplayService.h"
#include "ChatSettingsService.h"
#include "FriendsAndChatAnalytics.h"

#include "OSSScheduler.h"
#include "FriendsService.h"
#include "GameAndPartyService.h"
#include "FriendsFontStyleService.h"
#include "WebImageService.h"

#define LOCTEXT_NAMESPACE "FriendsAndChatManager"


/* FFriendsAndChatManager structors
 *****************************************************************************/

FFriendsAndChatManager::FFriendsAndChatManager() :
	ExternalOnlineSubsystem(nullptr),
	bAllowTeamChat(false)
	{}

FFriendsAndChatManager::~FFriendsAndChatManager( )
{
	Logout();
}

void FFriendsAndChatManager::Initialize(const FFriendsAndChatOptions& InOptions)
{
	SettingsService = FChatSettingsServiceFactory::Create();
	Analytics = FFriendsAndChatAnalyticsFactory::Create();
	OSSScheduler = FOSSSchedulerFactory::Create(Analytics);
	
	NotificationService = FChatNotificationServiceFactory::Create();

	WebImageService = FWebImageServiceFactory::Create();

	FriendsService = FFriendsServiceFactory::Create(OSSScheduler.ToSharedRef(), NotificationService.ToSharedRef(), WebImageService.ToSharedRef());

	NavigationService = FFriendsNavigationServiceFactory::Create(FriendsService.ToSharedRef(), InOptions.Game);

	GameAndPartyService = FGameAndPartyServiceFactory::Create(OSSScheduler.ToSharedRef(), FriendsService.ToSharedRef(), NotificationService.ToSharedRef(), InOptions.Game);

	MessageService = FMessageServiceFactory::Create(OSSScheduler.ToSharedRef(), FriendsService.ToSharedRef(), GameAndPartyService.ToSharedRef(), NotificationService.ToSharedRef());

	FontService = FFriendsFontStyleServiceFactory::Create(SettingsService.ToSharedRef());
	
	
	FriendViewModelFactory = FFriendViewModelFactoryFactory::Create(NavigationService.ToSharedRef(), FriendsService.ToSharedRef(), GameAndPartyService.ToSharedRef(), WebImageService.ToSharedRef());
	FriendsListFactory = FFriendListFactoryFactory::Create(FriendViewModelFactory.ToSharedRef(), FriendsService.ToSharedRef(), GameAndPartyService.ToSharedRef(), SettingsService.ToSharedRef());
	TSharedRef<IChatCommunicationService> CommunicationService = StaticCastSharedRef<IChatCommunicationService>(MessageService.ToSharedRef());
	MarkupServiceFactory = FFriendsChatMarkupServiceFactoryFactory::Create(CommunicationService, NavigationService.ToSharedRef(), FriendsListFactory.ToSharedRef(), GameAndPartyService.ToSharedRef());

	ExternalOnlineSubsystem = InOptions.ExternalOnlineSub;
}

/* IFriendsAndChatManager interface
 *****************************************************************************/
void FFriendsAndChatManager::Logout()
{
	if (OSSScheduler->IsLoggedIn())
	{
		GameAndPartyService->Logout();

		// Logout Message Service
		ChatRoomstoJoin.Empty();
		MessageService->OnChatPublicRoomJoined().RemoveAll(this);
		MessageService->Logout();

		// Logout Friends Service
		FriendsService->OnAddToast().RemoveAll(this);
		FriendsService->Logout();

		// Logout OSS
		OSSScheduler->Logout();

		// Clear the shared chat view model
		CachedViewModel.Reset();
	}
}

void FFriendsAndChatManager::Login(IOnlineSubsystem* InOnlineSub, bool bInIsGame, int32 LocalUserID)
{
	// OSS login
	OSSScheduler->Login(InOnlineSub, bInIsGame, LocalUserID);

	if (OSSScheduler->IsLoggedIn())
	{
		// Friends Service Login
		FriendsService->Login();
		if(!FriendsService->OnAddToast().IsBound())
		{
			FriendsService->OnAddToast().AddSP(this, &FFriendsAndChatManager::AddFriendsToast);
		}

		// Message Service Login
		MessageService->Login();
		if(!MessageService->OnChatPublicRoomJoined().IsBound())
		{
			MessageService->OnChatPublicRoomJoined().AddSP(this, &FFriendsAndChatManager::OnGlobalChatRoomJoined);
		}

		for (auto RoomName : ChatRoomstoJoin)
		{
			MessageService->JoinPublicRoom(RoomName);
		}

		// Game Party Service
		GameAndPartyService->Login();

		if (ExternalOnlineSubsystem)
		{
			FriendsService->QueryImportableFriends(ExternalOnlineSubsystem);
		}
	}
}

bool FFriendsAndChatManager::IsLoggedIn()
{
	return OSSScheduler->IsLoggedIn();
}

void FFriendsAndChatManager::SetOnline()
{
	OSSScheduler->SetOnline();
}

void FFriendsAndChatManager::SetAway()
{
	OSSScheduler->SetAway();
}

void FFriendsAndChatManager::SetOverridePresence(const FString& PresenceID)
{
	OSSScheduler->SetOverridePresence(PresenceID);
}

void FFriendsAndChatManager::ClearOverridePresence()
{
	OSSScheduler->ClearOverridePresence();
}

void FFriendsAndChatManager::SetAllowTeamChat()
{
	bAllowTeamChat = true;
	GameAndPartyService->SetTeamChatEnabled(bAllowTeamChat);
}

EOnlinePresenceState::Type FFriendsAndChatManager::GetOnlineStatus()
{
	return OSSScheduler->GetOnlineStatus();
}

void FFriendsAndChatManager::AddApplicationViewModel(const FString AppId, TSharedPtr<IFriendsApplicationViewModel> InApplicationViewModel)
{
	GameAndPartyService->AddApplicationViewModel(AppId, InApplicationViewModel);
}

void FFriendsAndChatManager::ClearApplicationViewModels()
{
	GameAndPartyService->ClearApplicationViewModels();
}

void FFriendsAndChatManager::AddRecentPlayerNamespace(const FString& Namespace)
{
	FriendsService->AddRecentPlayerNamespace(Namespace);
}

void FFriendsAndChatManager::SetAnalyticsProvider(const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider)
{
	Analytics->SetProvider(AnalyticsProvider);
}

void FFriendsAndChatManager::InsertNetworkChatMessage(const FString& InMessage)
{
	MessageService->InsertNetworkMessage(InMessage);
}

void FFriendsAndChatManager::InsertNetworkAdminMessage(const FString& InMessage)
{
	MessageService->InsertAdminMessage(InMessage);
}

void FFriendsAndChatManager::InsertLiveStreamMessage(const FString& InMessage)
{
	MessageService->InsertLiveStreamMessage(InMessage);
}

void FFriendsAndChatManager::JoinGlobalChatRoom(const FString& RoomName)
{
	if (!RoomName.IsEmpty() && MessageService->IsInGlobalChat() == false)
	{
		ChatRoomstoJoin.AddUnique(RoomName);
		MessageService->JoinPublicRoom(RoomName);
	}
}

void FFriendsAndChatManager::OnGlobalChatRoomJoined(const FString& ChatRoomID)
{
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

		Style = *InStyle;
		FFriendsAndChatModuleStyle::Initialize(Style);

		WebImageService->SetDefaultTitleIcon(&Style.FriendsListStyle.FriendImageBrush);
		FontService->SetFontStyles(Style.FriendsSmallFontStyle, Style.FriendsNormalFontStyle, Style.FriendsLargeFontStyle);

		SAssignNew(FriendListWidget, SOverlay)
		+SOverlay::Slot()
		[
			 SNew(SFriendsContainer, FontService.ToSharedRef(), SettingsService.ToSharedRef(), FFriendContainerViewModelFactory::Create(FriendsService.ToSharedRef(), GameAndPartyService.ToSharedRef(), MessageService.ToSharedRef(), ClanRepository.ToSharedRef(), FriendsListFactory.ToSharedRef(), NavigationService.ToSharedRef(), WebImageService.ToSharedRef(), SettingsService.ToSharedRef()))
			.FriendStyle(&Style)
			.Method(EPopupMethod::UseCurrentWindow)
		]
		+SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Bottom)
		[
			SAssignNew(FriendsNotificationBox, SNotificationList)
			.Font(Style.FriendsNormalFontStyle.FriendsFontNormal)
		];
	}

	// Clear notifications
	NotificationService->OnNotificationsAvailable().Broadcast(false);
	return FriendListWidget;
}

TSharedPtr< SWidget > FFriendsAndChatManager::GenerateStatusWidget( const FFriendsAndChatStyle* InStyle, bool ShowStatusOptions )
{
	if(ShowStatusOptions)
	{
		if (!ClanRepository.IsValid())
		{
			ClanRepository = FClanRepositoryFactory::Create();
		}
		check(ClanRepository.IsValid());

		TSharedRef<FFriendContainerViewModel> FriendsViewModel = FFriendContainerViewModelFactory::Create(FriendsService.ToSharedRef(), GameAndPartyService.ToSharedRef(), MessageService.ToSharedRef(), ClanRepository.ToSharedRef(), FriendsListFactory.ToSharedRef(), NavigationService.ToSharedRef(), WebImageService.ToSharedRef(), SettingsService.ToSharedRef());

		TSharedPtr<SWidget> HeaderWidget = SNew(SBox);

		// TODO need a new status widget if friends manager is really going to create it, one that shows online friends count, rather than allowing changing of status

		return HeaderWidget;
	}

	return SNew(SFriendUserHeader, FFriendsUserViewModelFactory::Create(FriendsService.ToSharedRef(), WebImageService.ToSharedRef()))
		.ShowPresenceBrush(false)
		.FriendStyle( InStyle )
		.FontStyle(&InStyle->FriendsNormalFontStyle);
}

TSharedPtr< SWidget > FFriendsAndChatManager::GenerateChatWidget(const FFriendsAndChatStyle* InStyle, TAttribute<FText> ActivationHintDelegate, EChatMessageType::Type MessageType, TSharedPtr<IFriendItem> FriendItem, TSharedPtr< SWindow > WindowPtr)
{
	TSharedRef<FChatDisplayService> ChatDisplayService = FChatDisplayServiceFactory::Create(MessageService.ToSharedRef());
	Style = *InStyle;

	TSharedPtr<FChatEntryViewModel> ChatEntryViewModel = FChatEntryViewModelFactory::Create(MessageService.ToSharedRef(), ChatDisplayService, nullptr, GameAndPartyService.ToSharedRef());

	TSharedPtr<FChatViewModel> ViewModel = FChatViewModelFactory::Create(FriendViewModelFactory.ToSharedRef(), MessageService.ToSharedRef(), NavigationService.ToSharedRef(), ChatDisplayService, FriendsService.ToSharedRef(), GameAndPartyService.ToSharedRef(), EChatViewModelType::Windowed);
	ViewModel->SetChannelFlags(MessageType);
	ViewModel->SetOutgoingMessageChannel(MessageType);

	ChatEntryViewModel->SetChatViewModel(ViewModel.ToSharedRef());

	if (FriendItem.IsValid())
	{
		TSharedPtr<FSelectedFriend> NewFriend;
		NewFriend = MakeShareable(new FSelectedFriend());
		NewFriend->DisplayName = FText::FromString(FriendItem->GetName());
		NewFriend->UserID = FriendItem->GetUniqueID();
		ViewModel->SetWhisperFriend(NewFriend);
	}

	ViewModel->RefreshMessages();
	ViewModel->SetIsActive(true);

	FontService->SetFontStyles(Style.FriendsSmallFontStyle, Style.FriendsNormalFontStyle, Style.FriendsLargeFontStyle);

	TSharedPtr<SChatWindow> ChatWidget = SNew(SChatWindow, ViewModel.ToSharedRef(), FontService.ToSharedRef())
			.FriendStyle(InStyle)
			.Method(EPopupMethod::UseCurrentWindow);

	TSharedPtr< SVerticalBox > ChatContainer = 
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		[
			ChatWidget.ToSharedRef()
		]
		+SVerticalBox::Slot()
		.VAlign(VAlign_Bottom)
		.AutoHeight()
		[
			SNew(SChatEntryWidget, ChatEntryViewModel.ToSharedRef(), FontService.ToSharedRef())
			.FriendStyle(InStyle)
		];

	if (WindowPtr.IsValid())
	{
		WindowPtr->GetOnWindowActivatedEvent().AddSP(ChatWidget.Get(), &SChatWindow::HandleWindowActivated);
		WindowPtr->GetOnWindowDeactivatedEvent().AddSP(ChatWidget.Get(), &SChatWindow::HandleWindowDeactivated);
	}

	return ChatContainer;
}

TSharedPtr<SWidget> FFriendsAndChatManager::GenerateFriendUserHeaderWidget(TSharedPtr<IFriendItem> FriendItem)
{
	if (FriendItem.IsValid())
	{
		TSharedPtr<FSelectedFriend> NewFriend;
		NewFriend = MakeShareable(new FSelectedFriend());
		NewFriend->DisplayName = FText::FromString(FriendItem->GetName());
		NewFriend->UserID = FriendItem->GetUniqueID();
		return SNew(SFriendUserHeader, FriendItem.ToSharedRef()).FriendStyle(&Style).FontStyle(&Style.FriendsNormalFontStyle).ShowUserName(true).Visibility(EVisibility::HitTestInvisible);
	}
	return nullptr;
}

void FFriendsAndChatManager::AddFriendsToast(const FText Message)
{
	if (FriendsNotificationBox.IsValid())
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 2.0f;
		Info.bUseLargeFont = false;
		FriendsNotificationBox->AddNotification(Info);
	}
}

TSharedPtr<IFriendsNavigationService> FFriendsAndChatManager::GetNavigationService()
{
	return NavigationService;
}

TSharedPtr<IChatNotificationService> FFriendsAndChatManager::GetNotificationService()
{
	return NotificationService;
}

TSharedPtr<IChatCommunicationService> FFriendsAndChatManager::GetCommunicationService()
{
	return MessageService;
}

TSharedPtr<IGameAndPartyService> FFriendsAndChatManager::GetGameAndPartyService()
{
	return GameAndPartyService;
}

TSharedRef< IChatDisplayService > FFriendsAndChatManager::GenerateChatDisplayService()
{
	return FChatDisplayServiceFactory::Create(MessageService.ToSharedRef());
}

TSharedPtr< IChatSettingsService > FFriendsAndChatManager::GetChatSettingsService()
{
	return SettingsService;
}

TSharedPtr< SWidget > FFriendsAndChatManager::GenerateChromeWidget(const struct FFriendsAndChatStyle* InStyle, TSharedRef<IChatDisplayService> InChatDisplayService, TSharedRef<IChatSettingsService> InChatSettingsService, TArray<TSharedRef<ICustomSlashCommand> >* CustomSlashCommands, bool CombineGameAndPartyChat, bool bDisplayGlobalChat)
{
	TSharedRef<FChatDisplayService> ChatDisplayService = StaticCastSharedRef<FChatDisplayService>(InChatDisplayService);

	// Enable in-game chat widget on commandline. We need to return a valid widget when disabled as UMG sets visibility on it
	if(ChatDisplayService->HideChatChrome())
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("ShowInGameChat")) == false)
		{
			return SNew(SHorizontalBox)
				.Visibility(EVisibility::Collapsed);
		}
	}

	Style = *InStyle;
	GameAndPartyService->SetShouldCombineGameChatIntoPartyTab(CombineGameAndPartyChat);

	TSharedRef<FFriendsChatMarkupService> MarkupService = MarkupServiceFactory->Create(!ChatDisplayService->HideChatChrome(), bDisplayGlobalChat, bAllowTeamChat);
	if(CustomSlashCommands != nullptr)
	{
		MarkupService->AddCustomSlashMarkupCommand(*CustomSlashCommands);
	}

	TSharedPtr<FChatEntryViewModel> ChatEntryViewModel = FChatEntryViewModelFactory::Create(MessageService.ToSharedRef(), ChatDisplayService, MarkupService, GameAndPartyService.ToSharedRef());

	if(!CachedViewModel.IsValid())
	{
		CachedViewModel = FChatChromeViewModelFactory::Create(NavigationService.ToSharedRef(), GameAndPartyService.ToSharedRef(), ChatDisplayService, InChatSettingsService, ChatEntryViewModel.ToSharedRef(), MarkupService, MessageService.ToSharedRef());

		if(bDisplayGlobalChat)
		{
			TSharedRef<FChatViewModel> GlobalChatViewModel = FChatViewModelFactory::Create(FriendViewModelFactory.ToSharedRef(), MessageService.ToSharedRef(), NavigationService.ToSharedRef(), ChatDisplayService, FriendsService.ToSharedRef(), GameAndPartyService.ToSharedRef(), EChatViewModelType::Base);
			GlobalChatViewModel->SetDefaultOutgoingChannel(EChatMessageType::Global);
			GlobalChatViewModel->SetDefaultChannelFlags(EChatMessageType::Global | EChatMessageType::Party | EChatMessageType::Whisper | EChatMessageType::Game | EChatMessageType::Admin);
			CachedViewModel->AddTab(FChatChromeTabViewModelFactory::Create(GlobalChatViewModel));
		}
		TSharedRef<FChatViewModel> PartyChatViewModel = FChatViewModelFactory::Create(FriendViewModelFactory.ToSharedRef(), MessageService.ToSharedRef(), NavigationService.ToSharedRef(), ChatDisplayService, FriendsService.ToSharedRef(), GameAndPartyService.ToSharedRef(), EChatViewModelType::Base);
		PartyChatViewModel->SetDefaultOutgoingChannel(EChatMessageType::Party);
		PartyChatViewModel->SetDefaultChannelFlags(EChatMessageType::Party | EChatMessageType::Whisper | EChatMessageType::Game| EChatMessageType::Admin);

		if(bAllowTeamChat)
		{
			TSharedRef<FChatViewModel> TeamChatViewModel = FChatViewModelFactory::Create(FriendViewModelFactory.ToSharedRef(), MessageService.ToSharedRef(), NavigationService.ToSharedRef(), ChatDisplayService, FriendsService.ToSharedRef(), GameAndPartyService.ToSharedRef(), EChatViewModelType::Base);
			TeamChatViewModel->SetDefaultOutgoingChannel(EChatMessageType::Team);
			TeamChatViewModel->SetDefaultChannelFlags(EChatMessageType::Team | EChatMessageType::Whisper | EChatMessageType::Admin);
			CachedViewModel->AddTab(FChatChromeTabViewModelFactory::Create(TeamChatViewModel));
		}

		TSharedRef<FChatViewModel> WhisperChatViewModel = FChatViewModelFactory::Create(FriendViewModelFactory.ToSharedRef(), MessageService.ToSharedRef(), NavigationService.ToSharedRef(), ChatDisplayService, FriendsService.ToSharedRef(), GameAndPartyService.ToSharedRef(), EChatViewModelType::Base);
		WhisperChatViewModel->SetDefaultOutgoingChannel(EChatMessageType::Whisper);
		WhisperChatViewModel->SetDefaultChannelFlags(EChatMessageType::Whisper| EChatMessageType::Admin);
		if(!bDisplayGlobalChat)
		{
			WhisperChatViewModel->SetIsActive(true);
		}

		TSharedRef<FChatViewModel> LiveStreamingChatViewModel = FChatViewModelFactory::Create(FriendViewModelFactory.ToSharedRef(), MessageService.ToSharedRef(), NavigationService.ToSharedRef(), ChatDisplayService, FriendsService.ToSharedRef(), GameAndPartyService.ToSharedRef(), EChatViewModelType::Base);
		LiveStreamingChatViewModel->SetDefaultOutgoingChannel(EChatMessageType::LiveStreaming);
		LiveStreamingChatViewModel->SetDefaultChannelFlags(EChatMessageType::LiveStreaming);

		TSharedRef<FChatViewModel> CombinedChatViewModel = FChatViewModelFactory::Create(FriendViewModelFactory.ToSharedRef(), MessageService.ToSharedRef(), NavigationService.ToSharedRef(), ChatDisplayService, FriendsService.ToSharedRef(), GameAndPartyService.ToSharedRef(), EChatViewModelType::Base);
		CombinedChatViewModel->SetDefaultOutgoingChannel(EChatMessageType::Combined);
		CombinedChatViewModel->SetDefaultChannelFlags(EChatMessageType::Whisper | EChatMessageType::Admin | EChatMessageType::Team | EChatMessageType::Party);

		CachedViewModel->AddTab(FChatChromeTabViewModelFactory::Create(PartyChatViewModel));
		CachedViewModel->AddTab(FChatChromeTabViewModelFactory::Create(WhisperChatViewModel));
		CachedViewModel->AddTab(FChatChromeTabViewModelFactory::Create(LiveStreamingChatViewModel));
		CachedViewModel->AddTab(FChatChromeTabViewModelFactory::Create(CombinedChatViewModel));
	}
	else
	{
		// Clone Chrome View Model. Keep messages, but add new style
		CachedViewModel = CachedViewModel->Clone(ChatDisplayService, InChatSettingsService, ChatEntryViewModel.ToSharedRef(), MarkupService);
	}

	TSharedPtr<SWidget> ChatWindow;

	FontService->SetFontStyles(Style.FriendsSmallFontStyle, Style.FriendsNormalFontStyle, Style.FriendsLargeFontStyle);

	if(ChatDisplayService->HideChatChrome())
	{
		ChatWindow = SNew(SInGameChat, CachedViewModel.ToSharedRef(), FontService.ToSharedRef())
		.FriendStyle(InStyle);
	}
	else
	{
		ChatWindow = SNew(SChatChrome, CachedViewModel.ToSharedRef(), FontService.ToSharedRef())
		.FriendStyle(InStyle);
	}
	
	ChatWindow->SetVisibility(EVisibility::SelfHitTestInvisible);
	return ChatWindow;
}

void FFriendsAndChatManager::OnSendPartyInvitationCompleteInternal(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& RecipientId, const ESendPartyInvitationCompletionResult Result)
{
	OnSendPartyInvitationComplete().Broadcast(LocalUserId, PartyId, RecipientId, Result);
}

#undef LOCTEXT_NAMESPACE

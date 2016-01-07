// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "FriendContainerViewModel.h"
#include "FriendViewModel.h"
#include "FriendsStatusViewModel.h"
#include "FriendListViewModel.h"
#include "FriendsUserViewModel.h"
#include "ClanCollectionViewModel.h"
#include "ClanRepository.h"
#include "IFriendList.h"
#include "FriendsNavigationService.h"

#include "FriendsService.h"
#include "MessageService.h"
#include "GameAndPartyService.h"
#include "ChatSettingsService.h"

class FriendContainerViewModelmpl
	: public FFriendContainerViewModel
{
public:

	virtual TSharedRef< class FFriendsUserViewModel > GetUserViewModel() override
	{
		return FFriendsUserViewModelFactory::Create(FriendsService, WebImageService);
	}

	virtual TSharedRef< FFriendsStatusViewModel > GetStatusViewModel() override
	{
		return FFriendsStatusViewModelFactory::Create(FriendsService);
	}

	virtual TSharedRef< FFriendListViewModel > GetFriendListViewModel(EFriendsDisplayLists::Type ListType) override
	{
		return FFriendListViewModelFactory::Create(FriendsListFactory->Create(ListType), ListType, ChatSettingService);
	}

	virtual TSharedRef< FClanCollectionViewModel > GetClanCollectionViewModel() override
	{
		return FClanCollectionViewModelFactory::Create(ClanRepository, FriendsListFactory, ChatSettingService);
	}

	virtual void RequestFriend(const FText& FriendName) const override
	{
		FriendsService->RequestFriendByName(FriendName);
	}

	virtual EVisibility GetGlobalChatButtonVisibility() const override
	{
		return MessageService->IsInGlobalChat() && GameAndPartyService->IsInLauncher() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	virtual void DisplayGlobalChatWindow() const override
	{
		if (MessageService->IsInGlobalChat())
		{
			MessageService->OpenGlobalChat();
			NavigationService->ChangeViewChannel(EChatMessageType::Global);
		}
	}

	virtual const FString GetName() const override
	{
		FString Nickname = FriendsService->GetUserNickname();
		return Nickname;
	}

	virtual void OpenConfirmDialog(FText Description, TSharedPtr<FFriendViewModel> FriendViewModel) override
	{
		ConfirmFriendViewModel = FriendViewModel;
		ConfirmVisibility = EVisibility::Visible;
	}

	virtual FText GetConfirmDescription() const override
	{
		return ConfirmDescription;
	}

	virtual EVisibility GetConfirmVisibility() const override
	{
		return ConfirmVisibility;
	}

	virtual void OnConfirm() override
	{
		if (ConfirmFriendViewModel.IsValid())
		{
			ConfirmFriendViewModel->ConfirmAction();
		}
		ConfirmVisibility = EVisibility::Collapsed;

	}
	
	virtual void OnCancel() override
	{
		if (ConfirmFriendViewModel.IsValid())
		{
			ConfirmFriendViewModel->CancelAction();
		}
		ConfirmVisibility = EVisibility::Collapsed;
	}

	~FriendContainerViewModelmpl()
	{
		Uninitialize();
	}

private:

	void Uninitialize()
	{
		NavigationService->OnChatViewChanged().RemoveAll(this);
	}

	FriendContainerViewModelmpl(
		const TSharedRef<FFriendsService>& InFriendsService,
		const TSharedRef<FGameAndPartyService>& InGameAndPartyService,
		const TSharedRef<FMessageService>& InMessageService,
		const TSharedRef<IClanRepository>& InClanRepository,
		const TSharedRef<IFriendListFactory>& InFriendsListFactory,
		const TSharedRef<FFriendsNavigationService>& InNavigationService,
		const TSharedRef<FWebImageService>& InWebImageService,
		const TSharedRef<FChatSettingsService>& InChatSettingService
		)
		: FriendsService(InFriendsService)
		, GameAndPartyService(InGameAndPartyService)
		, MessageService(InMessageService)
		, ClanRepository(InClanRepository)
		, FriendsListFactory(InFriendsListFactory)
		, NavigationService(InNavigationService)
		, WebImageService(InWebImageService)
		, ChatSettingService(InChatSettingService)
		, ConfirmVisibility(EVisibility::Collapsed)
	{
	}

private:
	TSharedRef<FFriendsService> FriendsService;
	TSharedRef<FGameAndPartyService> GameAndPartyService;
	TSharedRef<FMessageService> MessageService;
	TSharedRef<IClanRepository> ClanRepository;
	TSharedRef<IFriendListFactory> FriendsListFactory;
	TSharedRef<FFriendsNavigationService> NavigationService;
	TSharedRef<FWebImageService> WebImageService;
	TSharedRef<FChatSettingsService> ChatSettingService;

	EVisibility ConfirmVisibility;
	FText ConfirmDescription;
	TSharedPtr<FFriendViewModel> ConfirmFriendViewModel;

private:
	friend FFriendContainerViewModelFactory;
};

TSharedRef< FFriendContainerViewModel > FFriendContainerViewModelFactory::Create(
	const TSharedRef<FFriendsService>& FriendsService,
	const TSharedRef<FGameAndPartyService>& GameAndPartyService,
	const TSharedRef<FMessageService>& MessageService,
	const TSharedRef<IClanRepository>& ClanRepository,
	const TSharedRef<IFriendListFactory>& FriendsListFactory,
	const TSharedRef<FFriendsNavigationService>& NavigationService,
	const TSharedRef<FWebImageService>& WebImageService,
	const TSharedRef<FChatSettingsService>& ChatSettingService
	)
{
	TSharedRef< FriendContainerViewModelmpl > ViewModel(new FriendContainerViewModelmpl(FriendsService, GameAndPartyService, MessageService, ClanRepository, FriendsListFactory, NavigationService, WebImageService, ChatSettingService));
	return ViewModel;
}

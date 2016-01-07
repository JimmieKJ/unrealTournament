// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "FriendViewModel.h"
#include "FriendContainerViewModel.h"
#include "FriendListViewModel.h"
#include "IFriendList.h"
#include "ChatSettingsService.h"

#define LOCTEXT_NAMESPACE "FriendsAndChat"

class FFriendListViewModelImpl
	: public FFriendListViewModel
{
public:

	~FFriendListViewModelImpl()
	{
		Uninitialize();
	}

	virtual const TArray< TSharedPtr< class FFriendViewModel > >& GetFriendsList() const override
	{
		return FriendsList;
	}

	virtual FText GetListCountText() const override
	{
		if (ListType == EFriendsDisplayLists::DefaultDisplay && OnlineCount != FriendsList.Num())
		{
			return FText::Format(LOCTEXT("ListCount", "({0}/{1})"), FText::AsNumber(OnlineCount), FText::AsNumber(FriendsList.Num()));
		}
		return FText::Format(LOCTEXT("ListCountShort", "({0})"), FText::AsNumber(FriendsList.Num()));
	}

	virtual int32 GetListCount() const override
	{
		return FriendsList.Num();
	}

	virtual const FText GetListName() const override
	{
		return EFriendsDisplayLists::ToFText(ListType);
	}

	virtual const EFriendsDisplayLists::Type GetListType() const override
	{
		return ListType;
	}

	virtual EVisibility GetListVisibility() const override
	{
		return bIsEnabled && (FriendsListContainer->IsFilterSet() || FriendsList.Num() > 0) ? EVisibility::Visible : EVisibility::Collapsed;
	}

	virtual void SetListFilter(const FText& CommentText) override
	{
		FriendsListContainer->SetListFilter(CommentText);
	}

	virtual bool IsEnabled() const override
	{
		return bIsEnabled;
	}

	DECLARE_DERIVED_EVENT(FFriendListViewModelImpl , FFriendListViewModel::FFriendsListUpdated, FFriendsListUpdated);
	virtual FFriendsListUpdated& OnFriendsListUpdated() override
	{
		return FriendsListUpdatedEvent;
	}

private:
	void Initialize()
	{
		FriendsListContainer->OnFriendsListUpdated().AddSP(this, &FFriendListViewModelImpl::RefreshFriendsList);
		RefreshFriendsList();
		ChatSettingService->OnChatSettingStateUpdated().AddSP(this, &FFriendListViewModelImpl::HandleSettingsUpdated);
		UpdateEnabledState();
	}

	void HandleSettingsUpdated(EChatSettingsType::Type Setting, bool NewState)
	{
		UpdateEnabledState();
	}

	void UpdateEnabledState()
	{
		bIsEnabled = true;
		switch (ListType)
		{
			case EFriendsDisplayLists::OutgoingFriendInvitesDisplay:
			{
				if (ChatSettingService->GetFlagOption(EChatSettingsType::HideOutgoing) == true)
				{
					bIsEnabled = false;
				}
			}break;
			case EFriendsDisplayLists::RecentPlayersDisplay:
			{
				if (ChatSettingService->GetFlagOption(EChatSettingsType::HideRecent) == true)
				{
					bIsEnabled = false;
				}
			}break;
			case EFriendsDisplayLists::SuggestedFriendsDisplay:
			{
				if (ChatSettingService->GetFlagOption(EChatSettingsType::HideSuggestions) == true)
				{
					bIsEnabled = false;
				}
			}break;
			case EFriendsDisplayLists::BlockedPlayersDisplay:
			{
				if (ChatSettingService->GetFlagOption(EChatSettingsType::HideBlocked) == true)
				{
					bIsEnabled = false;
				}
			}break;
		}
	}

	void Uninitialize()
	{
		ChatSettingService->OnChatSettingStateUpdated().RemoveAll(this);
	}

	void RefreshFriendsList()
	{
		FriendsList.Empty();
		OnlineCount = FriendsListContainer->GetFriendList(FriendsList);
		FriendsListUpdatedEvent.Broadcast();
	}

	FFriendListViewModelImpl( 
		TSharedRef<IFriendList> InFriendsListContainer,
		EFriendsDisplayLists::Type InListType,
		const TSharedRef<FChatSettingsService>& InChatSettingService)
		: FriendsListContainer(InFriendsListContainer)
		, ListType(InListType)
		, ChatSettingService(InChatSettingService)
		, bIsEnabled(false)
	{
	}

private:

	// Hold the interface to the container of our list items
	TSharedRef<IFriendList> FriendsListContainer;

	// List type
	const EFriendsDisplayLists::Type ListType;

	TSharedRef<FChatSettingsService> ChatSettingService;

	/** Holds the list of friends. */
	TArray< TSharedPtr< FFriendViewModel > > FriendsList;

	// Event broadcast when list is refreshed
	FFriendsListUpdated FriendsListUpdatedEvent;

	// Number of items marked as Online
	int32 OnlineCount;

	// Is this list enabled
	bool bIsEnabled;

	// Factory
	friend FFriendListViewModelFactory;
};

TSharedRef< FFriendListViewModel > FFriendListViewModelFactory::Create(
	TSharedRef<IFriendList> FriendsListContainer,
	EFriendsDisplayLists::Type ListType,
	const TSharedRef<FChatSettingsService>& ChatSettingService)
{
	TSharedRef< FFriendListViewModelImpl > ViewModel(new FFriendListViewModelImpl(FriendsListContainer, ListType, ChatSettingService));
	ViewModel->Initialize();
	return ViewModel;
}

#undef LOCTEXT_NAMESPACE
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "FriendViewModel.h"
#include "FriendsViewModel.h"
#include "FriendListViewModel.h"
#include "IFriendList.h"

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
		return FText::AsNumber(FriendsList.Num());
	}

	virtual int32 GetListCount() const override
	{
		return FriendsList.Num();
	}

	virtual FText GetOnlineCountText() const override
	{
		FFormatNamedArguments Arguments;
		Arguments.Add( TEXT("ListCount"), FText::AsNumber(OnlineCount));
		const FText ListCount = FText::Format(LOCTEXT("FFriendListViewModelImpl_ListCount", "{ListCount} / "), Arguments);
		return ListCount;
	}

	virtual EVisibility GetOnlineCountVisibility() const
	{
		return ListType == EFriendsDisplayLists::DefaultDisplay ? EVisibility::Visible : EVisibility::Collapsed;
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
		return FriendsListContainer->IsFilterSet() || FriendsList.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
	}

	virtual void SetListFilter(const FText& CommentText) override
	{
		FriendsListContainer->SetListFilter(CommentText);
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
	}

	void Uninitialize()
	{
	}

	void RefreshFriendsList()
	{
		FriendsList.Empty();
		OnlineCount = FriendsListContainer->GetFriendList(FriendsList);
		FriendsListUpdatedEvent.Broadcast();
	}

	FFriendListViewModelImpl( 
		TSharedRef<IFriendList> InFriendsListContainer,
		EFriendsDisplayLists::Type InListType)
		: FriendsListContainer(InFriendsListContainer)
		, ListType(InListType)
	{
	}

private:
	TSharedRef<IFriendList> FriendsListContainer;
	const EFriendsDisplayLists::Type ListType;

	/** Holds the list of friends. */
	TArray< TSharedPtr< FFriendViewModel > > FriendsList;
	FFriendsListUpdated FriendsListUpdatedEvent;
	int32 OnlineCount;

	friend FFriendListViewModelFactory;
};

TSharedRef< FFriendListViewModel > FFriendListViewModelFactory::Create(
	TSharedRef<IFriendList> FriendsListContainer,
	EFriendsDisplayLists::Type ListType)
{
	TSharedRef< FFriendListViewModelImpl > ViewModel(new FFriendListViewModelImpl(FriendsListContainer, ListType));
	ViewModel->Initialize();
	return ViewModel;
}

#undef LOCTEXT_NAMESPACE
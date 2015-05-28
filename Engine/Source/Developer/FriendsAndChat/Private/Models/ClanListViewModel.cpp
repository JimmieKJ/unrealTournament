// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "ClanViewModel.h"
#include "ClanListViewModel.h"

#define LOCTEXT_NAMESPACE "FriendsAndChat"

class FClanListViewModelImpl : public FClanListViewModel
{
public:

	FClanListViewModelImpl(const TSharedRef<FClanViewModel>& InClanViewModel, EClanDisplayLists::Type InListType)
		: ViewModel(InClanViewModel)
		, ListType(InListType)
	{
	}

	~FClanListViewModelImpl()
	{
		Uninitialize();
	}

	virtual const TArray<TSharedRef<IClanInfo>>& GetClanList() const override
	{
		return ClanList;
	}

	virtual FText GetListCountText() const override
	{
		return FText::AsNumber(ClanList.Num());
	}

	virtual int32 GetListCount() const override
	{
		return ClanList.Num();
	}

	virtual const FText GetListName() const override
	{
		return EClanDisplayLists::ToFText(ListType);
	}

	virtual const EClanDisplayLists::Type GetListType() const override
	{
		return ListType;
	}

	virtual EVisibility GetListVisibility() const override
	{
		return ClanList.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
	}

	DECLARE_DERIVED_EVENT(FClanListViewModelImpl, FClanListViewModel::FClanListUpdated, FClanListUpdated);
	virtual FClanListUpdated& OnClanListUpdated() override
	{
		return ClanListUpdatedEvent;
	}

private:
	void Initialize()
	{
		RefreshClanList();
	}

	void Uninitialize()
	{
	}

	void RefreshClanList()
	{
		ViewModel->EnumerateJoinedClans(ClanList);
		ClanListUpdatedEvent.Broadcast();
	}


private:
	const TSharedRef<FClanViewModel> ViewModel;
	const EClanDisplayLists::Type ListType;

	/** Holds the list of clans. */
	TArray<TSharedRef<IClanInfo>> ClanList;
	FClanListUpdated ClanListUpdatedEvent;

	friend FClanListViewModelFactory;
};

TSharedRef< FClanListViewModel > FClanListViewModelFactory::Create(const TSharedRef<class FClanViewModel>& ClanViewModel, EClanDisplayLists::Type ListType)
{
	TSharedRef< FClanListViewModelImpl > ViewModel(new FClanListViewModelImpl(ClanViewModel, ListType));
	ViewModel->Initialize();
	return ViewModel;
}

#undef LOCTEXT_NAMESPACE
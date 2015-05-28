// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "ClanMemberList.h"
#include "IFriendItem.h"
#include "FriendViewModel.h"
#include "ClanInfo.h"

class FClanMemberListImpl
	: public FClanMemberList
{
public:

	virtual int32 GetFriendList(TArray< TSharedPtr<FFriendViewModel> >& OutFriendsList) override
	{
		OutFriendsList = FilteredMemberList;
		return 0;
	}

	virtual bool IsFilterSet()
	{
		return true;
	}

	virtual void SetListFilter(const FText& CommentText) override
	{
		FilteredMemberList.Empty();
		const FString CommentString = CommentText.ToString();
		TArray< TSharedPtr< IFriendItem > > FriendItemList = ClanInfo->GetMemberList();
		TArray< TSharedPtr< IFriendItem > > FilteredItemList;
		for(const auto& FriendItem : FriendItemList)
		{
			if(CommentString.IsEmpty() || FriendItem->GetName().Contains(CommentString))
			{
				FilteredItemList.Add(FriendItem);
			}
		}

		FilteredItemList.Sort(FCompareGroupByName());
		for(const auto& FriendItem : FilteredItemList)
		{
			FilteredMemberList.Add(FFriendViewModelFactory::Create(FriendItem.ToSharedRef()));
		}

		OnFriendsListUpdated().Broadcast();
	}

	DECLARE_DERIVED_EVENT(FClanMemberList, IFriendList::FFriendsListUpdated, FFriendsListUpdated);
	virtual FFriendsListUpdated& OnFriendsListUpdated() override
	{
		return FriendsListUpdatedEvent;
	}

public:
	FFriendsListUpdated FriendsListUpdatedEvent;
	TArray< TSharedPtr<FFriendViewModel> > FilteredMemberList;

private:
	void Initialize()
	{
		ClanInfo->OnChanged().AddSP(this, &FClanMemberListImpl::HandleMemberListUpdated);
		SetListFilter(FText::GetEmpty());
	}

	void HandleMemberListUpdated()
	{
		OnFriendsListUpdated().Broadcast();
	}

	FClanMemberListImpl(TSharedRef<IClanInfo> InClanInfo)
		: ClanInfo(InClanInfo)
	{}

private:

	TSharedRef<IClanInfo> ClanInfo;

	friend FClanMemberListFactory;
};

TSharedRef< FClanMemberList > FClanMemberListFactory::Create(TSharedRef<IClanInfo> ClanInfo)
{
	TSharedRef< FClanMemberListImpl > ChatList(new FClanMemberListImpl(ClanInfo));
	ChatList->Initialize();
	return ChatList;
}
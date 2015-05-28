// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FFriendListViewModel
	: public TSharedFromThis<FFriendListViewModel>
{
public:
	virtual ~FFriendListViewModel() {}
	virtual const TArray< TSharedPtr< class FFriendViewModel > >& GetFriendsList() const = 0;
	virtual FText GetListCountText() const = 0;
	virtual int32 GetListCount() const = 0;
	virtual FText GetOnlineCountText() const = 0;
	virtual EVisibility GetOnlineCountVisibility() const = 0;
	virtual const FText GetListName() const = 0;
	virtual const EFriendsDisplayLists::Type GetListType() const = 0;
	virtual EVisibility GetListVisibility() const = 0;
	virtual void SetListFilter(const FText& CommentText) = 0;

	DECLARE_EVENT(FFriendListViewModel, FFriendsListUpdated)
	virtual FFriendsListUpdated& OnFriendsListUpdated() = 0;
};

/**
 * Creates the implementation for an FriendListViewModel.
 *
 * @return the newly created FFriendListViewModel implementation.
 */
FACTORY(TSharedRef< FFriendListViewModel >, FFriendListViewModel,
	TSharedRef<class IFriendList> FriendsListContainer,
	EFriendsDisplayLists::Type ListType);
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Class containing the friend information - used to build the list view.
 */
class IFriendList
{
public:

	/** Virtual destructor. */
	virtual ~IFriendList() { }

	virtual int32 GetFriendList(TArray< TSharedPtr< class FFriendViewModel > >& OutFriendsList) = 0;

	virtual bool IsFilterSet() {return false;};

	virtual void SetListFilter(const FText& CommentText) {};

	DECLARE_EVENT(IFriendList, FFriendsListUpdated)
	virtual FFriendsListUpdated& OnFriendsListUpdated() = 0;

protected:
	/** Functor for sorting friends list */
	struct FCompareGroupByName
	{
		FORCEINLINE bool operator()( const TSharedPtr< IFriendItem > A, const TSharedPtr< IFriendItem > B ) const
		{
			check( A.IsValid() );
			check ( B.IsValid() );
			return ( A->GetName() < B->GetName() );
		}
	};
};

IFACTORY(TSharedRef<IFriendList>, IFriendList, 
	EFriendsDisplayLists::Type ListType
	);

FACTORY(TSharedRef< IFriendListFactory >, FFriendListFactory);

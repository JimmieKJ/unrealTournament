// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Type definition for shared pointers to instances of FNewsFeedItem. */
typedef TSharedPtr<struct FNewsFeedItem> FNewsFeedItemPtr;

/** Type definition for shared references to instances of FNewsFeedItem. */
typedef TSharedRef<struct FNewsFeedItem> FNewsFeedItemRef;


/**
 * Implements a view model for the news feed list.
 */
struct FNewsFeedItem
{
	/** The news excerpt. */
	FText Excerpt;

	/** The full news text. */
	FText FullText;

	/** Holds the name of the icon. */
	FString IconName;

	/** The date and time at which the news item was issued. */
	FDateTime Issued;

	/** The news item's unique identifier. */
	FGuid ItemId;

	/** Whether this news item has been marked as read. */
	bool Read;

	/** The news title. */
	FText Title;

	/** The URL to the full news release. */
	FString Url;

public:

	/**
	 * Creates and initializes a new item with the specified identifier.
	 *
	 * @param InItemId The item identifier.
	 */
	FNewsFeedItem( const FGuid& InItemId )
		: ItemId(InItemId)
	{ }
};

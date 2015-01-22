// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the news feed widget.
 */
class SNewsFeed
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SNewsFeed) { }
	SLATE_END_ARGS()

public:

	/**
	 * Destructor.
	 */
	~SNewsFeed( );

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 * @param InNewsFeedCache The news feed cache to use.
	 */
	void Construct( const FArguments& InArgs, const FNewsFeedCacheRef& InNewsFeedCache );

protected:

	/**
	 * Reloads the list of news feed items.
	 */
	void ReloadNewsFeedItems( );

private:

	// Callback for getting the visibility of the 'Loading failed' box.
	EVisibility HandleLoadFailedBoxVisibility( ) const;

	// Callback for clicking the 'Mark all as read' button.
	FReply HandleMarkAllAsReadClicked( );

	// Callback for getting the enabled state of the 'Mark all as read' button.
	bool HandleMarkAllAsReadIsEnabled( ) const;

	// Callback for completed news feeds loads.
	void HandleNewsFeedLoaderLoadCompleted( );

	// Callback for failed news feeds loads.
	void HandleNewsFeedLoaderLoadFailed( );

	// Callback for news feed settings changes.
	void HandleNewsFeedSettingsChanged( FName PropertyName );

	// Callback for generating a row widget in the device service list view.
	TSharedRef<ITableRow> HandleNewsListViewGenerateRow( FNewsFeedItemPtr NewsFeedItem, const TSharedRef<STableViewBase>& OwnerTable );

	// Callback for selecting a news feed item.
	void HandleNewsListViewSelectionChanged( FNewsFeedItemPtr Selection, ESelectInfo::Type SelectInfo );

	// Callback for getting the visibility of the news list view.
	EVisibility HandleNewsListViewVisibility( ) const;

	// Callback for getting the text of the 'news unavailable' notification.
	FText HandleNewsUnavailableText( ) const;

	// Callback for getting the visibility of the 'news unavailable' notification.
	EVisibility HandleNewsUnavailableVisibility( ) const;

	// Callback for clicking the 'Refresh' button.
	FReply HandleRefreshButtonClicked( ) const;

	// Callback for getting the visibility of the 'Refresh' button.
	EVisibility HandleRefreshButtonVisibility( ) const;

	// Callback for getting the visibility of the refresh throbber.
	EVisibility HandleRefreshThrobberVisibility( ) const;

	// Callback for clicking the 'Settings' button.
	FReply HandleSettingsButtonClicked( ) const;

	// Callback for clicking the 'Show older news' hyper-link.
	void HandleShowOlderNewsNavigate( );

	// Callback for getting the text of the 'Show older news' hyper-link.
	FText HandleShowOlderNewsText( ) const;

	// Callback for getting the visibility of the 'Show older news' hyper-link.
	EVisibility HandleShowOlderNewsVisibility( ) const;

private:

	// Holds the number of hidden news items.
	int32 HiddenNewsItemCount;

	// Holds a pointer the news feed cache.
	FNewsFeedCachePtr NewsFeedCache;

	// Holds the list of news items.
	TArray<FNewsFeedItemPtr> NewsFeedItemList;

	// Holds the list view for news items.
	TSharedPtr<SListView<FNewsFeedItemPtr>> NewsFeedItemListView;

	// Whether older news are currently being shown as well.
	bool ShowingOlderNews;
};

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 */
enum class ENewsFeedCultureSpec
{
	Full = 0,
	LanguageOnly = 1,
	None = 2
};


/**
 * Enumerates valid news feed cache states.
 */
namespace ENewsFeedState
{
	enum Type
	{
		/** Download of news feed has not started yet. */
		NotStarted = 0,

		/** News feed is being fetched from data source. */
		Loading = 1,

		/** News feed finished loading successfully. */
		Success = 2,

		/** News feed failed to load. */
		Failure = 3
	};
}


/** Type definition for shared pointers to instances of FNewsFeedCache. */
typedef TSharedPtr<class FNewsFeedCache> FNewsFeedCachePtr;

/** Type definition for shared references to instances of FNewsFeedCache. */
typedef TSharedRef<class FNewsFeedCache> FNewsFeedCacheRef;


/**
 * Implements a cache of news feed items that were fetched from the data source.
 */
class FNewsFeedCache
{
public:

	/**
	 * Default constructor.
	 */
	FNewsFeedCache( );

	/**
	 * Destructor.
	 */
	~FNewsFeedCache( );

public:

	/**
	 * Gets the specified icon.
	 *
	 * If the icon is not yet loaded, a placeholder icon will be returned.
	 * If the icon failed to load, an error icon will be returned.
	 *
	 * @param IconName The name of the icon to get.
	 * @return The icon brush.
	 */
	const FSlateBrush* GetIcon( const FString& IconName ) const;

	/**
	 * Gets the loaded news feed items.
	 *
	 * @return Collection of news feed items.
	 */
	const TArray<FNewsFeedItemPtr>& GetItems( ) const
	{
		return Items;
	}

	/**
	 * Checks whether the last attempt to load a feed has finished.
	 *
	 * @return true if feed loading finished, false otherwise.
	 * @see IsLoading
	 */
	bool IsFinished( ) const
	{
		return (LoaderState == ENewsFeedState::Success);
	}

	/**
	 * Checks whether this loader is currently loading a news feed.
	 *
	 * @return true if loading, false otherwise.
	 * @see IsFinished
	 */
	bool IsLoading( ) const
	{
		return (LoaderState == ENewsFeedState::Loading);
	}

	/**
	 * Loads the news feed from the given title file.
	 *
	 * @param InTitleFile The title file to read the news feed from.
	 * @return true if the news feed is being loaded, false otherwise.
	 */
	//bool LoadFeed( IOnlineTitleFilePtr InTitleFile );
	bool LoadFeed( );

public:

	/**
	 * Gets an event delegate that is executed when loading of a news feed has completed.
	 *
	 * @return The event delegate.
	 */
	DECLARE_EVENT(FNewsFeedCache, FOnNewsFeedLoadCompleted);
	FOnNewsFeedLoadCompleted& OnLoadCompleted( )
	{
		return LoadCompletedDelegate;
	}

	/**
	 * Gets an event delegate that is executed when loading of a news feed has failed.
	 *
	 * @return The event delegate.
	 */
	DECLARE_EVENT(FNewsFeedCache, FOnNewsFeedLoadFailed);
	FOnNewsFeedLoadFailed& OnLoadFailed( )
	{
		return LoadFailedDelegate;
	}

protected:

	/**
	 * Signals that the loading has failed.
	 */
	void FailLoad( );

	/**
	 * Attempts to load the title file.
	 */
	void LoadTitleFile( );

	/**
	 * Processes the specified icon file.
	 *
	 * @param IconName The name of the icon file.
	 */
	void ProcessIconFile( const FString& IconName );

	/**
	 * Processes the Json file containing the news feed items.
	 */
	void ProcessNewsFeedFile( );

	/**
	 * Converts the given binary data into a Slate brush.
	 *
	 * @param ResourceName 
	 * @param RawData The raw binary data to convert.
	 * @return The Slate brush.
	 */
	TSharedPtr<FSlateDynamicImageBrush> RawDataToBrush( FName ResourceName, const TArray< uint8 >& RawData ) const;

	/**
	 * Saves all settings.
	 */
	void SaveSettings( );

private:

	// Callback for when file enumeration has completed.
	void HandleEnumerateFilesComplete( bool bSuccess );

	// Callback for when file reading has completed.
	void HandleReadFileComplete( bool bSuccess, const FString& Filename );

	// Callback for ticks from the ticker.
	bool HandleTicker( float DeltaTime );

private:

	// Holds the header of the news feed file.
	FCloudFileHeader FeedFileHeader;

	// Holds the collection of loaded icons.
	TMap<FString, TSharedPtr<FSlateDynamicImageBrush> > Icons;

	// Holds the news feed items.
	TArray<FNewsFeedItemPtr> Items;

	// Holds the current state.
	ENewsFeedState::Type LoaderState;

	// Holds a delegate to be invoked when the feed needs to be auto-reloaded.
	FTickerDelegate TickDelegate;

	// Handle to the registered TickDelegate.
	FDelegateHandle TickDelegateHandle;

	// Holds the title file to load from.
	IOnlineTitleFilePtr TitleFile;

private:

	// Holds the culture currently being loaded with.
	ENewsFeedCultureSpec CurrentCultureSpec;

	// Holds an event delegate that is executed when loading of a news feed has completed.
	FOnNewsFeedLoadCompleted LoadCompletedDelegate;

	// Holds an event delegate that is executed when loading of a news feed has failed.
	FOnNewsFeedLoadFailed LoadFailedDelegate;
};

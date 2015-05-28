// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "MRUFavoritesList.h"
#include "ConfigCacheIni.h"
#include "SNotificationList.h"
#include "NotificationManager.h"

const FString FMainMRUFavoritesList::FAVORITES_INI_SECTION = TEXT("FavoriteFiles");

FMainMRUFavoritesList::FMainMRUFavoritesList()
	: FMRUList( TEXT("MRU") )
{
}

/** Destructor */
FMainMRUFavoritesList::~FMainMRUFavoritesList()
{
	FavoriteItems.Empty();
}

/** Populate MRU/Favorites list by reading saved values from the relevant INI file */
void FMainMRUFavoritesList::ReadFromINI()
{
	// Read in the MRU items
	InternalReadINI( Items, INISection, TEXT("MRUItem"), GetMaxItems() );

	// Read in the Favorite items
	InternalReadINI( FavoriteItems, FAVORITES_INI_SECTION, TEXT("FavoritesItem"), MaxItems );	
}

/** Save off the state of the MRU and favorites lists to the relevant INI file */
void FMainMRUFavoritesList::WriteToINI() const
{
	InternalWriteINI( Items, INISection, TEXT("MRUItem") );
	InternalWriteINI( FavoriteItems, FAVORITES_INI_SECTION, TEXT("FavoritesItem") );
}

/**
 * Add a new file item to the favorites list
 *
 * @param	Item	Filename of the item to add to the favorites list
 */
void FMainMRUFavoritesList::AddFavoritesItem( const FString& Item )
{
	// Only add the item if it isn't already a favorite!
	const FString CleanedName = FPaths::ConvertRelativePathToFull(Item);
	if ( !FavoriteItems.Contains( CleanedName ) )
	{
		FavoriteItems.Insert( CleanedName, 0 );
		WriteToINI();
	}
}

/**
 * Remove a file from the favorites list
 *
 * @param	Item	Filename of the item to remove from the favorites list
 */
void FMainMRUFavoritesList::RemoveFavoritesItem( const FString& Item )
{
	const FString CleanedName = FPaths::ConvertRelativePathToFull(Item);
	const int32 ItemIndex = FavoriteItems.Find( CleanedName );
	if ( ItemIndex != INDEX_NONE )
	{
		FavoriteItems.RemoveAt( ItemIndex );
		WriteToINI();
	}
}

/**
 * Moves the specified favorites item to the head of the list
 *
 * @param	Item	Filename of the item to move
 */
void FMainMRUFavoritesList::MoveFavoritesItemToHead(const FString& Item)
{
	const FString CleanedName = FPaths::ConvertRelativePathToFull(Item);
	if ( FavoriteItems.RemoveSingle(Item) == 1 )
	{
		FavoriteItems.Insert( CleanedName, 0 );
		WriteToINI();
	}
}

/**
 * Returns whether a filename is favorited or not
 *
 * @param	Item	Filename of the item to check
 *
 * @return	true if the provided item is in the favorite's list; false if it is not
 */
bool FMainMRUFavoritesList::ContainsFavoritesItem( const FString& Item )
{
	const FString CleanedName = FPaths::ConvertRelativePathToFull(Item);
	return FavoriteItems.Contains( CleanedName );
}

/**
 * Return the favorites item specified by the provided index
 *
 * @param	ItemIndex	Index of the favorites item to return
 *
 * @return	The favorites item specified by the provided index
 */
FString FMainMRUFavoritesList::GetFavoritesItem( int32 ItemIndex ) const
{
	check( FavoriteItems.IsValidIndex( ItemIndex ) );
	return FavoriteItems[ ItemIndex ];
}

/**
 * Verifies that the favorites item specified by the provided index still exists. If it does not, the item
 * is removed from the favorites list and the user is notified.
 *
 * @param	ItemIndex	Index of the favorites item to check
 *
 * @return	true if the item specified by the index was verified and still exists; false if it does not
 */
bool FMainMRUFavoritesList::VerifyFavoritesFile( int32 ItemIndex )
{
	check( FavoriteItems.IsValidIndex( ItemIndex ) );
	const FString& CurFileName = FavoriteItems[ ItemIndex ];

	const bool bFileExists = IFileManager::Get().FileSize( *CurFileName ) != INDEX_NONE;
	
	// If the file doesn't exist any more, remove it from the favorites list and alert the user
	if ( !bFileExists )
	{
		FNotificationInfo Info(FText::Format(NSLOCTEXT("UnrealEd", "Error_FavoritesFileDoesNotExist", "Map '{0}' does not exist - it will be removed from the Favorites list."), FText::FromString(FPaths::GetCleanFilename(CurFileName))));
		Info.bUseThrobber = false;
		Info.ExpireDuration = 8.0f;
		FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
		RemoveFavoritesItem( CurFileName );
	}

	return bFileExists;
}

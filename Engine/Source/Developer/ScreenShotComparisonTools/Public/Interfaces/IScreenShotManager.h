// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IScreenShotComparisonModule.h: Declares the IScreenShotComparisonModule interface.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "ImageComparer.h"
#include "Interfaces/IScreenShotData.h"

class IScreenShotManager;
struct FScreenShotDataItem;

/**
 * Type definition for shared pointers to instances of IScreenShotManager.
 */
typedef TSharedPtr<class IScreenShotManager> IScreenShotManagerPtr;

/**
 * Type definition for shared references to instances of IScreenShotManager.
 */
typedef TSharedRef<class IScreenShotManager> IScreenShotManagerRef;

struct FScreenShotDataItem;



/**
 * Delegate type for session filter changing.
 */
DECLARE_DELEGATE(FOnScreenFilterChanged)


/**
 * Interface for screen manager module.
 */
class IScreenShotManager
{
public:
	virtual ~IScreenShotManager(){ }

	/**
	 * Generate the screen shot data
	 */
	virtual void GenerateLists( ) = 0;

	/**
	 */
	virtual TArray< TSharedPtr<FScreenShotDataItem> >& GetScreenshotResults() = 0;

	/**
	 * Get the list of active platforms
	 *
	 * @return the array of platforms
	 */
	virtual TArray< TSharedPtr<FString> >& GetCachedPlatfomList( ) = 0;

	/**
	 * Register for screen shot updates
	 *
	 * @param InDelegate - Delegate register
	 */
	virtual void RegisterScreenShotUpdate( const FOnScreenFilterChanged& InDelegate ) = 0;

	/**
	 * Set the filter
	 *
	 * @param InFilter - The screen shot filter
	 */
	virtual void SetFilter(TSharedPtr<ScreenShotFilterCollection> InFilter ) = 0;

	/**
	 * Compares screenshots.
	 */
	virtual TFuture<void> CompareScreensotsAsync() = 0;

	/**
	 * Compares a specific screenshot, the shot path must be relative from the incoming unapproved directory.
	 */
	virtual TFuture<FImageComparisonResult> CompareScreensotAsync(FString RelativeImagePath) = 0;

	/**
	 * Exports the screenshots to the export location specified
	 */
	virtual TFuture<void> ExportScreensotsAsync(FString ExportPath = TEXT("")) = 0;

	/**
	 * Imports screenshot comparison data from a given path.
	 */
	virtual TSharedPtr<FComparisonResults> ImportScreensots(FString ImportPath) = 0;

	virtual FString GetLocalUnapprovedFolder() const = 0;

	virtual FString GetLocalApprovedFolder() const = 0;
};

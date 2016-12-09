// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenShotManager.cpp: Implements the FScreenShotManager class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "ImageComparer.h"
#include "Interfaces/IScreenShotManager.h"
#include "Helpers/MessageEndpoint.h"

class FScreenshotComparisons;
struct FScreenShotDataItem;

/**
 * Implements the ScreenShotManager that contains screen shot data.
 */
class FScreenShotManager : public IScreenShotManager 
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InMessageBus - The message bus to use.
	 */
	FScreenShotManager();

public:

	/**
	* Create some dummy data to test the UI
	*/
	void CreateData();

public:

	//~ Begin IScreenShotManager Interface

	virtual void GenerateLists() override;

	virtual TArray< TSharedPtr<FString> >& GetCachedPlatfomList() override;

	virtual TArray< TSharedPtr<FScreenShotDataItem> >& GetScreenshotResults() override;

	virtual void RegisterScreenShotUpdate(const FOnScreenFilterChanged& InDelegate ) override;

	virtual void SetFilter( TSharedPtr< ScreenShotFilterCollection > InFilter ) override;

	virtual TFuture<void> CompareScreensotsAsync() override;

	virtual TFuture<FImageComparisonResult> CompareScreensotAsync(FString RelativeImagePath) override;

	virtual TFuture<void> ExportScreensotsAsync(FString ExportPath = TEXT("")) override;

	virtual TSharedPtr<FComparisonResults> ImportScreensots(FString ImportPath) override;

	virtual FString GetLocalUnapprovedFolder() const override;

	virtual FString GetLocalApprovedFolder() const override;

	//~ End IScreenShotManager Interface

private:
	FString GetDefaultExportDirectory() const;
	FString GetComparisonResultsJsonFilename() const;

	void CompareScreensots();
	FImageComparisonResult CompareScreensot(FString Existing);
	void SaveComparisonResults(const FComparisonResults& Results) const;
	bool ExportComparisonResults(FString RootExportFolder);
	void CopyDirectory(const FString& DestDir, const FString& SrcDir);

	TSharedRef<FScreenshotComparisons> GenerateFileList() const;

private:
	FString ScreenshotApprovedFolder;
	FString ScreenshotUnapprovedFolder;
	FString ScreenshotDeltaFolder;
	FString ScreenshotResultsFolder;

	// Holds the list of active platforms
	TArray<TSharedPtr<FString> > CachedPlatformList;

	// Holds the array of created screen shot data items
	TArray< TSharedPtr<FScreenShotDataItem> > ScreenShotDataArray;

	// Holds a delegate to be invoked when the screen shot filter has changed.
	FOnScreenFilterChanged ScreenFilterChangedDelegate;
};

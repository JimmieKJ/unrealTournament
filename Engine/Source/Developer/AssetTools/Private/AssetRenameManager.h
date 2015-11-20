// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

struct FAssetRenameDataWithReferencers;

/** 
 * The manager to handle renaming assets.
 * This manager attempts to fix up references in memory if possible and only leaves UObjectRedirectors when needed.
 * Redirectors are left unless ALL of the following are true about the asset
 *    1) The asset has not yet been checked into source control. This does not apply when source control is disabled.
 *    2) The user is able and willing to check out all uasset files that directly reference the asset from source control. The files must be at head revision and not checked out by another user. This rule does not apply when source control is disabled.
 *    3) No maps reference the asset directly.
 *    4) All uasset files that directly reference the asset are writable on disk.
 */
class FAssetRenameManager : public TSharedFromThis<FAssetRenameManager>
{
public:
	/** Renames assets using the specified names. */
	void RenameAssets(const TArray<FAssetRenameData>& AssetsAndNames) const;

	/** Accessor for post rename event */
	FAssetPostRenameEvent& OnAssetPostRenameEvent() { return AssetPostRenameEvent; }

	/**
	 * Function that renames all FStringAssetReference object with the old asset path to the new one.
	 *
	 * @param PackagesToCheck Packages to check for referencing FStringAssetReference.
	 * @param OldAssetPath Old path.
	 * @param NewAssetPath New path.
	 */
	static void RenameReferencingStringAssetReferences(const TArray<UPackage *> PackagesToCheck, const FString& OldAssetPath, const FString& NewAssetPath);

private:
	/** Attempts to load and fix redirector references for the supplied assets */
	void FixReferencesAndRename(TArray<FAssetRenameData> AssetsAndNames) const;

	/** Get a list of assets referenced from CDOs */
	TArray<TWeakObjectPtr<UObject>> FindCDOReferencedAssets(const TArray<FAssetRenameDataWithReferencers>& AssetsToRename) const;

	/** Fills out the Referencing packages for all the assets described in AssetsToPopulate */
	void PopulateAssetReferencers(TArray<FAssetRenameDataWithReferencers>& AssetsToPopulate) const;

	/** Updates the source control status of the packages containing the assets to rename */
	bool UpdatePackageStatus(const TArray<FAssetRenameDataWithReferencers>& AssetsToRename) const;

	/** 
	  * Loads all referencing packages to assets in AssetsToRename, finds assets whose references can
	  * not be fixed up to mark that a redirector should be left, and returns a list of referencing packages to save.
	  */
	void LoadReferencingPackages(TArray<FAssetRenameDataWithReferencers>& AssetsToRename, TArray<UPackage*>& OutReferencingPackagesToSave) const;

	/** 
	  * Prompts to check out the source package and all referencing packages and marks assets whose referencing packages were not checked out to leave a redirector.
	  * Trims PackagesToSave when necessary.
	  * Returns true if the user opted to continue the operation or no dialog was required.
	  */
	bool CheckOutPackages(TArray<FAssetRenameDataWithReferencers>& AssetsToRename, TArray<UPackage*>& InOutReferencingPackagesToSave) const;

	/** Finds any collections that are referencing the assets to be renamed. Assets referenced by collections will leave redirectors */
	void DetectReferencingCollections(TArray<FAssetRenameDataWithReferencers>& AssetsToRename) const;

	/** Finds any read only packages and removes them from the save list. Assets referenced by these packages will leave redirectors. */ 
	void DetectReadOnlyPackages(TArray<FAssetRenameDataWithReferencers>& AssetsToRename, TArray<UPackage*>& InOutReferencingPackagesToSave) const;

	/** Performs the asset rename after the user has selected to proceed */
	void PerformAssetRename(TArray<FAssetRenameDataWithReferencers>& AssetsToRename) const;

	/** Saves all the referencing packages and updates SCC state */
	void SaveReferencingPackages(const TArray<UPackage*>& ReferencingPackagesToSave) const;

	/** Report any failures that may have happened during the rename */
	void ReportFailures(const TArray<FAssetRenameDataWithReferencers>& AssetsToRename) const;

private:

	/** Event issued at the end of the rename process */
	FAssetPostRenameEvent AssetPostRenameEvent;
};
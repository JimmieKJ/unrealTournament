// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** A class to represent a collection of assets */
class FCollection
{
public:
	FCollection();
	FCollection(FName InCollectionName, const FString& SourceFolder, const FString& CollectionExtension, bool InUseSCC);

	/** Loads content from a file into this collection. */
	bool LoadFromFile(const FString& InFilename, bool InUseSCC);
	/** Saves this collection to SourceFilename. If false, OutError is a human readable warning depicting the error. */
	bool Save(FText& OutError);
	/** Deletes the source file for this collection. If false, OutError is a human readable warning depicting the error. */
	bool DeleteSourceFile(FText& OutError);

	/** Adds a single asset to the collection */
	bool AddAssetToCollection(FName ObjectPath);
	/** Removes a single asset from the collection */
	bool RemoveAssetFromCollection(FName ObjectPath);
	/** Gets a list of assets in the collection. Static collections only. */
	void GetAssetsInCollection(TArray<FName>& Assets) const;
	/** Gets a list of classes in the collection. Static collections only. */
	void GetClassesInCollection(TArray<FName>& Classes) const;
	/** Gets a list of objects in the collection. Static collections only. */
	void GetObjectsInCollection(TArray<FName>& Objects) const;
	/** Returns true when the specified asset is in the collection. Static collections only. */
	bool IsAssetInCollection(FName ObjectPath) const;
	/** Resets a collection to its default values */
	void Clear();

	/** Returns true if the collection contains rules instead of a flat list */
	bool IsDynamic() const;

	/** Whether the collection has any contents */
	bool IsEmpty() const;

	/** Logs the contents of the collection */
	void PrintCollection() const;

	/** Returns the name of the collection */
	FORCEINLINE const FName& GetCollectionName() const { return CollectionName; }

private:
	/** Generates the header pairs for the collection file. */
	void GenerateHeaderPairs(TMap<FString,FString>& OutHeaderPairs);

	/** 
	  * Processes header pairs from the top of a collection file.
	  *
	  * @param InHeaderPairs The header pairs found at the start of a collection file
	  * @return true if the header was valid and loaded properly
	  */
	bool LoadHeaderPairs(const TMap<FString,FString>& InHeaderPairs);

	/** Merges the assets from the specified collection with this collection */
	void MergeWithCollection(const FCollection& Other);
	/** Gets the differences between Lists A and B */
	void GetDifferencesFromDisk(TArray<FName>& AssetsAdded, TArray<FName>& AssetsRemoved);
	/** Checks the shared collection out from source control so it may be saved. If false, OutError is a human readable warning depicting the error. */
	bool CheckoutCollection(FText& OutError);
	/** Checks the shared collection in to source control after it is saved. If false, OutError is a human readable warning depicting the error. */
	bool CheckinCollection(FText& OutError);
	/** Reverts the collection in the event that the save was not successful. If false, OutError is a human readable warning depicting the error.*/
	bool RevertCollection(FText& OutError);
	/** Marks the source file for delete in source control. If false, OutError is a human readable warning depicting the error. */
	bool DeleteFromSourceControl(FText& OutError);

private:
	/** The name of the collection */
	FName CollectionName;

	/** Source control is used if true */
	bool bUseSCC;

	/** The filename used to load this collection. Empty if it is new or never loaded from disk. */
	FString SourceFilename;

	/** A list of assets in the collection. Takes the form PackageName.AssetName */
	TArray<FName> AssetList;

	/** A set of assets in the collection. This mirrors AssetList entirely and is just used for fast lookups */
	TSet<FName> AssetSet;

	/** The list of assets that were in the collection last time it was loaded from or saved to disk */
	TArray<FName> DiskAssetList;

	/** The file version for this collection */
	int32 FileVersion;
};
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CollectionManagerTypes.h"

class ICollectionManager
{
public:
	/** Virtual destructor */
	virtual ~ICollectionManager() {}

	/** Returns the list of collection names of the specified share type */
	virtual void GetCollectionNames(ECollectionShareType::Type ShareType, TArray<FName>& CollectionNames) const = 0;

	/** Returns true if the collection exists */
	virtual bool CollectionExists(FName CollectionName, ECollectionShareType::Type ShareType) const = 0;

	/** Returns a list of asset paths found in the specified collection and share type */
	virtual bool GetAssetsInCollection(FName CollectionName, ECollectionShareType::Type ShareType, TArray<FName>& OutAssetPaths) const = 0;

	/** Returns a list of class paths found in the specified collection and share type */
	virtual bool GetClassesInCollection(FName CollectionName, ECollectionShareType::Type ShareType, TArray<FName>& ClassPaths) const = 0;

	/** Returns a list of object paths found in the specified collection and share type */
	virtual bool GetObjectsInCollection(FName CollectionName, ECollectionShareType::Type ShareType, TArray<FName>& ObjectPaths) const = 0;

	/** Returns a list of collections in which the specified asset exists of the specified share type */
	virtual void GetCollectionsContainingAsset(FName ObjectPath, ECollectionShareType::Type ShareType, TArray<FName>& OutCollectionNames) const = 0;

	/** Creates a unique collection name for the given type taking the form BaseName+(unique number) */
	virtual void CreateUniqueCollectionName(const FName& BaseName, ECollectionShareType::Type ShareType, FName& OutCollectionName) const = 0;

	/**
	  * Adds a collection to the asset registry. A .collection file will be added to disk.
	  *
	  * @param CollectionName The name of the new collection
	  * @param ShareType The way the collection is shared.
	  * @return true if the add was successful. If false, GetLastError will return a human readable string description of the error.
	  */
	virtual bool CreateCollection(FName CollectionName, ECollectionShareType::Type ShareType) = 0;

	/**
	  * Renames a collection to the asset registry. A .collection file will be added to disk and a .collection file will be removed.
	  *
	  * @param CurrentCollectionName The current name of the collection.
	  * @param CurrentShareType The current way the collection is shared.
	  * @param NewCollectionName The new name of the collection.
	  * @param NewShareType The new way the collection is shared.
	  * @return true if the add was successful. If false, GetLastError will return a human readable string description of the error.
	  */
	virtual bool RenameCollection(FName CurrentCollectionName, ECollectionShareType::Type CurrentShareType, FName NewCollectionName, ECollectionShareType::Type NewShareType) = 0;

	/**
	  * Removes a collection to the asset registry. A .collection file will be deleted from disk.
	  *
	  * @param CollectionName The name of the collection to remove.
	  * @param ShareType The way the collection is shared.
	  * @return true if the remove was successful
	  */
	virtual bool DestroyCollection(FName CollectionName, ECollectionShareType::Type ShareType) = 0;

	/**
	  * Adds an asset to the specified collection.
	  *
	  * @param CollectionName The collection in which to add the asset
	  * @param ShareType The way the collection is shared.
	  * @param ObjectPath the ObjectPath of the asset to add.
	  * @param OutNumAdded if non-NULL, the number of objects successfully added to the collection
	  * @return true if the add was successful. If false, GetLastError will return a human readable string description of the error.
	  */
	virtual bool AddToCollection(FName CollectionName, ECollectionShareType::Type ShareType, FName ObjectPath) = 0;
	virtual bool AddToCollection(FName CollectionName, ECollectionShareType::Type ShareType, const TArray<FName>& ObjectPaths, int32* OutNumAdded = NULL) = 0;

	/**
	  * Removes the asset from the specified collection.
	  *
	  * @param CollectionName The collection from which to remove the asset
	  * @param ShareType The way the collection is shared.
	  * @param ObjectPath the ObjectPath of the asset to remove.
	  * @param OutNumRemoved if non-NULL, the number of objects successfully removed from the collection
	  * @return true if the remove was successful. If false, GetLastError will return a human readable string description of the error.
	  */
	virtual bool RemoveFromCollection(FName CollectionName, ECollectionShareType::Type ShareType, FName ObjectPath) = 0;
	virtual bool RemoveFromCollection(FName CollectionName, ECollectionShareType::Type ShareType, const TArray<FName>& ObjectPaths, int32* OutNumRemoved = NULL) = 0;

	/**
	  * Removes all assets from the specified collection.
	  *
	  * @param CollectionName The collection from which to remove the asset
	  * @param ShareType The way the collection is shared.
	  * @return true if the remove was successful. If false, GetLastError will return a human readable string description of the error.
	  */
	virtual bool EmptyCollection(FName CollectionName, ECollectionShareType::Type ShareType) = 0;

	/**
	  * @param CollectionName The collection from which to remove the asset
	  * @param ShareType The way the collection is shared.
	  * @return true if the collection is empty.
	  */
	virtual bool IsCollectionEmpty(FName CollectionName, ECollectionShareType::Type ShareType) = 0;

	/** Returns the most recent error. */
	virtual FText GetLastError() const = 0;

	/** Event for when collections are created */
	DECLARE_EVENT_OneParam( ICollectionManager, FCollectionCreatedEvent, const FCollectionNameType& );
	virtual FCollectionCreatedEvent& OnCollectionCreated() = 0;

	/** Event for when collections are destroyed */
	DECLARE_EVENT_OneParam( ICollectionManager, FCollectionDestroyedEvent, const FCollectionNameType& );
	virtual FCollectionDestroyedEvent& OnCollectionDestroyed() = 0;

	/** Event for when assets are added to a collection */
	DECLARE_EVENT_TwoParams( ICollectionManager, FAssetsAddedEvent, const FCollectionNameType&, const TArray<FName>& );
	virtual FAssetsAddedEvent& OnAssetsAdded() = 0;

	/** Event for when assets are removed from a collection */
	DECLARE_EVENT_TwoParams( ICollectionManager, FAssetsRemovedEvent, const FCollectionNameType&, const TArray<FName>& );
	virtual FAssetsRemovedEvent& OnAssetsRemoved() = 0;

	/** Event for when collections are renamed */
	DECLARE_EVENT_TwoParams( ICollectionManager, FCollectionRenamedEvent, const FCollectionNameType&, const FCollectionNameType& );
	virtual FCollectionRenamedEvent& OnCollectionRenamed() = 0;
};
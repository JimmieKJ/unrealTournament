// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CollectionManagerPrivatePCH.h"

#define LOCTEXT_NAMESPACE "CollectionManager"

FCollectionManager::FCollectionManager()
{
	NumCollections = 0;
	LastError = LOCTEXT("Error_Unknown", "None");

	CollectionFolders[ECollectionShareType::CST_Local] = FPaths::GameSavedDir() / TEXT("Collections");
	CollectionFolders[ECollectionShareType::CST_Private] = FPaths::GameUserDeveloperDir() / TEXT("Collections");
	CollectionFolders[ECollectionShareType::CST_Shared] = FPaths::GameContentDir() / TEXT("Collections");

	CollectionExtension = TEXT("collection");

	LoadCollections();
}

FCollectionManager::~FCollectionManager()
{
	// Delete all collections in the cache
	for (int32 CacheIdx = 0; CacheIdx < ECollectionShareType::CST_All; ++CacheIdx)
	{
		for (TMap<FName, FCollection*>::TConstIterator CollectionIt(CachedCollections[CacheIdx]); CollectionIt; ++CollectionIt)
		{
			if ( CollectionIt.Value() )
			{
				delete CollectionIt.Value();
				NumCollections--;
			}
		}

		CachedCollections[CacheIdx].Empty();
	}

	// Make sure we have deleted all our allocated FCollection objects
	ensure(NumCollections == 0);
}

void FCollectionManager::GetCollectionNames(ECollectionShareType::Type ShareType, TArray<FName>& CollectionNames) const
{
	if ( ShareType == ECollectionShareType::CST_All )
	{
		// Asked for all share types, get names for each cache
		for (int32 CacheIdx = 0; CacheIdx < ECollectionShareType::CST_All; ++CacheIdx)
		{
			TArray<FName> Names;
			GetCollectionNames(ECollectionShareType::Type(CacheIdx), Names);
			CollectionNames.Append(Names);
		}
	}
	else
	{
		CachedCollections[ShareType].GenerateKeyArray(CollectionNames);
	}
}

bool FCollectionManager::CollectionExists(FName CollectionName, ECollectionShareType::Type ShareType) const
{
	if ( ShareType == ECollectionShareType::CST_All )
	{
		// Asked for all share types, get names for each cache
		for (int32 CacheIdx = 0; CacheIdx < ECollectionShareType::CST_All; ++CacheIdx)
		{
			if ( CollectionExists(CollectionName, ECollectionShareType::Type(CacheIdx)) )
			{
				// Collection exists in at least one cache
				return true;
			}
		}

		// Collection not found in any cache
		return false;
	}
	else
	{
		return CachedCollections[ShareType].Contains(CollectionName);
	}
}

bool FCollectionManager::GetAssetsInCollection(FName CollectionName, ECollectionShareType::Type ShareType, TArray<FName>& AssetsPaths) const
{
	if ( ShareType == ECollectionShareType::CST_All )
	{
		// Asked for all share types, find assets in the specified collection name in any cache
		bool bFoundAssets = false;

		for (int32 CacheIdx = 0; CacheIdx < ECollectionShareType::CST_All; ++CacheIdx)
		{
			if ( GetAssetsInCollection(CollectionName, ECollectionShareType::Type(CacheIdx), AssetsPaths) )
			{
				bFoundAssets = true;
			}
		}

		return bFoundAssets;
	}
	else
	{
		FCollection*const* CollectionPtr = CachedCollections[ShareType].Find(CollectionName);
		FCollection* Collection = NULL;
	
		if (CollectionPtr)
		{
			Collection = *CollectionPtr;
			ensure(Collection);
		}

		if ( Collection )
		{
			Collection->GetAssetsInCollection(AssetsPaths);
			return true;
		}

		return false;
	}
}

bool FCollectionManager::GetClassesInCollection(FName CollectionName, ECollectionShareType::Type ShareType, TArray<FName>& ClassPaths) const
{
	if ( ShareType == ECollectionShareType::CST_All )
	{
		// Asked for all share types, find assets in the specified collection name in any cache
		bool bFoundAssets = false;

		for (int32 CacheIdx = 0; CacheIdx < ECollectionShareType::CST_All; ++CacheIdx)
		{
			if ( GetClassesInCollection(CollectionName, ECollectionShareType::Type(CacheIdx), ClassPaths) )
			{
				bFoundAssets = true;
			}
		}

		return bFoundAssets;
	}
	else
	{
		FCollection*const* CollectionPtr = CachedCollections[ShareType].Find(CollectionName);
		FCollection* Collection = NULL;

		if (CollectionPtr)
		{
			Collection = *CollectionPtr;
			ensure(Collection);
		}

		if ( Collection )
		{
			Collection->GetClassesInCollection(ClassPaths);
			return true;
		}

		return false;
	}
}

bool FCollectionManager::GetObjectsInCollection(FName CollectionName, ECollectionShareType::Type ShareType, TArray<FName>& ObjectPaths) const
{
	if ( ShareType == ECollectionShareType::CST_All )
	{
		// Asked for all share types, find assets in the specified collection name in any cache
		bool bFoundAssets = false;

		for (int32 CacheIdx = 0; CacheIdx < ECollectionShareType::CST_All; ++CacheIdx)
		{
			if ( GetObjectsInCollection(CollectionName, ECollectionShareType::Type(CacheIdx), ObjectPaths) )
			{
				bFoundAssets = true;
			}
		}

		return bFoundAssets;
	}
	else
	{
		FCollection*const* CollectionPtr = CachedCollections[ShareType].Find(CollectionName);
		FCollection* Collection = NULL;

		if (CollectionPtr)
		{
			Collection = *CollectionPtr;
			ensure(Collection);
		}

		if ( Collection )
		{
			Collection->GetObjectsInCollection(ObjectPaths);
			return true;
		}

		return false;
	}
}

void FCollectionManager::GetCollectionsContainingAsset(FName ObjectPath, ECollectionShareType::Type ShareType, TArray<FName>& OutCollectionNames) const
{
	if ( ShareType == ECollectionShareType::CST_All )
	{
		// Asked for all share types, find collections containing the specified asset in any cache
		for (int32 CacheIdx = 0; CacheIdx < ECollectionShareType::CST_All; ++CacheIdx)
		{
			GetCollectionsContainingAsset(ObjectPath, ECollectionShareType::Type(CacheIdx), OutCollectionNames);
		}
	}
	else
	{
		// Ask each collection if it contains the asset
		for (TMap<FName,FCollection*>::TConstIterator CollectionIt(CachedCollections[ShareType]); CollectionIt; ++CollectionIt)
		{
			const FCollection* Collection = CollectionIt.Value();
			if ( ensure(Collection) && Collection->IsAssetInCollection(ObjectPath) )
			{
				OutCollectionNames.Add(CollectionIt.Key());
			}
		}
	}
}

void FCollectionManager::CreateUniqueCollectionName(const FName& BaseName, ECollectionShareType::Type ShareType, FName& OutCollectionName) const
{
	if(!ensure(ShareType != ECollectionShareType::CST_All))
	{
		return;
	}

	int32 IntSuffix = 1;
	bool CollectionAlreadyExists = false;
	do
	{
		if(IntSuffix <= 1)
		{
			OutCollectionName = BaseName;
		}
		else
		{
			OutCollectionName = *FString::Printf(TEXT("%s%d"), *BaseName.ToString(), IntSuffix);
		}

		CollectionAlreadyExists = CachedCollections[ShareType].Contains(OutCollectionName);
		++IntSuffix;
	}
	while(CollectionAlreadyExists);
}

bool FCollectionManager::CreateCollection(FName CollectionName, ECollectionShareType::Type ShareType)
{
	if ( !ensure(ShareType < ECollectionShareType::CST_All) )
	{
		// Bad share type
		LastError = LOCTEXT("Error_Internal", "There was an internal error.");
		return false;
	}

	// Try to add the collection
	bool bUseSCC = ShouldUseSCC(ShareType);
	FString SourceFolder = CollectionFolders[ShareType];

	FCollection NewCollection(CollectionName, SourceFolder, CollectionExtension, bUseSCC);
	FCollection* Collection = AddCollection(NewCollection, ShareType);

	if ( !Collection )
	{
		// Failed to add the collection, it already exists
		LastError = LOCTEXT("Error_AlreadyExists", "The collection already exists.");
		return false;
	}

	if ( Collection->Save(LastError) )
	{
		// Collection saved!
		CollectionCreatedEvent.Broadcast( FCollectionNameType( CollectionName, ShareType ) );
		return true;
	}
	else
	{
		// Collection failed to save, remove it from the cache
		RemoveCollection(Collection, ShareType);
		return false;
	}
}

bool FCollectionManager::RenameCollection (FName CurrentCollectionName, ECollectionShareType::Type CurrentShareType, FName NewCollectionName, ECollectionShareType::Type NewShareType)
{
	if ( !ensure(CurrentShareType < ECollectionShareType::CST_All) || !ensure(NewShareType < ECollectionShareType::CST_All) )
	{
		// Bad share type
		LastError = LOCTEXT("Error_Internal", "There was an internal error.");
		return false;
	}

	// Get the object paths for the assets in the collection
	TArray<FName> ObjectPaths;
	if ( !GetAssetsInCollection(CurrentCollectionName, CurrentShareType, ObjectPaths) )
	{
		// Failed to get assets in the current collection
		return false;
	}

	// Create a new collection
	if ( !CreateCollection(NewCollectionName, NewShareType) )
	{
		// Failed to create collection
		return false;
	}

	if ( ObjectPaths.Num() > 0 )
	{
		// Add all the objects from the old collection to the new collection
		if ( !AddToCollection(NewCollectionName, NewShareType, ObjectPaths) )
		{
			// Failed to add paths to the new collection. Destroy the collection we created.
			DestroyCollection(NewCollectionName, NewShareType);
			return false;
		}
	}

	// Delete the old collection
	if ( !DestroyCollection(CurrentCollectionName, CurrentShareType) )
	{
		// Failed to destroy the old collection. Destroy the collection we created.
		DestroyCollection(NewCollectionName, NewShareType);
		return false;
	}

	// Success
	FCollectionNameType OriginalCollectionNameType( CurrentCollectionName, CurrentShareType );
	FCollectionNameType NewCollectionNameType( NewCollectionName, NewShareType );
	CollectionRenamedEvent.Broadcast( OriginalCollectionNameType, NewCollectionNameType );
	return true;
}

bool FCollectionManager::DestroyCollection(FName CollectionName, ECollectionShareType::Type ShareType)
{
	if ( !ensure(ShareType < ECollectionShareType::CST_All) )
	{
		// Bad share type
		LastError = LOCTEXT("Error_Internal", "There was an internal error.");
		return false;
	}

	FCollection** CollectionPtr = CachedCollections[ShareType].Find(CollectionName);
	if (CollectionPtr != NULL && ensure(*CollectionPtr != NULL) )
	{
		FCollection* Collection = *CollectionPtr;
		
		if ( Collection->DeleteSourceFile(LastError) )
		{
			RemoveCollection(Collection, ShareType);
			CollectionDestroyedEvent.Broadcast( FCollectionNameType( CollectionName, ShareType ) );
			return true;
		}
		else
		{
			// Failed to delete the source file
			return false;
		}
	}
	else
	{
		// The collection doesn't exist
		LastError = LOCTEXT("Error_DoesntExist", "The collection doesn't exist.");
		return false;
	}
}

bool FCollectionManager::AddToCollection(FName CollectionName, ECollectionShareType::Type ShareType,  FName ObjectPath)
{
	TArray<FName> Paths;
	Paths.Add(ObjectPath);
	return AddToCollection(CollectionName, ShareType, Paths);
}

bool FCollectionManager::AddToCollection(FName CollectionName, ECollectionShareType::Type ShareType, const TArray<FName>& ObjectPaths, int32* OutNumAdded)
{
	if ( OutNumAdded )
	{
		*OutNumAdded = 0;
	}

	if ( !ensure(ShareType < ECollectionShareType::CST_All) )
	{
		// Bad share type
		LastError = LOCTEXT("Error_Internal", "There was an internal error.");
		return false;
	}

	FCollection** CollectionPtr = CachedCollections[ShareType].Find(CollectionName);
	FCollection* Collection = NULL;
	if (CollectionPtr != NULL)
	{
		Collection = *CollectionPtr;
	}
	else
	{
		// Collection doesn't exist
		LastError = LOCTEXT("Error_DoesntExist", "The collection doesn't exist.");
		return false;
	}

	if ( ensure(Collection) )
	{
		int32 NumAdded = 0;
		for (int32 ObjectIdx = 0; ObjectIdx < ObjectPaths.Num(); ++ObjectIdx)
		{
			if ( Collection->AddAssetToCollection(ObjectPaths[ObjectIdx]) )
			{
				NumAdded++;
			}
		}

		if ( NumAdded > 0 )
		{
			if ( Collection->Save(LastError) )
			{
				// Added and saved
				if ( OutNumAdded )
				{
					*OutNumAdded = NumAdded;
				}

				FCollectionNameType CollectionNameType( Collection->GetCollectionName(), ShareType );
				AssetsAddedEvent.Broadcast( CollectionNameType, ObjectPaths );
				return true;
			}
			else
			{
				// Added but not saved, revert the add
				for (int32 ObjectIdx = 0; ObjectIdx < ObjectPaths.Num(); ++ObjectIdx)
				{
					Collection->RemoveAssetFromCollection(ObjectPaths[ObjectIdx]);
				}
				return false;
			}
		}
		else
		{
			// Failed to add, all of the objects were already in the collection
			LastError = LOCTEXT("Error_AlreadyInCollection", "All of the assets were already in the collection.");
			return false;
		}
	}
	else
	{
		// Collection was never found. Perhaps it was null in the TMap
		LastError = LOCTEXT("Error_DoesntExist", "The collection doesn't exist.");
		return false;
	}
}

bool FCollectionManager::RemoveFromCollection(FName CollectionName, ECollectionShareType::Type ShareType, FName ObjectPath)
{
	TArray<FName> Paths;
	Paths.Add(ObjectPath);
	return RemoveFromCollection(CollectionName, ShareType, Paths);
}

bool FCollectionManager::RemoveFromCollection(FName CollectionName, ECollectionShareType::Type ShareType, const TArray<FName>& ObjectPaths, int32* OutNumRemoved)
{
	if ( OutNumRemoved )
	{
		*OutNumRemoved = 0;
	}

	if ( !ensure(ShareType < ECollectionShareType::CST_All) )
	{
		// Bad share type
		LastError = LOCTEXT("Error_Internal", "There was an internal error.");
		return false;
	}

	FCollection** CollectionPtr = CachedCollections[ShareType].Find(CollectionName);
	if (CollectionPtr != NULL)
	{
		FCollection* Collection = *CollectionPtr;
		
		TArray<FName> RemovedAssets;
		for (int32 ObjectIdx = 0; ObjectIdx < ObjectPaths.Num(); ++ObjectIdx)
		{
			const FName& ObjectPath = ObjectPaths[ObjectIdx];

			if ( Collection->RemoveAssetFromCollection(ObjectPath) )
			{
				RemovedAssets.Add(ObjectPath);
			}
		}

		if ( RemovedAssets.Num() == 0 )
		{
			// Failed to remove, none of the objects were in the collection
			LastError = LOCTEXT("Error_NotInCollection", "None of the assets were in the collection.");
			return false;
		}
			
		if ( Collection->Save(LastError) )
		{
			// Removed and saved
			if ( OutNumRemoved )
			{
				*OutNumRemoved = RemovedAssets.Num();
			}

			FCollectionNameType CollectionNameType( Collection->GetCollectionName(), ShareType );
			AssetsRemovedEvent.Broadcast( CollectionNameType, ObjectPaths );
			return true;
		}
		else
		{
			// Removed but not saved, revert the remove
			for (int32 AssetIdx = 0; AssetIdx < RemovedAssets.Num(); ++AssetIdx)
			{
				Collection->AddAssetToCollection(RemovedAssets[AssetIdx]);
			}
			return false;
		}
	}
	else
	{
		// Collection not found
		LastError = LOCTEXT("Error_DoesntExist", "The collection doesn't exist.");
		return false;
	}
}

bool FCollectionManager::EmptyCollection(FName CollectionName, ECollectionShareType::Type ShareType)
{
	if ( !ensure(ShareType < ECollectionShareType::CST_All) )
	{
		// Bad share type
		LastError = LOCTEXT("Error_Internal", "There was an internal error.");
		return false;
	}

	TArray<FName> ObjectPaths;
	if ( !GetAssetsInCollection(CollectionName, ShareType, ObjectPaths) )
	{
		// Failed to load collection
		return false;
	}
	
	if ( ObjectPaths.Num() == 0 )
	{
		// Collection already empty
		return true;
	}
	
	return RemoveFromCollection(CollectionName, ShareType, ObjectPaths);
}

bool FCollectionManager::IsCollectionEmpty(FName CollectionName, ECollectionShareType::Type ShareType)
{
	if ( !ensure(ShareType < ECollectionShareType::CST_All) )
	{
		// Bad share type
		LastError = LOCTEXT("Error_Internal", "There was an internal error.");
		return true;
	}

	FCollection*const* CollectionPtr = CachedCollections[ShareType].Find(CollectionName);
	FCollection* Collection = NULL;

	if ( CollectionPtr )
	{
		Collection = *CollectionPtr;
		ensure(Collection);
	}

	if ( Collection )
	{
		return Collection->IsEmpty();
	}

	return true;
}

void FCollectionManager::LoadCollections()
{
	const double LoadStartTime = FPlatformTime::Seconds();
	int32 NumCollectionsLoaded = 0;

	for (int32 CacheIdx = 0; CacheIdx < ECollectionShareType::CST_All; ++CacheIdx)
	{
		const FString& CollectionFolder = CollectionFolders[CacheIdx];
		const FString WildCard = FString::Printf(TEXT("%s/*.%s"), *CollectionFolder, *CollectionExtension);

		TArray<FString> Filenames;
		IFileManager::Get().FindFiles(Filenames, *WildCard, true, false);

		for (int32 FileIdx = 0; FileIdx < Filenames.Num(); ++FileIdx)
		{
			const FString Filename = CollectionFolder / Filenames[FileIdx];

			FCollection Collection;
			const bool bUseSCC = ShouldUseSCC(ECollectionShareType::Type(CacheIdx));
			if ( Collection.LoadFromFile(Filename, bUseSCC) )
			{
				AddCollection(Collection, ECollectionShareType::Type(CacheIdx));
			}
			else
			{
				UE_LOG(LogCollectionManager, Warning, TEXT("Failed to load collection file %s"), *Filename);
			}
		}
		
		NumCollectionsLoaded += CachedCollections[CacheIdx].Num();
	}

	UE_LOG( LogCollectionManager, Log, TEXT( "Loaded %d collections in %0.6f seconds" ), NumCollectionsLoaded, FPlatformTime::Seconds() - LoadStartTime );
}

bool FCollectionManager::ShouldUseSCC(ECollectionShareType::Type ShareType) const
{
	return ShareType != ECollectionShareType::CST_Local && ShareType != ECollectionShareType::CST_System;
}

FCollection* FCollectionManager::AddCollection(const FCollection& Collection, ECollectionShareType::Type ShareType)
{
	if ( !ensure(ShareType < ECollectionShareType::CST_All) )
	{
		// Bad share type
		return NULL;
	}

	FName CollectionName = Collection.GetCollectionName();
	if ( CachedCollections[ShareType].Contains(CollectionName) )
	{
		UE_LOG(LogCollectionManager, Warning, TEXT("Failed to add collection '%s' because it already exists."), *CollectionName.ToString());
		return NULL;
	}

	FCollection* NewCollection = new FCollection(Collection);
	NumCollections++;

	CachedCollections[ShareType].Add(CollectionName, NewCollection);

	return NewCollection;
}

bool FCollectionManager::RemoveCollection(FCollection* Collection, ECollectionShareType::Type ShareType)
{
	if ( !ensure(ShareType < ECollectionShareType::CST_All) )
	{
		// Bad share type
		return false;
	}

	bool bRemoved = false;

	if ( ensure(Collection && Collection->GetCollectionName() != NAME_None) )
	{
		bRemoved = CachedCollections[ShareType].Remove(Collection->GetCollectionName()) > 0;

		if ( bRemoved )
		{
			delete Collection;
			NumCollections--;
		}
	}

	return bRemoved;
}

#undef LOCTEXT_NAMESPACE

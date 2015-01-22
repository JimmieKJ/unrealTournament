// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

struct FSourcesData
{
	TArray<FName> PackagePaths;
	TArray<FCollectionNameType> Collections;

	bool IsEmpty() const
	{
		return PackagePaths.Num() == 0 && Collections.Num() == 0;
	}

	FARFilter MakeFilter(bool bRecurse, bool bUsingFolders) const
	{
		FARFilter Filter;

		// Package Paths
		Filter.PackagePaths = PackagePaths;
		Filter.bRecursivePaths = bRecurse || !bUsingFolders;

		// Collections
		TArray<FName> ObjectPathsFromCollections;
		if ( Collections.Num() )
		{
			// Collection manager module should already be loaded since it may cause a hitch on the first search
			FCollectionManagerModule& CollectionManagerModule = FModuleManager::GetModuleChecked<FCollectionManagerModule>(TEXT("CollectionManager"));

			for ( int32 CollectionIdx = 0; CollectionIdx < Collections.Num(); ++CollectionIdx )
			{
				// Find the collection
				const FCollectionNameType& CollectionNameType = Collections[CollectionIdx];

				CollectionManagerModule.Get().GetAssetsInCollection(CollectionNameType.Name, CollectionNameType.Type, ObjectPathsFromCollections);
				CollectionManagerModule.Get().GetClassesInCollection(CollectionNameType.Name, CollectionNameType.Type, ObjectPathsFromCollections);
			}
		}
		Filter.ObjectPaths = ObjectPathsFromCollections;

		return Filter;
	}
};
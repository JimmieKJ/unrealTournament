// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** A list item representing a collection */
struct FCollectionItem
{
	struct FCompareFCollectionItemByName
	{
		FORCEINLINE bool operator()( const TSharedPtr<FCollectionItem>& A, const TSharedPtr<FCollectionItem>& B ) const
		{
			return A->CollectionName < B->CollectionName;
		}
	};

	/** The name of the collection */
	FString CollectionName;

	/** The type of the collection */
	ECollectionShareType::Type CollectionType;

	/** If true, will set up an inline rename after next ScrollIntoView */
	bool bRenaming;

	/** If true, this item will be created the next time it is renamed */
	bool bNewCollection;

	/** Broadcasts whenever renaming a collection item is requested */
	DECLARE_MULTICAST_DELEGATE( FRenamedRequestEvent )

	/** Broadcasts whenever a rename is requested */
	FRenamedRequestEvent OnRenamedRequestEvent;

	/** Constructor */
	FCollectionItem(const FString& InCollectionName, const ECollectionShareType::Type& InCollectionType)
		: CollectionName(InCollectionName)
		, CollectionType(InCollectionType)
		, bRenaming(false)
		, bNewCollection(false)
	{}
};

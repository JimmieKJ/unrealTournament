// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** The struct representing an item in the asset tree */
struct FTreeItem : public TSharedFromThis<FTreeItem>
{
	struct FCompareFTreeItemByName
	{
		FORCEINLINE bool operator()( TSharedPtr<FTreeItem> A, TSharedPtr<FTreeItem> B ) const
		{
			return A->DisplayName.ToString() < B->DisplayName.ToString();
		}
	};

	/** The display name of the tree item (typically the same as FolderName, but may be localized for known folder types) */
	FText DisplayName;
	/** The leaf-name of the tree item folder */
	FString FolderName;
	/** The path of the tree item including the name */
	FString FolderPath;
	/** If true, this folder is in the process of being named */
	bool bNamingFolder;

	/** The children of this tree item */
	TArray< TSharedPtr<FTreeItem> > Children;

	/** The parent folder for this item */
	TWeakPtr<FTreeItem> Parent;

	/** Broadcasts whenever renaming a tree item is requested */
	DECLARE_MULTICAST_DELEGATE( FRenamedRequestEvent )

	/** Broadcasts whenever a rename is requested */
	FRenamedRequestEvent OnRenamedRequestEvent;

	/** Constructor */
	FTreeItem(FText InDisplayName, FString InFolderName, FString InFolderPath, TSharedPtr<FTreeItem> InParent, bool InNamingFolder = false)
		: DisplayName(MoveTemp(InDisplayName))
		, FolderName(MoveTemp(InFolderName))
		, FolderPath(MoveTemp(InFolderPath))
		, bNamingFolder(InNamingFolder)
		, Parent(MoveTemp(InParent))
	{}

	/** Returns true if this item is a child of the specified item */
	bool IsChildOf(const FTreeItem& InParent)
	{
		auto CurrentParent = Parent.Pin();
		while (CurrentParent.IsValid())
		{
			if (CurrentParent.Get() == &InParent)
			{
				return true;
			}

			CurrentParent = CurrentParent->Parent.Pin();
		}

		return false;
	}

	/** Returns the child item by name or NULL if the child does not exist */
	TSharedPtr<FTreeItem> GetChild (const FString& InChildFolderName)
	{
		for ( int32 ChildIdx = 0; ChildIdx < Children.Num(); ++ChildIdx )
		{
			if ( Children[ChildIdx]->FolderName == InChildFolderName )
			{
				return Children[ChildIdx];
			}
		}

		return TSharedPtr<FTreeItem>();
	}

	/** Finds the child who's path matches the one specified */
	TSharedPtr<FTreeItem> FindItemRecursive (const FString& InFullPath)
	{
		if ( InFullPath == FolderPath )
		{
			return SharedThis(this);
		}

		for ( int32 ChildIdx = 0; ChildIdx < Children.Num(); ++ChildIdx )
		{
			const TSharedPtr<FTreeItem>& Item = Children[ChildIdx]->FindItemRecursive(InFullPath);
			if ( Item.IsValid() )
			{
				return Item;
			}
		}

		return TSharedPtr<FTreeItem>(NULL);
	}

	/** Sort the children by name */
	void SortChildren ()
	{
		Children.Sort( FCompareFTreeItemByName() );
	}
};

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CollectionManagerModule.h"

struct FCollectionItem;
class SCollectionListItem;

/**
 * The list view of collections.
 */
class SCollectionView : public SCompoundWidget
{
	friend class FCollectionContextMenu;
public:
	DECLARE_DELEGATE_OneParam( FOnCollectionSelected, const FCollectionNameType& /*SelectedCollection*/);

	SLATE_BEGIN_ARGS( SCollectionView )
		: _AllowCollectionButtons(true)
		, _AllowRightClickMenu(true)
		, _AllowCollapsing(true)
		, _AllowContextMenu(true)
		{}

		/** Called when a collection was selected */
		SLATE_EVENT( FOnCollectionSelected, OnCollectionSelected )

		/** If true, collection buttons will be displayed */
		SLATE_ARGUMENT( bool, AllowCollectionButtons )
		SLATE_ARGUMENT( bool, AllowRightClickMenu )
		SLATE_ARGUMENT( bool, AllowCollapsing )
		SLATE_ARGUMENT( bool, AllowContextMenu )

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs );

	/** Selects the specified collections */
	void SetSelectedCollections(const TArray<FCollectionNameType>& CollectionsToSelect);

	/** Clears selection of all collections */
	void ClearSelection();

	/** Gets all the currently selected collections */
	TArray<FCollectionNameType> GetSelectedCollections() const;

	/** Sets the state of the collection view to the one described by the history data */
	void ApplyHistoryData ( const FHistoryData& History );

	/** Saves any settings to config that should be persistent between editor sessions */
	void SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const;

	/** Loads any settings to config that should be persistent between editor sessions */
	void LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString);

	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;

private:

	/** True if the selection changed delegate is allowed at the moment */
	bool ShouldAllowSelectionChangedDelegate() const;

	/** Creates the menu for the add collection button */
	FReply MakeAddCollectionMenu();

	/** Gets the visibility of the add collection button */
	EVisibility GetAddCollectionButtonVisibility() const;
	
	/** Sets up an inline creation process for a new collection of the specified type */
	void CreateCollectionItem( ECollectionShareType::Type CollectionType );

	/** Sets up an inline rename for the specified collection */
	void RenameCollectionItem( const TSharedPtr<FCollectionItem>& ItemToRename );

	/** Remove a collection item from the list */
	void RemoveCollectionItems( const TArray<TSharedPtr<FCollectionItem>>& ItemsToRemove );

	/** Returns the visibility of the collection list */
	EVisibility GetCollectionListVisibility() const;

	/** Creates a list item for the collection list */
	TSharedRef<ITableRow> GenerateCollectionRow( TSharedPtr<FCollectionItem> CollectionItem, const TSharedRef<STableViewBase>& OwnerTable );

	/** Makes the context menu for the collection list */
	TSharedPtr<SWidget> MakeCollectionListContextMenu();

	/** Handler for collection list selection changes */
	void CollectionSelectionChanged( TSharedPtr< FCollectionItem > CollectionItem, ESelectInfo::Type SelectInfo );

	/** Handler for the user dropping assets on a collection */
	void CollectionAssetsDropped(const TArray<FAssetData>& AssetList, const TSharedPtr<FCollectionItem>& CollectionItem, FText& OutMessage);

	/** Handles focusing a collection item widget after it has been created with the intent to rename */
	void CollectionItemScrolledIntoView( TSharedPtr<FCollectionItem> CollectionItem, const TSharedPtr<ITableRow>& Widget );

	/** Checks whether the selected collection is not allowed to be renamed */
	bool IsCollectionNotRenamable() const;

	/** Handler for when a name was given to a collection. Returns false if the rename or create failed and sets OutWarningMessage depicting what happened. */
	bool CollectionNameChangeCommit( const TSharedPtr< FCollectionItem >& CollectionItem, const FString& NewName, bool bChangeConfirmed, FText& OutWarningMessage );

	/** Checks if the collection name being committed is valid */
	bool CollectionVerifyRenameCommit(const TSharedPtr< FCollectionItem >& CollectionItem, const FString& NewName, const FSlateRect& MessageAnchor, FText& OutErrorMessage);

	/** Handles an on collection created event */
	void HandleCollectionCreated( const FCollectionNameType& Collection );

	/** Handles an on collection renamed event */
	void HandleCollectionRenamed( const FCollectionNameType& OriginalCollection, const FCollectionNameType& NewCollection );

	/** Handles an on collection destroyed event */
	void HandleCollectionDestroyed( const FCollectionNameType& Collection );

	/** Updates the collections shown in the list view */
	void UpdateCollectionItems();
private:

	/** A helper class to manage PreventSelectionChangedDelegateCount by incrementing it when constructed (on the stack) and decrementing when destroyed */
	class FScopedPreventSelectionChangedDelegate
	{
	public:
		FScopedPreventSelectionChangedDelegate(const TSharedRef<SCollectionView>& InCollectionView)
			: CollectionView(InCollectionView)
		{
			CollectionView->PreventSelectionChangedDelegateCount++;
		}

		~FScopedPreventSelectionChangedDelegate()
		{
			check(CollectionView->PreventSelectionChangedDelegateCount > 0);
			CollectionView->PreventSelectionChangedDelegateCount--;
		}

	private:
		TSharedRef<SCollectionView> CollectionView;
	};

	/** The collection list widget */
	TSharedPtr< SListView< TSharedPtr<FCollectionItem>> > CollectionListPtr;

	/** The list of collections */
	TArray< TSharedPtr<FCollectionItem> > CollectionItems;

	/** The context menu logic and data */
	TSharedPtr<class FCollectionContextMenu> CollectionContextMenu;

	/** The collections SExpandableArea */
	TSharedPtr< SExpandableArea > CollectionsExpandableAreaPtr;

	/** Delegate to invoke when selection changes. */
	FOnCollectionSelected OnCollectionSelected;

	/** If true, collection buttons (such as add) are allowed */
	bool bAllowCollectionButtons;

	/** If true, the user will be able to access the right click menu of a collection */
	bool bAllowRightClickMenu;

	/** If > 0, the selection changed delegate will not be called. Used to update the tree from an external source or in certain bulk operations. */
	int32 PreventSelectionChangedDelegateCount;

	/** Commands handled by this widget */
	TSharedPtr< FUICommandList > Commands;
};
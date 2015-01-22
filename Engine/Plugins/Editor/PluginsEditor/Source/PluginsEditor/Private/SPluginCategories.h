// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PluginCategoryTreeItem.h"

typedef STreeView< FPluginCategoryTreeItemPtr > SPluginCategoryTreeView;


/**
 * Tree view that displays all of the plugin categories and allows the user to switch views
 */
class SPluginCategories : public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS( SPluginCategories )
	{
	}

	SLATE_END_ARGS()


	/** Widget constructor */
	void Construct( const FArguments& Args, const TSharedRef< class SPluginsEditor > Owner );

	/** Destructor */
	~SPluginCategories();

	/** @return Gets the owner of this categories tree */
	SPluginsEditor& GetOwner();

	/** @return Returns the currently selected category item */
	FPluginCategoryTreeItemPtr GetSelectedCategory() const;

	/** Selects the specified category */
	void SelectCategory( const FPluginCategoryTreeItemPtr& CategoryToSelect );

	/** @return Returns true if the specified item is currently expanded in the tree */
	bool IsItemExpanded( const FPluginCategoryTreeItemPtr Item ) const;


private:
	
	/** Called to generate a widget for the specified tree item */
	TSharedRef<ITableRow> PluginCategoryTreeView_OnGenerateRow( FPluginCategoryTreeItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable );

	/** Given a tree item, fills an array of child items */
	void PluginCategoryTreeView_OnGetChildren( FPluginCategoryTreeItemPtr Item, TArray< FPluginCategoryTreeItemPtr >& OutChildren );

	/** Called when the user clicks on a plugin item, or when selection changes by some other means */
	void PluginCategoryTreeView_OnSelectionChanged( FPluginCategoryTreeItemPtr Item, ESelectInfo::Type SelectInfo );

	/** Rebuilds the category tree from scratch */
	void RebuildAndFilterCategoryTree();

	/** SWidget overrides */
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;


private:

	/** Weak pointer back to its owner */
	TWeakPtr< class SPluginsEditor > OwnerWeak;

	/** The tree view widget for our plugin categories tree */
	TSharedPtr< SPluginCategoryTreeView > PluginCategoryTreeView;

	/** Root list of categories */
	TArray< FPluginCategoryTreeItemPtr > RootPluginCategories;

	/** True if something has changed with either our filtering or the loaded plugin set, and we need to
	    do a full refresh */
	bool bNeedsRefresh;
};


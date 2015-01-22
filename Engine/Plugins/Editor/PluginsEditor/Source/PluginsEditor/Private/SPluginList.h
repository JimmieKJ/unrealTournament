// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PluginListItem.h"

typedef SListView< FPluginListItemPtr > SPluginListView;


/**
 * A filtered list of plugins, driven by a category selection
 */
class SPluginList : public SCompoundWidget
{
	SLATE_BEGIN_ARGS( SPluginList )
	{
	}

	SLATE_END_ARGS()

	/** Widget constructor */
	void Construct( const FArguments& Args, const TSharedRef< class SPluginsEditor > Owner );

	/** Destructor */
	~SPluginList();

	/** @return Gets the owner of this list */
	SPluginsEditor& GetOwner();

	/** Called to invalidate the list */
	void SetNeedsRefresh();


private:

	/** Called when the plugin text filter has changed what its filtering */
	void OnPluginTextFilterChanged();

	/** Recursively gathers plugins from a category and all sub-categories.  Updates the PluginListItems structure. */
	void GetPluginsRecursively( const TSharedPtr< class FPluginCategoryTreeItem >& Category );

	/** Called to generate a widget for the specified list item */
	TSharedRef<ITableRow> PluginListView_OnGenerateRow( FPluginListItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable );

	/** Rebuilds the list of plugins from scratch and applies filtering. */
	void RebuildAndFilterPluginList();

	/** SWidget overrides */
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;


private:

	/** Weak pointer back to its owner */
	TWeakPtr< class SPluginsEditor > OwnerWeak;

	/** The list view widget for our plugins list */
	TSharedPtr< SPluginListView > PluginListView;

	/** List of everything that we want to display in the plugin list */
	TArray< FPluginListItemPtr > PluginListItems;

	/** True if something has changed with either our filtering or the loaded plugin set, and we need to
	    do a full refresh */
	bool bNeedsRefresh;

};


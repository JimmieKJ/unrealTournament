// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Widget that represents a single category in the category tree view
 */
class SPluginCategoryTreeItem : public SCompoundWidget
{
	SLATE_BEGIN_ARGS( SPluginCategoryTreeItem )
	{
	}

	SLATE_END_ARGS()


	/** Widget constructor */
	void Construct( const FArguments& Args, const TSharedRef< class SPluginCategories > Owner, const TSharedRef< class FPluginCategoryTreeItem >& Item );


private:

	/** @return Gets the icon brush to use for this item's current state */
	const FSlateBrush* GetIconBrush() const;


private:

	/** The item we're representing the in tree */
	TSharedPtr< class FPluginCategoryTreeItem > ItemData;

	/** Weak pointer back to its owner */
	TWeakPtr< class SPluginCategories > OwnerWeak;
};


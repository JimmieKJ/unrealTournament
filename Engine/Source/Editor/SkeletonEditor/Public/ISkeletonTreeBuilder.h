// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"

class ISkeletonTreeItem;

/** Output struct for builders to use */
struct SKELETONEDITOR_API FSkeletonTreeBuilderOutput
{
	FSkeletonTreeBuilderOutput(TArray<TSharedPtr<class ISkeletonTreeItem>>& InItems, TArray<TSharedPtr<class ISkeletonTreeItem>>& InLinearItems)
		: Items(InItems)
		, LinearItems(InLinearItems)
	{}

	/** 
	 * Add an item to the output
	 * @param	InItem			The item to add
	 * @param	InParentName	The name of the item's parent
	 * @param	InParentTypes	The types of items to search. If this is empty all items will be searched.
	 */
	void Add(const TSharedPtr<class ISkeletonTreeItem>& InItem, const FName& InParentName, TArrayView<const FName> InParentTypes);

	/** 
	 * Add an item to the output
	 * @param	InItem			The item to add
	 * @param	InParentName	The name of the item's parent
	 * @param	InParentType	The type of items to search. If this is NAME_None all items will be searched.
	 */
	void Add(const TSharedPtr<class ISkeletonTreeItem>& InItem, const FName& InParentName, const FName& InParentType);

	/** 
	 * Find the item with the specified name
	 * @param	InName	The item's name
	 * @param	InTypes	The types of items to search. If this is empty all items will be searched.
	 * @return the item found, or an invalid ptr if it was not found.
	 */
	TSharedPtr<class ISkeletonTreeItem> Find(const FName& InName, TArrayView<const FName> InTypes) const;

	/** 
	 * Find the item with the specified name
	 * @param	InName	The item's name
	 * @param	InType	The type of items to search. If this is NAME_None all items will be searched.
	 * @return the item found, or an invalid ptr if it was not found.
	 */
	TSharedPtr<class ISkeletonTreeItem> Find(const FName& InName, const FName& InType) const;

private:
	/** The items that are built by this builder */
	TArray<TSharedPtr<class ISkeletonTreeItem>>& Items;

	/** A linearized list of all items in OutItems (for easier searching) */
	TArray<TSharedPtr<class ISkeletonTreeItem>>& LinearItems;
};

/** Basic filter used when re-filtering the tree */
struct FSkeletonTreeFilterArgs
{
	FSkeletonTreeFilterArgs()
		: bWillFilter(false)
		, bFlattenHierarchyOnFilter(true)
	{}

	/** Whether or not we have any kind of filtering */
	bool bWillFilter;

	/** Whether to flatten the hierarchy so filtered items appear in a linear list */
	bool bFlattenHierarchyOnFilter;
};

/** Interface to implement to provide custom build logic to skeleton trees */
class ISkeletonTreeBuilder
{
public:

	virtual ~ISkeletonTreeBuilder() {};

	/**
	 * Build an array of skeleton tree items to display in the tree.
	 * @param	InArgs			Parameters to use when building the tree
	 * @param	Output			The items that are built by this builder
	 */
	virtual void Build(FSkeletonTreeBuilderOutput& Output) = 0;

	/** Apply filtering to the tree items */
	virtual void Filter(const FSkeletonTreeFilterArgs& InArgs, const TArray<TSharedPtr<class ISkeletonTreeItem>>& InItems, TArray<TSharedPtr<class ISkeletonTreeItem>>& OutFilteredItems) = 0;
};

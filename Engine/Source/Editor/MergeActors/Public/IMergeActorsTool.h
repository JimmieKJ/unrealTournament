// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

/**
* Merge Actors tool interface
*/
class IMergeActorsTool
{

public:

	/** Virtual destructor */
	virtual ~IMergeActorsTool() {}

	/**
	 * Gets the widget instance associated with this tool
	 */
	virtual TSharedRef<SWidget> GetWidget() = 0;

	/**
	 * Get the name of the icon displayed in the Merge Actors toolbar
	 */
	virtual FName GetIconName() const = 0;

	/**
	 * Get Tooltip text displayed in the Merge Actors toolbar
	 */
	virtual FText GetTooltipText() const = 0;

	/**
	 * Get default name for the merged asset package
	 */
	virtual FString GetDefaultPackageName() const = 0;

	/**
	 * Perform merge operation
	 *
	 * @param	PackageName		Long package name of the asset to create
	 * @return	true if the merge succeeded
	 */
	virtual bool RunMerge(const FString& PackageName) = 0;

};

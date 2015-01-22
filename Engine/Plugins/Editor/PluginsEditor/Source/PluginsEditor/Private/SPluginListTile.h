// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Widget that represents a "tile" for a single plugin in our plugins list
 */
class SPluginListTile : public SCompoundWidget
{
	SLATE_BEGIN_ARGS( SPluginListTile )
	{
	}

	SLATE_END_ARGS()


	/** Widget constructor */
	void Construct( const FArguments& Args, const TSharedRef< class SPluginList > Owner, const TSharedRef< class FPluginListItem >& Item );


private:

	/** Returns the checked state for the enabled checkbox */
	ECheckBoxState IsPluginEnabled() const;

	/** Called when the enabled checkbox is clicked */
	void OnEnablePluginCheckboxChanged(ECheckBoxState NewCheckedState);

	/** Called when 'Uninstall' is clicked on a plugin's tile */
	FReply OnUninstallButtonClicked();


private:

	/** The item we're representing the in tree */
	TSharedPtr< class FPluginListItem > ItemData;

	/** Weak pointer back to its owner */
	TWeakPtr< class SPluginList > OwnerWeak;

	/** Brush resource for the image that is dynamically loaded */
	TSharedPtr< FSlateDynamicImageBrush > PluginIconDynamicImageBrush;
};


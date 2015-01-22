// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Unreal UMG editor actions
 */
class FUMGEditorCommands : public TCommands<FUMGEditorCommands>
{

public:
	FUMGEditorCommands() : TCommands<FUMGEditorCommands>
	(
		"UMGEditor", // Context name for fast lookup
		NSLOCTEXT("Contexts", "UMGEditor", "UMG Editor"), // Localized context name for displaying
		"EditorViewport",  // Parent
		FEditorStyle::GetStyleSetName() // Icon Style Set
	)
	{
	}
	
	/**
	 * UMG Editor Commands
	 */
	

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;

public:
};

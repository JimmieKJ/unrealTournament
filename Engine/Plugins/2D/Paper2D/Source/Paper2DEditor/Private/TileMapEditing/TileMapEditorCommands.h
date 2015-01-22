// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PaperStyle.h"

class FTileMapEditorCommands : public TCommands<FTileMapEditorCommands>
{
public:
	FTileMapEditorCommands()
		: TCommands<FTileMapEditorCommands>(
			TEXT("TileMapEditor"), // Context name for fast lookup
			NSLOCTEXT("Contexts", "TileMapEditor", "Tile Map Editor"), // Localized context name for displaying
			NAME_None, // Parent
			FPaperStyle::Get()->GetStyleSetName() // Icon Style Set
			)
	{
	}

	// TCommand<> interface
	virtual void RegisterCommands() override;
	// End of TCommand<> interface

public:
	// Show toggles
	TSharedPtr<FUICommandInfo> SetShowCollision;
	
	TSharedPtr<FUICommandInfo> SetShowPivot;

	// Misc. actions
	TSharedPtr<FUICommandInfo> FocusOnTileMap;



	TSharedPtr<FUICommandInfo> AddNewLayerAbove;
	TSharedPtr<FUICommandInfo> AddNewLayerBelow;
	TSharedPtr<FUICommandInfo> DeleteLayer;
	TSharedPtr<FUICommandInfo> DuplicateLayer;
	TSharedPtr<FUICommandInfo> MergeLayerDown;
	TSharedPtr<FUICommandInfo> MoveLayerUp;
	TSharedPtr<FUICommandInfo> MoveLayerDown;
};
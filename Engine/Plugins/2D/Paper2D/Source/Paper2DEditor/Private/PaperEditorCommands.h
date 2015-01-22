// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PaperStyle.h"

class FPaperEditorCommands : public TCommands<FPaperEditorCommands>
{
public:
	FPaperEditorCommands()
		: TCommands<FPaperEditorCommands>(
			TEXT("PaperEditor"), // Context name for fast lookup
			NSLOCTEXT("Contexts", "PaperEditor", "Paper2D Editor"), // Localized context name for displaying
			NAME_None, // Parent
			FPaperStyle::Get()->GetStyleSetName() // Icon Style Set
			)
	{
	}

	// TCommand<> interface
	virtual void RegisterCommands() override;
	// End of TCommand<> interface

public:
	// Edit mode trigger commands
	TSharedPtr<FUICommandInfo> EnterTileMapEditMode;
	
	// Commands inside tile map editing mode
	TSharedPtr<FUICommandInfo> SelectPaintTool;
	TSharedPtr<FUICommandInfo> SelectEraserTool;
	TSharedPtr<FUICommandInfo> SelectFillTool;

	TSharedPtr<FUICommandInfo> SelectVisualLayersPaintingMode;
	TSharedPtr<FUICommandInfo> SelectCollisionLayersPaintingMode;
};
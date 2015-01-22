// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SoundCueEditorModule.h"
#include "SSoundCuePalette.h"

void SSoundCuePalette::Construct(const FArguments& InArgs)
{
	// Auto expand the palette as there's so few nodes
	SGraphPalette::Construct(SGraphPalette::FArguments().AutoExpandActionMenu(true));
}

void SSoundCuePalette::CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
{
	const USoundCueGraphSchema* Schema = GetDefault<USoundCueGraphSchema>();

	FGraphActionMenuBuilder ActionMenuBuilder;

	// Determine all possible actions
	Schema->GetPaletteActions(ActionMenuBuilder);

	//@TODO: Avoid this copy
	OutAllActions.Append(ActionMenuBuilder);
}

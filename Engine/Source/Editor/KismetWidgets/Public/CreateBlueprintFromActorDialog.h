// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// FCreateBlueprintFromActorDialog

class FCreateBlueprintFromActorDialog
{
public:
	/** 
	 * Static function to access constructing this window.
	 *
	 * @param bInHarvest		true if the components of the selected actors should be harvested for the blueprint.
	 */
	static KISMETWIDGETS_API void OpenDialog(bool bInHarvest);
private:

	/** 
	 * Will create the blueprint
	 *
	 * @param InAssetPath		Path of the asset to create
	 * @param bInHarvest		true if the components of the selected actors should be harvested for the blueprint.
	 */
	static void OnCreateBlueprint(const FString& InAssetPath, bool bInHarvest);
};

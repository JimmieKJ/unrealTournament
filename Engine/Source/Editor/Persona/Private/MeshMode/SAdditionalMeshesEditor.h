// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SAdditionalMeshesDisplayPanel;

/**
 * Implements the profile page for the session launcher wizard.
 */
class SAdditionalMeshesEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SAdditionalMeshesEditor ) { }
	SLATE_END_ARGS()

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs	- The construction arguments.
	 * @param InPersona - The owning Persona editor
	 */
	void Construct(const FArguments& InArgs, TSharedPtr<FPersona> InPersona);

private:

	/**
	 * Helper function to get the preview scene for Persona
	 *
	 * @return	Persona's Preview Scene
	 */
	class FPreviewScene* GetPersonaPreviewScene();

	/**
	 * Handler for the editor panels clear all button
	 */
	FReply OnClearAllClicked();

	/**
	 * Handler for the Add Mesh button, makes the asset picker pop-up menu
	 *
	 * @return	The new asset picker menu for display
	 */
	TSharedRef<SWidget> MakeAssetPickerMenu();

	/**
	 * Handler for asset picker pop-up menu. Tests potentially displayable items to see whether they match the current skeleton.
	 *
	 * @param AssetData - The asset item to be tested.
	 * @return	True if the item should be filtered out of the list, false if it should be displayed.
	 */
	bool ShouldFilterAssetBasedOnSkeleton(const FAssetData& AssetData);

	/**
	 * Handler for asset picker pop-up menu. Handles a particular asset being selected.
	 *
	 * @param AssetData - The asset that has been selected.
	 */
	void OnAssetSelectedFromPicker(const FAssetData& AssetData);

	/**
	 * Handler for a user removing a mesh in the loaded meshes panel
	 *
	 * @param MeshToRemove - The mesh to be removed.
	 */
	void OnRemoveAdditionalMesh(USkeletalMeshComponent* MeshToRemove);

	// Pointer back to owning Persona instance (the keeper of state)
	TWeakPtr<class FPersona>	PersonaPtr;

	// Pointer to the panels ComboButton for canceling the pop-up menu.
	TSharedPtr<SComboButton>	PickerComboButton;

	// Pointer to the panels WrapBox for displaying each loaded mesh in.
	TSharedPtr<SAdditionalMeshesDisplayPanel>		LoadedMeshButtonsHolder;

	//filter string for populating asset picker
	FString SkeletonNameAssetFilter;
};
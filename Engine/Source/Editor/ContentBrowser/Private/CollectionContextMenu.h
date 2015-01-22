// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FCollectionContextMenu : public TSharedFromThis<FCollectionContextMenu>
{
public:
	/** Constructor */
	FCollectionContextMenu(const TWeakPtr<SCollectionView>& InCollectionView);

	/** Bind menu selection commands to the command list */
	void BindCommands(TSharedPtr< FUICommandList > InCommandList);

	/** Makes the collection list context menu widget */
	TSharedPtr<SWidget> MakeCollectionListContextMenu(TSharedPtr< FUICommandList > InCommandList);

	/** Makes the new collection submenu */
	void MakeNewCollectionSubMenu(FMenuBuilder& MenuBuilder);

	/** Update the source control flag the 'CanExecute' functions rely on */
	void UpdateProjectSourceControl();

	/** Can the selected collections be renamed? */
	bool CanRenameSelectedCollections() const;

protected:
	/** Makes the set color submenu */
	void MakeSetColorSubMenu(FMenuBuilder& MenuBuilder);

private:
	/** Handler for when a collection is selected in the "New" menu */
	void ExecuteNewCollection(ECollectionShareType::Type CollectionType);

	/** Handler for when "Rename Collection" is selected */
	void ExecuteRenameCollection();

	/** Handler for when "Destroy Collection" is selected */
	void ExecuteDestroyCollection();

	/** Handler for when "Destroy Collection" is confirmed */
	FReply ExecuteDestroyCollectionConfirmed(TArray<TSharedPtr<FCollectionItem>> CollectionList);

	/** Handler for when reset color is selected */
	void ExecuteResetColor();

	/** Handler for when new or set color is selected */
	void ExecutePickColor();

	/** Handler to check to see if "New Collection" can be executed */
	bool CanExecuteNewCollection(ECollectionShareType::Type CollectionType) const;

	/** Handler to check to see if "Rename Collection" can be executed */
	bool CanExecuteRenameCollection() const;

	/** Handler to check to see if "Destroy Collection" can be executed */
	bool CanExecuteDestroyCollection(bool bAnyManagedBySCC) const;

	/** Checks to see if any of the selected collections use custom colors */
	bool SelectedHasCustomColors() const;

	/** Callback when the color picker dialog has been closed */
	void NewColorComplete(const TSharedRef<SWindow>& Window);

	/** Callback when the color is picked from the set color submenu */
	FReply OnColorClicked( const FLinearColor InColor );

	/** Resets the colors of the selected collections */
	void ResetColors();

	TWeakPtr<SCollectionView> CollectionView;

	/** Flag caching whether the project is under source control */
	bool bProjectUnderSourceControl;
};

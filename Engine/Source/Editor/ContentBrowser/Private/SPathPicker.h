// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * A sources view designed for path picking
 */
class SPathPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SPathPicker ){}

		/** A struct containing details about how the path picker should behave */
		SLATE_ARGUMENT(FPathPickerConfig, PathPickerConfig)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs );

private:

	/** Handler for the context menu for folder items */
	TSharedPtr<SWidget> GetFolderContextMenu(const TArray<FString>& SelectedPaths, FContentBrowserMenuExtender_SelectedPaths InMenuExtender, FOnCreateNewFolder InOnCreateNewFolder);

	/** Handler for creating a new folder in the path picker */
	void CreateNewFolder(FString FolderPath, FOnCreateNewFolder InOnCreateNewFolder);

	/** Sets the selected paths in this picker */
	void SetPaths(const TArray<FString>& NewPaths);

private:

	/** The path view in this picker */
	TSharedPtr<SPathView> PathViewPtr;
};
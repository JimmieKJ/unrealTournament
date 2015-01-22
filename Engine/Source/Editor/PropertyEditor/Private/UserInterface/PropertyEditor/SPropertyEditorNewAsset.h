// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class SPropertyEditorNewAsset : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPropertyEditorNewAsset)
	{}

	SLATE_END_ARGS()

	static UObject* Create(TWeakObjectPtr<UFactory> FactoryPtr);

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, TSharedPtr<SWindow> InParentWindow, TWeakObjectPtr<UFactory> InFactoryPtr);

private:

 	/** Callback when the "create asset" button is clicked. */
 	FReply OnCreateNewAssetClicked();
 
 	/** Callback when the selected asset path has changed. */
 	void OnSelectAssetPath(const FString& Path);
 
 	/** Destroys the window when the operation is cancelled. */
 	FReply OnCancelCreateNewAsset();

	/** Callback when the user changes the filename for the Blueprint */
 	void OnFilenameChanged(const FText& InNewName);
 
 	/** Callback to see if creating an asset is enabled */
 	bool IsCreateNewAssetEnabled() const;

private:
	/** The window this widget is nested in */
	TWeakPtr<SWindow> ParentWindowPtr;

	/** The factory used to create a new asset */
	TWeakObjectPtr<UFactory> FactoryPtr;

 	/** The selected path to create the asset */
 	FString AssetPath;
 
 	/** Filename textbox widget */
 	TSharedPtr<SEditableTextBox> FilenameWidget;

 	/** True if an error is currently being reported */
 	bool bIsReportingError;

	/** Pointer to the new asset */
	UObject* NewAsset;
};

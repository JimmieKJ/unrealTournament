// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/*-----------------------------------------------------------------------------
   SPhATPreviewViewportToolBar
-----------------------------------------------------------------------------*/

class SPhATPreviewViewportToolBar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS(SPhATPreviewViewportToolBar){}
		SLATE_ARGUMENT(TWeakPtr<FPhAT>, PhATPtr)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class SEditorViewport> InRealViewport);

private:

	/** Generates the toolbar perspective menu content */
	TSharedRef<SWidget> GeneratePerspectiveMenu() const;
	/** Generates the toolbar view menu content */
	TSharedRef<SWidget> GenerateShowMenu() const;

	/** Generates the toolbar modes menu content */
	TSharedRef<SWidget> GenerateModesMenu() const;

	/**
	* Generates the toolbar options menu content
	*
	* @return The widget containing the options menu content
	*/
	TSharedRef<SWidget> GenerateOptionsMenu() const;


	/**
	* Generates the toolbar FOV menu content
	*
	* @return The widget containing the FOV menu content
	*/
	TSharedRef<SWidget> GenerateFOVMenu() const;

	/** Called by the FOV slider in the perspective viewport to get the FOV value */
	float OnGetFOVValue() const;

	/** Called when the FOV slider is adjusted in the perspective viewport */
	void OnFOVValueChanged(float NewValue);

	FText GetCameraMenuLabel() const;
	const FSlateBrush* GetCameraMenuLabelIcon() const;

private:
	/** The viewport that we are in */
	TWeakPtr<FPhAT> PhATPtr;
};

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
	TSharedRef<SWidget> GenerateViewMenu() const;

	/** Generates the toolbar modes menu content */
	TSharedRef<SWidget> GenerateModesMenu() const;

private:
	/** The viewport that we are in */
	TWeakPtr<FPhAT> PhATPtr;
};

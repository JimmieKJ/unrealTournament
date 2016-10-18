// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SEditorViewport.h"

class FPhAT;
class FPhATEdPreviewViewportClient;

/*-----------------------------------------------------------------------------
   SPhATViewport
-----------------------------------------------------------------------------*/

class SPhATPreviewViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SPhATPreviewViewport)
		{}

		SLATE_ARGUMENT(TWeakPtr<FPhAT>, PhAT)
	SLATE_END_ARGS()

	/** Destructor */
	~SPhATPreviewViewport();

	/** SCompoundWidget interface */
	void Construct(const FArguments& InArgs);

	/** Refreshes the viewport */
	void RefreshViewport();

	/** Returns true if the viewport is visible */
	bool IsVisible() const override;

	virtual void OnFocusViewportToSelection() override;

	/** Accessors */
	TSharedPtr<FSceneViewport> GetViewport() const;
	TSharedPtr<FPhATEdPreviewViewportClient> GetViewportClient() const;
	TSharedPtr<SViewport> GetViewportWidget() const;
	
public:
	/** The parent tab where this viewport resides */
	TWeakPtr<SDockTab> ParentTab;

	void SetViewportType(ELevelViewportType ViewType);
	void RotateViewportType();

protected:
	/** SEditorViewport interface */
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;

private:
	/** Pointer back to the PhysicsAsset editor tool that owns us */
	TWeakPtr<FPhAT> PhATPtr;
	
	/** Level viewport client */
	TSharedPtr<FPhATEdPreviewViewportClient> EditorViewportClient;
};

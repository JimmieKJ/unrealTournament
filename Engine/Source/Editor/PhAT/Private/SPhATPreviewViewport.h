// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FPhAT;


/*-----------------------------------------------------------------------------
   SPhATViewport
-----------------------------------------------------------------------------*/

class SPhATPreviewViewport : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPhATPreviewViewport)
		{}

		SLATE_ARGUMENT(TWeakPtr<FPhAT>, PhAT)
	SLATE_END_ARGS()

	/** Destructor */
	~SPhATPreviewViewport();

	/** SCompoundWidget interface */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	void Construct(const FArguments& InArgs);

	/** Refreshes the viewport */
	void RefreshViewport();

	/** Returns true if the viewport is visible */
	bool IsVisible() const;
	EVisibility GetWidgetVisibility() const;

	/** Accessors */
	TSharedPtr<FSceneViewport> GetViewport() const;
	TSharedPtr<FPhATEdPreviewViewportClient> GetViewportClient() const;
	TSharedPtr<SViewport> GetViewportWidget() const;
	
public:
	/** The parent tab where this viewport resides */
	TWeakPtr<SDockTab> ParentTab;

	void SetViewportType(ELevelViewportType ViewType);

private:
	/** Pointer back to the PhysicsAsset editor tool that owns us */
	TWeakPtr<FPhAT> PhATPtr;
	
	/** Level viewport client */
	TSharedPtr<FPhATEdPreviewViewportClient> ViewportClient;

	/** Slate viewport for rendering and I/O */
	TSharedPtr<FSceneViewport> Viewport;

	/** Viewport widget*/
	TSharedPtr<SViewport> ViewportWidget;
};

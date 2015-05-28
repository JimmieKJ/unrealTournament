// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/UnrealEd/Public/Toolkits/BaseToolkit.h"
#include "SCompoundWidget.h"

/**
 * Slate widget wrapping an FAssetThumbnail and Viewport
 */
class SLandscapeAssetThumbnail : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SLandscapeAssetThumbnail )
		: _ThumbnailSize( 64,64 ) {}
		SLATE_ARGUMENT( FIntPoint, ThumbnailSize )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UObject* Asset, TSharedRef<class FAssetThumbnailPool> ThumbnailPool);
	~SLandscapeAssetThumbnail();

	void SetAsset(UObject* Asset);

private:
	void OnMaterialCompilationFinished(UMaterialInterface* MaterialInterface);

	TSharedPtr<class FAssetThumbnail> AssetThumbnail;
};

/**
 * Mode Toolkit for the Landscape Editor Mode
 */
class FLandscapeToolKit : public FModeToolkit
{
public:
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	/** Initializes the geometry mode toolkit */
	virtual void Init(const TSharedPtr< class IToolkitHost >& InitToolkitHost) override;

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual class FEdModeLandscape* GetEditorMode() const override;
	virtual TSharedPtr<class SWidget> GetInlineContent() const override;

	void NotifyToolChanged();
	void NotifyBrushChanged();

private:
	/** Geometry tools widget */
	TSharedPtr<class SLandscapeEditor> LandscapeEditorWidgets;
};

/**
 * Slate widgets for the Landscape Editor Mode
 */
class SLandscapeEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SLandscapeEditor ){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FLandscapeToolKit> InParentToolkit);

	void NotifyToolChanged();
	void NotifyBrushChanged();

protected:
	class FEdModeLandscape* GetEditorMode() const;

	FText GetErrorText() const;

	bool GetLandscapeEditorIsEnabled() const;

	bool GetIsPropertyVisible(const struct FPropertyAndParent& PropertyAndParent) const;

	void OnChangeMode(FName ModeName);
	bool IsModeEnabled(FName ModeName) const;
	bool IsModeActive(FName ModeName) const;

protected:
	// Command list bound to this window
	TSharedPtr<FUICommandList> CommandList;
	TSharedPtr<SErrorText> Error;

	TSharedPtr<class IDetailsView> DetailsPanel;
};

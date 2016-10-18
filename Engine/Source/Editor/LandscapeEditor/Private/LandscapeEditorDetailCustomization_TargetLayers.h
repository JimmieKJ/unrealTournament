// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LandscapeEditorDetailCustomization_Base.h"


/**
 * Slate widgets customizer for the target layers list in the Landscape Editor
 */

class FLandscapeEditorDetailCustomization_TargetLayers : public FLandscapeEditorDetailCustomization_Base
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

protected:
	static bool ShouldShowTargetLayers();
	static bool ShouldShowPaintingRestriction();
	static EVisibility GetVisibility_PaintingRestriction();
	static bool ShouldShowVisibilityTip();
	static EVisibility GetVisibility_VisibilityTip();
};

class FLandscapeEditorCustomNodeBuilder_TargetLayers : public IDetailCustomNodeBuilder
{
public:
	FLandscapeEditorCustomNodeBuilder_TargetLayers(TSharedRef<FAssetThumbnailPool> ThumbnailPool);
	~FLandscapeEditorCustomNodeBuilder_TargetLayers();

	virtual void SetOnRebuildChildren( FSimpleDelegate InOnRegenerateChildren ) override;
	virtual void GenerateHeaderRowContent( FDetailWidgetRow& NodeRow ) override;
	virtual void GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder ) override;
	virtual void Tick( float DeltaTime ) override {}
	virtual bool RequiresTick() const override { return false; }
	virtual bool InitiallyCollapsed() const override { return false; }
	virtual FName GetName() const override { return "TargetLayers"; }

protected:
	TSharedRef<FAssetThumbnailPool> ThumbnailPool;

	static class FEdModeLandscape* GetEditorMode();

	void GenerateRow(IDetailChildrenBuilder& ChildrenBuilder, const TSharedRef<FLandscapeTargetListInfo> Target);

	static bool GetTargetLayerIsSelected(const TSharedRef<FLandscapeTargetListInfo> Target);
	static void OnTargetSelectionChanged(const TSharedRef<FLandscapeTargetListInfo> Target);
	static TSharedPtr<SWidget> OnTargetLayerContextMenuOpening(const TSharedRef<FLandscapeTargetListInfo> Target);
	static void OnExportLayer(const TSharedRef<FLandscapeTargetListInfo> Target);
	static void OnImportLayer(const TSharedRef<FLandscapeTargetListInfo> Target);
	static void OnReimportLayer(const TSharedRef<FLandscapeTargetListInfo> Target);
	static void OnFillLayer(const TSharedRef<FLandscapeTargetListInfo> Target);
	static void OnClearLayer(const TSharedRef<FLandscapeTargetListInfo> Target);
	static bool ShouldFilterLayerInfo(const class FAssetData& AssetData, FName LayerName);
	static void OnTargetLayerSetObject(const FAssetData& AssetData, const TSharedRef<FLandscapeTargetListInfo> Target);
	static EVisibility GetTargetLayerInfoSelectorVisibility(const TSharedRef<FLandscapeTargetListInfo> Target);
	static EVisibility GetTargetLayerCreateVisibility(const TSharedRef<FLandscapeTargetListInfo> Target);
	static EVisibility GetTargetLayerMakePublicVisibility(const TSharedRef<FLandscapeTargetListInfo> Target);
	static EVisibility GetTargetLayerDeleteVisibility(const TSharedRef<FLandscapeTargetListInfo> Target);
	static TSharedRef<SWidget> OnGetTargetLayerCreateMenu(const TSharedRef<FLandscapeTargetListInfo> Target);
	static void OnTargetLayerCreateClicked(const TSharedRef<FLandscapeTargetListInfo> Target, bool bNoWeightBlend);
	static FReply OnTargetLayerMakePublicClicked(const TSharedRef<FLandscapeTargetListInfo> Target);
	static FReply OnTargetLayerDeleteClicked(const TSharedRef<FLandscapeTargetListInfo> Target);
	static FSlateColor GetLayerUsageDebugColor(const TSharedRef<FLandscapeTargetListInfo> Target);
	static EVisibility GetDebugModeLayerUsageVisibility(const TSharedRef<FLandscapeTargetListInfo> Target);
	static EVisibility GetDebugModeLayerUsageVisibility_Invert(const TSharedRef<FLandscapeTargetListInfo> Target);
	static EVisibility GetDebugModeColorChannelVisibility(const TSharedRef<FLandscapeTargetListInfo> Target);
	static ECheckBoxState DebugModeColorChannelIsChecked(const TSharedRef<FLandscapeTargetListInfo> Target, int32 Channel);
	static void OnDebugModeColorChannelChanged(ECheckBoxState NewCheckedState, const TSharedRef<FLandscapeTargetListInfo> Target, int32 Channel);
};

class SLandscapeEditorSelectableBorder : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SLandscapeEditorSelectableBorder)
		: _Content()
		, _HAlign(HAlign_Fill)
		, _VAlign(VAlign_Fill)
		, _Padding(FMargin(2.0f))
	{ }
		SLATE_DEFAULT_SLOT(FArguments, Content)

		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		SLATE_ATTRIBUTE(FMargin, Padding)

		SLATE_EVENT(FOnContextMenuOpening, OnContextMenuOpening)
		SLATE_EVENT(FSimpleDelegate, OnSelected)
		SLATE_ATTRIBUTE(bool, IsSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	const FSlateBrush* GetBorder() const;

protected:
	FOnContextMenuOpening OnContextMenuOpening;
	FSimpleDelegate OnSelected;
	TAttribute<bool> IsSelected;
};
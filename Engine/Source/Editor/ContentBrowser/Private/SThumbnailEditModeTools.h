// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SThumbnailEditModeTools : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SThumbnailEditModeTools)
		: _SmallView(false)
		{}
		
		SLATE_ARGUMENT( bool, SmallView )

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs, const TSharedPtr<FAssetThumbnail>& InAssetThumbnail );

	// SWidget Interface
	FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;

protected:
	/** Gets the visibility for the primitives toolbar */
	EVisibility GetPrimitiveToolsVisibility() const;

	/** Gets the brush used to display the currently used primitive */
	const FSlateBrush* GetCurrentPrimitiveBrush() const;

	/** Sets the primitive type for the supplied thumbnail, if possible */
	FReply ChangePrimitive();

	/** Helper accessors for ThumbnailInfo objects */
	USceneThumbnailInfo* GetSceneThumbnailInfo();
	USceneThumbnailInfoWithPrimitive* GetSceneThumbnailInfoWithPrimitive();
	USceneThumbnailInfoWithPrimitive* ConstGetSceneThumbnailInfoWithPrimitive() const;

	/** Event fired when the asset data for this asset is loaded or changed */
	void OnAssetDataChanged();

protected:
	bool bModifiedThumbnailWhileDragging;
	FIntPoint DragStartLocation;
	TWeakPtr<FAssetThumbnail> AssetThumbnail;
	TWeakObjectPtr<USceneThumbnailInfo> SceneThumbnailInfo;
	bool bInSmallView;
};
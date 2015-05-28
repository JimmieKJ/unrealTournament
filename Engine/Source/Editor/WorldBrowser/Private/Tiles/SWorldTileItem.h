// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateTextures.h"
#include "GraphEditor.h"
#include "SNodePanel.h"
#include "AssetThumbnail.h"

class FTileItemThumbnail : public ISlateViewport, public TSharedFromThis<FTileItemThumbnail>
{
public:
	FTileItemThumbnail(FSlateTextureRenderTarget2DResource* InThumbnailRenderTarget, TSharedPtr<FLevelModel> InItemModel);
	~FTileItemThumbnail();

	/* ISlateViewport interface */
	virtual FIntPoint GetSize() const override;
	virtual class FSlateShaderResource* GetViewportRenderTargetTexture() const override;
	virtual bool RequiresVsync() const override;
	
	/** Request thumbnail redraw*/
	void UpdateThumbnail();

private:
	void UpdateTextureData(FObjectThumbnail* ObjectThumbnail);

private:
	TSharedPtr<FLevelModel>					LevelModel;
	/** Rendering resource for slate */
	FSlateTexture2DRHIRef*					ThumbnailTexture;
	/** Shared render target for slate */
	FSlateTextureRenderTarget2DResource*	ThumbnailRenderTarget;
};

//----------------------------------------------------------------
//
//
//----------------------------------------------------------------
class SWorldTileItem 
	: public SNodePanel::SNode
{
public:
	SLATE_BEGIN_ARGS(SWorldTileItem)
	{}
		/** The world data */
		SLATE_ARGUMENT(TSharedPtr<FWorldTileCollectionModel>, InWorldModel)
		/** Data for the asset this item represents */
		SLATE_ARGUMENT(TSharedPtr<FWorldTileModel>, InItemModel)
		//
		SLATE_ARGUMENT(FSlateTextureRenderTarget2DResource*, ThumbnailRenderTarget)
	SLATE_END_ARGS()

	~SWorldTileItem();

	void Construct(const FArguments& InArgs);
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	
	// SNodePanel::SNode interface start
	virtual FVector2D GetDesiredSizeForMarquee() const override;
	virtual UObject* GetObjectBeingDisplayed() const override;
	virtual FVector2D GetPosition() const override;
	virtual const FSlateBrush* GetShadowBrush(bool bSelected) const override;
	// SNodePanel::SNode interface end
	
	/** @return Deferred item refresh */
	void RequestRefresh();

	/** @return LevelModel associated with this item */
	TSharedPtr<FLevelModel>	GetLevelModel() const;
	
	/** @return Item width in world units */
	FOptionalSize GetItemWidth() const;
	
	/** @return Item height in world units */
	FOptionalSize GetItemHeight() const;
	
	/** @return Rectangle in world units for this item as FSlateRect*/
	FSlateRect GetItemRect() const;
	
	/** @return Whether this item can be edited (loaded and not locked) */
	bool IsItemEditable() const;

	/** @return Whether this item is selected */
	bool IsItemSelected() const;
	
	/** @return Whether this item is enabled */
	bool IsItemEnabled() const;

private:
	// SWidget interface start
	virtual TSharedPtr<IToolTip> GetToolTip() override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	// SWidget interface end

	TSharedRef<SToolTip> CreateToolTipWidget();
	
	/** Tile tooltips fields */
	FText GetLevelNameText() const;
	FText GetPositionText() const;
	FText GetBoundsExtentText() const;
	FText GetLevelLayerNameText() const;
	FText GetLevelLayerDistanceText() const;
	
public:
	bool							bAffectedByMarquee;

private:
	/** The world data */
	TSharedPtr<FWorldTileCollectionModel> WorldModel;

	/** The data for this item */
	TSharedPtr<FWorldTileModel>		TileModel;

	mutable bool					bNeedRefresh;
	bool							bIsDragging;

	TSharedPtr<FTileItemThumbnail>	Thumbnail;
	/** The actual widget for the thumbnail. */
	TSharedPtr<SViewport>			ThumbnailViewport;
	
	/** */
	const FSlateBrush*				ProgressBarImage;

	/** Curve sequence to drive loading animation */
	FCurveSequence					CurveSequence;
};


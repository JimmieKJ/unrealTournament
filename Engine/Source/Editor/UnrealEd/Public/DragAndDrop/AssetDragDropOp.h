// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Runtime/AssetRegistry/Public/AssetData.h"
#include "AssetThumbnail.h"
#include "ClassIconFinder.h"
#include "DecoratedDragDropOp.h"

class FAssetDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FAssetDragDropOp, FDecoratedDragDropOp)

	/** Data for the asset this item represents */
	TArray<FAssetData> AssetData;

	/** Pool for maintaining and rendering thumbnails */
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;

	/** Handle to the thumbnail resource */
	TSharedPtr<FAssetThumbnail> AssetThumbnail;

	/** The actor factory to use if converting this asset to an actor */
	TWeakObjectPtr< UActorFactory > ActorFactory;

	UNREALED_API static TSharedRef<FAssetDragDropOp> New(const FAssetData& InAssetData, UActorFactory* ActorFactory = NULL);

	UNREALED_API static TSharedRef<FAssetDragDropOp> New(const TArray<FAssetData>& InAssetData, UActorFactory* ActorFactory = NULL);

public:
	UNREALED_API virtual ~FAssetDragDropOp();

	UNREALED_API virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;

	UNREALED_API void Init();

	UNREALED_API EVisibility GetTooltipVisibility() const;

private:
	int32 ThumbnailSize;
};

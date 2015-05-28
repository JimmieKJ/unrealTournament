// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "AssetDragDropOp.h"

TSharedRef<FAssetDragDropOp> FAssetDragDropOp::New(const FAssetData& InAssetData, UActorFactory* ActorFactory /*= NULL*/)
{
	TArray<FAssetData> AssetDataArray;
	AssetDataArray.Add(InAssetData);
	return New(AssetDataArray, ActorFactory);
}

TSharedRef<FAssetDragDropOp> FAssetDragDropOp::New(const TArray<FAssetData>& InAssetData, UActorFactory* ActorFactory /*= NULL*/)
{
	TSharedRef<FAssetDragDropOp> Operation = MakeShareable(new FAssetDragDropOp);

	Operation->MouseCursor = EMouseCursor::GrabHandClosed;

	Operation->ThumbnailSize = 64;

	Operation->AssetData = InAssetData;
	Operation->ActorFactory = ActorFactory;

	Operation->Init();

	Operation->Construct();
	return Operation;
}

FAssetDragDropOp::~FAssetDragDropOp()
{
	if ( ThumbnailPool.IsValid() )
	{
		// Release all rendering resources being held onto
		ThumbnailPool->ReleaseResources();
	}
}

TSharedPtr<SWidget> FAssetDragDropOp::GetDefaultDecorator() const
{
	TSharedPtr<SWidget> ThumbnailWidget;

	if ( AssetThumbnail.IsValid() )
	{
		ThumbnailWidget = AssetThumbnail->MakeThumbnailWidget();
	}
	else
	{
		ThumbnailWidget = SNew(SImage) .Image( FEditorStyle::GetDefaultBrush() );
	}
		
	const FSlateBrush* ActorTypeBrush = FEditorStyle::GetDefaultBrush();
	if ( ActorFactory.IsValid() && AssetData.Num() > 0 )
	{
		AActor* DefaultActor = ActorFactory->GetDefaultActor( AssetData[0] );
		ActorTypeBrush = FClassIconFinder::FindIconForActor( DefaultActor );
	}

	return 
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ContentBrowser.AssetDragDropTooltipBackground"))
		.Content()
		[
			SNew(SHorizontalBox)

			// Left slot is asset thumbnail
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			[
				SNew(SBox) 
				.WidthOverride(ThumbnailSize) 
				.HeightOverride(ThumbnailSize)
				.Content()
				[
					SNew(SOverlay)

					+SOverlay::Slot()
					[
						ThumbnailWidget.ToSharedRef()
					]

					+SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Top)
					.Padding(FMargin(0, 4, 0, 0))
					[
						SNew(SBorder)
						.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
						.Visibility(AssetData.Num() > 1 ? EVisibility::Visible : EVisibility::Collapsed)
						.Content()
						[
							SNew(STextBlock) .Text(FText::AsNumber(AssetData.Num()))
						]
					]

					+SOverlay::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Bottom)
					.Padding(FMargin(4, 4))
					[
						SNew(SImage)
						.Image( ActorTypeBrush )
						.Visibility( (ActorTypeBrush != FEditorStyle::GetDefaultBrush()) ? EVisibility::Visible : EVisibility::Collapsed)
					]
				]
			]

			// Right slot is for optional tooltip
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.Visibility(this, &FAssetDragDropOp::GetTooltipVisibility)
				.Content()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(3.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SImage) 
						.Image(this, &FAssetDragDropOp::GetIcon)
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0,0,3,0)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock) 
						.Text(this, &FAssetDragDropOp::GetHoverText)
					]
				]
			]
		];
}

void FAssetDragDropOp::Init()
{
	if ( AssetData.Num() > 0 && ThumbnailSize > 0 )
	{
		// Load all assets first so that there is no loading going on while attempting to drag
		// Can cause unsafe frame reentry 
		for( FAssetData& Data : AssetData )
		{
			Data.GetAsset();
		}

		// Create a thumbnail pool to hold the single thumbnail rendered
		ThumbnailPool = MakeShareable( new FAssetThumbnailPool(1, /*InAreRealTileThumbnailsAllowed=*/false) );

		// Create the thumbnail handle
		AssetThumbnail = MakeShareable( new FAssetThumbnail( AssetData[0], ThumbnailSize, ThumbnailSize, ThumbnailPool ) );

		// Request the texture then tick the pool once to render the thumbnail
		AssetThumbnail->GetViewportRenderTargetTexture();
		ThumbnailPool->Tick(0);
	}
}

EVisibility FAssetDragDropOp::GetTooltipVisibility() const
{
	if ( !CurrentHoverText.IsEmpty() )
	{
		return EVisibility::Visible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

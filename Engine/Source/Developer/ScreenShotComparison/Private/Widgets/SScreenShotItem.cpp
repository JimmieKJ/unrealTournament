// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SScreenShotItem.cpp: Implements the SScreenShotItem class.
=============================================================================*/

#include "ScreenShotComparisonPrivatePCH.h"

void SScreenShotItem::Construct( const FArguments& InArgs )
{
	ScreenShotData = InArgs._ScreenShotData;

	CachedActualImageSize = FIntPoint::NoneValue;

	LoadBrush();
	// Create the screen shot data widget.
	ChildSlot
	[
		SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding( 8.0f, 8.0f )
		[
			SNew( SBorder )
			.BorderImage( FEditorStyle::GetBrush( "ToolBar.Background" ) )
			[
				SNew( SVerticalBox )
				+SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Padding( 4.0f, 4.0f )
				[
					SNew( STextBlock ) .Text( FText::FromString(ScreenShotData->GetName()) )
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Padding( 4.0f, 4.0f )
				[
					// The image to use. This needs to be a dynamic brush at some point.
					SNew( SImage )
					.Image( DynamicBrush.Get() )
					.OnMouseButtonDown(this,&SScreenShotItem::OnImageClicked)
				]
			]
		]
	]; 
}

FIntPoint SScreenShotItem::GetActualImageSize()
{
	if( CachedActualImageSize == FIntPoint::NoneValue )
	{
		//Find the image size by reading in the image on disk.  
		TArray<uint8> RawFileData;
		if( FFileHelper::LoadFileToArray( RawFileData, *DynamicBrush->GetResourceName().ToString() ) )
		{
			IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );
			IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::PNG );

			if ( ImageWrapper.IsValid() && ImageWrapper->SetCompressed( RawFileData.GetData(), RawFileData.Num() ) )
			{			
				const TArray<uint8>* RawData = NULL;
				if (ImageWrapper->GetRaw( ERGBFormat::BGRA, 8, RawData))
				{
					CachedActualImageSize.X = ImageWrapper->GetWidth();
					CachedActualImageSize.Y = ImageWrapper->GetHeight();
				}
			}
		}
	}

	return CachedActualImageSize;
}

FReply SScreenShotItem::OnImageClicked(const FGeometry& InGeometry, const FPointerEvent& InEvent)
{
	const FIntPoint ImageSize = GetActualImageSize();

	if( ImageSize.X > 256 || ImageSize.Y > 128 )
	{
		FString AssetFilename = ScreenShotData->GetAssetName();

		TSharedRef<SScreenShotImagePopup> PopupImage =
			SNew(SScreenShotImagePopup)
			.ImageAssetName(FName(*AssetFilename))
			.ImageSize(ImageSize);

		auto ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared()).ToSharedRef();

		// Center ourselves in the parent window
		auto PopupWindow = SNew(SWindow)
			.Title( FText::FromString(AssetFilename))
			.IsPopupWindow(false)
			.SizingRule(ESizingRule::UserSized)
			.ClientSize(FVector2D(1024,768))
			.AutoCenter(EAutoCenter::PreferredWorkArea)
			.SupportsMaximize(true)
			.SupportsMinimize(true)
			.FocusWhenFirstShown(true)
			.ActivateWhenFirstShown(true)
			.Content()
			[
				PopupImage
			];

		FSlateApplication::Get().AddWindowAsNativeChild(PopupWindow, ParentWindow, true );
	}
	return FReply::Handled();
}

void SScreenShotItem::LoadBrush()
{
	// The asset filename
	FString AssetFilename = ScreenShotData->GetAssetName();

	// Create the dynamic brush
	DynamicBrush = MakeShareable( new FSlateDynamicImageBrush( *AssetFilename, FVector2D(256,128) ) );
}

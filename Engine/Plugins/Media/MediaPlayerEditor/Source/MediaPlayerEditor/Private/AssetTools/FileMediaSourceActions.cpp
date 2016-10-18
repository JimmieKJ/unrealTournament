// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPCH.h"
#include "FileMediaSourceActions.h"


#define LOCTEXT_NAMESPACE "AssetTypeActions"


/* FFileMediaSourceActions constructors
 *****************************************************************************/

FFileMediaSourceActions::FFileMediaSourceActions(const TSharedRef<ISlateStyle>& InStyle)
	: Style(InStyle)
{ }


/* FAssetTypeActions_Base interface
 *****************************************************************************/

FText FFileMediaSourceActions::GetName() const
{
	return LOCTEXT("AssetTypeActions_FileMediaSource", "File Media Source");
}


UClass* FFileMediaSourceActions::GetSupportedClass() const
{
	return UFileMediaSource::StaticClass();
}


TSharedPtr<class SWidget> FFileMediaSourceActions::GetThumbnailOverlay(const FAssetData& AssetData) const
{
	TWeakObjectPtr<UFileMediaSource> FileMediaSource = Cast<UFileMediaSource>(AssetData.GetAsset());

	if (!FileMediaSource.IsValid())
	{
		return nullptr;
	}

	return SNew(SBorder)
		.BorderImage(FEditorStyle::GetNoBrush())
		.Visibility_Lambda([=]() -> EVisibility {
			return (FileMediaSource.IsValid() && FileMediaSource->PrecacheFile)
				? EVisibility::HitTestInvisible
				: EVisibility::Hidden;
		})
		.Padding(FMargin(0.0f, 0.0f, 2.0f, 7.0f))
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		[
			SNew(SImage)
				.Image(Style->GetBrush("MediaPlayerEditor.FileMediaSourcePrecached"))
		];
}


#undef LOCTEXT_NAMESPACE

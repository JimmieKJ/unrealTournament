// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPCH.h"
#include "MediaTextureCustomization.h"


#define LOCTEXT_NAMESPACE "FMediaTextureCustomization"


/* IDetailCustomization interface
 *****************************************************************************/

void FMediaTextureCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{/*
	DetailBuilder.GetObjectsBeingCustomized(CustomizedMediaTextures);
	MediaPlayerProperty = DetailBuilder.GetProperty("MediaPlayer");
	VideoTrackIndexProperty = DetailBuilder.GetProperty("VideoTrackIndex");

	// customize MediaPlayer category
	IDetailCategoryBuilder& MediaPlayerCategory = DetailBuilder.EditCategory(TEXT("MediaPlayer"));
	{
		// video track index
		IDetailPropertyRow& VideoTrackIndexRow = MediaPlayerCategory.AddProperty(VideoTrackIndexProperty);

		VideoTrackIndexRow.DisplayName(LOCTEXT("VideoTrack", "Video Track"));
		VideoTrackIndexRow.CustomWidget()
			.NameContent()
			[
				VideoTrackIndexProperty->CreatePropertyNameWidget()
			]
			.ValueContent()
			.MaxDesiredWidth(0.0f)
			[
				SNew(SComboButton)
					.OnGetMenuContent(this, &FMediaTextureCustomization::HandleVideoTrackComboButtonMenuContent)
 					.ContentPadding(FMargin( 2.0f, 2.0f ))
					.ButtonContent()
					[
						SNew(STextBlock) 
							.Text(this, &FMediaTextureCustomization::HandleVideoTrackComboButtonText)
							.Font(IDetailLayoutBuilder::GetDetailFont())
					]
			];
	}*/
}


/* FMediaTextureCustomization callbacks
 *****************************************************************************/

TSharedRef<SWidget> FMediaTextureCustomization::HandleVideoTrackComboButtonMenuContent() const
{/*
	// get assigned media player asset
	UObject* MediaPlayerObj = nullptr;
	FPropertyAccess::Result Result = MediaPlayerProperty->GetValue(MediaPlayerObj);

	if (Result == FPropertyAccess::MultipleValues)
	{
		return SNullWidget::NullWidget;
	}

	UMediaPlayer* MediaPlayer = Cast<UMediaPlayer>(MediaPlayerObj);

	if (MediaPlayer == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	// get media tracks
	TSharedPtr<IMediaPlayer> Player = MediaPlayer->GetPlayer();

	if (!Player.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	const TArray<IMediaVideoTrackRef>& VideoTracks = Player->GetVideoTracks();

	// populate the menu
	FMenuBuilder MenuBuilder(true, nullptr);

	for (int32 VideoTrackIndex = 0; VideoTrackIndex < VideoTracks.Num(); ++VideoTrackIndex)
	{
		FUIAction Action(FExecuteAction::CreateSP(this, &FMediaTextureCustomization::HandleVideoTrackComboButtonMenuEntryExecute, VideoTrackIndex));

		MenuBuilder.AddMenuEntry(
			VideoTracks[VideoTrackIndex]->GetStream().GetDisplayName(),
			TAttribute<FText>(),
			FSlateIcon(),
			Action
		);
	}

	return MenuBuilder.MakeWidget();*/
	return SNullWidget::NullWidget;
}


void FMediaTextureCustomization::HandleVideoTrackComboButtonMenuEntryExecute(int32 TrackIndex)
{
	VideoTrackIndexProperty->SetValue(TrackIndex);
}


FText FMediaTextureCustomization::HandleVideoTrackComboButtonText() const
{/*
	// get assigned media player asset
	UObject* MediaPlayerObj = nullptr;
	FPropertyAccess::Result Result = MediaPlayerProperty->GetValue(MediaPlayerObj);

	if (Result != FPropertyAccess::Success)
	{
		return FText::GetEmpty();
	}

	UMediaPlayer* MediaPlayer = Cast<UMediaPlayer>(MediaPlayerObj);

	if (MediaPlayer == nullptr)
	{
		return LOCTEXT("NoMediaPlayerSelected", "No Media Asset selected");
	}

	// get selected value
	int32 VideoTrackIndex = -1;
	Result = VideoTrackIndexProperty->GetValue(VideoTrackIndex);

	if (Result != FPropertyAccess::Success)
	{
		return FText::GetEmpty();
	}

	// get selected media track
	TSharedPtr<IMediaPlayer> Player = MediaPlayer->GetPlayer();

	if (!Player.IsValid())
	{
		return LOCTEXT("NoMediaLoaded", "No media loaded");
	}

	auto VideoTracks = Player->GetVideoTracks();

	if (!VideoTracks.IsValidIndex(VideoTrackIndex))
	{
		return LOCTEXT("SelectVideoTrack", "Select a Video Track...");
	}

	// get track name
	return VideoTracks[VideoTrackIndex]->GetStream().GetDisplayName();*/
	return FText::GetEmpty();
}


#undef LOCTEXT_NAMESPACE

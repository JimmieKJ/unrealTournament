// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "IMediaPlayer.h"
#include "IMediaTrack.h"
#include "MediaPlayer.h"
#include "MediaTextureCustomization.h"


#define LOCTEXT_NAMESPACE "FMediaTextureCustomization"


/* IDetailCustomization interface
 *****************************************************************************/

void FMediaTextureCustomization::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
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
	}
}


/* FMediaTextureCustomization callbacks
 *****************************************************************************/

TSharedRef<SWidget> FMediaTextureCustomization::HandleVideoTrackComboButtonMenuContent() const
{
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
	IMediaPlayerPtr Player = MediaPlayer->GetPlayer();

	if (!Player.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	const TArray<IMediaTrackRef>& MediaTracks = Player->GetTracks();

	// populate the menu
	FMenuBuilder MenuBuilder(true, nullptr);

	for (const IMediaTrackRef& Track : MediaTracks)
	{
		if (Track->GetType() == EMediaTrackTypes::Video)
		{
			FUIAction Action(FExecuteAction::CreateSP(this, &FMediaTextureCustomization::HandleVideoTrackComboButtonMenuEntryExecute, Track->GetIndex()));

			MenuBuilder.AddMenuEntry(
				Track->GetDisplayName(),
				TAttribute<FText>(),
				FSlateIcon(),
				Action
			);
		}
	}

	return MenuBuilder.MakeWidget();
}


void FMediaTextureCustomization::HandleVideoTrackComboButtonMenuEntryExecute( uint32 TrackIndex )
{
	VideoTrackIndexProperty->SetValue((int32)TrackIndex);
}


FText FMediaTextureCustomization::HandleVideoTrackComboButtonText() const
{
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
	IMediaPlayerPtr Player = MediaPlayer->GetPlayer();

	if (!Player.IsValid())
	{
		return LOCTEXT("NoMediaLoaded", "No media loaded");
	}

	IMediaTrackPtr Track = Player->GetTrack(VideoTrackIndex, EMediaTrackTypes::Video);

	// generate track name string
	if (Track.IsValid())
	{
		return Track->GetDisplayName();
	}

	return LOCTEXT("SelectVideoTrack", "Select a Video Track...");
}


#undef LOCTEXT_NAMESPACE

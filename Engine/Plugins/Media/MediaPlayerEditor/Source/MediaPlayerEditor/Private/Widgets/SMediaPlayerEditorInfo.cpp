// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPCH.h"
#include "SMediaPlayerEditorInfo.h"


#define LOCTEXT_NAMESPACE "SMediaPlayerEditorInfo"


/* SMediaPlayerEditorInfo interface
 *****************************************************************************/

void SMediaPlayerEditorInfo::Construct(const FArguments& InArgs, UMediaPlayer& InMediaPlayer, const TSharedRef<ISlateStyle>& InStyle)
{
	MediaPlayer = &InMediaPlayer;

	ChildSlot
	[
		SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(4.0)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SAssignNew(MediaInfoBox, SMultiLineEditableTextBox)
							.HintText(LOCTEXT("NoMediaOpened", "No media opened"))
							.IsReadOnly(true)
					]
			]
	];

	MediaPlayer->OnMediaEvent().AddSP(this, &SMediaPlayerEditorInfo::HandleMediaPlayerMediaEvent);
}


/* SMediaPlayerEditorInfo callbacks
 *****************************************************************************/

void SMediaPlayerEditorInfo::HandleMediaPlayerMediaEvent(EMediaEvent Event)
{
	if ((Event == EMediaEvent::MediaOpened) || (Event == EMediaEvent::MediaOpenFailed) || (Event == EMediaEvent::TracksChanged))
	{
		TSharedPtr<IMediaPlayer> Player = MediaPlayer->GetPlayer();

		if (Player.IsValid())
		{
			TRange<float> ThinnedForwardRates = Player->GetControls().GetSupportedRates(EMediaPlaybackDirections::Forward, false);
			TRange<float> UnthinnedForwardRates = Player->GetControls().GetSupportedRates(EMediaPlaybackDirections::Forward, true);
			TRange<float> ThinnedReverseRates = Player->GetControls().GetSupportedRates(EMediaPlaybackDirections::Reverse, false);
			TRange<float> UnthinnedReverseRates = Player->GetControls().GetSupportedRates(EMediaPlaybackDirections::Reverse, true);

			FFormatNamedArguments Arguments;
			{
				Arguments.Add(TEXT("PlayerName"), FText::FromName(MediaPlayer->GetPlayerName()));
				Arguments.Add(TEXT("SupportsScrubbing"), MediaPlayer->SupportsScrubbing() ? GYes : GNo);
				Arguments.Add(TEXT("SupportsSeeking"), MediaPlayer->SupportsSeeking() ? GYes : GNo);
				Arguments.Add(TEXT("PlayerInfo"), FText::FromString(Player->GetInfo()));

				Arguments.Add(TEXT("ForwardRateThinned"), ThinnedForwardRates.IsEmpty()
					? LOCTEXT("RateNotSupported", "Not supported")
					: FText::Format(LOCTEXT("RatesFormat", "{0} to {1}"), FText::AsNumber(ThinnedForwardRates.GetLowerBoundValue()), FText::AsNumber(ThinnedForwardRates.GetUpperBoundValue()))
				);

				Arguments.Add(TEXT("ForwardRateUnthinned"), UnthinnedForwardRates.IsEmpty()
					? LOCTEXT("RateNotSupported", "Not supported")
					: FText::Format(LOCTEXT("RatesFormat", "{0} to {1}"), FText::AsNumber(UnthinnedForwardRates.GetLowerBoundValue()), FText::AsNumber(UnthinnedForwardRates.GetUpperBoundValue()))
				);

				Arguments.Add(TEXT("ReverseRateThinned"), ThinnedReverseRates.IsEmpty()
					? LOCTEXT("RateNotSupported", "Not supported")
					: FText::Format(LOCTEXT("RatesFormat", "{0} to {1}"), FText::AsNumber(ThinnedReverseRates.GetLowerBoundValue()), FText::AsNumber(ThinnedReverseRates.GetUpperBoundValue()))
				);

				Arguments.Add(TEXT("ReverseRateUnthinned"), UnthinnedReverseRates.IsEmpty()
					? LOCTEXT("RateNotSupported", "Not supported")
					: FText::Format(LOCTEXT("RatesFormat", "{0} to {1}"), FText::AsNumber(UnthinnedReverseRates.GetLowerBoundValue()), FText::AsNumber(UnthinnedReverseRates.GetUpperBoundValue()))
				);				
			}

			MediaInfoBox->SetText(
				FText::Format(LOCTEXT("InfoFormat",
					"Player: {PlayerName}\n"
					"\n"
					"Forward Rates\n"
					"    Thinned: {ForwardRateThinned}\n"
					"    Unthinned: {ForwardRateUnthinned}\n"
					"\n"
					"Reverse Rates\n"
					"    Thinned: {ReverseRateThinned}\n"
					"    Unthinned: {ReverseRateUnthinned}\n"
					"\n"
					"Capabilities\n"
					"    Scrubbing: {SupportsScrubbing}\n"
					"    Seeking: {SupportsSeeking}\n"
					"\n"
					"{PlayerInfo}"),
					Arguments
				)
			);
		}
		else
		{
			MediaInfoBox->SetText(FText::GetEmpty());
		}
	}
	else if (Event == EMediaEvent::MediaClosed)
	{
		MediaInfoBox->SetText(FText::GetEmpty());
	}
}


#undef LOCTEXT_NAMESPACE

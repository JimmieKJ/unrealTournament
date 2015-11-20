// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPrivatePCH.h"
#include "IMediaPlayer.h"
#include "SNumericEntryBox.h"


#define LOCTEXT_NAMESPACE "SMediaPlayerEditorViewer"


/* SMediaPlayerEditorPlayer structors
 *****************************************************************************/

SMediaPlayerEditorViewer::SMediaPlayerEditorViewer()
	: CaptionBuffer(MakeShareable(new FMediaSampleBuffer))
	, MediaPlayer(nullptr)
{ }


SMediaPlayerEditorViewer::~SMediaPlayerEditorViewer()
{
	if (MediaPlayer != nullptr)
	{
		MediaPlayer->OnMediaChanged().RemoveAll(this);
	}
}


/* SMediaPlayerEditorPlayer interface
 *****************************************************************************/

void SMediaPlayerEditorViewer::Construct(const FArguments& InArgs, UMediaPlayer* InMediaPlayer, const TSharedRef<ISlateStyle>& InStyle)
{
	MediaPlayer = InMediaPlayer;
	Viewport = MakeShareable(new FMediaPlayerEditorViewport());

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f, 0.0f)
			[
				SNew(SOverlay)

				+ SOverlay::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
							.Visibility(this, &SMediaPlayerEditorViewer::HandleNoMediaSelectedTextVisibility)

						+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SImage)
									.Image(FCoreStyle::Get().GetBrush("Icons.Error"))
							]

						+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(4.0f, 0.0f, 0.0f, 0.0f)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
									.Text(LOCTEXT("NoMediaSelectedText", "Please pick a media source in the Details panel!"))
							]
					]

				+ SOverlay::Slot()
					[
						SNew(SHorizontalBox)
							.Visibility(this, &SMediaPlayerEditorViewer::HandleTrackSelectionBoxVisibility)

						+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								// video track selector
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
											.Text(LOCTEXT("VideoTrackLabel", "Video Track:"))
									]

								+ SHorizontalBox::Slot()
									.FillWidth(1.0f)
									.Padding(4.0f, 0.0f)
									.VAlign(VAlign_Center)
									[
										SAssignNew(VideoTrackComboBox, SComboBox<IMediaVideoTrackPtr>)
											.OnGenerateWidget(this, &SMediaPlayerEditorViewer::HandleVideoTrackComboBoxGenerateWidget)
											.OnSelectionChanged(this, &SMediaPlayerEditorViewer::HandleVideoTrackComboBoxSelectionChanged)
											.OptionsSource(&VideoTracks)
											.ToolTipText(LOCTEXT("CaptionTrackToolTip", "Select the video track to be played"))
											[
												SNew(STextBlock)
													.Text(this, &SMediaPlayerEditorViewer::HandleVideoTrackComboBoxText)
											]
									]
							]

						+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(16.0f, 0.0f, 0.0f, 0.0f)
							[
								// audio track selector
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
											.Text(LOCTEXT("AudioTrackLabel", "Audio Track:"))
									]

								+ SHorizontalBox::Slot()
									.FillWidth(1.0f)
									.Padding(4.0f, 0.0f, 0.0f, 0.0f)
									.VAlign(VAlign_Center)
									[
										SAssignNew(AudioTrackComboBox, SComboBox<IMediaAudioTrackPtr>)
											.OnGenerateWidget(this, &SMediaPlayerEditorViewer::HandleAudioTrackComboBoxGenerateWidget)
											.OnSelectionChanged(this, &SMediaPlayerEditorViewer::HandleAudioTrackComboBoxSelectionChanged)
											.OptionsSource(&AudioTracks)
											.ToolTipText(LOCTEXT("CaptionTrackToolTip", "Select the audio track to be played"))
											[
												SNew(STextBlock)
													.Text(this, &SMediaPlayerEditorViewer::HandleAudioTrackComboBoxText)
											]
									]
							]

						+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(16.0f, 0.0f, 0.0f, 0.0f)
							[
								// caption track selector
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
											.Text(LOCTEXT("CaptionTrackLabel", "Caption Track:"))
									]

								+ SHorizontalBox::Slot()
									.FillWidth(1.0f)
									.Padding(4.0f, 0.0f, 0.0f, 0.0f)
									.VAlign(VAlign_Center)
									[
										SAssignNew(CaptionTrackComboBox, SComboBox<IMediaCaptionTrackPtr>)
											.OnGenerateWidget(this, &SMediaPlayerEditorViewer::HandleCaptionTrackComboBoxGenerateWidget)
											.OnSelectionChanged(this, &SMediaPlayerEditorViewer::HandleCaptionTrackComboBoxSelectionChanged)
											.OptionsSource(&CaptionTracks)
											.ToolTipText(LOCTEXT("CaptionTrackToolTip", "Select the caption text to be displayed"))
											[
												SNew(STextBlock)
													.Text(this, &SMediaPlayerEditorViewer::HandleCaptionTrackComboBoxText)
											]
									]
							]

						+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.HAlign(HAlign_Right)
							.Padding(16.0f, 0.0f, 0.0f, 0.0f)
							[
								// playback rate spin box
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
											.Text(LOCTEXT("PlaybackRateLabel", "Playback Rate:"))
									]

								+ SHorizontalBox::Slot()
									.FillWidth(1.0f)
									.Padding(4.0f, 0.0f, 0.0f, 0.0f)
									.VAlign(VAlign_Center)
									[
										SNew(SNumericEntryBox<float>)
											.Delta(1.0f)
											.MaxValue(this, &SMediaPlayerEditorViewer::HandlePlaybackRateBoxMaxValue)
											.MinValue(this, &SMediaPlayerEditorViewer::HandlePlaybackRateBoxMinValue)
											.Value(this, &SMediaPlayerEditorViewer::HandlePlaybackRateSpinBoxValue)
											.OnValueChanged(this, &SMediaPlayerEditorViewer::HandlePlaybackRateBoxValueChanged)
									]
							]
					]
			]

		+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.0f, 4.0f, 0.0f, 8.0f)
			[
				SNew(SOverlay)

				+ SOverlay::Slot()
					[
						// movie viewport
						SNew(SViewport)
							.EnableGammaCorrection(false)
							.ViewportInterface(Viewport)
					]

				+ SOverlay::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Top)
					.Padding(FMargin(12.0f, 8.0f))
					[
						// playback state
						SNew(STextBlock)
							.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 20))
							.ShadowOffset(FVector2D(1.f, 1.f))
							.Text(this, &SMediaPlayerEditorViewer::HandleOverlayStateText)
					]

				+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Bottom)
					.Padding(16.0f)
					[
						// caption text
						SNew(STextBlock)
							.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 16))
							.ShadowOffset(FVector2D(1.f, 1.f))
							.Text(this, &SMediaPlayerEditorViewer::HandleOverlayCaptionText)
							.WrapTextAt(600.0f)
					]
			]

		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)

				// elapsed time
				+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
							.Text(this, &SMediaPlayerEditorViewer::HandleElapsedTimeTextBlockText)
							.ToolTipText(LOCTEXT("ElapsedTimeTooltip", "Elapsed Time"))
					]

				// scrubber
				+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(8.0f, 0.0f)
					[
						SAssignNew(ScrubberSlider, SSlider)
							.IsEnabled(this, &SMediaPlayerEditorViewer::HandlePositionSliderIsEnabled)
							.OnMouseCaptureBegin(this, &SMediaPlayerEditorViewer::HandlePositionSliderMouseCaptureBegin)
							.OnMouseCaptureEnd(this, &SMediaPlayerEditorViewer::HandlePositionSliderMouseCaptureEnd)
							.OnValueChanged(this, &SMediaPlayerEditorViewer::HandlePositionSliderValueChanged)
							.Orientation(Orient_Horizontal)
							.Value(this, &SMediaPlayerEditorViewer::HandlePositionSliderValue)
					]

				// remaining time
				+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
							.Text(this, &SMediaPlayerEditorViewer::HandleRemainingTimeTextBlockText)
							.ToolTipText(LOCTEXT("RemainingTimeTooltip", "Remaining Time"))
					]
			]
	];

	MediaPlayer->OnMediaChanged().AddRaw(this, &SMediaPlayerEditorViewer::HandleMediaPlayerMediaChanged);
	MediaPlayer->OnTracksChanged().AddRaw(this, &SMediaPlayerEditorViewer::HandleMediaPlayerTracksChanged);

	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SMediaPlayerEditorViewer::HandleActiveTimer));
	ReloadTracks();
}


/* SMediaPlayerEditorPlayer implementation
 *****************************************************************************/

void SMediaPlayerEditorViewer::ReloadTracks()
{
	// clear track collections
	IMediaCaptionTrackPtr SelectedCaptionTrack = CaptionTrackComboBox->GetSelectedItem();

	if (SelectedCaptionTrack.IsValid())
	{
		SelectedCaptionTrack->GetStream().RemoveSink(CaptionBuffer);
	}

	AudioTracks.Reset();
	CaptionTracks.Reset();
	VideoTracks.Reset();

	// get track collections
	if (MediaPlayer != nullptr)
	{
		IMediaPlayerPtr Player = MediaPlayer->GetPlayer();

		if (Player.IsValid())
		{
			for (auto AudioTrack : Player->GetAudioTracks())
			{
				AudioTracks.Add(AudioTrack);
			}

			for (auto CaptionTrack : Player->GetCaptionTracks())
			{
				CaptionTracks.Add(CaptionTrack);
			}

			for (auto VideoTrack : Player->GetVideoTracks())
			{
				VideoTracks.Add(VideoTrack);
			}
		}
	}

	// refresh combo box selections
	if (!AudioTrackComboBox->GetSelectedItem().IsValid() || !AudioTracks.Contains(AudioTrackComboBox->GetSelectedItem()))
	{
		if (AudioTracks.Num() > 0)
		{
			AudioTrackComboBox->SetSelectedItem(AudioTracks[0]);
		}
		else
		{
			AudioTrackComboBox->SetSelectedItem(nullptr);
		}		
	}

	if (!CaptionTrackComboBox->GetSelectedItem().IsValid() || !CaptionTracks.Contains(CaptionTrackComboBox->GetSelectedItem()))
	{
		if (CaptionTracks.Num() > 0)
		{
			CaptionTrackComboBox->SetSelectedItem(CaptionTracks[0]);
			CaptionTracks[0]->GetStream().AddSink(CaptionBuffer);
		}
		else
		{
			CaptionTrackComboBox->SetSelectedItem(nullptr);
		}		
	}

	if (!VideoTrackComboBox->GetSelectedItem().IsValid() || !VideoTracks.Contains(VideoTrackComboBox->GetSelectedItem()))
	{
		if (VideoTracks.Num() > 0)
		{
			VideoTrackComboBox->SetSelectedItem(VideoTracks[0]);
		}
		else
		{
			VideoTrackComboBox->SetSelectedItem(nullptr);
		}		
	}

	AudioTrackComboBox->RefreshOptions();
	CaptionTrackComboBox->RefreshOptions();
	VideoTrackComboBox->RefreshOptions();

	// refresh viewport
	Viewport->Initialize(VideoTrackComboBox->GetSelectedItem());
}


/* SMediaPlayerEditorPlayer callbacks
 *****************************************************************************/

EActiveTimerReturnType SMediaPlayerEditorViewer::HandleActiveTimer(double InCurrentTime, float InDeltaTime)
{
	return EActiveTimerReturnType::Continue;
}


TSharedRef<SWidget> SMediaPlayerEditorViewer::HandleAudioTrackComboBoxGenerateWidget(IMediaAudioTrackPtr Value) const
{
	return SNew(STextBlock)
		.Text(Value->GetStream().GetDisplayName());
}


void SMediaPlayerEditorViewer::HandleAudioTrackComboBoxSelectionChanged(IMediaAudioTrackPtr Selection, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo != ESelectInfo::Direct)
	{
		ReloadTracks();
	}
}


FText SMediaPlayerEditorViewer::HandleAudioTrackComboBoxText() const
{
	IMediaAudioTrackPtr AudioTrack = AudioTrackComboBox->GetSelectedItem();

	if (AudioTrack.IsValid())
	{
		return AudioTrack->GetStream().GetDisplayName();
	}

	if (AudioTracks.Num() == 0)
	{
		return LOCTEXT("NoAudioTracksAvailable", "No audio tracks available");
	}

	return LOCTEXT("SelectAudioTrack", "Select an audio track...");
}


TSharedRef<SWidget> SMediaPlayerEditorViewer::HandleCaptionTrackComboBoxGenerateWidget(IMediaCaptionTrackPtr Value) const
{
	return SNew(STextBlock)
		.Text(Value->GetStream().GetDisplayName());
}


void SMediaPlayerEditorViewer::HandleCaptionTrackComboBoxSelectionChanged(IMediaCaptionTrackPtr Selection, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo != ESelectInfo::Direct)
	{
		ReloadTracks();
	}
}


FText SMediaPlayerEditorViewer::HandleCaptionTrackComboBoxText() const
{
	IMediaCaptionTrackPtr CaptionTrack = CaptionTrackComboBox->GetSelectedItem();

	if (CaptionTrack.IsValid())
	{
		return CaptionTrack->GetStream().GetDisplayName();
	}

	if (CaptionTracks.Num() == 0)
	{
		return LOCTEXT("NoCaptionTracksAvailable", "No caption tracks available");
	}

	return LOCTEXT("SelectCaptionTrack", "Select a caption track...");
}


FText SMediaPlayerEditorViewer::HandleElapsedTimeTextBlockText() const
{
	return FText::AsTimespan(MediaPlayer->GetTime());
}


void SMediaPlayerEditorViewer::HandleMediaPlayerMediaChanged()
{
	ReloadTracks();
}


void SMediaPlayerEditorViewer::HandleMediaPlayerTracksChanged()
{
	ReloadTracks();
}


EVisibility SMediaPlayerEditorViewer::HandleNoMediaSelectedTextVisibility() const
{
	return MediaPlayer->GetUrl().IsEmpty() ? EVisibility::Visible : EVisibility::Hidden;
}


FText SMediaPlayerEditorViewer::HandleOverlayCaptionText() const
{
	TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> CurrentCaption = CaptionBuffer->GetCurrentSample();

	if (!CurrentCaption.IsValid())
	{
		return FText::GetEmpty();
	}

	return FText::FromString(FString((TCHAR*)CurrentCaption->GetData()));
}


FText SMediaPlayerEditorViewer::HandleOverlayStateText() const
{
	if (MediaPlayer->GetUrl().IsEmpty())
	{
		return LOCTEXT("StateOverlayNoMedia", "No Media");
	}

	if (MediaPlayer->IsPaused())
	{
		return LOCTEXT("StateOverlayPaused", "Paused");
	}

	if (MediaPlayer->IsStopped())
	{
		return LOCTEXT("StateOverlayStopped", "Stopped");
	}

	float Rate = MediaPlayer->GetRate();

	if (FMath::IsNearlyZero(Rate) || (Rate == 1.0f))
	{
		return FText::GetEmpty();
	}

	if (Rate < 0.0f)
	{
		return FText::Format(LOCTEXT("StateOverlayReverseFormat", "Reverse ({0}x)"), FText::AsNumber(-Rate));
	}

	return FText::Format(LOCTEXT("StateOverlayForwardFormat", "Forward ({0}x)"), FText::AsNumber(Rate));
}


TOptional<float> SMediaPlayerEditorViewer::HandlePlaybackRateBoxMaxValue() const
{
	IMediaPlayerPtr Player = MediaPlayer->GetPlayer();

	if (Player.IsValid())
	{
		return Player->GetMediaInfo().GetSupportedRates(EMediaPlaybackDirections::Forward, false).GetUpperBoundValue();
	}

	return 0.0f;
}


TOptional<float> SMediaPlayerEditorViewer::HandlePlaybackRateBoxMinValue() const
{
	IMediaPlayerPtr Player = MediaPlayer->GetPlayer();

	if (Player.IsValid())
	{
		return Player->GetMediaInfo().GetSupportedRates(EMediaPlaybackDirections::Reverse, false).GetLowerBoundValue();
	}

	return 0.0f;
}


TOptional<float> SMediaPlayerEditorViewer::HandlePlaybackRateSpinBoxValue() const
{
	return MediaPlayer->GetRate();
}


void SMediaPlayerEditorViewer::HandlePlaybackRateBoxValueChanged( float NewValue )
{
	MediaPlayer->SetRate(NewValue);
}


bool SMediaPlayerEditorViewer::HandlePositionSliderIsEnabled() const
{
	return MediaPlayer->SupportsSeeking();
}


void SMediaPlayerEditorViewer::HandlePositionSliderMouseCaptureBegin()
{
	if (MediaPlayer->SupportsScrubbing())
	{
		PreScrubRate = MediaPlayer->GetRate();
		MediaPlayer->SetRate(0.0f);
	}
}


void SMediaPlayerEditorViewer::HandlePositionSliderMouseCaptureEnd()
{
	if (MediaPlayer->SupportsScrubbing())
	{
		MediaPlayer->SetRate(PreScrubRate);
	}
}


float SMediaPlayerEditorViewer::HandlePositionSliderValue() const
{
	if (MediaPlayer->GetDuration() > FTimespan::Zero())
	{
		return (float)MediaPlayer->GetTime().GetTicks() / (float)MediaPlayer->GetDuration().GetTicks();
	}

	return 0.0;
}


void SMediaPlayerEditorViewer::HandlePositionSliderValueChanged( float NewValue )
{
	if (!ScrubberSlider->HasMouseCapture() || MediaPlayer->SupportsScrubbing())
	{
		MediaPlayer->Seek(MediaPlayer->GetDuration() * NewValue);
	}
}


FText SMediaPlayerEditorViewer::HandleRemainingTimeTextBlockText() const
{
	FTimespan Remaining = MediaPlayer->GetDuration() - MediaPlayer->GetTime();

	if (Remaining <= FTimespan::Zero())
	{
		return FText::GetEmpty();
	}

	return FText::AsTimespan(Remaining);
}


EVisibility SMediaPlayerEditorViewer::HandleTrackSelectionBoxVisibility() const
{
	return MediaPlayer->GetUrl().IsEmpty() ? EVisibility::Hidden : EVisibility::Visible;
}


TSharedRef<SWidget> SMediaPlayerEditorViewer::HandleVideoTrackComboBoxGenerateWidget(IMediaVideoTrackPtr Value) const
{
	return SNew(STextBlock)
		.Text(Value->GetStream().GetDisplayName());
}


void SMediaPlayerEditorViewer::HandleVideoTrackComboBoxSelectionChanged(IMediaVideoTrackPtr Selection, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo != ESelectInfo::Direct)
	{
		ReloadTracks();
	}
}


FText SMediaPlayerEditorViewer::HandleVideoTrackComboBoxText() const
{
	IMediaVideoTrackPtr VideoTrack = VideoTrackComboBox->GetSelectedItem();

	if (VideoTrack.IsValid())
	{
		return VideoTrack->GetStream().GetDisplayName();
	}

	if (VideoTracks.Num() == 0)
	{
		return LOCTEXT("NoVideoTracksAvailable", "No video tracks available");
	}

	return LOCTEXT("SelectVideoTrack", "Select a video track...");
}


#undef LOCTEXT_NAMESPACE

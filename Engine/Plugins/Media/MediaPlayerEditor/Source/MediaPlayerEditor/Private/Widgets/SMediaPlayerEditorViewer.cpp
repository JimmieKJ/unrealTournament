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

void SMediaPlayerEditorViewer::Construct( const FArguments& InArgs, UMediaPlayer* InMediaPlayer, const TSharedRef<ISlateStyle>& InStyle )
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
										SAssignNew(VideoTrackComboBox, SComboBox<IMediaTrackPtr>)
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
										SAssignNew(AudioTrackComboBox, SComboBox<IMediaTrackPtr>)
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
										SAssignNew(CaptionTrackComboBox, SComboBox<IMediaTrackPtr>)
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
	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SMediaPlayerEditorViewer::HandleActiveTimer));
	ReloadMediaPlayer();
}


/* SMediaPlayerEditorPlayer implementation
 *****************************************************************************/

void SMediaPlayerEditorViewer::ReloadMediaPlayer()
{
	// clear track collections
	IMediaTrackPtr SelectedCaptionTrack = CaptionTrackComboBox->GetSelectedItem();

	if (SelectedCaptionTrack.IsValid())
	{
		SelectedCaptionTrack->RemoveSink(CaptionBuffer);
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
			for (IMediaTrackPtr Track : Player->GetTracks())
			{
				switch (Track->GetType())
				{
				case EMediaTrackTypes::Audio:
					AudioTracks.Add(Track);
					break;

				case EMediaTrackTypes::Caption:
					CaptionTracks.Add(Track);
					break;

				case EMediaTrackTypes::Video:
					VideoTracks.Add(Track);
					break;
				}
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
			CaptionTracks[0]->AddSink(CaptionBuffer);
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


TSharedRef<SWidget> SMediaPlayerEditorViewer::HandleAudioTrackComboBoxGenerateWidget( IMediaTrackPtr Value ) const
{
	return SNew(STextBlock)
		.Text(Value->GetDisplayName());
}


void SMediaPlayerEditorViewer::HandleAudioTrackComboBoxSelectionChanged( IMediaTrackPtr Selection, ESelectInfo::Type SelectInfo )
{
	if (SelectInfo != ESelectInfo::Direct)
	{
		ReloadMediaPlayer();
	}
}


FText SMediaPlayerEditorViewer::HandleAudioTrackComboBoxText() const
{
	IMediaTrackPtr AudioTrack = AudioTrackComboBox->GetSelectedItem();

	if (AudioTrack.IsValid())
	{
		return AudioTrack->GetDisplayName();
	}

	if (AudioTracks.Num() == 0)
	{
		return LOCTEXT("NoAudioTracksAvailable", "No audio tracks available");
	}

	return LOCTEXT("SelectAudioTrack", "Select an audio track...");
}


TSharedRef<SWidget> SMediaPlayerEditorViewer::HandleCaptionTrackComboBoxGenerateWidget( IMediaTrackPtr Value ) const
{
	return SNew(STextBlock)
		.Text(Value->GetDisplayName());
}


void SMediaPlayerEditorViewer::HandleCaptionTrackComboBoxSelectionChanged( IMediaTrackPtr Selection, ESelectInfo::Type SelectInfo )
{
	if (SelectInfo != ESelectInfo::Direct)
	{
		ReloadMediaPlayer();
	}
}


FText SMediaPlayerEditorViewer::HandleCaptionTrackComboBoxText() const
{
	IMediaTrackPtr CaptionTrack = CaptionTrackComboBox->GetSelectedItem();

	if (CaptionTrack.IsValid())
	{
		return CaptionTrack->GetDisplayName();
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
	ReloadMediaPlayer();
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
	return FText::AsTimespan(MediaPlayer->GetDuration() - MediaPlayer->GetTime());
}


EVisibility SMediaPlayerEditorViewer::HandleTrackSelectionBoxVisibility() const
{
	return MediaPlayer->GetUrl().IsEmpty() ? EVisibility::Hidden : EVisibility::Visible;
}


TSharedRef<SWidget> SMediaPlayerEditorViewer::HandleVideoTrackComboBoxGenerateWidget( IMediaTrackPtr Value ) const
{
	return SNew(STextBlock)
		.Text(Value->GetDisplayName());
}


void SMediaPlayerEditorViewer::HandleVideoTrackComboBoxSelectionChanged( IMediaTrackPtr Selection, ESelectInfo::Type SelectInfo )
{
	if (SelectInfo != ESelectInfo::Direct)
	{
		ReloadMediaPlayer();
	}
}


FText SMediaPlayerEditorViewer::HandleVideoTrackComboBoxText() const
{
	IMediaTrackPtr VideoTrack = VideoTrackComboBox->GetSelectedItem();

	if (VideoTrack.IsValid())
	{
		return VideoTrack->GetDisplayName();
	}

	if (VideoTracks.Num() == 0)
	{
		return LOCTEXT("NoVideoTracksAvailable", "No video tracks available");
	}

	return LOCTEXT("SelectVideoTrack", "Select a video track...");
}


#undef LOCTEXT_NAMESPACE

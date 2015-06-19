// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the contents of the viewer tab in the UMediaPlayer asset editor.
 */
class SMediaPlayerEditorViewer
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMediaPlayerEditorViewer) { }
	SLATE_END_ARGS()

public:

	/** Default constructor. */
	SMediaPlayerEditorViewer();

	/** Destructor. */
	~SMediaPlayerEditorViewer();

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InMediaPlayer The UMediaPlayer asset to show the details for.
	 * @param InStyleSet The style set to use.
	 */
	void Construct( const FArguments& InArgs, UMediaPlayer* InMediaPlayer, const TSharedRef<ISlateStyle>& InStyle );

protected:

	/** Reloads the media texture and updates all options. */
	void ReloadMediaPlayer();

private:

	/** Callback for the active timer. */
	EActiveTimerReturnType HandleActiveTimer(double InCurrentTime, float InDeltaTime);

	/** Callback for generating a widget for a value in the audio track combo box. */
	TSharedRef<SWidget> HandleAudioTrackComboBoxGenerateWidget( IMediaTrackPtr Value ) const;

	/** Callback for changing the selected audio track. */
	void HandleAudioTrackComboBoxSelectionChanged( IMediaTrackPtr Selection, ESelectInfo::Type SelectInfo );

	/** Callback for getting the text in the audio track combo box. */
	FText HandleAudioTrackComboBoxText() const;

	/** Callback for generating a widget for a value in the caption track combo box. */
	TSharedRef<SWidget> HandleCaptionTrackComboBoxGenerateWidget( IMediaTrackPtr Value ) const;

	/** Callback for changing the selected audio track. */
	void HandleCaptionTrackComboBoxSelectionChanged( IMediaTrackPtr Selection, ESelectInfo::Type SelectInfo );

	/** Callback for getting the text in the caption track combo box. */
	FText HandleCaptionTrackComboBoxText() const;

	/** Callback for getting the text of the 'Elapsed Time' text block. */
	FText HandleElapsedTimeTextBlockText() const;

	/** Callback for when the media player's media has changed. */
	void HandleMediaPlayerMediaChanged();

	/** Callback for getting the visibility of the 'No media selected' warning text. */
	EVisibility HandleNoMediaSelectedTextVisibility() const;

	/** Callback for getting the text of the movie captions. */
	FText HandleOverlayCaptionText() const;

	/** Callback for getting the text of the playback state overlay. */
	FText HandleOverlayStateText() const;

	/** Callback for getting the maximum value of the playback rate spin box. */
	TOptional<float> HandlePlaybackRateBoxMaxValue() const;

	/** Callback for getting the minimum value of the playback rate spin box. */
	TOptional<float> HandlePlaybackRateBoxMinValue() const;

	/** Callback for getting the current value of the playback rate spin box. */
	TOptional<float> HandlePlaybackRateSpinBoxValue() const;

	/** Callback for committing a new value to the playback rate spin box. */
	void HandlePlaybackRateBoxValueChanged( float NewValue );

	/** Callback for getting the enabled state of the position slider. */
	bool HandlePositionSliderIsEnabled() const;

	/** Callback for when the position slider captures the mouse. */
	void HandlePositionSliderMouseCaptureBegin();

	/** Callback for when the position slider releases the mouse. */
	void HandlePositionSliderMouseCaptureEnd();

	/** Callback for getting the value of the position slider. */
	float HandlePositionSliderValue() const;

	/** Callback for changing the value of the 'Position' slider. */
	void HandlePositionSliderValueChanged( float NewValue );

	/** Callback for getting the text of the 'Remaining Time' text block. */
	FText HandleRemainingTimeTextBlockText() const;

	/** Callback for getting the visibility of the track selection box. */
	EVisibility HandleTrackSelectionBoxVisibility() const;

	/** Callback for generating a widget for a value in the video track combo box. */
	TSharedRef<SWidget> HandleVideoTrackComboBoxGenerateWidget( IMediaTrackPtr Value ) const;

	/** Callback for changing the selected video track. */
	void HandleVideoTrackComboBoxSelectionChanged( IMediaTrackPtr Selection, ESelectInfo::Type SelectInfo );

	/** Callback for getting the text in the video track combo box. */
	FText HandleVideoTrackComboBoxText() const;

private:

	/** The collection of available audio tracks. */
	TArray<IMediaTrackPtr> AudioTracks;

	/** Holds the audio track combo box. */
	TSharedPtr<SComboBox<IMediaTrackPtr>> AudioTrackComboBox;

	/** Buffers text from the selected caption track. */
	FMediaSampleBufferRef CaptionBuffer;

	/** The collection of available caption tracks. */
	TArray<IMediaTrackPtr> CaptionTracks;

	/** Holds the caption track combo box. */
	TSharedPtr<SComboBox<IMediaTrackPtr>> CaptionTrackComboBox;

	/** Pointer to the media player that is being viewed. */
	UMediaPlayer* MediaPlayer;

	/** The playback rate prior to scrubbing. */
	float PreScrubRate;

	/** Holds the scrubber slider. */
	TSharedPtr<SSlider> ScrubberSlider;

	/** The collection of available video tracks. */
	TArray<IMediaTrackPtr> VideoTracks;

	/** Holds the video track combo box. */
	TSharedPtr<SComboBox<IMediaTrackPtr>> VideoTrackComboBox;

	/** Holds the Slate viewport. */
	TSharedPtr<FMediaPlayerEditorViewport> Viewport;
};

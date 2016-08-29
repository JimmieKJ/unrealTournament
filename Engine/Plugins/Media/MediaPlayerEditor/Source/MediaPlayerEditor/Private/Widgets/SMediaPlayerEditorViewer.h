// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


enum class EMediaTrackType;
class ISlateStyle;
class SSlider;
class UMediaPlayer;


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
	void Construct(const FArguments& InArgs, UMediaPlayer& InMediaPlayer, const TSharedRef<ISlateStyle>& InStyle);

protected:

	/** Open the media URL in the url text box. */
	void OpenUrlTextBoxUrl();

	/** Set the name of the desired native media player. */
	void SetDesiredPlayerName(FName PlayerName);

private:

	/** Callback for the active timer. */
	EActiveTimerReturnType HandleActiveTimer(double InCurrentTime, float InDeltaTime);

	/** Callback for media player events. */
	void HandleMediaPlayerMediaEvent(EMediaEvent Event);

	/** Callback for getting the text of the movie captions. */
	FText HandleOverlayCaptionText() const;

	/** Callback for getting the text of the player name overlay. */
	FText HandleOverlayPlayerNameText() const;

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

	/** Callback for creating the player sub-menu. */
	void HandlePlayerMenuNewMenu(FMenuBuilder& MenuBuilder);

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

	/** Callback for handling button up events on the movie texture border. */
	FReply HandleTextureMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& MouseEvent);

	/** Callback for getting the text of the timer text block. */
	FText HandleTimerTextBlockText() const;

	/** Callback for getting the tool tip of the timer text block. */
	FText HandleTimerTextBlockToolTipText() const;

	/** Callback for creating the a track sub-menu. */
	void HandleTrackMenuNewMenu(FMenuBuilder& MenuBuilder, EMediaPlayerTrack TrackType);

	/** Callback for handling key down events in the URL text box. */
	FReply HandleUrlBoxKeyDown(const FGeometry&, const FKeyEvent& KeyEvent);

private:

	/** Pointer to the media player that is being viewed. */
	UMediaPlayer* MediaPlayer;

	/** The playback rate prior to scrubbing. */
	float PreScrubRate;

	/** Holds the scrubber slider. */
	TSharedPtr<SSlider> ScrubberSlider;

	/** The style set to use for this widget. */
	TSharedPtr<ISlateStyle> Style;

	/** Media URL text box. */
	TSharedPtr<SEditableTextBox> UrlTextBox;
};

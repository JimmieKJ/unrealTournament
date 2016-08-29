// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


class UMaterial;
class UMaterialExpressionTextureSample;
class UMediaPlayer;
class UMediaTexture;


/**
 * Handles content output in the viewer tab in the UMediaPlayer asset editor.
 */
class SMediaPlayerEditorOutput
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMediaPlayerEditorOutput) { }
	SLATE_END_ARGS()

public:

	/** Default constructor. */
	SMediaPlayerEditorOutput();

	/** Destructor. */
	~SMediaPlayerEditorOutput();

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InMediaPlayer The UMediaPlayer asset to show the details for.
	 */
	void Construct(const FArguments& InArgs, UMediaPlayer& InMediaPlayer);

public:

	//~ SWidget interface

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

protected:

	/** Update the material to reflect the media player's latest video textures. */
	void UpdateMaterial();

	/** Update the sound wave to reflect the media player's latest sound output. */
	void UpdateSoundWave();

private:

	/** Callback for media events from the media player. */
	void HandleMediaPlayerMediaEvent(EMediaEvent Event);

private:

	/** The audio component used to play the current sound wave. */
	UAudioComponent* AudioComponent;

	/** The media sound wave currently being played. */
	TWeakObjectPtr<UMediaSoundWave> CurrentSoundWave;

	/** The media texture currently being shown. */
	TWeakObjectPtr<UMediaTexture> CurrentTexture;

	/** The media sound wave to use if the player doesn't have one assigned. */
	UMediaSoundWave* DefaultSoundWave;

	/** The media texture to use if the player doesn't have one assigned. */
	UMediaTexture* DefaultTexture;

	/** The material that wraps the video texture for display in an SImage. */
	UMaterial* Material;

	/** The Slate brush that renders the material. */
	TSharedPtr<FSlateBrush> MaterialBrush;

	/** The media player whose video texture is shown in this widget. */
	UMediaPlayer* MediaPlayer;

	/** Whether Play-In-Editor is currently active. */
	bool PieActive;

	/** The video texture sampler in the wrapper material. */
	UMaterialExpressionTextureSample* TextureSampler;
};

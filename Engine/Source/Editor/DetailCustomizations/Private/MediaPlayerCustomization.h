// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


// forward declarations
enum class EMediaPlaybackDirections;


/**
 * Implements a details view customization for the UMediaPlayer class.
 */
class FMediaPlayerCustomization
	: public IDetailCustomization
{
public:

	// IDetailCustomization interface

	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

public:

	/**
	 * Creates an instance of this class.
	 *
	 * @return The new instance.
	 */
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FMediaPlayerCustomization());
	}

private:

	/** Callback for getting the text of the Duration text block. */
	FText HandleDurationTextBlockText() const;

	/** Callback for getting the text of the ForwardRates text block. */
	FText HandleForwardRatesTextBlockText() const;

	/** Callback for getting the text of a supported playback rate text block. */
	FText HandleSupportedRatesTextBlockText( EMediaPlaybackDirections Direction, bool Unthinned ) const;

	/** Callback for getting the text of the SupportsScrubbing text block. */
	FText HandleSupportsScrubbingTextBlockText() const;

	/** Callback for getting the text of the SupportsSeeking text block. */
	FText HandleSupportsSeekingTextBlockText() const;

	/** Callback for getting the selected path in the URL picker widget. */
	FString HandleUrlPickerFilePath() const;

	/** Callback for getting the file type filter for the URL picker. */
	FString HandleUrlPickerFileTypeFilter() const;

	/** Callback for picking a path in the URL picker. */
	void HandleUrlPickerPathPicked( const FString& PickedPath );

	/** Callback for getting the visibility of warning icon for invalid URLs. */
	EVisibility HandleUrlWarningIconVisibility() const;

private:

	/** The collection of UMediaPlayer assets being customized. */
	TArray<TWeakObjectPtr<UObject>> CustomizedMediaPlayers;

	/** Pointer to the URL property handle. */
	TSharedPtr<IPropertyHandle> UrlProperty;
};

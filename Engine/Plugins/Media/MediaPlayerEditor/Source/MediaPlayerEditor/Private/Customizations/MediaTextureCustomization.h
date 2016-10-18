// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"


class IPropertyHandle;
class SWidget;


/**
 * Implements a details view customization for the UMediaTexture class.
 */
class FMediaTextureCustomization
	: public IDetailCustomization
{
public:

	//~ IDetailCustomization interface

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

public:

	/**
	 * Creates an instance of this class.
	 *
	 * @return The new instance.
	 */
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FMediaTextureCustomization());
	}

private:

	/** Callback for generating the menu content of the VideoTrack combo box. */
	TSharedRef<SWidget> HandleVideoTrackComboButtonMenuContent() const;

	/** Callback for selecting a track in the VideoTrack combo box. */
	void HandleVideoTrackComboButtonMenuEntryExecute(int32 TrackIndex);

	/** Callback for getting the text of the VideoTrack combo box. */
	FText HandleVideoTrackComboButtonText() const;

private:

	/** The collection of media textures being customized */
	TArray<TWeakObjectPtr<UObject>> CustomizedMediaTextures;

	/** Pointer to the MediaPlayer property handle. */
	TSharedPtr<IPropertyHandle> MediaPlayerProperty;

	/** Pointer to the VideoTrackIndex property handle. */
	TSharedPtr<IPropertyHandle> VideoTrackIndexProperty;
};

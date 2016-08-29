// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the media library of the MediaPlaylist asset editor.
 */
class SMediaPlaylistEditorMedia
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMediaPlaylistEditorMedia) { }
	SLATE_END_ARGS()

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InMediaPlaylist The MediaPlaylist asset to show the details for.
	 * @param InStyleSet The style set to use.
	 */
	void Construct(const FArguments& InArgs, UMediaPlaylist& InMediaPlaylist, const TSharedRef<ISlateStyle>& InStyle);

private:

	/** Callback for double-clicking an asset in the asset picker. */
	void HandleAssetPickerAssetDoubleClicked(const class FAssetData& AssetData);

private:

	/** Pointer to the MediaPlaylist asset that is being viewed. */
	UMediaPlaylist* MediaPlaylist;
};

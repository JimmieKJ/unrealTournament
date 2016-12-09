// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SConstraintCanvas.h"

class SRichTextBlock;
class UMediaPlayer;
enum class EMediaOverlayType;

/**
 * Draws text overlays for the UMediaPlayer asset editor.
 */
class SMediaPlayerEditorOverlay
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMediaPlayerEditorOverlay) { }
	SLATE_END_ARGS()

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

	void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

protected:

	/**
	 * Add the media player's text overlays of the specified type.
	 *
	 * @param Type The type of overlays to add.
	 */
	void AddOverlays(EMediaOverlayType Type, float LayoutScale, int32& InOutSlotIndex);

private:

	/** The canvas to draw into. */
	TSharedPtr<SConstraintCanvas> Canvas;

	/** The media player whose video texture is shown in this widget. */
	UMediaPlayer* MediaPlayer;

	SConstraintCanvas::FSlot* Slots[10];

	TSharedPtr<SRichTextBlock> TextBlocks[10];
};

// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPCH.h"
#include "SMediaPlayerEditorStats.h"


#define LOCTEXT_NAMESPACE "SMediaPlayerEditorStats"


/* SMediaPlayerEditorStats interface
 *****************************************************************************/

void SMediaPlayerEditorStats::Construct(const FArguments& InArgs, UMediaPlayer& InMediaPlayer, const TSharedRef<ISlateStyle>& InStyle)
{
	MediaPlayer = &InMediaPlayer;

	ChildSlot
	[
		SNew(SBorder)
			.Padding(4.0f)
			[
				SNew(SMultiLineEditableText)
					.Text(this, &SMediaPlayerEditorStats::HandleStatsTextBlockText)
			]
	];
}


/* SMediaPlayerEditorStats callbacks
 *****************************************************************************/

FText SMediaPlayerEditorStats::HandleStatsTextBlockText() const
{
	return LOCTEXT("StatsNotAvailable", "Not Available");
}


#undef LOCTEXT_NAMESPACE

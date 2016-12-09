// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMediaPlayerEditorStats.h"
#include "IMediaPlayer.h"
#include "MediaPlayer.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "SMediaPlayerEditorStats"


/* SMediaPlayerEditorStats interface
 *****************************************************************************/

void SMediaPlayerEditorStats::Construct(const FArguments& InArgs, UMediaPlayer& InMediaPlayer, const TSharedRef<ISlateStyle>& InStyle)
{
	MediaPlayer = &InMediaPlayer;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SScrollBox)

				+ SScrollBox::Slot()
					.Padding(4.0f)
					[
						SAssignNew(StatsTextBlock, STextBlock)
							.Text(this, &SMediaPlayerEditorStats::HandleStatsTextBlockText)
					]
			]

		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.HAlign(HAlign_Right)
					.Padding(2.0f)
					[
						SNew(SButton)
							.Text(LOCTEXT("CopyClipboardButtonText", "Copy to Clipboard"))
							.ToolTipText(LOCTEXT("CopyClipboardButtonHint", "Copy the media statistics to the the clipboard"))
							.OnClicked_Lambda([this]() -> FReply {
								FPlatformMisc::ClipboardCopy(*StatsTextBlock->GetText().ToString());
								return FReply::Handled();
							})
					]
			]
	];
}


/* SMediaPlayerEditorStats callbacks
 *****************************************************************************/

FText SMediaPlayerEditorStats::HandleStatsTextBlockText() const
{
	TSharedPtr<IMediaPlayer> Player = MediaPlayer->GetPlayer();

	if (!Player.IsValid())
	{
		return LOCTEXT("NoMediaOpened", "No media opened");
	}

	return FText::FromString(Player->GetStats());
}


#undef LOCTEXT_NAMESPACE

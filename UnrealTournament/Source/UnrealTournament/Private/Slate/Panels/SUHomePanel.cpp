
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "../SUTStyle.h"
#include "../SUWScaleBox.h"
#include "Slate/SlateGameResources.h"
#include "SUHomePanel.h"
#include "../SUWindowsMainMenu.h"



#if !UE_SERVER

void SUHomePanel::ConstructPanel(FVector2D ViewportSize)
{
	this->ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SOverlay)
		+SOverlay::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				[
					SNew(SUWScaleBox)
					.bMaintainAspectRatio(false)
					[
						SNew(SImage)
						.Image(SUTStyle::Get().GetBrush("UT.HomePanel.Background"))
					]
				]
			]
		]
		+ SOverlay::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			BuildHomePanel()
		]
	];

}

TSharedRef<SWidget> SUHomePanel::BuildHomePanel()
{
	return SNew(SOverlay)
		+SOverlay::Slot()
		.Padding(64.0,32.0,6.0,32.0)
		[
			SNew(SVerticalBox)

			// Training Grounds / Last Server

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox).WidthOverride(800).HeightOverride(170)
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						[
							SNew(SImage)
							.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
						]
						+SOverlay::Slot()
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.Padding(10.0,30.0)
							.AutoWidth()
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(SBox).WidthOverride(200).HeightOverride(100)
									[
										SNew(SImage)
										.Image(SUTStyle::Get().GetBrush("UT.HomePanel.TutorialLogo"))
									]
								]
							]
						]
						+SOverlay::Slot()
						.Padding(125.0f,20.0f,0.0,0.0)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("BASIC TRAINING")))
								.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Huge")
								.ColorAndOpacity(FLinearColor(1.0f, 0.412f, 0.027f, 1.0f))
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(5.0)
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("Select this option to being training to become the ultimate warrior!")))
								.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
							]
						]
						+ SOverlay::Slot()
						[
							SNew(SButton)
							.ButtonStyle(SUTStyle::Get(),"UT.HomePanel.Button")
							.OnClicked(this, &SUHomePanel::BasicTraining_Click)
						]
					]
				]
			]


			// PLAY

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0,10.0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox).WidthOverride(800).HeightOverride(270)
					[

						SNew(SHorizontalBox)

						// QUICK PLAY - DEATHMATCH
						+SHorizontalBox::Slot()
						.Padding(0,0.0,20.0,0.0)
						.AutoWidth()
						[
							SNew(SBox).WidthOverride(380)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()
								[
									SNew(SImage)
									.Image(SUTStyle::Get().GetBrush("UT.HomePanel.DMBadge"))
								]
								+ SOverlay::Slot()
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill)
									.Padding(0.0,210.0)
									[
										SNew(SBorder)
										.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Shaded"))
										[
											SNew(SVerticalBox)
											+SVerticalBox::Slot()
											.HAlign(HAlign_Center)
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("QUICK PLAY")))
												.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
											]
											+SVerticalBox::Slot()
											.HAlign(HAlign_Center)
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("DEATHMATCH")))
												.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
												.ColorAndOpacity(FLinearColor(1.0f, 0.412f, 0.027f, 1.0f))
											]
										]

									]
									
								]
								+ SOverlay::Slot()
								[
									SNew(SButton)
									.ButtonStyle(SUTStyle::Get(), "UT.HomePanel.Button")
									.OnClicked(this, &SUHomePanel::QuickMatch_DM_Click)
								]

							]
						]

						// QUICK PLAY - CTF
						+SHorizontalBox::Slot()
						.Padding(20.0,0.0,0.0,0.0)
						.AutoWidth()
						[
							SNew(SBox).WidthOverride(380)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()
								[
									SNew(SImage)
									.Image(SUTStyle::Get().GetBrush("UT.HomePanel.CTFBadge"))
								]
								+ SOverlay::Slot()
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill)
									.Padding(0.0,210.0)
									[
										SNew(SBorder)
										.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Shaded"))
										[
											SNew(SVerticalBox)
											+SVerticalBox::Slot()
											.HAlign(HAlign_Center)
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("QUICK PLAY")))
												.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
											]
											+SVerticalBox::Slot()
											.HAlign(HAlign_Center)
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("CAPTURE THE FLAG")))
												.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
												.ColorAndOpacity(FLinearColor(1.0f, 0.412f, 0.027f, 1.0f))
											]
										]

									]
									
								]
								+ SOverlay::Slot()
								[
									SNew(SButton)
									.ButtonStyle(SUTStyle::Get(), "UT.HomePanel.Button")
									.OnClicked(this, &SUHomePanel::QuickMatch_CTF_Click)
								]
							]
						]
					]
				]
			]


			// OFFLINE ACTION / FIND Match

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0,10.0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox).WidthOverride(800).HeightOverride(270)
					[
						SNew(SHorizontalBox)

						// OFFLINE ACTION
						+SHorizontalBox::Slot()
						.Padding(0.0,0.0,20.0,0.0)
						.AutoWidth()
						[
							SNew(SBox).WidthOverride(380)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()
								[
									SNew(SImage)
									.Image(SUTStyle::Get().GetBrush("UT.HomePanel.IABadge"))
								]
								+ SOverlay::Slot()
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill)
									.Padding(0.0,230.0)
									[
										SNew(SBorder)
										.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Shaded"))
										[
											SNew(SVerticalBox)
											+SVerticalBox::Slot()
											.HAlign(HAlign_Center)
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("OFFLINE ACTION")))
												.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
											]
										]

									]
									
								]
								+ SOverlay::Slot()
								[
									SNew(SButton)
									.ButtonStyle(SUTStyle::Get(), "UT.HomePanel.Button")
									.OnClicked(this, &SUHomePanel::OfflineAction_Click)
								]
							]
						]

						// FIND A MATCH
						+SHorizontalBox::Slot()
						.Padding(20.0,0.0,0.0,0.0)
						.AutoWidth()
						[
							SNew(SBox).WidthOverride(380)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()
								[
									SNew(SImage)
									.Image(SUTStyle::Get().GetBrush("UT.HomePanel.FMBadge"))
								]
								+ SOverlay::Slot()
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill)
									.Padding(0.0,230.0)
									[
										SNew(SBorder)
										.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Shaded"))
										[
											SNew(SVerticalBox)
											+SVerticalBox::Slot()
											.HAlign(HAlign_Center)
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("FIND A MATCH")))
												.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
											]
										]

									]
									
								]
								+ SOverlay::Slot()
								[
									SNew(SButton)
									.ButtonStyle(SUTStyle::Get(), "UT.HomePanel.Button")
									.OnClicked(this, &SUHomePanel::FindAMatch_Click)
								]
							]
						]
					]
				]
			]


			// Frag Center / Replays

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0,10.0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox).WidthOverride(800).HeightOverride(180)
					[

						SNew(SHorizontalBox)

						// Frag Center
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f,0.0f,20.0f,0.0)
						[
							SNew(SBox).WidthOverride(180)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()
								[
									SNew(SBorder)
									.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
									[
										SNew(SImage)
										.Image(SUTStyle::Get().GetBrush("UT.HomePanel.FragCenterLogo"))
									]
								]

								+ SOverlay::Slot()
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill)
									.Padding(0.0,155.0)
									[
										SNew(SBorder)
										.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Shaded"))
										[
											SNew(SVerticalBox)
											+SVerticalBox::Slot()
											.HAlign(HAlign_Center)
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("FRAG CENTER")))
												.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
											]
										]
									]
									
								]

								+ SOverlay::Slot()
								[
									SNew(SButton)
									.ButtonStyle(SUTStyle::Get(), "UT.HomePanel.Button")
									.OnClicked(this, &SUHomePanel::FragCenter_Click)
								]
							]
						]

						// Replay

						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(10.0f,0.0f,10.0f,0.0)
						[
							SNew(SBox).WidthOverride(180)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()
								[
									SNew(SImage)
									.Image(SUTStyle::Get().GetBrush("UT.HomePanel.Replays"))
								]

								+ SOverlay::Slot()
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill)
									.Padding(0.0,155.0)
									[
										SNew(SBorder)
										.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Shaded"))
										[
											SNew(SVerticalBox)
											+SVerticalBox::Slot()
											.HAlign(HAlign_Center)
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("RECENT MATCHES")))
												.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
											]
										]
									]
									
								]


								+ SOverlay::Slot()
								[
									SNew(SButton)
									.ButtonStyle(SUTStyle::Get(), "UT.HomePanel.Button")
									.OnClicked(this, &SUHomePanel::RecentMatches_Click)
								]
							]
						]

						// Live Videos

						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(10.0f,0.0f,10.0f,0.0)
						[
							SNew(SBox).WidthOverride(180)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()
								[
									SNew(SBorder)
									.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
									[
										SNew(SImage)
										.Image(SUTStyle::Get().GetBrush("UT.HomePanel.Live"))
									]
								]

								+ SOverlay::Slot()
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill)
									.Padding(0.0,155.0)
									[
										SNew(SBorder)
										.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Shaded"))
										[
											SNew(SVerticalBox)
											+SVerticalBox::Slot()
											.HAlign(HAlign_Center)
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("WATCH LIVE")))
												.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
											]
										]
									]
									
								]

								+ SOverlay::Slot()
								[
									SNew(SButton)
									.ButtonStyle(SUTStyle::Get(), "UT.HomePanel.Button")
									.OnClicked(this, &SUHomePanel::WatchLive_Click)
								]
							]
						]

						// Training Videos

						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(20.0f,0.0f,0.0f,0.0)
						[
							SNew(SBox).WidthOverride(180)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()
								[
									SNew(SBorder)
									.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
									[
										SNew(SImage)
										.Image(SUTStyle::Get().GetBrush("UT.HomePanel.Flak"))
									]
								]

								+ SOverlay::Slot()
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill)
									.Padding(0.0,155.0)
									[
										SNew(SBorder)
										.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Shaded"))
										[
											SNew(SVerticalBox)
											+SVerticalBox::Slot()
											.HAlign(HAlign_Center)
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("TRAINING VIDEOS")))
												.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
											]
										]
									]
									
								]


								+ SOverlay::Slot()
								[
									SNew(SButton)
									.ButtonStyle(SUTStyle::Get(), "UT.HomePanel.Button")
									.OnClicked(this, &SUHomePanel::TrainingVideos_Click)
								]
							]
						]

					]
				]
			]

		];
}

FReply SUHomePanel::BasicTraining_Click()
{
	TSharedPtr<SUWindowsMainMenu> MainMenu = StaticCastSharedPtr<SUWindowsMainMenu>(GetParentWindow());
	if (MainMenu.IsValid()) MainMenu->OpenTutorialMenu();
	return FReply::Handled();
}

FReply SUHomePanel::QuickMatch_DM_Click()
{
	TSharedPtr<SUWindowsMainMenu> MainMenu = StaticCastSharedPtr<SUWindowsMainMenu>(GetParentWindow());
	if (MainMenu.IsValid()) MainMenu->QuickPlay(FQuickMatchTypeRulesetTag::DM);
	return FReply::Handled();
}

FReply SUHomePanel::QuickMatch_CTF_Click()
{
	TSharedPtr<SUWindowsMainMenu> MainMenu = StaticCastSharedPtr<SUWindowsMainMenu>(GetParentWindow());
	if (MainMenu.IsValid()) MainMenu->QuickPlay(FQuickMatchTypeRulesetTag::CTF);
	return FReply::Handled();
}

FReply SUHomePanel::OfflineAction_Click()
{
	TSharedPtr<SUWindowsMainMenu> MainMenu = StaticCastSharedPtr<SUWindowsMainMenu>(GetParentWindow());
	if (MainMenu.IsValid()) MainMenu->ShowGamePanel();
	return FReply::Handled();
}

FReply SUHomePanel::FindAMatch_Click()
{
	TSharedPtr<SUWindowsMainMenu> MainMenu = StaticCastSharedPtr<SUWindowsMainMenu>(GetParentWindow());
	if (MainMenu.IsValid()) MainMenu->OnShowServerBrowserPanel();
	return FReply::Handled();
}

FReply SUHomePanel::FragCenter_Click()
{
	TSharedPtr<SUWindowsMainMenu> MainMenu = StaticCastSharedPtr<SUWindowsMainMenu>(GetParentWindow());
	if (MainMenu.IsValid()) MainMenu->ShowFragCenter();
	return FReply::Handled();
}

FReply SUHomePanel::RecentMatches_Click()
{
	TSharedPtr<SUWindowsMainMenu> MainMenu = StaticCastSharedPtr<SUWindowsMainMenu>(GetParentWindow());
	if (MainMenu.IsValid()) MainMenu->RecentReplays();
	return FReply::Handled();
}

FReply SUHomePanel::WatchLive_Click()
{
	TSharedPtr<SUWindowsMainMenu> MainMenu = StaticCastSharedPtr<SUWindowsMainMenu>(GetParentWindow());
	if (MainMenu.IsValid()) MainMenu->ShowLiveGameReplays();
	return FReply::Handled();
}

FReply SUHomePanel::TrainingVideos_Click()
{
	TSharedPtr<SUWindowsMainMenu> MainMenu = StaticCastSharedPtr<SUWindowsMainMenu>(GetParentWindow());
	if (MainMenu.IsValid()) MainMenu->ShowCommunity();
	return FReply::Handled();
}


#endif

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "../SUTStyle.h"
#include "../SUWScaleBox.h"
#include "Slate/SlateGameResources.h"
#include "SUHomePanel.h"
#include "../SUWindowsMainMenu.h"
#include "../Widgets/SUTButton.h"



#if !UE_SERVER

static FName UT_DEFAULT_BACKGROUND(TEXT("UT.HomePanel.Background"));
static FName UT_SPOOKY_BACKGROUND(TEXT("UT.SpookyBackground"));


void SUHomePanel::ConstructPanel(FVector2D ViewportSize)
{
	FName Background = UT_DEFAULT_BACKGROUND;
	if (FDateTime().Now().GetMonth() == 10) Background = UT_SPOOKY_BACKGROUND;

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
						.Image(SUTStyle::Get().GetBrush(Background))
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

	AnnouncmentTimer = 3.0;

}

void SUHomePanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (AnnouncmentTimer > 0)
	{
		AnnouncmentTimer -= InDeltaTime;
		if (AnnouncmentTimer <= 0.0)
		{
			BuildAnnouncement();
		}
	}

	if (AnnouncmentFadeTimer > 0)
	{
		AnnouncmentFadeTimer -= InDeltaTime;
	}
}

FLinearColor SUHomePanel::GetFadeColor() const
{
	FLinearColor Color = FLinearColor::White;
	Color.A = FMath::Clamp<float>(1.0 - (AnnouncmentFadeTimer / 0.8f),0.0f, 1.0f);
	return Color;
}

FSlateColor SUHomePanel::GetFadeBKColor() const
{
	FLinearColor Color = FLinearColor::White;
	Color.A = FMath::Clamp<float>(1.0 - (AnnouncmentFadeTimer / 0.8f),0.0f, 1.0f);
	return Color;
}


void SUHomePanel::BuildAnnouncement()
{
	TSharedPtr<SVerticalBox> SlotBox;
	int32 Day = 0;
	int32 Month = 0;
	int32 Year = 0;

	FDateTime Now = FDateTime::UtcNow();
	Now.GetDate(Year, Month, Day);

	if (Year == 2015 && Month <= 9 && Day <= 19)
	{
		AnnouncmentFadeTimer = 0.8;

		AnnouncementBox->AddSlot().FillHeight(1.0)
		[
			SNew(SCanvas)
		];

		AnnouncementBox->AddSlot().AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
			.ColorAndOpacity(this, &SUHomePanel::GetFadeColor)
			.BorderBackgroundColor(this, &SUHomePanel::GetFadeBKColor)
			[
				SAssignNew(SlotBox,SVerticalBox)
				+SVerticalBox::Slot()
				.Padding(5.0,5.0,5.0,5.0)
				.AutoHeight()
				[
					SNew(SRichTextBlock)
					.Text(NSLOCTEXT("CTFExhibition", "CTFExhibitionMessage", "Join us Saturday <UT.Font.Notice.Gold>September 19th</> for our <UT.Font.Notice.Gold>CTF Exhibition tournament</> featuring top players from around the world!  We will be streaming LIVE on Twitch and YouTube starting at <UT.Font.Notice.Gold>1 PM EST</>. Watch for a chance to <UT.Font.Notice.Gold>win fantastic prizes</> donated by NVIDIA, Corsair, Logitech and more!"))	
					.TextStyle(SUTStyle::Get(), "UT.Font.Notice")
					.DecoratorStyleSet(&SUTStyle::Get())
					.AutoWrapText(true)
				]
			]
		];

		int32 Hour = Now.GetHour();
		if (Year == 2015 && Month == 9 && Day == 19 && Hour >= 3 && Hour <= 21)
		{
			SlotBox->AddSlot()
			.Padding(0.0,0.0,0.0,5.0)
			.HAlign(HAlign_Right)
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(10.0,5.0,21.0,5.0)
				.HAlign(HAlign_Right)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoHeight()
					[
						SNew(SButton)
						.ButtonStyle(SUTStyle::Get(),"UT.SimpleButton")
						.OnClicked(this, &SUHomePanel::ViewTournament,0)
						.ContentPadding(FMargin(32.0,5.0,32.0,5.0))
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("CTFExhibition", "CTFExhibitionWatchOnYouTube", "Watch on Youtube"))
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
						]
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(10.0,5.0,21.0,5.0)
				.HAlign(HAlign_Right)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoHeight()
					[
						SNew(SButton) 
						.ButtonStyle(SUTStyle::Get(),"UT.SimpleButton")
						.OnClicked(this, &SUHomePanel::ViewTournament,1)
						.ContentPadding(FMargin(32.0,5.0,32.0,5.0))
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("CTFExhibition", "CTFExhibitionWatchOnTwitch", "Watch on Twitch"))
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
						]
					]
				]
			];
		}
	}
}

FReply SUHomePanel::ViewTournament(int32 Which)
{
	FString Error;
	if (Which == 0) FPlatformProcess::LaunchURL(TEXT("https://gaming.youtube.com/unrealtournament"), NULL, &Error);
	if (Which == 1) FPlatformProcess::LaunchURL(TEXT("http://www.twitch.tv/unrealtournament"), NULL, &Error);
	return FReply::Handled();
}

TSharedRef<SWidget> SUHomePanel::BuildHomePanel()
{
	return SNew(SOverlay)

		+SOverlay::Slot()
		.Padding(920.0,32.0,0.0,0.0)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()

			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox).WidthOverride(940).HeightOverride(940)
					[
						SAssignNew(AnnouncementBox, SVerticalBox)
					]
				]
			]
		]

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
									SNew(SBox).WidthOverride(100).HeightOverride(100)
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
								.Text(FText::FromString(TEXT("Train to become the ultimate Unreal Tournament warrior!")))
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
									.Padding(0.0,206.0)
									[
										SNew(SBorder)
										.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Shaded"))
										[
											SNew(SVerticalBox)
											+SVerticalBox::Slot()
											.AutoHeight()
											.HAlign(HAlign_Center)
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("QUICK PLAY")))
												.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
											]
											+SVerticalBox::Slot()
											.AutoHeight()
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
									.Padding(0.0,206.0)
									[
										SNew(SBorder)
										.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Shaded"))
										[
											SNew(SVerticalBox)
											+SVerticalBox::Slot()
											.AutoHeight()
											.HAlign(HAlign_Center)
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("QUICK PLAY")))
												.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
											]
											+SVerticalBox::Slot()
											.HAlign(HAlign_Center)
											.AutoHeight()
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
									SNew(SImage)
									.Image(SUTStyle::Get().GetBrush("UT.HomePanel.NewChallenge"))
									.Visibility(this, &SUHomePanel::ShowNewChallengeImage)
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

EVisibility SUHomePanel::ShowNewChallengeImage() const
{
	if (PlayerOwner.IsValid() && PlayerOwner->MCPPulledData.bValid)
	{
		if (PlayerOwner->ChallengeRevisionNumber< PlayerOwner->MCPPulledData.ChallengeRevisionNumber)
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Hidden;
}

#endif
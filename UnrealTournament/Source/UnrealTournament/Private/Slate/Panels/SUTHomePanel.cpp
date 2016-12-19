
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SlateBasics.h"
#include "../SUTUtils.h"
#include "../SUTStyle.h"
#include "../Widgets/SUTScaleBox.h"
#include "../Widgets/SUTBorder.h"
#include "Slate/SlateGameResources.h"
#include "SUTHomePanel.h"
#include "../Menus/SUTMainMenu.h"
#include "../Widgets/SUTButton.h"
#include "NetworkVersion.h"
#include "UTGameInstance.h"
#include "BlueprintContextLibrary.h"
#include "MatchmakingContext.h"


#if !UE_SERVER

void SUTHomePanel::ConstructPanel(FVector2D ViewportSize)
{
	this->ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SAssignNew(AnimWidget, SUTBorder)
			.OnAnimEnd(this, &SUTHomePanel::AnimEnd)
			[
				BuildHomePanel()
			]
		]
	];

	AnnouncmentTimer = 3.0;

}

void SUTHomePanel::OnShowPanel(TSharedPtr<SUTMenuBase> inParentWindow)
{
	SUTPanelBase::OnShowPanel(inParentWindow);

	if (AnimWidget.IsValid())
	{
		AnimWidget->Animate(FVector2D(100.0f, 0.0f), FVector2D(0.0f, 0.0f), 0.0f, 1.0f, 0.3f);
	}
}



void SUTHomePanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
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

	if (NewChallengeBox.IsValid())
	{
		if (NewChallengeImage.IsValid())
		{
			float Scale = 1.0f + (0.1 * FMath::Sin(PlayerOwner->GetWorld()->GetTimeSeconds() * 10.0f));
			NewChallengeImage->SetRenderTransform(FSlateRenderTransform(Scale));
		}
	}

}

FLinearColor SUTHomePanel::GetFadeColor() const
{
	FLinearColor Color = FLinearColor::White;
	Color.A = FMath::Clamp<float>(1.0 - (AnnouncmentFadeTimer / 0.8f),0.0f, 1.0f);
	return Color;
}

FSlateColor SUTHomePanel::GetFadeBKColor() const
{
	FLinearColor Color = FLinearColor::White;
	Color.A = FMath::Clamp<float>(1.0 - (AnnouncmentFadeTimer / 0.8f),0.0f, 1.0f);
	return Color;
}


void SUTHomePanel::BuildAnnouncement()
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
			.ColorAndOpacity(this, &SUTHomePanel::GetFadeColor)
			.BorderBackgroundColor(this, &SUTHomePanel::GetFadeBKColor)
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
						.OnClicked(this, &SUTHomePanel::ViewTournament,0)
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
						.OnClicked(this, &SUTHomePanel::ViewTournament,1)
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

FReply SUTHomePanel::ViewTournament(int32 Which)
{
	FString Error;
	if (Which == 0) FPlatformProcess::LaunchURL(TEXT("https://gaming.youtube.com/unrealtournament"), NULL, &Error);
	if (Which == 1) FPlatformProcess::LaunchURL(TEXT("http://www.twitch.tv/unrealtournament"), NULL, &Error);
	return FReply::Handled();
}

TSharedRef<SWidget> SUTHomePanel::BuildHomePanel()
{

	FString BuildVersion = FApp::GetBuildVersion();
	int32 Pos = -1;
	if (BuildVersion.FindLastChar('-', Pos)) BuildVersion = BuildVersion.Right(BuildVersion.Len() - (Pos + 2));

	TSharedPtr<SOverlay> Final;
	TSharedPtr<SHorizontalBox> QuickPlayBox;

	SAssignNew(Final, SOverlay)

		// Announcement box
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
		.Padding(64.0,50.0,6.0,32.0)
		[
			SNew(SVerticalBox)

			// Main Button

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(800).HeightOverride(143)
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SUTImage)
								.Image(SUTStyle::Get().GetBrush("UT.Logo.Loading")).WidthOverride(400).HeightOverride(143)	
							]
							+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(0.0f,0.0f,0.0f,10.0f)
							[
								SNew(SBox).WidthOverride(400).HAlign(HAlign_Right)
								[
									SNew(STextBlock)
									.Text(FText::Format(NSLOCTEXT("Common","VersionFormat","Build {0}"), FText::FromString(BuildVersion)))
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
									.ColorAndOpacity(FLinearColor(1.0f, 0.412f, 0.027f, 1.0f))
								]
							]
						]
					]
				]
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox).WidthOverride(800).HeightOverride(220)
					[
						BuildMainButtonContent()
					]
				]
			]


			// PLAY

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0,30.0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox).WidthOverride(800).HeightOverride(270)
					[
						SAssignNew(QuickPlayBox, SHorizontalBox)
					]
				]
			]

			+ SVerticalBox::Slot()
			.Padding(0.0,30.0)
			.AutoHeight()
			[
				BuildRankedPlaylist()
			]
		]
		+SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot().FillHeight(1.0f).HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Top)
				[
					SAssignNew(PartyBox, SUTPartyWidget, PlayerOwner->PlayerController)
				]
			]
		]
		+SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot().FillHeight(1.0f).HAlign(HAlign_Right).Padding(0.0f,142.0f,0.0f,0.0f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Top)
				[
					SAssignNew(PartyInviteBox, SUTPartyInviteWidget, PlayerOwner->PlayerController)
				]
			]
		];


	BuildQuickplayButton(QuickPlayBox, TEXT("UT.HomePanel.CTFBadge"), NSLOCTEXT("SUTHomePanel","QP_FlagRun","FLAG RUN"), EMenuCommand::MC_QuickPlayFlagrun);
	BuildQuickplayButton(QuickPlayBox, TEXT("UT.HomePanel.DMBadge"), NSLOCTEXT("SUTHomePanel","QP_DM","DEATHMATCH"), EMenuCommand::MC_QuickPlayDM, 25.0f);
	BuildQuickplayButton(QuickPlayBox, TEXT("UT.HomePanel.TeamShowdownBadge"), NSLOCTEXT("SUTHomePanel","QP_Showdown","SHOWDOWN"), EMenuCommand::MC_QuickPlayShowdown);

	return Final.ToSharedRef();
}

TSharedRef<SWidget> SUTHomePanel::BuildMainButtonContent()
{
	if (PlayerOwner->HasProgressionKeys({ FName(TEXT("PROGRESSION_KEY_NoLongerANoob")) }))
	{
		return SNew(SUTButton)
			.ButtonStyle(SUTStyle::Get(), "UT.HomePanel.Button")
			.OnClicked(this, &SUTHomePanel::OfflineAction_Click)
			.ToolTipText( TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateUObject(PlayerOwner.Get(), &UUTLocalPlayer::GetMenuCommandTooltipText, EMenuCommand::MC_Challenges) ))
			.bSpringButton(true)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SUTImage)
					.Image(SUTStyle::Get().GetBrush("UT.HomePanel.Challenges"))
					.WidthOverride(250)
					.HeightOverride(270)
				]
				+SOverlay::Slot()
				.VAlign(VAlign_Bottom)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot().AutoHeight().Padding(0.0f,130.0f,0.0f,0.0f)
					[
						SNew(SBorder)
						.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Shaded"))
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.Padding(10.0f,5.0f)
							.AutoWidth()
							[
								SAssignNew(NewChallengeBox, SVerticalBox)
								.Visibility(this, &SUTHomePanel::ShowNewChallengeImage)
								+SVerticalBox::Slot()
								.AutoHeight()
								[
									SAssignNew(NewChallengeImage, SUTImage)
									.Image(SUTStyle::Get().GetBrush("UT.HomePanel.ChallengesNewIcon"))
									.WidthOverride(72.0f).HeightOverride(72.0f)
									.RenderTransformPivot(FVector2D(0.5f, 0.5f))
								]
							]
							+SHorizontalBox::Slot()
							.Padding(20.0f,5.0f,0.0,0.0)
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("CHALLENGES")))
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Large")
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("See how well you stack up against the best AI in the world!")))
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
									.ColorAndOpacity(FLinearColor(1.0f, 0.412f, 0.027f, 1.0f))
								]
							]
						]

					]
				]
			];
	}
	else
	{
		return SNew(SUTButton)
			.ButtonStyle(SUTStyle::Get(), "UT.HomePanel.Button")
			.OnClicked(this, &SUTHomePanel::BasicTraining_Click)
			.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTHomePanel","BasicTrainingTT","Complete several training sessions to lean all of the skills needed to compete in the tournament.")))
			.bSpringButton(true)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SUTImage)
					.Image(SUTStyle::Get().GetBrush("UT.HomePanel.BasicTraining"))
					.WidthOverride(250)
					.HeightOverride(270)
				]
				+SOverlay::Slot()
				.VAlign(VAlign_Bottom)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot().AutoHeight().Padding(0.0f,130.0f,0.0f,0.0f)
					[
						SNew(SBorder)
						.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Shaded"))
						[
							SNew(SOverlay)
							+SOverlay::Slot()
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.Padding(10.0f,5.0f)
								.AutoWidth()
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot()
									.AutoHeight()
									[
										SNew(SBox).WidthOverride(72).HeightOverride(72)
										[
											SNew(SImage)
											.Image(SUTStyle::Get().GetBrush("UT.HomePanel.TutorialLogo"))
										]
									]
								]
							]
							+SOverlay::Slot()
							.Padding(100.0f,5.0f,0.0,0.0)
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("BASIC TRAINING")))
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Large")
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("Train to become the ultimate Unreal Tournament warrior!")))
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
									.ColorAndOpacity(FLinearColor(1.0f, 0.412f, 0.027f, 1.0f))
								]
							]
						]

					]
				]
			];
	}
}

void SUTHomePanel::BuildQuickplayButton(TSharedPtr<SHorizontalBox> QuickPlayBox, FName BackgroundTexture, FText Caption, FName QuickMatchType, float Padding)
{
	QuickPlayBox->AddSlot()
		.Padding(Padding,0.0,Padding,0.0)
		.AutoWidth()
		[
			SNew(SBox).WidthOverride(250)
			[
				SNew(SUTButton)
				.ButtonStyle(SUTStyle::Get(), "UT.HomePanel.Button")
				.bSpringButton(true)
				.OnClicked(this, &SUTHomePanel::QuickPlayClick, QuickMatchType)
				.ToolTipText( TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateUObject(PlayerOwner.Get(), &UUTLocalPlayer::GetMenuCommandTooltipText, QuickMatchType) ) )
				.ContentPadding(FMargin(2.0f, 2.0f, 2.0f, 2.0f))
				[
					SNew(SOverlay)
					+SOverlay::Slot()
					[
						SNew(SUTImage)
						.Image(SUTStyle::Get().GetBrush(BackgroundTexture))
						.WidthOverride(250)
						.HeightOverride(270)
					]
					+SOverlay::Slot()
					.VAlign(VAlign_Bottom)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill)
						[
							SNew(SBorder)
							.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Shaded"))
							.Padding(FMargin(0.0f,0.0f,0.0f,0.0f))
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
									.Text(Caption)
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
									.ColorAndOpacity(FLinearColor(1.0f, 0.412f, 0.027f, 1.0f))
								]
							]
						]
					]
				]
			]
		];
}

FReply SUTHomePanel::QuickPlayClick(FName QuickMatchType)
{
	TSharedPtr<SUTMainMenu> MainMenu = StaticCastSharedPtr<SUTMainMenu>(GetParentWindow());
	if (MainMenu.IsValid())
	{
		if (PlayerOwner->IsMenuOptionLocked(QuickMatchType))
		{
			if (QuickMatchType == EMenuCommand::MC_QuickPlayDM) PlayerOwner->LaunchTutorial(ETutorialTags::TUTTAG_DM, EEpicDefaultRuleTags::Deathmatch);
			else if (QuickMatchType == EMenuCommand::MC_QuickPlayFlagrun) PlayerOwner->LaunchTutorial(ETutorialTags::TUTTAG_Flagrun, EEpicDefaultRuleTags::FlagRun);
			else if (QuickMatchType == EMenuCommand::MC_QuickPlayShowdown) PlayerOwner->LaunchTutorial(ETutorialTags::TUTTAG_Showdown, EEpicDefaultRuleTags::TEAMSHOWDOWN);
		}
		else
		{
			if (QuickMatchType == EMenuCommand::MC_QuickPlayDM) MainMenu->QuickPlay(EEpicDefaultRuleTags::Deathmatch);
			else if (QuickMatchType == EMenuCommand::MC_QuickPlayFlagrun) MainMenu->QuickPlay(EEpicDefaultRuleTags::FlagRun);
			else if (QuickMatchType == EMenuCommand::MC_QuickPlayShowdown) MainMenu->QuickPlay(EEpicDefaultRuleTags::TEAMSHOWDOWN);
		}
	}

	return FReply::Handled();
}

FReply SUTHomePanel::BasicTraining_Click()
{
	TSharedPtr<SUTMainMenu> MainMenu = StaticCastSharedPtr<SUTMainMenu>(GetParentWindow());
	if (MainMenu.IsValid()) MainMenu->OpenTutorialMenu();
	return FReply::Handled();
}


FReply SUTHomePanel::OfflineAction_Click()
{
	TSharedPtr<SUTMainMenu> MainMenu = StaticCastSharedPtr<SUTMainMenu>(GetParentWindow());
	if (MainMenu.IsValid())
	{
		MainMenu->ShowGamePanel();
	}
	return FReply::Handled();
}

FReply SUTHomePanel::FindAMatch_Click()
{
	TSharedPtr<SUTMainMenu> MainMenu = StaticCastSharedPtr<SUTMainMenu>(GetParentWindow());
	if (MainMenu.IsValid())
	{
		if (PlayerOwner->IsMenuOptionLocked(EMenuCommand::MC_FindAMatch))
		{
			PlayerOwner->LaunchTutorial(ETutorialTags::TUTTAG_Progress);
		}
		else
		{
			MainMenu->OnShowServerBrowserPanel();
		}
	}
	return FReply::Handled();
}

FReply SUTHomePanel::FragCenter_Click()
{

	PlayerOwner->UpdateFragCenter();

	TSharedPtr<SUTMainMenu> MainMenu = StaticCastSharedPtr<SUTMainMenu>(GetParentWindow());
	if (MainMenu.IsValid()) MainMenu->ShowFragCenter();
	return FReply::Handled();
}

FReply SUTHomePanel::RecentMatches_Click()
{
	TSharedPtr<SUTMainMenu> MainMenu = StaticCastSharedPtr<SUTMainMenu>(GetParentWindow());
	if (MainMenu.IsValid()) MainMenu->RecentReplays();
	return FReply::Handled();
}

FReply SUTHomePanel::WatchLive_Click()
{
	TSharedPtr<SUTMainMenu> MainMenu = StaticCastSharedPtr<SUTMainMenu>(GetParentWindow());
	if (MainMenu.IsValid()) MainMenu->ShowLiveGameReplays();
	return FReply::Handled();
}

FReply SUTHomePanel::TrainingVideos_Click()
{
	TSharedPtr<SUTMainMenu> MainMenu = StaticCastSharedPtr<SUTMainMenu>(GetParentWindow());
	if (MainMenu.IsValid()) MainMenu->ShowCommunity();
	return FReply::Handled();
}

EVisibility SUTHomePanel::ShowNewChallengeImage() const
{
	if (PlayerOwner.IsValid() && PlayerOwner->MCPPulledData.bValid)
	{
		if (PlayerOwner->ChallengeRevisionNumber< PlayerOwner->MCPPulledData.ChallengeRevisionNumber)
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}

FSlateColor SUTHomePanel::GetFragCenterWatchNowColorAndOpacity() const
{
	if (PlayerOwner->IsFragCenterNew())
	{
		float Alpha = 0.5 + (0.5 * FMath::Abs<float>(FMath::Sin(PlayerOwner->GetWorld()->GetTimeSeconds() * 3)));
		return FSlateColor(FLinearColor(1.0,1.0,1.0, Alpha));
	}
	return FSlateColor(FLinearColor(1.0,1.0,1.0,0.0));
}

void SUTHomePanel::OnHidePanel()
{
	bClosing = true;
	if (AnimWidget.IsValid())
	{
		AnimWidget->Animate(FVector2D(0.0f, 0.0f), FVector2D(-100.0f, 0.0f), 1.0f, 0.0f, 0.3f);
	}
	else
	{
		SUTPanelBase::OnHidePanel();
	}
}


void SUTHomePanel::AnimEnd()
{
	if (bClosing)
	{
		bClosing = false;
		TSharedPtr<SWidget> Panel = this->AsShared();
		ParentWindow->PanelHidden(Panel);
		ParentWindow.Reset();
	}
}

TSharedRef<SWidget> SUTHomePanel::BuildRankedPlaylist()
{
	UUTGameInstance* UTGameInstance = CastChecked<UUTGameInstance>(PlayerOwner->GetGameInstance());
	if (UTGameInstance && UTGameInstance->GetPlaylistManager() && PlayerOwner->IsLoggedIn())
	{
		int32 NumPlaylists = UTGameInstance->GetPlaylistManager()->GetNumPlaylists();
		if (NumPlaylists > 0)
		{
			TSharedPtr<SHorizontalBox> FinalBox;
			TSharedPtr<SHorizontalBox> RankedBox;

			SAssignNew(FinalBox, SHorizontalBox);

			FinalBox->AddSlot()
			.AutoWidth()
			.Padding(FMargin(2.0f, 0.0f))
			[
				SNew(SUTImage)
				.Image(SUTStyle::Get().GetBrush("UT.HomePanel.NewFragCenter.Transparent"))
				.WidthOverride(196).HeightOverride(196)
			];

			FinalBox->AddSlot().AutoWidth().VAlign(VAlign_Bottom)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("- Ranked Matches -")))
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium.Bold")

				]
				+SVerticalBox::Slot().AutoHeight().Padding(0.0f,5.0f,0.0f,0.0f)
				[
					SAssignNew(RankedBox, SHorizontalBox)
				]
			];
				
			for (int32 i = 0; i < NumPlaylists; i++)
			{
				FString PlaylistName;
				int32 MaxTeamCount, MaxTeamSize, MaxPartySize, PlaylistId;

				if (UTGameInstance->GetPlaylistManager()->GetPlaylistId(i, PlaylistId) &&
					PlayerOwner->IsRankedMatchmakingEnabled(PlaylistId) &&
					UTGameInstance->GetPlaylistManager()->GetPlaylistName(PlaylistId, PlaylistName) &&
					UTGameInstance->GetPlaylistManager()->GetMaxTeamInfoForPlaylist(PlaylistId, MaxTeamCount, MaxTeamSize, MaxPartySize))
				{
					FString PlaylistPlayerCount = FString::Printf(TEXT("%dv%d"), MaxTeamSize, MaxTeamSize);
					FName SlateBadgeName = UTGameInstance->GetPlaylistManager()->GetPlaylistSlateBadge(PlaylistId);
					if (SlateBadgeName == NAME_None) SlateBadgeName = FName(TEXT("UT.HomePanel.DMBadge"));

					RankedBox->AddSlot()
					.AutoWidth()

					.Padding(FMargin(2.0f, 0.0f))
					[
						SNew(SUTButton)
						.ButtonStyle(SUTStyle::Get(), "UT.HomePanel.Button")
						.bSpringButton(true)
						.OnClicked(FOnClicked::CreateSP(this, &SUTHomePanel::OnStartRankedPlaylist, PlaylistId))
						.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTHomePanel","Ranked","Play a ranked match and earn XP.")))
						[

							SNew(SOverlay)
							+ SOverlay::Slot()
							[
								SNew(SBox)
								.WidthOverride(128)
								.HeightOverride(128)
								[
									SNew(SImage)
									.Image(SUTStyle::Get().GetBrush(SlateBadgeName))
								]
							]
							+ SOverlay::Slot()
							[
								SNew(SBox)
								.WidthOverride(128)
								.HeightOverride(128)
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot()
									.HAlign(HAlign_Center)
									.VAlign(VAlign_Top)
									.FillHeight(1.0f)
									[
										SNew(STextBlock)
										.Text(FText::FromString(PlaylistName))
										.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
									]
									+SVerticalBox::Slot()
									.HAlign(HAlign_Fill)
									.VAlign(VAlign_Center)
									.AutoHeight()
									[
										SNew(SBorder)
										.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Shaded"))
										[
											SNew(SVerticalBox)
											+SVerticalBox::Slot()
											.AutoHeight()
											.HAlign(HAlign_Center)
											.VAlign(VAlign_Center)
											.Padding(0.0f, 0.0f, 0.0f, 2.0f)
											[
												SNew(STextBlock)
												.Text(FText::FromString(PlaylistPlayerCount))
												.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium.Bold")
											]
										]
									]
								]
							]
						]
					];					
				}
			}
			return FinalBox.ToSharedRef();
		}
	}
	return SNullWidget::NullWidget;
}

FReply SUTHomePanel::OnStartRankedPlaylist(int32 PlaylistId)
{
	if (PlayerOwner.IsValid())
	{
		if (!PlayerOwner->IsRankedMatchmakingEnabled(PlaylistId))
		{
			PlayerOwner->ShowToast(NSLOCTEXT("SUTPartyWidget", "RankedPlayDisabled", "This playlist is not currently active"));
			return FReply::Handled();
		}

		UMatchmakingContext* MatchmakingContext = Cast<UMatchmakingContext>(UBlueprintContextLibrary::GetContext(PlayerOwner->GetWorld(), UMatchmakingContext::StaticClass()));
		if (MatchmakingContext)
		{
			MatchmakingContext->StartMatchmaking(PlaylistId);
		}
	}

	return FReply::Handled();
}



#endif
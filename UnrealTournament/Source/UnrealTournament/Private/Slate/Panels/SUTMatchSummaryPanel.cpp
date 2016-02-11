// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SUTMatchSummaryPanel.h"
#include "../SUWindowsStyle.h"
#include "../SUTStyle.h"
#include "../SUTUtils.h"
#include "../Dialogs/SUTWeaponConfigDialog.h"
#include "SUTInGameHomePanel.h"
#include "SNumericEntryBox.h"
#include "UTCanvasRenderTarget2D.h"
#include "EngineModule.h"
#include "SlateMaterialBrush.h"
#include "UTPlayerCameraManager.h"
#include "UTCharacterContent.h"
#include "UTWeap_ShockRifle.h"
#include "UTWeaponAttachment.h"
#include "UTHUD.h"
#include "Engine/UserInterfaceSettings.h"
#include "UTGameEngine.h"
#include "Animation/SkeletalMeshActor.h"
#include "UTPlayerState.h"
#include "../Widgets/SUTTabWidget.h"
#include "UTConsole.h"
#include "UTHUDWidgetMessage.h"
#include "UTHUDWidgetMessage_GameMessages.h"
#include "../Widgets/SUTXPBar.h"
#include "../Widgets/SUTButton.h"
#include "UTTrophyRoom.h"
#include "UTGameViewportClient.h"

#if !UE_SERVER
#include "Runtime/AppFramework/Public/Widgets/Colors/SColorPicker.h"
#endif

#include "AssetData.h"

static const float PLAYER_SPACING = 75.0f;
static const float PLAYER_ALTOFFSET = 80.0f;
static const float MIN_TEAM_SPACING = 120.f;
static const float TEAM_CAMERA_OFFSET = 550.0f;
static const float TEAM_CAMERA_ZOFFSET = 115.0f;
static const float LARGETEAM_CAMERA_ZOFFSET = 100.0f;
static const float ALL_CAMERA_OFFSET = 400.0f;
static const float ALL_CAMERA_ANGLE = -5.0f;
static const float TEAMANGLE = 45.0f;

#if !UE_SERVER

#include "SScaleBox.h"
#include "Widgets/SDragImage.h"

void FTeamCamera::InitCam(class SUTMatchSummaryPanel* MatchWidget)
{
	CamFlags |= CF_ShowSwitcher | CF_ShowPlayerNames | CF_Team;
	MatchWidget->ViewedTeamNum = TeamNum;
	MatchWidget->GetTeamCamTransforms(TeamNum, CamStart, CamEnd);

	MatchWidget->CameraTransform.SetLocation(CamStart.GetLocation());
	MatchWidget->CameraTransform.SetRotation(CamStart.Rotator().Quaternion());

	MatchWidget->ShowTeam(MatchWidget->ViewedTeamNum);
	MatchWidget->TeamCamAlpha = 0.5f;
}

void FCharacterCamera::InitCam(class SUTMatchSummaryPanel* MatchWidget)
{
	CamFlags |= CF_ShowSwitcher | CF_ShowInfoWidget | CF_Player;
	if (Character.IsValid())
	{
		AUTCharacter *ViewedCharacter = Character.Get();
		MatchWidget->ViewedChar = Character;
		MatchWidget->ViewedTeamNum = ViewedCharacter->GetTeamNum() != 255 ? ViewedCharacter->GetTeamNum() : 0;
		MatchWidget->ShowCharacter(ViewedCharacter);
		MatchWidget->FriendStatus = NAME_None;
		MatchWidget->BuildInfoPanel();

		FRotator Dir = ViewedCharacter->GetActorRotation();
		FVector Location = ViewedCharacter->GetActorLocation() + (Dir.Vector() * 300.0f);
		Location += Dir.Quaternion().GetAxisY() * -60.0f + FVector(0.0f, 0.0f, 75.0f);
		Dir.Yaw += 180.0f;

		CameraTransform.SetLocation(Location);
		CameraTransform.SetRotation(Dir.Quaternion());

		MatchWidget->CameraTransform.SetLocation(Location + 100.f*Dir.Vector());
		MatchWidget->CameraTransform.SetRotation(Dir.Quaternion());
	}
}

void FAllCamera::InitCam(class SUTMatchSummaryPanel* MatchWidget)
{
	CamFlags |= CF_All;
	float CameraOffset = MatchWidget->GetAllCameraOffset();
	CameraTransform.SetLocation(FVector(CameraOffset, 0.0f, -1.f * CameraOffset * FMath::Sin(ALL_CAMERA_ANGLE * PI / 180.f)));
	CameraTransform.SetRotation(FRotator(ALL_CAMERA_ANGLE, 180.0f, 0.0f).Quaternion());
	MatchWidget->ShowAllCharacters();
}

void SUTMatchSummaryPanel::Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> InPlayerOwner)
{
	PlayerOwner = InPlayerOwner;
	TrophyRoom = InArgs._TrophyRoom;

	GameState = Cast<AUTGameState>(TrophyRoom->GetWorld()->GameState);

	ViewingState = MatchSummaryViewState::ViewingTeam;

	this->ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SAssignNew(Content, SOverlay)
	];

	FVector2D CurrentViewportSize;
	GetPlayerOwner()->ViewportClient->GetViewportSize(CurrentViewportSize);

	StatsWidth = 0.0f;
	LastChatCount = 0;

	ShotStartTime = 0.0f;
	CurrentShot = 0;

	ViewedTeamNum = 0;
	CameraTransform.SetLocation(FVector(5000.0f, 0.0f, 0.0f)); //35.0
	CameraTransform.SetRotation(FRotator(0.0f, 180.0f, 0.0f).Quaternion());
	//DesiredCameraTransform = CameraTransform;
	TeamCamAlpha = 0.5f;
	bAutoScrollTeam = true;
	AutoScrollTeamDirection = 1.0f;

	
	FVector2D ResolutionScale(CurrentViewportSize.X / 1280.0f, CurrentViewportSize.Y / 720.0f);

	UUTGameUserSettings* Settings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());

	// Add the results
	Content->AddSlot()
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Center)
		.Padding(0.0f, 460.0f, 0.0f, 0.0f)
		[
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f))
			.BorderImage(SUWindowsStyle::Get().GetBrush("UT.Background.Dark"))
			.Padding(0)
			.Visibility(this, &SUTMatchSummaryPanel::GetSwitcherVisibility)
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.FillWidth(1.0f)
				[
					SNew(SBox)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(this, &SUTMatchSummaryPanel::GetSwitcherText)
						.ColorAndOpacity(this, &SUTMatchSummaryPanel::GetSwitcherColor)
						.TextStyle(SUWindowsStyle::Get(), "UT.Dialog.TitleTextStyle")
					]
				]
			]
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Right)
		.Padding(0.0f)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(TAttribute<FOptionalSize>::Create(TAttribute<FOptionalSize>::FGetter::CreateSP(this, &SUTMatchSummaryPanel::GetStatsWidth)))
			.Content()
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBorder)
				]
				+ SOverlay::Slot()
				[
					SNew(SImage)
					.Image(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
					.ColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f))
				]
				+ SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.VAlign(VAlign_Fill)
					[
						SAssignNew(InfoPanel, SOverlay)
					]
					+ SVerticalBox::Slot()
					.Padding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
					.AutoHeight()
					[
						SAssignNew(XPOverlay, SOverlay)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBox)
						.MaxDesiredHeight(2.0f)
						.WidthOverride(1000.0f)
						.Content()
						[
							SNew(SImage)
							.Image(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
							.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f, 0.8f))
						]
					]
					+ SVerticalBox::Slot()
					.Padding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
					.AutoHeight()
					[
						SAssignNew(FriendPanel, SHorizontalBox)
					]
				]
			]
		]
	];

	// We setup an offset for the chat window.  
	float ButtonOffset = 0.0f;
	if ( GameState.IsValid() && GameState->HasMatchStarted() )
	{
		ButtonOffset = 75.0f;
		// Add the results
		Content->AddSlot()
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Fill)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().AutoWidth().Padding(FMargin(5.0f,5.0f,5.0f,5.0f))
				[
					SNew(SBox).WidthOverride(66).HeightOverride(66)
					[
						SNew(SUTButton)
						.ButtonStyle(SUTStyle::Get(),"UT.SimpleButton.Medium")
						.OnClicked(this, &SUTMatchSummaryPanel::OnSwitcherPrevious)
						.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTMatchSummaryPanel","Previous","Previous Player or Team")))
						.Content()
						[
							SNew(SImage)
							.Image(SUTStyle::Get().GetBrush("UT.Icon.Back"))
						]
					]
				]
				+SHorizontalBox::Slot().AutoWidth().Padding(FMargin(5.0f,5.0f,5.0f,5.0f))
				[
					SNew(SBox).WidthOverride(66).HeightOverride(66)
					[
						SNew(SUTButton)
						.ButtonStyle(SUTStyle::Get(),"UT.SimpleButton.Medium")
						.OnClicked(this, &SUTMatchSummaryPanel::OnSwitcherNext)
						.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTMatchSummaryPanel","Next","Next Player or Team")))
						.Content()
						[
							SNew(SImage)
							.Image(SUTStyle::Get().GetBrush("UT.Icon.Forward"))
						]
					]
				]
				+SHorizontalBox::Slot().AutoWidth().Padding(FMargin(5.0f,5.0f,5.0f,5.0f))
				[
					SNew(SBox).WidthOverride(66).HeightOverride(66)
					[
						SNew(SUTButton)
						.ButtonStyle(SUTStyle::Get(),"UT.SimpleButton.Medium")
						.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTMatchSummaryPanel","TeamView","Team View")))
						.OnClicked(this, &SUTMatchSummaryPanel::OnExpandViewClicked)
						.Visibility(this, &SUTMatchSummaryPanel::GetTeamViewVis)
						.Content()
						[
							SNew(SImage)
							.Image(SUTStyle::Get().GetBrush("UT.Icon.ChangeTeam"))
						]
					]
				]
				+SHorizontalBox::Slot().AutoWidth().Padding(FMargin(5.0f,5.0f,5.0f,5.0f))
				[
					SNew(SBox).WidthOverride(66).HeightOverride(66)
					[
						SNew(SUTButton)
						.ButtonStyle(SUTStyle::Get(),"UT.SimpleButton.Medium")
						.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTMatchSummaryPanel","hideSummary","Hide Summary")))
						.OnClicked(this, &SUTMatchSummaryPanel::HideMatchPanel)
						.Visibility(this, &SUTMatchSummaryPanel::GetExitViewVis)
						.Content()
						[
							SNew(SImage)
							.Image(SUTStyle::Get().GetBrush("UT.Icon.Exit"))
						]
					]
				]
			]
		];
	}

	// Create a TeamAnchor for each team found
	int32 TeamCount = 0;
	if (GameState.IsValid())
	{
		for (TActorIterator<AUTTeamInfo> It(GameState->GetWorld()); It; ++It)
		{
			TeamCount++;
		}
	}

	if (TrophyRoom.IsValid() && GameState.IsValid())
	{
		TrophyRoom->GetAllCameraTransform(CameraTransform);
	}

	ViewTeam(0);

	// Turn on Screen Space Reflection max quality
	auto SSRQualityCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SSR.Quality"));
	OldSSRQuality = SSRQualityCVar->GetInt();
	SSRQualityCVar->Set(4, ECVF_SetByCode);

	if (ParentPanel.IsValid()) ParentPanel->FocusChat();

}


SUTMatchSummaryPanel::~SUTMatchSummaryPanel()
{
	// Reset Screen Space Reflection max quality, wish there was a cleaner way to reset the flags
	auto SSRQualityCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SSR.Quality"));
	EConsoleVariableFlags Flags = SSRQualityCVar->GetFlags();
	Flags = (EConsoleVariableFlags)(((uint32)Flags & ~ECVF_SetByMask) | ECVF_SetByScalability);
	SSRQualityCVar->Set(OldSSRQuality, ECVF_SetByCode);
	SSRQualityCVar->SetFlags(Flags);

}

void SUTMatchSummaryPanel::OnTabButtonSelectionChanged(const FText& NewText)
{
	static const FText Score = NSLOCTEXT("AUTGameMode", "Score", "Score");
	static const FText Weapons = NSLOCTEXT("AUTGameMode", "Weapons", "Weapons");
	static const FText Rewards = NSLOCTEXT("AUTGameMode", "Rewards", "Rewards");
	static const FText Movement = NSLOCTEXT("AUTGameMode", "Movement", "Movement");

	AUTPlayerController* UTPC = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);
	AUTPlayerState* PS = ViewedChar.IsValid() ? Cast<AUTPlayerState>(ViewedChar->PlayerState) : nullptr;
	if (UTPC != nullptr && PS != nullptr && !PS->IsPendingKill() && PS->IsValidLowLevel())
	{
		if (NewText.EqualTo(Score))
		{
			UTPC->ServerSetViewedScorePS(PS, 0);
		}
		else if (NewText.EqualTo(Weapons))
		{
			UTPC->ServerSetViewedScorePS(PS, 1);
		}
		else if (NewText.EqualTo(Rewards))
		{
			UTPC->ServerSetViewedScorePS(PS, 2);
		}
		else if (NewText.EqualTo(Movement))
		{
			UTPC->ServerSetViewedScorePS(PS, 3);
		}
		else
		{
			UTPC->ServerSetViewedScorePS(nullptr, 0);
		}
	}

	if (ParentPanel.IsValid()) ParentPanel->FocusChat();

}

void SUTMatchSummaryPanel::BuildInfoPanel()
{
	StatList.Empty();
	InfoPanel->ClearChildren();

	InfoPanel->AddSlot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(TabWidget, SUTTabWidget)
			.OnTabButtonSelectionChanged(this, &SUTMatchSummaryPanel::OnTabButtonSelectionChanged)
			.TabTextStyle(SUWindowsStyle::Get(), "UT.MatchSummary.TabButton.TextStyle")
		];

	//Build all of the player stats
	AUTPlayerState* UTPS = ViewedChar.IsValid() ? Cast<AUTPlayerState>(ViewedChar->PlayerState) : nullptr;
	HighlightBoxes.Empty();
	if (UTPS != nullptr && !UTPS->IsPendingKill() && UTPS->IsValidLowLevel())
	{
		AUTGameMode* DefaultGameMode = GameState.IsValid() && GameState->GameModeClass ? Cast<AUTGameMode>(GameState->GameModeClass->GetDefaultObject()) : NULL;
		if (DefaultGameMode != nullptr)
		{
			//Build the highlights
			if (GameState->HasMatchStarted())
			{
				TSharedPtr<SVerticalBox> VBox;
				TabWidget->AddTab(NSLOCTEXT("AUTGameMode", "Highlights", "Highlights"),
					SNew(SOverlay)
					+ SOverlay::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Fill)
					[
						SAssignNew(VBox, SVerticalBox)
					]);

				TArray<FText> Highlights = GameState->GetPlayerHighlights(UTPS);

				//Cap at 5 highlights, 4 if own rank change
				bool bShowBadgeHighlight = (UTPS && Cast<AUTPlayerController>(UTPS->GetOwner()) && Cast<AUTPlayerController>(UTPS->GetOwner())->bBadgeChanged);
				int32 MaxHighlights = bShowBadgeHighlight ? 4 : 5;
				if (Highlights.Num() > MaxHighlights)
				{
					Highlights.SetNum(5);
				}

				for (int32 i = 0; i < Highlights.Num(); i++)
				{
					TSharedPtr<class SBorder> HighlightBorder;
					VBox->AddSlot()
						.Padding(100, 20)
						.AutoHeight()
						[
							SAssignNew(HighlightBorder, SBorder)
							.BorderImage(SUWindowsStyle::Get().GetBrush("UT.MatchSummary.Highlight.Border"))
							.Padding(2)
							.Content()
							[
								SNew(SBox)
								.MinDesiredHeight(100.0f)
								.Content()
								[
									SNew(SOverlay)
									+ SOverlay::Slot()
									[
										SNew(SImage)
										.Image(SUWindowsStyle::Get().GetBrush("UT.MatchSummary.Highlight.BG"))
									]
									+ SOverlay::Slot()
										.VAlign(VAlign_Center)
										.HAlign(HAlign_Fill)
										[
											SNew(SRichTextBlock)
											.Text(Highlights[i])
											.TextStyle(SUWindowsStyle::Get(), "UT.MatchSummary.HighlightText.Normal")
											.Justification(ETextJustify::Center)
											.DecoratorStyleSet(&SUWindowsStyle::Get())
											.AutoWrapText(false)
										]
								]
							]
						];
					HighlightBoxes.Add(HighlightBorder);
				}

				if (bShowBadgeHighlight)
				{
					int32 Badge = 0;
					int32 Level = 0;
					UTPS->GetBadgeFromELO(DefaultGameMode, Badge, Level);
					FText RankNumber = FText::AsNumber(Level + 1);
					TSharedPtr<class SBorder> HighlightBorder;
					VBox->AddSlot()
						.Padding(100, 20)
						.AutoHeight()
						[
							SAssignNew(HighlightBorder, SBorder)
							.BorderImage(SUWindowsStyle::Get().GetBrush("UT.MatchSummary.Highlight.Border"))
							.Padding(2)
							.Content()
							[
								SNew(SBox)
								.MinDesiredHeight(100.0f)
								.Content()
								[
									SNew(SOverlay)
									+ SOverlay::Slot()
									[
										SNew(SImage)
										.Image(SUWindowsStyle::Get().GetBrush("UT.MatchSummary.Highlight.BG"))
									]
									+ SOverlay::Slot()
										.HAlign(HAlign_Fill)
										[
											SNew(SHorizontalBox)
											+ SHorizontalBox::Slot()
											.AutoWidth()
											.VAlign(VAlign_Center)
											[
												SNew(SRichTextBlock)
												.Text(NSLOCTEXT("AUTGameMode", "RankChanged", "Rank Updated "))
												.TextStyle(SUWindowsStyle::Get(), "UT.MatchSummary.HighlightText.Normal")
												.Justification(ETextJustify::Center)
												.DecoratorStyleSet(&SUWindowsStyle::Get())
												.AutoWrapText(false)
											]
											+ SHorizontalBox::Slot()
												.AutoWidth()
												[
													SNew(SBox)
													.WidthOverride(100)
													.HeightOverride(100)
													[
														SNew(SOverlay)
														+ SOverlay::Slot()
														[
															SNew(SImage)
															.Image(UTPS->GetELOBadgeImage(DefaultGameMode, false))
														]
														+ SOverlay::Slot()
															[
																SNew(SVerticalBox)
																+ SVerticalBox::Slot()
																.HAlign(HAlign_Center)
																.VAlign(VAlign_Center)
																.Padding(FMargin(-2.0, 0.0, 0.0, 0.0))
																[
																	SNew(STextBlock)
																	.Text(RankNumber)
																	.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tween.Bold")
																	.ShadowOffset(FVector2D(0.0f, 2.0f))
																	.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f))
																]
															]
													]
												]

										]
								]
							]
						];
					HighlightBoxes.Add(HighlightBorder);
				}

				AUTGameMode* Game = GameState->GetWorld()->GetAuthGameMode<AUTGameMode>();
				AUTPlayerState* LocalPS = (GetPlayerOwner().IsValid() && GetPlayerOwner()->PlayerController) ? Cast<AUTPlayerState>(GetPlayerOwner()->PlayerController->PlayerState) : NULL;
				if ((UTPS == LocalPS) && Game && Game->bOfflineChallenge && GameState->HasMatchEnded())
				{
					FText ChallengeResult = NSLOCTEXT("AUTGameMode", "FailedChallenge", "Offline Challenge:  No stars earned.");
					if (Game->PlayerWonChallenge())
					{
						FFormatNamedArguments Args;
						Args.Add(TEXT("NumStars"), FText::AsNumber(Game->ChallengeDifficulty+1));
						Args.Add(TEXT("RosterChange"), GetPlayerOwner()->RosterUpgradeText);
						ChallengeResult = GetPlayerOwner()->RosterUpgradeText.IsEmpty()
											? FText::Format(NSLOCTEXT("AUTGameMode", "Won Challenge", "Offline Challenge: <UT.MatchSummary.HighlightText.Value>{NumStars}</> stars earned."), Args)
											: FText::Format(NSLOCTEXT("AUTGameMode", "Won Challenge", "<UT.MatchSummary.HighlightText.Value>{NumStars}</> stars earned. {RosterChange}"), Args);
					}

					TSharedPtr<class SBorder> HighlightBorder;
					VBox->AddSlot()
						.Padding(100, 20)
						.AutoHeight()
						[
							SAssignNew(HighlightBorder, SBorder)
							.BorderImage(SUWindowsStyle::Get().GetBrush("UT.MatchSummary.Highlight.Border"))
							.Padding(2)
							.Content()
							[
								SNew(SBox)
								.MinDesiredHeight(100.0f)
								.Content()
								[
									SNew(SOverlay)
									+ SOverlay::Slot()
									[
										SNew(SImage)
										.Image(SUWindowsStyle::Get().GetBrush("UT.MatchSummary.Highlight.BG"))
									]
									+ SOverlay::Slot()
										.VAlign(VAlign_Center)
										.HAlign(HAlign_Fill)
										[
											SNew(SRichTextBlock)
											.Text(ChallengeResult)
											.TextStyle(SUWindowsStyle::Get(), "UT.MatchSummary.HighlightText.Normal")
											.Justification(ETextJustify::Center)
											.DecoratorStyleSet(&SUWindowsStyle::Get())
											.AutoWrapText(false)
										]
								]
							]
						];
					HighlightBoxes.Add(HighlightBorder);
				}
				if (UTPS == LocalPS)
				{
					for (int32 i = 0; i < HighlightBoxes.Num(); i++)
					{
						if (HighlightBoxes[i].IsValid())
						{
							HighlightBoxes[i]->SetVisibility(EVisibility::Hidden);
						}
					}
				}
			}

			UTPS->BuildPlayerInfo(TabWidget, StatList);

			//Build the player stats only if the match has started
			if (GameState->HasMatchStarted())
			{
				DefaultGameMode->BuildPlayerInfo(UTPS, TabWidget, StatList);
			}

			TabWidget->SelectTab(0);
		}
	}

	if (ParentPanel.IsValid()) ParentPanel->FocusChat();
}


void SUTMatchSummaryPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	BuildFriendPanel();

	if (CameraShots.IsValidIndex(CurrentShot) && GameState.IsValid())
	{
		float ElapsedTime = GameState->GetWorld()->RealTimeSeconds - ShotStartTime;
		if (CameraShots[CurrentShot]->TickCamera(this, ElapsedTime, InDeltaTime, CameraTransform))
		{
			SetCamShot(CurrentShot+1);
		}

		//Update the trophy cam if the trophy room is in the level
		if (TrophyRoom.IsValid())
		{
			TrophyRoom->SetCameraTransform(CameraTransform);
		}
	}

	//Update the mouse player highlighting if were using the in level trophy room
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);
	if (GameState.IsValid() && UTPC != nullptr && HasCamFlag(CF_CanInteract))
	{
		FVector WorldLocation, WorldDirection;
		UTPC->DeprojectMousePositionToWorld(WorldLocation, WorldDirection);
		UpdatePlayerHighlight(GameState->GetWorld(), WorldLocation, WorldDirection);
	}

	if (UTPC != nullptr && UTPC->MyUTHUD != nullptr)
	{
		UTPC->MyUTHUD->bForceScores = HasCamFlag(CF_ShowScoreboard);
	}

	bool bShowXPBar = HasCamFlag(CF_ShowXPBar);
	int32 NumBoxes = HighlightBoxes.Num();
	if (bShowXPBar && bFirstViewOwnHighlights && GameState.IsValid())
	{
		NumBoxes = int32(1.5f * (GameState->GetWorld()->RealTimeSeconds - ShotStartTime));
		bShowXPBar = bShowXPBar && (NumBoxes > HighlightBoxes.Num());
		NumBoxes = FMath::Min(HighlightBoxes.Num(), NumBoxes);
		int32 OldBoxes = FMath::Min(HighlightBoxes.Num(), int32(1.5f * (GameState->GetWorld()->RealTimeSeconds - ShotStartTime - InDeltaTime)));
		if (NumBoxes != OldBoxes)
		{
			AUTPlayerController* UTPC = GetPlayerOwner().IsValid() ? Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController) : nullptr;
			if (UTPC != nullptr && UTPC->MyUTHUD != nullptr)
			{
				if (UTPC->MyUTHUD->GetScoreboard())
				{
					UTPC->ClientPlaySound(UTPC->MyUTHUD->GetScoreboard()->ScoreUpdateSound, 3.f);
				}
			}
		}
	}
	for (int32 i = 0; i < NumBoxes; i++)
	{
		if (HighlightBoxes[i].IsValid())
		{
			HighlightBoxes[i]->SetVisibility(EVisibility::Visible);
		}
	}

	//Create the xp widget when on final shot
	if (bShowXPBar)
	{
		if (!XPBar.IsValid() && PlayerOwner.IsValid() && GameState.IsValid() && GameState->GetMatchState() == MatchState::WaitingPostMatch)
		{
			if (PlayerOwner->IsEarningXP())
			{
				XPOverlay->AddSlot()
					[
						SAssignNew(XPBar, SUTXPBar).PlayerOwner(PlayerOwner)
					];
			}
		}
		if (XPBar.IsValid())
		{
			XPBar->SetVisibility(EVisibility::Visible);
		}
	}
	else if (XPBar.IsValid())
	{
		XPBar->SetVisibility(EVisibility::Hidden);
	}
}

void SUTMatchSummaryPanel::SetCamShot(int32 ShotIndex)
{
	if (CameraShots.IsValidIndex(ShotIndex) && GameState.IsValid())
	{
		CurrentShot = ShotIndex;
		ShotStartTime = GameState->GetWorld()->RealTimeSeconds;
		CameraShots[CurrentShot]->InitCam(this);

		//Make sure all the characters are rotated in their proper team rotation
		/*for (int32 iTeam = 0; iTeam < TeamPreviewMeshs.Num(); iTeam++)
		{
			for (int32 iCharacter = 0; iCharacter < TeamPreviewMeshs[iTeam].Num(); iCharacter++)
			{
				if (TeamAnchors.IsValidIndex(iTeam))
				{
					TeamPreviewMeshs[iTeam][iCharacter]->SetActorRotation(TeamAnchors[iTeam]->GetActorRotation());
				}
			}
		}*/
	}
}

void SUTMatchSummaryPanel::SetupIntroCam()
{
	CameraShots.Empty();

	//View teams
	int32 NumViewTeams = FMath::Max(GameState->Teams.Num(), 1);

	//7 seconds for the team camera pan works well with the current song
	AUTGameMode* DefaultGame = GameState->GameModeClass->GetDefaultObject<AUTGameMode>();
	float TimePerTeam = (DefaultGame ? DefaultGame->IntroDisplayTime : 2.5f)/NumViewTeams;

	//Add camera pan for each team
	for (int32 i = 0; i < NumViewTeams; i++)
	{
		TSharedPtr<FTeamCamera> TeamCam = MakeShareable(new FTeamCamera(i));
		TeamCam->Time = TimePerTeam;
		TeamCam->CamFlags |= CF_ShowPlayerNames | CF_Intro;
		CameraShots.Add(TeamCam);
	}
	SetCamShot(0);

	//Play the intro music
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);
	USoundBase* Music = LoadObject<USoundBase>(NULL, TEXT("/Game/RestrictedAssets/Audio/Music/Music_FragCenterIntro.Music_FragCenterIntro"), NULL, LOAD_NoWarn | LOAD_Quiet);
	if (UTPC != nullptr && Music != nullptr)
	{
		UTPC->ClientPlaySound(Music);
	}
}

void SUTMatchSummaryPanel::GetTeamCamTransforms(int32 TeamNum, FTransform& Start, FTransform& End)
{
	if (TrophyRoom != nullptr)
	{
		FTransform Transform;
		TrophyRoom->GetTeamCameraTransform(TeamNum, Transform);

		FVector Dir = Transform.Rotator().Vector();
		FVector Location = Transform.GetLocation();

		Start.SetLocation(Location - 100.f * Dir.GetSafeNormal());
		Start.SetRotation(Dir.Rotation().Quaternion());
		End.SetLocation(Location);
		End.SetRotation(Dir.Rotation().Quaternion());
	}
}

void SUTMatchSummaryPanel::SetupMatchCam()
{
	/*int32 TeamToView = 0;
	if (GameState.IsValid() && GameState->GetMatchState() == MatchState::WaitingPostMatch && GameState->WinningTeam != nullptr)
	{
		TeamToView = GameState->WinningTeam->GetTeamNum();
	}
	else if (GetPlayerOwner().IsValid() && Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController) != nullptr)
	{
		TeamToView = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController)->GetTeamNum();
	}
	if (TeamToView == 255)
	{
		TeamToView = 0;
	}

	CameraShots.Empty();

	AUTGameMode* DefaultGame = GameState->GameModeClass->GetDefaultObject<AUTGameMode>();
	if (TeamPreviewMeshs.IsValidIndex(TeamToView) && GameState.IsValid())
	{
		TArray<AUTCharacter*> &TeamCharacters = TeamPreviewMeshs[TeamToView];
		int32 NumWinnersToShow = FMath::Min(int32(GameState->NumWinnersToShow), TeamCharacters.Num());

		if (NumWinnersToShow > 0)
		{
			float DisplayTime = DefaultGame ? DefaultGame->WinnerSummaryDisplayTime : 5.f;
			// determine match highlight scores to use for sorting
			for (int32 i = 0; i < TeamCharacters.Num(); i++)
			{
				AUTPlayerState* PS = TeamCharacters[i] ? Cast<AUTPlayerState>(TeamCharacters[i]->PlayerState) : NULL;
				if (PS)
				{
					PS->MatchHighlightScore = GameState->MatchHighlightScore(PS);
				}
			}

			// sort winners
			bool(*SortFunc)(const AUTCharacter&, const AUTCharacter&);
			SortFunc = [](const AUTCharacter& A, const AUTCharacter& B)
			{
				AUTPlayerState* PSA = Cast<AUTPlayerState>(A.PlayerState);
				AUTPlayerState* PSB = Cast<AUTPlayerState>(B.PlayerState);
				return !PSB || (PSA && (PSA->MatchHighlightScore > PSB->MatchHighlightScore));
			};
			TeamCharacters.Sort(SortFunc);

			// add winner shots
			for (int32 i = 0; i < NumWinnersToShow; i++)
			{
				TSharedPtr<FCharacterCamera> PlayerCam = MakeShareable(new FCharacterCamera(TeamCharacters[i]));
				PlayerCam->Time = DisplayTime;
				CameraShots.Add(PlayerCam);
			}
		}
	}

	// View own progress
	if (GetPlayerOwner().IsValid() && Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController) != nullptr)
	{
		bFirstViewOwnHighlights = true;
		AUTPlayerState* LocalPS = Cast<AUTPlayerState>(GetPlayerOwner()->PlayerController->PlayerState);
		AUTCharacter* LocalChar = NULL;
		if (LocalPS)
		{
			int32 LocalTeam = LocalPS->GetTeamNum();
			if (LocalTeam == 255)
			{
				LocalTeam = 0;
			}
			RecreateAllPlayers(LocalTeam);
			if (LocalTeam < TeamPreviewMeshs.Num())
			{
				TArray<AUTCharacter*> &TeamCharacters = TeamPreviewMeshs[LocalTeam];
				for (int32 iPlayer = 0; iPlayer < TeamCharacters.Num(); iPlayer++)
				{
					APlayerState* PS = (TeamCharacters[iPlayer] && TeamCharacters[iPlayer]->PlayerState && !TeamCharacters[iPlayer]->PlayerState->IsPendingKillPending()) ? TeamCharacters[iPlayer]->PlayerState : NULL;
					if (PS == LocalPS)
					{
						LocalChar = TeamCharacters[iPlayer];
						break;
					}
				}
			}
		}
		if (LocalChar)
		{
			TSharedPtr<FCharacterCamera> PlayerCam = MakeShareable(new FCharacterCamera(LocalChar));
			PlayerCam->Time = 8.0f;
			PlayerCam->CamFlags |= CF_ShowXPBar;
			CameraShots.Add(PlayerCam);
		}
	}

	// show winning team
	TSharedPtr<FTeamCameraPan> InteractTeamCam = MakeShareable(new FTeamCameraPan(TeamToView));
	InteractTeamCam->CamFlags |= CF_CanInteract | CF_ShowPlayerNames | CF_Highlights;
	InteractTeamCam->Time = DefaultGame ? DefaultGame->TeamSummaryDisplayTime : 5.f;
	CameraShots.Add(InteractTeamCam);

	SetCamShot(0);*/
}

void SUTMatchSummaryPanel::HideAllPlayersBut(AUTCharacter* UTC)
{
	// hide everyone else, show this player
	/*for (int32 i = 0; i< PlayerPreviewMeshs.Num(); i++)
	{
		PlayerPreviewMeshs[i]->HideCharacter(PlayerPreviewMeshs[i] != UTC);
	}
	for (auto Weapon : PreviewWeapons)
	{
		AUTCharacter* Holder = Cast<AUTCharacter>(Weapon->Instigator);
		Weapon->SetActorHiddenInGame(Holder != UTC);
	}*/
}

FReply SUTMatchSummaryPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	//Send the key to the console since it doesn't have focus with the dialog open
	UUTConsole* UTConsole = (GetPlayerOwner() != nullptr && GetPlayerOwner()->ViewportClient != nullptr) ? Cast<UUTConsole>(GetPlayerOwner()->ViewportClient->ViewportConsole) : nullptr;
	if (UTConsole != nullptr)
	{
		if (UTConsole->InputKey(0, InKeyEvent.GetKey(), EInputEvent::IE_Pressed))
		{
			return FReply::Handled();
		}
	}

	//Pass taunt buttons along to the player controller
	UUTPlayerInput* UTInput = Cast<UUTPlayerInput>(GetPlayerOwner()->PlayerController->PlayerInput);
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);

	if (UTInput != nullptr && UTPC != nullptr)
	{
		for (auto& Bind : UTInput->CustomBinds)
		{
			if (Bind.Command == TEXT("PlayTaunt") && InKeyEvent.GetKey().GetFName() == Bind.KeyName)
			{
				UTPC->PlayTaunt();
				return FReply::Handled();
			}
			if (Bind.Command == TEXT("PlayTaunt2") && InKeyEvent.GetKey().GetFName() == Bind.KeyName)
			{
				UTPC->PlayTaunt2();
				return FReply::Handled();
			}
		}

		//Pass the key event to PlayerInput 
		/*if (UTInput->InputKey(InKeyEvent.GetKey(), EInputEvent::IE_Pressed, 1.0f, false))
		{
			return FReply::Handled();
		}*/
	}

	return FReply::Unhandled();
}

FReply SUTMatchSummaryPanel::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	//Send the key to the console since it doesn't have focus with the dialog open
	UUTConsole* UTConsole = (GetPlayerOwner() != nullptr && GetPlayerOwner()->ViewportClient != nullptr) ? Cast<UUTConsole>(GetPlayerOwner()->ViewportClient->ViewportConsole) : nullptr;
	if (UTConsole != nullptr)
	{
		if (UTConsole->InputKey(0, InKeyEvent.GetKey(), EInputEvent::IE_Released))
		{
			return FReply::Handled();
		}
	}

	UUTPlayerInput* UTInput = Cast<UUTPlayerInput>(GetPlayerOwner()->PlayerController->PlayerInput);
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);

	if (UTInput != nullptr && UTPC != nullptr)
	{
		auto MenuKeys = UTInput->GetKeysForAction("ShowMenu");
		for (auto& MenuKey : MenuKeys)
		{
			if (InKeyEvent.GetKey().GetFName() == MenuKey.Key)
			{

				if (GetPlayerOwner()->GetCurrentMenu().IsValid())
				{
					GetPlayerOwner()->HideMenu();
				}
				else
				{
					GetPlayerOwner()->ShowMenu(TEXT(""));
				}

				return FReply::Handled();
			}
		}
	}

	//Pass the key event to PlayerInput 
	/*UUTPlayerInput* UTInput = Cast<UUTPlayerInput>(GetPlayerOwner()->PlayerController->PlayerInput);
	if (UTInput != nullptr)
	{
		if (UTInput->InputKey(InKeyEvent.GetKey(), EInputEvent::IE_Released, 1.0f, false))
		{
			return FReply::Handled();
		}
	}*/

	return FReply::Unhandled();
}

FReply SUTMatchSummaryPanel::OnKeyChar(const FGeometry& InGeometry, const FCharacterEvent& InCharacterEvent)
{
	//Send the character to the console since it doesn't have focus with the dialog open
	UUTConsole* UTConsole = (GetPlayerOwner() != nullptr && GetPlayerOwner()->ViewportClient != nullptr) ? Cast<UUTConsole>(GetPlayerOwner()->ViewportClient->ViewportConsole) : nullptr;
	if (UTConsole != nullptr)
	{
		FString CharacterString;
		CharacterString += InCharacterEvent.GetCharacter();
		if (UTConsole->InputChar(InCharacterEvent.GetUserIndex(), CharacterString))
		{
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

void SUTMatchSummaryPanel::PlayTauntByClass(AUTPlayerState* PS, TSubclassOf<AUTTaunt> TauntToPlay, float EmoteSpeed)
{
	AUTCharacter* UTC = FindCharacter(PS);
	if (UTC != nullptr)
	{
		UTC->PlayTauntByClass(TauntToPlay, EmoteSpeed);
	}
}

void SUTMatchSummaryPanel::SetEmoteSpeed(AUTPlayerState* PS, float EmoteSpeed)
{
	AUTCharacter* UTC = FindCharacter(PS);
	if (UTC != nullptr)
	{
		UTC->SetEmoteSpeed(EmoteSpeed);
	}
}

AUTCharacter* SUTMatchSummaryPanel::FindCharacter(class AUTPlayerState* PS)
{
	if (PS != nullptr)
	{
		/*for (AUTCharacter* UTC : PlayerPreviewMeshs)
		{
			if (UTC->PlayerState != nullptr && UTC->PlayerState == PS)
			{
				return UTC;
			}
		}*/
	}
	return nullptr;
}

void SUTMatchSummaryPanel::ViewCharacter(AUTCharacter* NewChar)
{
	if (NewChar != nullptr)
	{
		NewChar->UpdateTacComMesh(false);
		TSharedPtr<FCharacterCamera> PlayerCam = MakeShareable(new FCharacterCamera(NewChar));
		PlayerCam->CamFlags |= CF_CanInteract;
		CameraShots.Empty();
		CameraShots.Add(PlayerCam);
		SetCamShot(0);

		FriendStatus = NAME_None;

		CurrentlyViewedCharacter = NewChar;
		ChangeViewingState(MatchSummaryViewState::ViewingSingle);

		if (GetPlayerOwner().IsValid() && Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController) != nullptr)
		{
			AUTPlayerState* LocalPS = Cast<AUTPlayerState>(GetPlayerOwner()->PlayerController->PlayerState);
			if (LocalPS == NewChar->PlayerState)
			{
				PlayerCam->CamFlags |= CF_ShowXPBar;
			}
			else
			{
				bFirstViewOwnHighlights = false;
			}
		}
		BuildInfoPanel();
	}
}

void SUTMatchSummaryPanel::SelectPlayerState(AUTPlayerState* PS)
{
	AUTCharacter* UTC = FindCharacter(PS);
	if (UTC != nullptr)
	{
		ViewCharacter(UTC);
	}
}

void SUTMatchSummaryPanel::ViewTeam(int32 NewTeam)
{
	CurrentlyViewedCharacter.Reset();

	if (TrophyRoom.IsValid())
	{
		if (TrophyRoom->TeamGroups.Num() == 0)
		{
			return;
		}
		ViewedTeamNum = NewTeam;
		if (!TrophyRoom->TeamGroups.IsValidIndex(ViewedTeamNum))
		{
			ViewedTeamNum = 0;
		}

		TSharedPtr<FTeamCameraPan> TeamCam = MakeShareable(new FTeamCameraPan(NewTeam));
		TeamCam->CamFlags |= CF_CanInteract;
		if (GameState.IsValid() && GameState->GetMatchState() == MatchState::WaitingPostMatch)
		{
			TeamCam->CamFlags |= CF_Highlights;
		}
		CameraShots.Empty();
		CameraShots.Add(TeamCam);
		SetCamShot(0);
		ChangeViewingState(MatchSummaryViewState::ViewingTeam);
		bAutoScrollTeam = true;
		BuildInfoPanel();
		//	RecreateAllPlayers(NewTeam);
	}
}

void SUTMatchSummaryPanel::ShowTeam(int32 TeamNum)
{
	/*bFirstViewOwnHighlights = false;
	RecreateAllPlayers(TeamNum);*/
}

void SUTMatchSummaryPanel::ShowCharacter(AUTCharacter* UTC)
{
	/*AUTPlayerState* ViewedPS = UTC ? Cast<AUTPlayerState>(UTC->PlayerState) : NULL;
	int32 TeamNum = (ViewedPS && ViewedPS->Team) ? ViewedPS->Team->TeamIndex : 0;
	if (TeamNum != ViewedTeamNum)
	{
		RecreateAllPlayers(TeamNum);
	}
	HideAllPlayersBut(UTC);*/
}

void SUTMatchSummaryPanel::ShowAllCharacters()
{
	/*int32 TeamToView = 0;
	if ((TeamPreviewMeshs.Num() > 1) && GameState.IsValid() && GameState->GetMatchState() == MatchState::WaitingPostMatch && GameState->WinningTeam != nullptr)
	{
		TeamToView = GameState->WinningTeam->GetTeamNum();
	}
	ShowTeam(TeamToView);*/
}

float SUTMatchSummaryPanel::GetAllCameraOffset()
{
	int32 MaxSize = 1;
	/*for (int32 iTeam = 0; iTeam < TeamPreviewMeshs.Num(); iTeam++)
	{
		MaxSize = FMath::Max(MaxSize, TeamPreviewMeshs[iTeam].Num());
	}*/
	MaxSize += 1.f;
/*	if (TeamPreviewMeshs.Num() < 2)
	{
		// players are across rather than angled.
		return ALL_CAMERA_OFFSET + (MaxSize * PLAYER_SPACING) / FMath::Tan(45.f * PI / 180.f);
	}*/
	float BaseCameraOffset = ALL_CAMERA_OFFSET + 0.5f * MaxSize * PLAYER_SPACING * FMath::Sin(TEAMANGLE * PI / 180.f);
	float Width = 2.f*MIN_TEAM_SPACING + 0.5f * MaxSize * PLAYER_SPACING * FMath::Cos(TEAMANGLE * PI / 180.f);
	return BaseCameraOffset + Width / FMath::Tan(45.f * PI / 180.f);
}

void SUTMatchSummaryPanel::ViewAll()
{
	CurrentlyViewedCharacter.Reset();
	TSharedPtr<FAllCamera> AllCam = MakeShareable(new FAllCamera);
	AllCam->CamFlags = CF_CanInteract | CF_ShowScoreboard;
	CameraShots.Empty();
	CameraShots.Add(AllCam);
	SetCamShot(0);
}

UUTScoreboard* SUTMatchSummaryPanel::GetScoreboard()
{
	AUTPlayerController* UTPC = GetPlayerOwner().IsValid() ? Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController) : nullptr;
	if (UTPC != nullptr && UTPC->MyUTHUD != nullptr)
	{
		UUTScoreboard* Scoreboard = UTPC->MyUTHUD->GetScoreboard();
		if (Scoreboard != nullptr)
		{
			//Make sure the hud owner is set before calling any Scoreboard functions
			Scoreboard->UTHUDOwner = UTPC->MyUTHUD;
			return Scoreboard;
		}
	}
	return nullptr;
}

/*void SUTMatchSummaryPanel::OnMouseDownPlayerPreview(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	MousePos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	//Try to Click the scoreboard first
	if (HasCamFlag(CF_ShowScoreboard) && HasCamFlag(CF_CanInteract))
	{
		UUTScoreboard* Scoreboard = GetScoreboard();
		if (Scoreboard != nullptr && Scoreboard->AttemptSelection(MousePos))
		{
			Scoreboard->SelectionClick();
			return;
		}
	}

	//Click a character
	if (HasCamFlag(CF_CanInteract) && !HasCamFlag(CF_Player) && HighlightedChar.IsValid())
	{
		ViewCharacter(HighlightedChar.Get());
	}
}*/

bool SUTMatchSummaryPanel::HasCamFlag(ECamFlags CamFlag) const
{
	TSharedPtr<FMatchCamera> Shot = GetCurrentShot();
	if (Shot.IsValid())
	{
		return (Shot->CamFlags & (uint32)CamFlag) > 0;
	}
	return false;
}
/*
void SUTMatchSummaryPanel::DragPlayerPreview(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (HasCamFlag(CF_CanInteract))
	{
		if (HasCamFlag(CF_Team))
		{
			//Scroll the team left/right
			if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && TeamPreviewMeshs.IsValidIndex(ViewedTeamNum) && TeamPreviewMeshs[ViewedTeamNum].Num() > 0)
			{
				float Dist = FVector::Dist(TeamPreviewMeshs[ViewedTeamNum][0]->GetActorLocation(), TeamPreviewMeshs[ViewedTeamNum][TeamPreviewMeshs[ViewedTeamNum].Num() - 1]->GetActorLocation());

				if (Dist > 0.0f)
				{
					TeamCamAlpha -= MouseEvent.GetCursorDelta().X / Dist;
					TeamCamAlpha = FMath::Clamp(TeamCamAlpha, 0.0f, 1.0f);
				}
				bAutoScrollTeam = false;
			}
		}
		else if (HasCamFlag(CF_Player) && ViewedChar.IsValid())
		{
			//Rotate the character
			if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
			{
				ViewedChar->SetActorRotation(ViewedChar->GetActorRotation() + FRotator(0, 0.2f * -MouseEvent.GetCursorDelta().X, 0.0f));
			}
			//Pan up and down
			else if (MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
			{
				if (CameraShots.Num() > 0)
				{
					FVector Location = CameraShots[0]->CameraTransform.GetLocation();
					Location.Z = FMath::Clamp(Location.Z + MouseEvent.GetCursorDelta().Y, -25.0f, 70.0f);
					CameraShots[0]->CameraTransform.SetLocation(Location);
				}
			}
		}
	}
}*/

/*void SUTMatchSummaryPanel::ZoomPlayerPreview(float WheelDelta)
{
	if (HasCamFlag(CF_CanInteract))
	{
		if (HasCamFlag(CF_Player) && WheelDelta < 0.0f)
		{
			ViewTeam(ViewedTeamNum);
		}
		else if (HasCamFlag(CF_Team) && WheelDelta < 0.0f)
		{
			ViewAll();
		}
	}

	//Send the scroll wheel to player input so they can change speed of the taunt
	UUTPlayerInput* UTInput = Cast<UUTPlayerInput>(GetPlayerOwner()->PlayerController->PlayerInput);
	if (UTInput != nullptr)
	{
		FKey ScrollKey = WheelDelta > 0.0f ? EKeys::MouseScrollUp : EKeys::MouseScrollDown;
		UTInput->InputKey(ScrollKey, EInputEvent::IE_Pressed, 1.0f, false);
	}
}*/

FReply SUTMatchSummaryPanel::OnSwitcherNext()
{
	if (TrophyRoom.IsValid())
	{
		if (HasCamFlag(CF_Team))
		{
			int32 Index = ViewedTeamNum + 1;
			ViewTeam(TrophyRoom->TeamGroups.IsValidIndex(Index) ? Index : 0);
		}
		else if (HasCamFlag(CF_Player) && ViewedChar.IsValid())
		{
			if (TrophyRoom.IsValid())
			{
				ViewCharacter(TrophyRoom->FindCharacter(TrophyRoom->NextCharacter(Cast<AUTPlayerState>(ViewedChar->PlayerState), false)));
			}
		}
	}
	if (ParentPanel.IsValid()) ParentPanel->FocusChat();
	return FReply::Handled();
}

FReply SUTMatchSummaryPanel::OnSwitcherPrevious()
{
	if (TrophyRoom.IsValid())
	{
		if (HasCamFlag(CF_Team))
		{
			int32 Index = ViewedTeamNum - 1;
			ViewTeam(TrophyRoom->TeamGroups.IsValidIndex(Index) ? Index : TrophyRoom->TeamGroups.Num() - 1);
		}
		else if (HasCamFlag(CF_Player) && ViewedChar.IsValid())
		{
			ViewCharacter(TrophyRoom->FindCharacter(TrophyRoom->NextCharacter(Cast<AUTPlayerState>(ViewedChar->PlayerState), true)));
		}
	}
	if (ParentPanel.IsValid()) ParentPanel->FocusChat();
	return FReply::Handled();
}

FText SUTMatchSummaryPanel::GetSwitcherText() const
{
	if (HasCamFlag(CF_Player) && ViewedChar.IsValid() && ViewedChar->PlayerState != nullptr)
	{
		return FText::FromString(ViewedChar->PlayerState->PlayerName);
	}
	if (HasCamFlag(CF_Team) && GameState.IsValid() && GameState->Teams.IsValidIndex(ViewedTeamNum) && (GameState->Teams[ViewedTeamNum] != NULL))
	{
		return FText::Format(NSLOCTEXT("SUTMatchSummaryPanel", "Team", "{0} Team"), GameState->Teams[ViewedTeamNum]->TeamName);
	}
	return FText::GetEmpty();
}

FSlateColor SUTMatchSummaryPanel::GetSwitcherColor() const
{
	if (HasCamFlag(CF_Team) && GameState.IsValid() && GameState->Teams.IsValidIndex(ViewedTeamNum) && GameState->Teams[ViewedTeamNum] != nullptr)
	{
		return FMath::LerpStable(GameState->Teams[ViewedTeamNum]->TeamColor, FLinearColor::White, 0.3f);
	}
	return FLinearColor::White;
}

EVisibility SUTMatchSummaryPanel::GetSwitcherVisibility() const
{
	if (HasCamFlag(CF_ShowSwitcher) && !GetSwitcherText().IsEmpty())
	{
		return EVisibility::Visible;
	}
	return EVisibility::Hidden;
}

EVisibility SUTMatchSummaryPanel::GetSwitcherButtonVisibility() const
{
	return (HasCamFlag(CF_CanInteract) && !HasCamFlag(CF_All)) ? EVisibility::Visible : EVisibility::Hidden;
}

FOptionalSize SUTMatchSummaryPanel::GetStatsWidth() const
{
	float WantedWidth = HasCamFlag(CF_ShowInfoWidget) ? 1050.f : 0.0f;
	StatsWidth = GameState.IsValid() && GameState->GetWorld() ? FMath::FInterpTo(StatsWidth, WantedWidth, GameState->GetWorld()->DeltaTimeSeconds, 10.0f) : 10.f;
	return FOptionalSize(StatsWidth);
}

FReply SUTMatchSummaryPanel::OnClose()
{
	if (GameState.IsValid())
	{
		int32 TeamToView = 0;
		if (GameState->GetMatchState() == MatchState::WaitingToStart)
		{
			/*if ((TeamPreviewMeshs.Num() > 1) && GetPlayerOwner().IsValid() && Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController) != nullptr)
			{
				TeamToView = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController)->GetTeamNum();
			}*/
			ViewTeam(TeamToView);
		}
		else if (GameState->GetMatchState() == MatchState::WaitingPostMatch)
		{
			/*if ((TeamPreviewMeshs.Num() > 1) && (GameState->WinningTeam != nullptr))
			{
				TeamToView = GameState->WinningTeam->GetTeamNum();
			}*/
			ViewTeam(TeamToView);
		}
		else
		{
			GetPlayerOwner()->CloseMatchSummary();
		}
	}
	else
	{
		GetPlayerOwner()->CloseMatchSummary();
	}
	return FReply::Handled();
}

FReply SUTMatchSummaryPanel::OnSendFriendRequest()
{
	if (ViewedChar.IsValid() && ViewedChar->PlayerState != nullptr)
	{
		if (FriendStatus != FFriendsStatus::FriendRequestPending && FriendStatus != FFriendsStatus::IsBot)
		{
			GetPlayerOwner()->RequestFriendship(ViewedChar->PlayerState->UniqueId.GetUniqueNetId());

			FriendPanel->ClearChildren();
			FriendPanel->AddSlot()
				.Padding(10.0, 0.0, 0.0, 0.0)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUTPlayerInfoDialog", "FriendRequestPending", "You have sent a friend request..."))
					.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
				];

			FriendStatus = FFriendsStatus::FriendRequestPending;
		}
	}
	return FReply::Handled();
}

FText SUTMatchSummaryPanel::GetFunnyText()
{
	return NSLOCTEXT("SUTPlayerInfoDialog", "FunnyDefault", "Viewing self.");
}

void SUTMatchSummaryPanel::BuildFriendPanel()
{
	if (ViewedChar.IsValid() && ViewedChar->PlayerState != nullptr)
	{
		FName NewFriendStatus;
		if (GetPlayerOwner()->PlayerController->PlayerState == ViewedChar->PlayerState)
		{
			NewFriendStatus = FFriendsStatus::IsYou;
		}
		else if (ViewedChar->PlayerState->bIsABot)
		{
			NewFriendStatus = FFriendsStatus::IsBot;
		}
		else
		{
			NewFriendStatus = GetPlayerOwner()->IsAFriend(ViewedChar->PlayerState->UniqueId) ? FFriendsStatus::Friend : FFriendsStatus::NotAFriend;
		}

		bool bRequiresRefresh = false;
		if (FriendStatus == FFriendsStatus::FriendRequestPending)
		{
			if (NewFriendStatus == FFriendsStatus::Friend)
			{
				FriendStatus = NewFriendStatus;
				bRequiresRefresh = true;
			}
		}
		else
		{
			bRequiresRefresh = FriendStatus != NewFriendStatus;
			FriendStatus = NewFriendStatus;
		}

		if (bRequiresRefresh)
		{
			FriendPanel->ClearChildren();
			if (FriendStatus == FFriendsStatus::IsYou)
			{
				FText FunnyText = GetFunnyText();
				FriendPanel->AddSlot()
					.Padding(10.0, 0.0, 0.0, 0.0)
					[
						SNew(STextBlock)
						.Text(FunnyText)
						.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
					];
			}
			else if (FriendStatus == FFriendsStatus::IsBot)
			{
				FriendPanel->AddSlot()
					.Padding(10.0, 0.0, 0.0, 0.0)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("SUTPlayerInfoDialog", "IsABot", "AI (C) Liandri Corp."))
						.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
					];
			}
			else if (FriendStatus == FFriendsStatus::Friend)
			{
				FriendPanel->AddSlot()
					.Padding(10.0, 0.0, 0.0, 0.0)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("SUTPlayerInfoDialog", "IsAFriend", "Is your friend"))
						.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
					];
			}
			else if (FriendStatus == FFriendsStatus::FriendRequestPending)
			{
			}
			else
			{
				FriendPanel->AddSlot()
					.AutoWidth()
					.Padding(10.0, 0.0, 0.0, 0.0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
						.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
						.Text(NSLOCTEXT("SUTPlayerInfoDialog", "SendFriendRequest", "Send Friend Request"))
						.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
						.OnClicked(this, &SUTMatchSummaryPanel::OnSendFriendRequest)
					];
			}
		}
	}
}

void SUTMatchSummaryPanel::ChangeViewingState(FName NewViewState)
{
	ViewingState = NewViewState;
}

FReply SUTMatchSummaryPanel::OnExpandViewClicked()
{
	if (ViewingState == MatchSummaryViewState::ViewingSingle)
	{
		uint32 TeamToView = CurrentlyViewedCharacter->GetTeamNum();
		if (TeamToView == 255)
		{
			TeamToView = 0;
		}
		ViewTeam(TeamToView);
		ChangeViewingState(MatchSummaryViewState::ViewingTeam);
	}

	return FReply::Handled();
}

EVisibility SUTMatchSummaryPanel::GetTeamViewVis() const
{
	return (ViewingState == MatchSummaryViewState::ViewingSingle) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SUTMatchSummaryPanel::GetExitViewVis() const
{
	return EVisibility::Visible;
}

FReply SUTMatchSummaryPanel::HideMatchPanel()
{
	//TIMTEMP:: Need to set viewtarget back
	/*AUTPlayerController* UTPC = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);
	if (UTPC)
	{
		//Make sure the view target is set back
		AActor* ViewTarget = UTPC;
		if (UTPC->GetPawn() != nullptr)
		{
			ViewTarget = UTPC->GetPawn();
		}
		else if (UTPC->GetSpectatorPawn() != nullptr)
		{
			ViewTarget = UTPC->GetSpectatorPawn();
		}
		UTPC->SetViewTarget(ViewTarget);
	}*/

	if (ParentPanel.IsValid())
	{
		ParentPanel->HideMatchSummary();
	}
	return FReply::Handled();
}

FReply SUTMatchSummaryPanel::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);
	if (UTPC != nullptr && UTPC->MyUTHUD != nullptr)
	{
		UTPC->MyUTHUD->bForceScores = HasCamFlag(CF_ShowScoreboard);
	}

	MousePos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	return FReply::Unhandled();
}

void SUTMatchSummaryPanel::MovePlayerPreview(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	MousePos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
}

FReply SUTMatchSummaryPanel::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	//Try to Click the scoreboard first
	if (HasCamFlag(CF_ShowScoreboard) && HasCamFlag(CF_CanInteract))
	{
		FVector2D MousePosition;
		UUTScoreboard* Scoreboard = GetScoreboard();
		if (Scoreboard != nullptr && Scoreboard->UTHUDOwner != nullptr && Scoreboard->UTHUDOwner->ScoreboardPage == 0 && GetGameMousePosition(MousePosition)
			&& (Scoreboard->UTHUDOwner->bShowScores || Scoreboard->UTHUDOwner->bForceScores)
			&& Scoreboard->AttemptSelection(MousePosition))
		{
			Scoreboard->SelectionClick();
			return FReply::Handled();
		}
	}

	//Click a character
	if (HasCamFlag(CF_CanInteract) && !HasCamFlag(CF_Player) && HighlightedChar.IsValid())
	{
		ViewCharacter(HighlightedChar.Get());
		return FReply::Handled();
	}
	return HandleInput(MouseEvent.GetEffectingButton(), EInputEvent::IE_Pressed);
}

FReply SUTMatchSummaryPanel::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Unhandled();
}

FReply SUTMatchSummaryPanel::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	/*if (TrophyRoom.IsValid())
	{
		TrophyRoom->MoveCamera(FVector(0.0f, 0.0f, MouseEvent.GetWheelDelta()));
	}*/

	//scroll wheel to leave character view
	/*if (HasCamFlag(CF_CanInteract))
	{
		if (HasCamFlag(CF_Player) && MouseEvent.GetWheelDelta() < 0.0f)
		{
			ViewAll();
		}
	}*/

	FKey ScrollKey = MouseEvent.GetWheelDelta() > 0.0f ? EKeys::MouseScrollUp : EKeys::MouseScrollDown;
	return HandleInput(ScrollKey, EInputEvent::IE_Pressed);
}

bool SUTMatchSummaryPanel::GetGameMousePosition(FVector2D& MousePosition)
{
	// We need to get the mouse input but the mouse event only has the mouse in screen space.  We need it in viewport space and there
	// isn't a good way to get there.  So we punt and just get it from the game viewport.
	UUTGameViewportClient* GVC = Cast<UUTGameViewportClient>(PlayerOwner->ViewportClient);
	if (GVC)
	{
		return GVC->GetMousePosition(MousePosition);
	}
	return false;
}

bool SUTMatchSummaryPanel::IsActionKey(const FKey Key, const FName ActionName)
{
	UUTPlayerInput* UTInput = Cast<UUTPlayerInput>(GetPlayerOwner()->PlayerController->PlayerInput);
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);

	if (UTInput != nullptr && UTPC != nullptr)
	{
		auto ActionKeys = UTInput->GetKeysForAction(ActionName);
		for (auto& ActionKey : ActionKeys)
		{
			if (Key == ActionKey.Key)
			{
				return true;
			}
		}
	}
	return false;
}

FReply SUTMatchSummaryPanel::HandleInput(const FKey Key, const EInputEvent InputEvent)
{
	UUTPlayerInput* UTInput = Cast<UUTPlayerInput>(GetPlayerOwner()->PlayerController->PlayerInput);
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);
	if (UTInput != nullptr && UTPC != nullptr)
	{
		if (InputEvent == EInputEvent::IE_Pressed && IsActionKey(Key, "PlayTaunt"))
		{
			UTPC->PlayTaunt();
			return FReply::Handled();
		}
		if (InputEvent == EInputEvent::IE_Pressed && IsActionKey(Key, "PlayTaunt2"))
		{
			UTPC->PlayTaunt2();
			return FReply::Handled();
		}
		if (InputEvent == EInputEvent::IE_Pressed && IsActionKey(Key, "SlowerEmote"))
		{
			UTPC->SlowerEmote();
			return FReply::Handled();
		}
		if (InputEvent == EInputEvent::IE_Pressed && IsActionKey(Key, "FasterEmote"))
		{
			UTPC->FasterEmote();
			return FReply::Handled();
		}
		if (InputEvent == EInputEvent::IE_Released && IsActionKey(Key, "ShowMenu"))
		{
			UTPC->execShowMenu();
			return FReply::Handled();
		}
		if (IsActionKey(Key, "ShowScores"))
		{
			UTPC->ToggleScoreboard(InputEvent == EInputEvent::IE_Pressed);
			return FReply::Handled();
		}
		if (InputEvent == EInputEvent::IE_Pressed && IsActionKey(Key, "StartFire"))
		{
			UTPC->ServerRestartPlayer();
			return FReply::Handled();
		}
		if (InputEvent == EInputEvent::IE_Pressed && IsActionKey(Key, "StartAltFire"))
		{
			UTPC->ServerRestartPlayerAltFire();
			return FReply::Handled();
		}

		//Special caster control case for starting the game with enter
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTPC->PlayerState);
		AUTGameState* GS = Cast<AUTGameState>(UTPC->GetWorld()->GameState);
		if (Key == EKeys::Enter && GS != nullptr && PS != nullptr && PS->bCaster && !PS->bReadyToPlay)
		{
			UTPC->ServerRestartPlayer();
			return FReply::Handled();
		}

		UUTScoreboard* Scoreboard = GetScoreboard();
		if (InputEvent == EInputEvent::IE_Pressed && Scoreboard != nullptr && Scoreboard->UTHUDOwner != nullptr && Scoreboard->UTHUDOwner->bShowScores)
		{
			if (Key == EKeys::Left)
			{
				Scoreboard->AdvancePage(-1);
				return FReply::Handled();
			}
			else if (Key == EKeys::Right)
			{
				Scoreboard->AdvancePage(+1);
				return FReply::Handled();
			}
			else if (Key == EKeys::Up)
			{
				UTPC->AdvanceStatsPage(-1);
				return FReply::Handled();
			}
			else if (Key == EKeys::Down)
			{
				UTPC->AdvanceStatsPage(+1);
				return FReply::Handled();
			}
		}
	}
	return FReply::Unhandled();
}

bool SUTMatchSummaryPanel::CanSelectPlayerState(AUTPlayerState* PS)
{
	AUTCharacter* UTC = FindCharacter(PS);
	if (UTC != nullptr && !UTC->bHidden)
	{
		return true;
	}
	return false;
}

void SUTMatchSummaryPanel::UpdatePlayerHighlight(UWorld* World, FVector Start, FVector Direction)
{
	//Check if the mouse is over a player and apply taccom effect
	if (HasCamFlag(CF_CanInteract) && !HasCamFlag(CF_Player))
	{
		FHitResult Hit;
		World->LineTraceSingleByChannel(Hit, Start, Start + Direction * 50000.0f, ECC_Pawn, FCollisionQueryParams(FName(TEXT("CharacterMouseOver"))));
		if (Hit.bBlockingHit && Cast<AUTCharacter>(Hit.GetActor()) != nullptr)
		{
			AUTCharacter* HitChar = Cast<AUTCharacter>(Hit.GetActor());
			if (HitChar != HighlightedChar && HighlightedChar != nullptr)
			{
				HighlightedChar->UpdateTacComMesh(false);
			}

			if (HitChar != nullptr)
			{
				HitChar->UpdateTacComMesh(true);
				HighlightedChar = HitChar;
			}
		}
		else if (HighlightedChar != nullptr)
		{
			HighlightedChar->UpdateTacComMesh(false);
			HighlightedChar = nullptr;
		}
	}
}

#endif
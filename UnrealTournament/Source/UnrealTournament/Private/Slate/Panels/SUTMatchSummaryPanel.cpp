// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

#if !UE_SERVER
#include "Runtime/AppFramework/Public/Widgets/Colors/SColorPicker.h"
#endif

#include "AssetData.h"

static const float PLAYER_SPACING = 75.0f;
static const float PLAYER_ALTOFFSET = 80.0f;
static const float MIN_TEAM_SPACING = 120.f;
static const float TEAM_CAMERA_OFFSET = 500.0f;
static const float TEAM_CAMERA_ZOFFSET = 0.0f;
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
	GameState = InArgs._GameState;

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

	// allocate a preview scene for rendering
	PlayerPreviewWorld = UWorld::CreateWorld(EWorldType::Game, true); // NOTE: Custom depth does not work with EWorldType::Preview
	PlayerPreviewWorld->bHack_Force_UsesGameHiddenFlags_True = true;
	PlayerPreviewWorld->bShouldSimulatePhysics = true;
#if WITH_EDITOR
	PlayerPreviewWorld->bEnableTraceCollision = false;
#endif
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(PlayerPreviewWorld);
	PlayerPreviewWorld->InitializeActorsForPlay(FURL(), true);
	ViewState.Allocate();
	{
		UClass* EnvironmentClass = LoadObject<UClass>(nullptr, TEXT("/Game/RestrictedAssets/UI/PlayerPreviewEnvironment.PlayerPreviewEnvironment_C"));
		PreviewEnvironment = PlayerPreviewWorld->SpawnActor<AActor>(EnvironmentClass, FVector(500.f, 50.f, 0.f), FRotator(0, 0, 0));
	}

	//Spawn a gamestate and add the taccom overlay so we can outline the character on mouse over
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.ObjectFlags |= RF_Transient;
	AUTGameState* NewGS = PlayerPreviewWorld->SpawnActor<AUTGameState>(GameState->GetClass(), SpawnInfo);
	if (NewGS != nullptr)
	{
		TSubclassOf<class AUTCharacter> DefaultPawnClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *GetDefault<AUTGameMode>()->PlayerPawnObject.ToStringReference().ToString(), NULL, LOAD_NoWarn));
		if (DefaultPawnClass != nullptr)
		{
			NewGS->AddOverlayMaterial(DefaultPawnClass.GetDefaultObject()->TacComOverlayMaterial);
		}
	}
	
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(NULL, TEXT("/Game/RestrictedAssets/UI/PlayerPreviewProxy.PlayerPreviewProxy"));
	if (BaseMat != NULL)
	{
		PlayerPreviewTexture = Cast<UUTCanvasRenderTarget2D>(UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(GetPlayerOwner().Get(), UUTCanvasRenderTarget2D::StaticClass(), CurrentViewportSize.X, CurrentViewportSize.Y));
		PlayerPreviewTexture->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
		PlayerPreviewTexture->OnNonUObjectRenderTargetUpdate.BindSP(this, &SUTMatchSummaryPanel::UpdatePlayerRender);
		PlayerPreviewMID = UMaterialInstanceDynamic::Create(BaseMat, PlayerPreviewWorld);
		PlayerPreviewMID->SetTextureParameterValue(FName(TEXT("TheTexture")), PlayerPreviewTexture);
		PlayerPreviewBrush = new FSlateMaterialBrush(*PlayerPreviewMID, CurrentViewportSize);
	}
	else
	{
		PlayerPreviewTexture = NULL;
		PlayerPreviewMID = NULL;
		PlayerPreviewBrush = new FSlateMaterialBrush(*UMaterial::GetDefaultMaterial(MD_Surface), CurrentViewportSize);
	}

	PlayerPreviewTexture->TargetGamma = GEngine->GetDisplayGamma();
	PlayerPreviewTexture->InitCustomFormat(CurrentViewportSize.X, CurrentViewportSize.Y, PF_B8G8R8A8, false);
	PlayerPreviewTexture->UpdateResourceImmediate();

	FVector2D ResolutionScale(CurrentViewportSize.X / 1280.0f, CurrentViewportSize.Y / 720.0f);

	UUTGameUserSettings* Settings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());

	// Add the preview area
	Content->AddSlot()
	[
		SNew(SScaleBox)
		.Stretch(EStretch::ScaleToFill)
		[
			SNew(SDragImage)
			.Image(PlayerPreviewBrush)
			.OnDrag(this, &SUTMatchSummaryPanel::DragPlayerPreview)
			.OnZoom(this, &SUTMatchSummaryPanel::ZoomPlayerPreview)
			.OnMousePressed(this, &SUTMatchSummaryPanel::OnMouseDownPlayerPreview)
		]
	];


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

	// Add the Chat Display
	Content->AddSlot()
	.VAlign(VAlign_Bottom)
	.HAlign(HAlign_Left)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(140.0f)
			.WidthOverride(800.0f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.7))
					.BorderImage(SUWindowsStyle::Get().GetBrush("UT.Background.Dark"))
					.Padding(0)
				]
				+ SOverlay::Slot()
				[
					SAssignNew(ChatScroller, SScrollBox)
					+ SScrollBox::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.VAlign(VAlign_Bottom)
						.AutoHeight()
						[
							SAssignNew(ChatDisplay, SRichTextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UWindows.Chat.Text.Global")
							.Justification(ETextJustify::Left)
							.DecoratorStyleSet(&SUWindowsStyle::Get())
							.AutoWrapText(true)
						]
					]
				]
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(ButtonOffset)
			[
				SNew(SCanvas)
			]
		]
	];


	// Create a TeamAnchor for each team found
	int32 TeamCount = 0;
	if (GameState.IsValid())
	{
		for (TActorIterator<AUTTeamInfo> It(GameState->GetWorld()); It; ++It)
		{
			TeamCount++;
		}
	}
	TeamCount = FMath::Max(TeamCount, 1);

	for (int32 iTeam = 0; iTeam < TeamCount; iTeam++)
	{
		AActor* TeamAnchor = PlayerPreviewWorld->SpawnActor<AActor>(ATargetPoint::StaticClass(), FVector(0.f, 0.f, 0.0f), FRotator::ZeroRotator);
		if (TeamAnchor != nullptr)
		{
			TeamAnchors.Add(TeamAnchor);
		}
	}

	ViewTeam(0);

	// Turn on Screen Space Reflection max quality
	auto SSRQualityCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SSR.Quality"));
	OldSSRQuality = SSRQualityCVar->GetInt();
	SSRQualityCVar->Set(4, ECVF_SetByCode);
}

void SUTMatchSummaryPanel::SetInitialCams()
{
	//Set the camera state based on the game state
	if (GameState.IsValid())
	{
		if (!GameState->HasMatchStarted())
		{
			SetupIntroCam();
		}
		//View the winning team at the end of game
		else if(GameState->GetMatchState() == MatchState::WaitingPostMatch)
		{
			SetupMatchCam();
		}
		else
		{
			int32 TeamToView = 0;
			if (GetPlayerOwner().IsValid() && Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController) != nullptr)
			{
				TeamToView = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController)->GetTeamNum();
			}
			ViewTeam(TeamToView);
		}
	}
}

SUTMatchSummaryPanel::~SUTMatchSummaryPanel()
{
	// Reset Screen Space Reflection max quality, wish there was a cleaner way to reset the flags
	auto SSRQualityCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SSR.Quality"));
	EConsoleVariableFlags Flags = SSRQualityCVar->GetFlags();
	Flags = (EConsoleVariableFlags)(((uint32)Flags & ~ECVF_SetByMask) | ECVF_SetByScalability);
	SSRQualityCVar->Set(OldSSRQuality, ECVF_SetByCode);
	SSRQualityCVar->SetFlags(Flags);

	if (!GExitPurge)
	{
		if (PlayerPreviewTexture != NULL)
		{
			PlayerPreviewTexture->OnNonUObjectRenderTargetUpdate.Unbind();
			PlayerPreviewTexture = NULL;
		}
		FlushRenderingCommands();
		if (PlayerPreviewBrush != NULL)
		{
			// FIXME: Slate will corrupt memory if this is deleted. Must be referencing it somewhere that doesn't get cleaned up...
			//		for now, we'll take the minor memory leak (the texture still gets GC'ed so it's not too bad)
			//delete PlayerPreviewBrush;
			PlayerPreviewBrush->SetResourceObject(NULL);
			PlayerPreviewBrush = NULL;
		}
		for (int32 i = 0; i < PlayerPreviewMeshs.Num(); i++)
		{
			PlayerPreviewMeshs[i]->Destroy();
		}
		PlayerPreviewMeshs.Empty();
		if (PlayerPreviewWorld != NULL)
		{
			PlayerPreviewWorld->DestroyWorld(true);
			GEngine->DestroyWorldContext(PlayerPreviewWorld);
			PlayerPreviewWorld = NULL;
			GetPlayerOwner()->GetWorld()->ForceGarbageCollection(true);
		}
	}
	ViewState.Destroy();
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

				//Cap at 5 highlights
				if (Highlights.Num() > 5)
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
}

void SUTMatchSummaryPanel::UpdateChatText()
{
	if (PlayerOwner->ChatArchive.Num() != LastChatCount)
	{
		LastChatCount = PlayerOwner->ChatArchive.Num();

		FString RichText = TEXT("");
		for (int32 i = 0; i<PlayerOwner->ChatArchive.Num(); i++)
		{
			TSharedPtr<FStoredChatMessage> Msg = PlayerOwner->ChatArchive[i];
			if (i>0) RichText += TEXT("\n");
			if (Msg->Type != ChatDestinations::MOTD)
			{
				FString Style;
				FString PlayerName = Msg->Sender + TEXT(": ");

				if (Msg->Type == ChatDestinations::Friends)
				{
					Style = TEXT("UWindows.Chat.Text.Friends");
				}
				else if (Msg->Type == ChatDestinations::System || Msg->Type == ChatDestinations::MOTD)
				{
					Style = TEXT("UWindows.Chat.Text.Admin");
					PlayerName.Empty(); //Don't show player name for system messages
				}
				else if (Msg->Type == ChatDestinations::Lobby)
				{
					Style = TEXT("UWindows.Chat.Text.Lobby");
				}
				else if (Msg->Type == ChatDestinations::Match)
				{
					Style = TEXT("UWindows.Chat.Text.Match");
				}
				else if (Msg->Type == ChatDestinations::Local)
				{
					Style = TEXT("UWindows.Chat.Text.Local");
				}
				else if (Msg->Type == ChatDestinations::Team)
				{
					if (Msg->Color.R > Msg->Color.B)
					{
						Style = TEXT("UWindows.Chat.Text.Team.Red");
					}
					else
					{
						Style = TEXT("UWindows.Chat.Text.Team.Blue");
					}
				}
				else
				{
					Style = TEXT("UWindows.Chat.Text.Global");
				}

				RichText += FString::Printf(TEXT("<%s>%s%s</>"), *Style, *PlayerName, *Msg->Message);
			}
			else
			{
				RichText += Msg->Message;
			}
		}

		ChatDisplay->SetText(FText::FromString(RichText));
		ChatScroller->SetScrollOffset(290999);
	}
}

void SUTMatchSummaryPanel::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PlayerPreviewTexture);
	Collector.AddReferencedObject(PlayerPreviewMID);
	Collector.AddReferencedObject(PlayerPreviewWorld);

	for (int32 i = 0; i < TeamPreviewMeshs.Num(); i++)
	{
		Collector.AddReferencedObjects(TeamPreviewMeshs[i]);
	}
	Collector.AddReferencedObjects(PreviewAnimations);
}

void SUTMatchSummaryPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (GameState.IsValid() && (GameState->GetMatchState() == MatchState::WaitingToStart))
	{
		// recreate players if something has changed
		bool bPlayersAreValid = true;
		int32 TotalPlayers = 0;

		// @TODO FIXMESTEVE - this could be reported to match summary on valid change, rather than checking every tick
		TArray<AUTCharacter*> &TeamCharacters = TeamPreviewMeshs[ViewedTeamNum];
		for (int32 iPlayer = 0; iPlayer < TeamCharacters.Num(); iPlayer++)
		{
			AUTPlayerState* PS = (TeamCharacters[iPlayer] && TeamCharacters[iPlayer]->PlayerState) ? Cast<AUTPlayerState>(TeamCharacters[iPlayer]->PlayerState) : NULL;
			if (!PS || PS->bOnlySpectator || PS->IsPendingKillPending() || (GameState->bTeamGame && (!PS->Team || (PS->Team->TeamIndex != ViewedTeamNum))))
			{
				bPlayersAreValid = false;
				break;
			}
			if (!PS->bIsInactive)
			{
				TotalPlayers++;
			}
		}
		if (TotalPlayers != GameState->PlayerArray.Num())
		{
			bPlayersAreValid = false;
		}
		if (!bPlayersAreValid)
		{
			RecreateAllPlayers(ViewedTeamNum);
		}
	}
	if (PlayerPreviewWorld != nullptr)
	{
		PlayerPreviewWorld->Tick(LEVELTICK_All, InDeltaTime);
	}

	if ( PlayerPreviewTexture != nullptr )
	{
		PlayerPreviewTexture->UpdateResource();
	}

	UpdateChatText();
	BuildFriendPanel();

	if (CameraShots.IsValidIndex(CurrentShot) && GameState.IsValid())
	{
		float ElapsedTime = GameState->GetWorld()->RealTimeSeconds - ShotStartTime;
		if (CameraShots[CurrentShot]->TickCamera(this, ElapsedTime, InDeltaTime, CameraTransform))
		{
			SetCamShot(CurrentShot+1);
		}
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
		XPBar->SetVisibility(EVisibility::Visible);
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
		for (int32 iTeam = 0; iTeam < TeamPreviewMeshs.Num(); iTeam++)
		{
			for (int32 iCharacter = 0; iCharacter < TeamPreviewMeshs[iTeam].Num(); iCharacter++)
			{
				if (TeamAnchors.IsValidIndex(iTeam))
				{
					TeamPreviewMeshs[iTeam][iCharacter]->SetActorRotation(TeamAnchors[iTeam]->GetActorRotation());
				}
			}
		}
	}
}

void SUTMatchSummaryPanel::SetupIntroCam()
{
	CameraShots.Empty();

	//View teams
	int32 NumViewTeams = FMath::Max(GameState->Teams.Num(), 1);

	//7 seconds for the team camera pan works well with the current song
	float TimePerTeam = 6.8f / NumViewTeams;

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
	if (TeamAnchors.IsValidIndex(TeamNum))
	{
		int32 TeamCharacterNum = TeamPreviewMeshs.IsValidIndex(TeamNum) ? TeamPreviewMeshs[TeamNum].Num() : 5;
		float CamZOffset = (TeamCharacterNum > 5) ? LARGETEAM_CAMERA_ZOFFSET : TEAM_CAMERA_ZOFFSET;

		FVector Dir = TeamAnchors[TeamNum]->GetActorRotation().Vector();
		FVector Location = TeamAnchors[TeamNum]->GetActorLocation() + (Dir.GetSafeNormal() * TEAM_CAMERA_OFFSET) + FVector(0.0f, 0.0f, CamZOffset);
		Dir = FVector(0.f, 0.f, 45.f) + TeamAnchors[TeamNum]->GetActorLocation() - Location;
		Start.SetLocation(Location - 100.f * Dir.GetSafeNormal());
		Start.SetRotation(Dir.Rotation().Quaternion());
		End.SetLocation(Location);
		End.SetRotation(Dir.Rotation().Quaternion());
	}
}

void SUTMatchSummaryPanel::SetupMatchCam()
{
	int32 TeamToView = 0;
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

	SetCamShot(0);
}

void SUTMatchSummaryPanel::HideAllPlayersBut(AUTCharacter* UTC)
{
	// hide everyone else, show this player
	for (int32 i = 0; i< PlayerPreviewMeshs.Num(); i++)
	{
		PlayerPreviewMeshs[i]->HideCharacter(PlayerPreviewMeshs[i] != UTC);
	}
	for (auto Weapon : PreviewWeapons)
	{
		AUTCharacter* Holder = Cast<AUTCharacter>(Weapon->Instigator);
		Weapon->SetActorHiddenInGame(Holder != UTC);
	}
}

void SUTMatchSummaryPanel::RecreateAllPlayers(int32 TeamIndex)
{
	//Destroy all the characters and their weapons
	for (auto Weapon : PreviewWeapons)
	{
		Weapon->Destroy();
	}
	PreviewWeapons.Empty();

	for (auto Char : PlayerPreviewMeshs)
	{
		Char->Destroy();
	}
	PlayerPreviewMeshs.Empty();
	TeamPreviewMeshs.Empty();
	if (TeamIndex == 255)
	{
		TeamIndex = 0;
	}

	//Gather All of the playerstates
	TArray<TArray<class AUTPlayerState*> > TeamPlayerStates;
	if (GameState.IsValid())
	{
		for (TActorIterator<AUTPlayerState> It(GameState->GetWorld()); It; ++It)
		{
			AUTPlayerState* PS = *It;

			if (!PS->bOnlySpectator && !PS->IsPendingKillPending() && (!PS->bIsInactive ||(GameState->HasMatchStarted() && (PS->Score > 0.f))))
			{
				int32 TeamNum = PS->GetTeamNum() == 255 ? 0 : PS->GetTeamNum();
				if (!TeamPlayerStates.IsValidIndex(TeamNum))
				{
					TeamPlayerStates.SetNum(TeamNum + 1);
				}
				TeamPlayerStates[TeamNum].Add(PS);
			}
		}
	}

	if (!TeamPlayerStates.IsValidIndex(TeamIndex))
	{
		return;
	}
	// determine match highlight scores to use for sorting
	for (int32 i = 0; i < TeamPlayerStates[TeamIndex].Num(); i++)
	{
		AUTPlayerState* PS = TeamPlayerStates[TeamIndex][i];
		if (PS != nullptr)
		{
			PS->MatchHighlightScore = GameState->MatchHighlightScore(PS);
		}
	}

	// sort winners
	bool(*SortFunc)(const AUTPlayerState&, const AUTPlayerState&);
	SortFunc = [](const AUTPlayerState& A, const AUTPlayerState& B)
	{
		return (A.MatchHighlightScore > B.MatchHighlightScore);
	};
	TeamPlayerStates[TeamIndex].Sort(SortFunc);

	//Create an actor that all team actors will attach to for easy team manipulation
	if (TeamAnchors.IsValidIndex(TeamIndex))
	{
		float BaseOffsetY = 2.f * PLAYER_SPACING;
		//Spawn all of the characters for this team
		for (int32 iPlayer = 0; iPlayer < TeamPlayerStates[TeamIndex].Num(); iPlayer++)
		{
			int32 PlayerRowIndex = iPlayer % 5;
			int32 PlayerRow = iPlayer / 5;
			// X is forward to back, Y is left to right
			float CurrentOffsetX = 0.25f * (5.f - PlayerRowIndex) *  PLAYER_ALTOFFSET - 1.6f * PLAYER_ALTOFFSET * PlayerRow;
			float CurrentOffsetY = ((PlayerRowIndex % 2 == 0) ? PLAYER_SPACING * (int32(PlayerRowIndex / 2) + 0.8f*PlayerRow + 2) : PLAYER_SPACING * (2 - int32((PlayerRowIndex + 1) / 2)) - 0.8f*PlayerRow) - BaseOffsetY;
			float CurrentOffsetZ = 10.f * PlayerRow;
			AUTCharacter* NewCharacter = RecreatePlayerPreview(TeamPlayerStates[TeamIndex][iPlayer], FVector(CurrentOffsetX, CurrentOffsetY, CurrentOffsetZ), FRotator(0.f));
			NewCharacter->AttachRootComponentToActor(TeamAnchors[TeamIndex], NAME_None, EAttachLocation::KeepWorldPosition);

			//Add the character to the team list
			if (!TeamPreviewMeshs.IsValidIndex(TeamIndex))
			{
				TeamPreviewMeshs.SetNum(TeamIndex + 1);
			}
			TeamPreviewMeshs[TeamIndex].Add(NewCharacter);
		}
	}
}

static int32 WeaponIndex = 0;

AUTCharacter* SUTMatchSummaryPanel::RecreatePlayerPreview(AUTPlayerState* NewPS, FVector Location, FRotator Rotation)
{
	AUTWeaponAttachment* PreviewWeapon = nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TSubclassOf<class APawn> DefaultPawnClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *GetDefault<AUTGameMode>()->PlayerPawnObject.ToStringReference().ToString(), NULL, LOAD_NoWarn));

	AUTCharacter* PlayerPreviewMesh = PlayerPreviewWorld->SpawnActor<AUTCharacter>(DefaultPawnClass, Location, Rotation, SpawnParams);
	
	if (PlayerPreviewMesh)
	{
		// We need to get our tick functions registered, this seemed like best way to do it
		PlayerPreviewMesh->BeginPlay();

		PlayerPreviewMesh->PlayerState = NewPS; //PS needed for team colors
		PlayerPreviewMesh->Health = 100; //Set to 100 so the TacCom Overlay doesn't show damage
		PlayerPreviewMesh->DeactivateSpawnProtection();
		
		PlayerPreviewMesh->ApplyCharacterData(NewPS->GetSelectedCharacter());
		PlayerPreviewMesh->NotifyTeamChanged();

		PlayerPreviewMesh->SetHatClass(NewPS->HatClass);
		PlayerPreviewMesh->SetHatVariant(NewPS->HatVariant);
		PlayerPreviewMesh->SetEyewearClass(NewPS->EyewearClass);
		PlayerPreviewMesh->SetEyewearVariant(NewPS->EyewearVariant);

		int32 WeaponIndexToSpawn = 0;
		if (!PreviewWeapon)
		{
			UClass* PreviewAttachmentType = NewPS->FavoriteWeapon ? NewPS->FavoriteWeapon->GetDefaultObject<AUTWeapon>()->AttachmentType : NULL;
			if (!PreviewAttachmentType)
			{
				// @TODO FIXMESTEVE - should always have a favorite weapon (choose from stats)
				UClass* PreviewAttachments[6];
				PreviewAttachments[0] = LoadClass<AUTWeaponAttachment>(NULL, TEXT("/Game/RestrictedAssets/Weapons/LinkGun/BP_LinkGun_Attach.BP_LinkGun_Attach_C"), NULL, LOAD_None, NULL);
				PreviewAttachments[1] = LoadClass<AUTWeaponAttachment>(NULL, TEXT("/Game/RestrictedAssets/Weapons/Sniper/BP_Sniper_Attach.BP_Sniper_Attach_C"), NULL, LOAD_None, NULL);
				PreviewAttachments[2] = LoadClass<AUTWeaponAttachment>(NULL, TEXT("/Game/RestrictedAssets/Weapons/RocketLauncher/BP_Rocket_Attachment.BP_Rocket_Attachment_C"), NULL, LOAD_None, NULL);
				PreviewAttachments[3] = LoadClass<AUTWeaponAttachment>(NULL, TEXT("/Game/RestrictedAssets/Weapons/ShockRifle/ShockAttachment.ShockAttachment_C"), NULL, LOAD_None, NULL);
				PreviewAttachments[4] = LoadClass<AUTWeaponAttachment>(NULL, TEXT("/Game/RestrictedAssets/Weapons/Flak/BP_Flak_Attach.BP_Flak_Attach_C"), NULL, LOAD_None, NULL);
				PreviewAttachments[5] = PreviewAttachments[3];
				WeaponIndexToSpawn = WeaponIndex % 6;
				PreviewAttachmentType = PreviewAttachments[WeaponIndexToSpawn];
				WeaponIndex++;
			}
			if (PreviewAttachmentType != NULL)
			{
				PreviewWeapon = PlayerPreviewWorld->SpawnActor<AUTWeaponAttachment>(PreviewAttachmentType, FVector(0, 0, 0), FRotator(0, 0, 0));
			}
		}
		if (PreviewWeapon)
		{
			PreviewWeapon->Instigator = PlayerPreviewMesh;
			PreviewWeapon->BeginPlay();
			PreviewWeapon->AttachToOwner();
			PreviewWeapons.Add(PreviewWeapon);
		}

		if (NewPS->IsFemale())
		{
			switch (WeaponIndexToSpawn)
			{
			case 1:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPoseFemale_Sniper.MatchPoseFemale_Sniper"));
				break;
			case 2:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPoseFemale_Flak_B.MatchPoseFemale_Flak_B"));
				break;
			case 4:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPoseFemale_Flak.MatchPoseFemale_Flak"));
				break;
			case 0:
			case 3:
			case 5:
			default:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPoseFemale_ShockRifle.MatchPoseFemale_ShockRifle"));
			}
		}
		else
		{
			switch (WeaponIndexToSpawn)
			{
			case 1:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPose_Sniper.MatchPose_Sniper"));
				break;
			case 2:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPose_Flak_B.MatchPose_Flak_B"));
				break;
			case 4:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPose_Flak.MatchPose_Flak"));
				break;
			case 0:
			case 3:
			case 5:
			default:
				PlayerPreviewAnim = LoadObject<UAnimationAsset>(NULL, TEXT("/Game/RestrictedAssets/Animations/Universal/Misc_Poses/MatchPose_ShockRifle.MatchPose_ShockRifle"));
			}
		}
		
		PreviewAnimations.AddUnique(PlayerPreviewAnim);

		PlayerPreviewMesh->GetMesh()->PlayAnimation(PlayerPreviewAnim, true);
		PlayerPreviewMesh->GetMesh()->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;

		PlayerPreviewMeshs.Add(PlayerPreviewMesh);
		return PlayerPreviewMesh;
	}
	return nullptr;
}

void SUTMatchSummaryPanel::UpdatePlayerRender(UCanvas* C, int32 Width, int32 Height)
{
	FEngineShowFlags ShowFlags(ESFIM_Game);
	//ShowFlags.SetLighting(false); // FIXME: create some proxy light and use lit mode
	ShowFlags.SetMotionBlur(false);
	ShowFlags.SetGrain(false);
	//ShowFlags.SetPostProcessing(false);
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(PlayerPreviewTexture->GameThread_GetRenderTargetResource(), PlayerPreviewWorld->Scene, ShowFlags).SetRealtimeUpdate(true));

	//	EngineShowFlagOverride(ESFIM_Game, VMI_Lit, ViewFamily.EngineShowFlags, NAME_None, false);
	const float PreviewFOV = 45;
	const float AspectRatio = Width / (float)Height;

	FSceneViewInitOptions PlayerPreviewInitOptions;
	PlayerPreviewInitOptions.SetViewRectangle(FIntRect(0, 0, C->SizeX, C->SizeY));
	PlayerPreviewInitOptions.ViewOrigin = CameraTransform.GetLocation();
	PlayerPreviewInitOptions.ViewRotationMatrix = FInverseRotationMatrix(CameraTransform.Rotator());
	PlayerPreviewInitOptions.ViewRotationMatrix = PlayerPreviewInitOptions.ViewRotationMatrix * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));
	PlayerPreviewInitOptions.ProjectionMatrix =
		FReversedZPerspectiveMatrix(
		FMath::Max(0.001f, PreviewFOV) * (float)PI / 360.0f,
		AspectRatio,
		1.0f,
		GNearClippingPlane);
	PlayerPreviewInitOptions.ViewFamily = &ViewFamily;
	PlayerPreviewInitOptions.SceneViewStateInterface = ViewState.GetReference();
	PlayerPreviewInitOptions.BackgroundColor = FLinearColor::Black;
	PlayerPreviewInitOptions.WorldToMetersScale = GetPlayerOwner()->GetWorld()->GetWorldSettings()->WorldToMeters;
	PlayerPreviewInitOptions.CursorPos = FIntPoint(-1, -1);

	ViewFamily.bUseSeparateRenderTarget = true;

	FSceneView* View = new FSceneView(PlayerPreviewInitOptions); // note: renderer gets ownership
	View->ViewLocation = FVector::ZeroVector;
	View->ViewRotation = FRotator::ZeroRotator;
	FPostProcessSettings PPSettings = GetDefault<AUTPlayerCameraManager>()->DefaultPPSettings;

	ViewFamily.Views.Add(View);

	View->StartFinalPostprocessSettings(CameraTransform.GetLocation());
	//View->OverridePostProcessSettings(PPSettings, 1.0f);
	View->EndFinalPostprocessSettings(PlayerPreviewInitOptions);

	// workaround for hacky renderer code that uses GFrameNumber to decide whether to resize render targets
	--GFrameNumber;
	GetRendererModule().BeginRenderingViewFamily(C->Canvas, &ViewFamily);

	// Force the preview mesh and weapons to put the highest mips into memory if visible. This assumes each char has a weapon
	FConvexVolume Frustum;
	GetViewFrustumBounds(Frustum, PlayerPreviewInitOptions.ComputeViewProjectionMatrix(), true);
	for (auto Weapon : PreviewWeapons)
	{
		AUTCharacter* Holder = Cast<AUTCharacter>(Weapon->Instigator);
		if (Holder != nullptr)
		{
			if (!Holder->bHidden)
			{
				FVector Origin, BoxExtent;
				Holder->GetActorBounds(true, Origin, BoxExtent);

				if (Frustum.IntersectBox(Origin, BoxExtent))
				{
					if (Holder->Hat)
					{
						Holder->Hat->PrestreamTextures(1, true);
					}
					Holder->PrestreamTextures(1, true);
					Weapon->PrestreamTextures(1, true);
					continue;
				}
			}
			Holder->PrestreamTextures(0, false);
			Weapon->PrestreamTextures(0, false);
			if (Holder->Hat)
			{
				Holder->Hat->PrestreamTextures(0, false);
			}
		}
	}

	//Check if the mouse is over a player and apply taccom effect
	if (HasCamFlag(CF_CanInteract) && !HasCamFlag(CF_Player))
	{
		FVector Start, Direction;
		View->DeprojectFVector2D(MousePos, Start, Direction);

		FHitResult Hit;
		PlayerPreviewWorld->LineTraceSingleByChannel(Hit, Start, Start + Direction * 50000.0f, COLLISION_TRACE_WEAPON, FCollisionQueryParams(FName(TEXT("CharacterMouseOver"))));
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

	//Draw the player names above their heads
	if (HasCamFlag(CF_ShowPlayerNames))
	{
		//Helper for making sure player names don't overlap
		//TODO: do this better. Smooth the spacing of names when they overlap
		struct FPlayerName
		{
			FPlayerName(FString InPlayerName, FString InHighlight, FVector InLocation3D, FVector2D InLocation, FVector2D InSize, UFont* InFont)
				: PlayerName(InPlayerName), Highlight(InHighlight), Location3D(InLocation3D), Location(InLocation), Size(InSize), DrawFont(InFont) {}
			FString PlayerName;
			FString Highlight;
			FVector Location3D;
			FVector2D Location;
			FVector2D Size;
			UFont* DrawFont;
		};

		UFont* HighlightFont = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->TinyFont;
		UFont* SmallFont = HasCamFlag(CF_Team) ? AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->SmallFont : AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->TinyFont;
		AUTCharacter* SelectedChar = ViewedChar.IsValid() ? ViewedChar.Get() : (HighlightedChar.IsValid() ? HighlightedChar.Get() : NULL);

		//Gather all of the player names
		TArray<FPlayerName> PlayerNames;
		int32 NumHighlights = 0;
		for (AUTCharacter* UTC : PlayerPreviewMeshs)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(UTC->PlayerState);
			if (PS != nullptr && !UTC->bHidden && FVector::DotProduct(CameraTransform.Rotator().Vector(), (UTC->GetActorLocation() - CameraTransform.GetLocation())) > 0.0f)
			{
				FVector ActorLocation = UTC->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f);
				FVector2D ScreenLoc;
				View->WorldToPixel(ActorLocation, ScreenLoc);

				FString MainHighlight = ((NumHighlights < 5) && HasCamFlag(CF_Highlights)) ? GameState->ShortPlayerHighlightText(PS).ToString() : TEXT("");
				NumHighlights++;
				PlayerNames.Add(FPlayerName(PS->PlayerName, MainHighlight, ActorLocation, ScreenLoc, FVector2D(0.f, 0.f), SmallFont));
			}
		}

		int32 NumNames = HasCamFlag(CF_Highlights) ? FMath::Min(PlayerNames.Num(), 5) : PlayerNames.Num();
		for (int32 i = 0; i < NumNames; i++)
		{
			//Draw the Player name
			FFontRenderInfo FontInfo;
			FontInfo.bEnableShadow = true;
			FontInfo.bClipText = true;
			C->DrawColor = FColor::White;
			float XL = 0.f, YL = 0.f;
			C->TextSize(PlayerNames[i].DrawFont, PlayerNames[i].PlayerName, XL, YL);
			PlayerNames[i].Size = FVector2D(XL, YL);
			C->DrawText(PlayerNames[i].DrawFont, FText::FromString(PlayerNames[i].PlayerName), PlayerNames[i].Location.X - 0.5f*XL, PlayerNames[i].Location.Y - 0.5f*YL, 1.0f, 1.0f, FontInfo);
			if (HasCamFlag(CF_Highlights))
			{
				C->DrawColor = FColor::Yellow;
				C->TextSize(HighlightFont, PlayerNames[i].Highlight, XL, YL);
				C->DrawText(HighlightFont, FText::FromString(PlayerNames[i].Highlight), PlayerNames[i].Location.X - 0.5f*XL, PlayerNames[i].Location.Y + 0.25f * YL, 1.0f, 1.0f, FontInfo);
				PlayerNames[i].Size.X = FMath::Max(PlayerNames[i].Size.X, XL);
				PlayerNames[i].Size.Y += YL;
			}

			//Move the remaining names out of the way
			for (int32 j = i + 1; j < PlayerNames.Num(); j++)
			{
				FPlayerName& A = PlayerNames[i];
				FPlayerName& B = PlayerNames[j];

				//if the names intersect, move it up along the Y axis
				if (A.Location.X < B.Location.X + B.Size.X && A.Location.X + A.Size.X > B.Location.X
					&& A.Location.Y < B.Location.Y + B.Size.Y && A.Location.Y + A.Size.Y > B.Location.Y)
				{
					B.Location.Y = A.Location.Y - (B.Size.Y * 0.6f);
				}
			}
		}
	}

	//Draw any needed hud canvas stuff
	AUTPlayerController* UTPC = GetPlayerOwner().IsValid() ? Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController) : nullptr;
	if (UTPC != nullptr && UTPC->MyUTHUD != nullptr)
	{
		TArray<UUTHUDWidget*> DrawWidgets;

		//Draw the scoreboard if its not the intro
		if (HasCamFlag(CF_ShowScoreboard))
		{
			UUTScoreboard* Scoreboard = UTPC->MyUTHUD->GetScoreboard();
			if (Scoreboard != nullptr)
			{
				DrawWidgets.Add(Scoreboard);
			}
		}

		UUTHUDWidget* GameMessagesWidget = UTPC->MyUTHUD->FindHudWidgetByClass(UUTHUDWidgetMessage_GameMessages::StaticClass(), true);
		if (GameMessagesWidget != nullptr)
		{
			DrawWidgets.Add(GameMessagesWidget);
		}

		//Draw all the widgets
		for (auto Widget : DrawWidgets)
		{
			Widget->PreDraw(UTPC->GetWorld()->DeltaTimeSeconds, UTPC->MyUTHUD, C, FVector2D(C->SizeX * 0.5f, C->SizeY * 0.5f));
			Widget->Draw(UTPC->GetWorld()->DeltaTimeSeconds);
			Widget->PostDraw(UTPC->GetWorld()->GetTimeSeconds());
		}
	}
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
		for (AUTCharacter* UTC : PlayerPreviewMeshs)
		{
			if (UTC->PlayerState != nullptr && UTC->PlayerState == PS)
			{
				return UTC;
			}
		}
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
	if (TeamAnchors.Num() == 0)
	{
		return;
	}
	ViewedTeamNum = NewTeam;
	if (!TeamAnchors.IsValidIndex(ViewedTeamNum))
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
	RecreateAllPlayers(NewTeam);
}

void SUTMatchSummaryPanel::ShowTeam(int32 TeamNum)
{
	bFirstViewOwnHighlights = false;
	RecreateAllPlayers(TeamNum);
}

void SUTMatchSummaryPanel::ShowCharacter(AUTCharacter* UTC)
{
	AUTPlayerState* ViewedPS = UTC ? Cast<AUTPlayerState>(UTC->PlayerState) : NULL;
	int32 TeamNum = (ViewedPS && ViewedPS->Team) ? ViewedPS->Team->TeamIndex : 0;
	if (TeamNum != ViewedTeamNum)
	{
		RecreateAllPlayers(TeamNum);
	}
	HideAllPlayersBut(UTC);
}

void SUTMatchSummaryPanel::ShowAllCharacters()
{
	int32 TeamToView = 0;
	if ((TeamPreviewMeshs.Num() > 1) && GameState.IsValid() && GameState->GetMatchState() == MatchState::WaitingPostMatch && GameState->WinningTeam != nullptr)
	{
		TeamToView = GameState->WinningTeam->GetTeamNum();
	}
	ShowTeam(TeamToView);
}

float SUTMatchSummaryPanel::GetAllCameraOffset()
{
	int32 MaxSize = 1;
	for (int32 iTeam = 0; iTeam < TeamPreviewMeshs.Num(); iTeam++)
	{
		MaxSize = FMath::Max(MaxSize, TeamPreviewMeshs[iTeam].Num());
	}
	MaxSize += 1.f;
	if (TeamPreviewMeshs.Num() < 2)
	{
		// players are across rather than angled.
		return ALL_CAMERA_OFFSET + (MaxSize * PLAYER_SPACING) / FMath::Tan(45.f * PI / 180.f);
	}
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

void SUTMatchSummaryPanel::OnMouseDownPlayerPreview(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
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
}

bool SUTMatchSummaryPanel::HasCamFlag(ECamFlags CamFlag) const
{
	TSharedPtr<FMatchCamera> Shot = GetCurrentShot();
	if (Shot.IsValid())
	{
		return (Shot->CamFlags & (uint32)CamFlag) > 0;
	}
	return false;
}

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
}

void SUTMatchSummaryPanel::ZoomPlayerPreview(float WheelDelta)
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
}

FReply SUTMatchSummaryPanel::OnSwitcherNext()
{
	if (HasCamFlag(CF_Team))
	{
		int32 Index = ViewedTeamNum + 1;
		ViewTeam(TeamAnchors.IsValidIndex(Index) ? Index : 0);
	}
	else if (HasCamFlag(CF_Player) && ViewedChar.IsValid())
	{
		int32 Index = PlayerPreviewMeshs.Find(ViewedChar.Get());
		if (Index == INDEX_NONE)
		{
			Index = 0;
		}

		Index++;
		if (PlayerPreviewMeshs.IsValidIndex(Index))
		{
			ViewCharacter(PlayerPreviewMeshs[Index]);
		}
		else
		{
			ViewCharacter(PlayerPreviewMeshs[0]);
		}
	}
	return FReply::Handled();
}

FReply SUTMatchSummaryPanel::OnSwitcherPrevious()
{
	if (HasCamFlag(CF_Team))
	{
		int32 Index = ViewedTeamNum - 1;
		ViewTeam(TeamAnchors.IsValidIndex(Index) ? Index : TeamAnchors.Num() - 1);
	}
	else if (HasCamFlag(CF_Player) && ViewedChar.IsValid())
	{
		int32 Index = PlayerPreviewMeshs.Find(ViewedChar.Get());
		if (Index == INDEX_NONE)
		{
			Index = 0;
		}

		Index--;
		if (PlayerPreviewMeshs.IsValidIndex(Index))
		{
			ViewCharacter(PlayerPreviewMeshs[Index]);
		}
		else
		{
			ViewCharacter(PlayerPreviewMeshs[PlayerPreviewMeshs.Num() - 1]);
		}
	}
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
			if ((TeamPreviewMeshs.Num() > 1) && GetPlayerOwner().IsValid() && Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController) != nullptr)
			{
				TeamToView = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController)->GetTeamNum();
			}
			ViewTeam(TeamToView);
		}
		else if (GameState->GetMatchState() == MatchState::WaitingPostMatch)
		{
			if ((TeamPreviewMeshs.Num() > 1) && (GameState->WinningTeam != nullptr))
			{
				TeamToView = GameState->WinningTeam->GetTeamNum();
			}
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
	if (ParentPanel.IsValid())
	{
		ParentPanel->HideMatchSummary();
	}
	return FReply::Handled();
}


#endif
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "SUWMatchSummary.h"
#include "SUWindowsStyle.h"
#include "SUWWeaponConfigDialog.h"
#include "SNumericEntryBox.h"
#include "../Public/UTCanvasRenderTarget2D.h"
#include "EngineModule.h"
#include "SlateMaterialBrush.h"
#include "../Public/UTPlayerCameraManager.h"
#include "UTCharacterContent.h"
#include "UTWeap_ShockRifle.h"
#include "UTWeaponAttachment.h"
#include "UTHUD.h"
#include "Engine/UserInterfaceSettings.h"
#include "UTGameEngine.h"
#include "Animation/SkeletalMeshActor.h"
#include "UTCTFGameState.h"
#include "UTCTFGameMode.h"
#include "UTPlayerState.h"
#include "Private/Slate/Widgets/SUTTabWidget.h"
#include "Panels/SUInGameHomePanel.h"
#include "UTConsole.h"
#include "UTHUDWidgetMessage.h"
#include "UTHUDWidgetMessage_GameMessages.h"

#if !UE_SERVER
#include "Runtime/AppFramework/Public/Widgets/Colors/SColorPicker.h"
#endif

#include "AssetData.h"

// scale factor for weapon/view bob sliders (i.e. configurable value between 0 and this)
static const float BOB_SCALING_FACTOR = 1.f;

static const float PLAYER_SPACING = 180.0f;
static const float TEAM_CAMERA_OFFSET = 1200.0f;
static const float ALL_CAMERA_OFFSET = 4000.0f;
static const float ALL_CAMERA_ANGLE = 15.0f;

#if !UE_SERVER

#include "SScaleBox.h"
#include "Widgets/SDragImage.h"

void SUWMatchSummary::Construct(const FArguments& InArgs)
{
	FVector2D ViewportSize;
	InArgs._PlayerOwner->ViewportClient->GetViewportSize(ViewportSize);

	PlayerOwner = InArgs._PlayerOwner;
	GameState = InArgs._GameState;

	StatsWidth = 0.0f;
	LastChatCount = 0;
	IntroTime = 0;

	ViewMode = VM_Team;
	ViewedTeamNum = 0;
	CameraTransform.SetLocation(FVector(5000.0f, 0.0f, 35.0f));
	CameraTransform.SetRotation(FRotator(0.0f, 180.0f, 0.0f).Quaternion());
	DesiredCameraTransform = CameraTransform;
	TeamCamAlpha = 0.5f;

	// allocate a preview scene for rendering
	PlayerPreviewWorld = UWorld::CreateWorld(EWorldType::Game, true); // NOTE: Custom depth does not work with EWorldType::Preview
	PlayerPreviewWorld->bHack_Force_UsesGameHiddenFlags_True = true;
	PlayerPreviewWorld->bShouldSimulatePhysics = true;
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
		TSubclassOf<class AUTCharacter> DefaultPawnClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *GetDefault<AUTGameMode>()->PlayerPawnObject.ToStringReference().AssetLongPathname, NULL, LOAD_NoWarn));
		if (DefaultPawnClass != nullptr)
		{
			NewGS->AddOverlayMaterial(DefaultPawnClass.GetDefaultObject()->TacComOverlayMaterial);
		}
	}
	
	PlayerPreviewAnimBlueprint = LoadObject<UClass>(nullptr, TEXT("/Game/RestrictedAssets/UI/ABP_PlayerPreview.ABP_PlayerPreview_C"));

	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(NULL, TEXT("/Game/RestrictedAssets/UI/PlayerPreviewProxy.PlayerPreviewProxy"));
	if (BaseMat != NULL)
	{
		PlayerPreviewTexture = Cast<UUTCanvasRenderTarget2D>(UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(GetPlayerOwner().Get(), UUTCanvasRenderTarget2D::StaticClass(), ViewportSize.X, ViewportSize.Y));
		PlayerPreviewTexture->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
		PlayerPreviewTexture->OnNonUObjectRenderTargetUpdate.BindSP(this, &SUWMatchSummary::UpdatePlayerRender);
		PlayerPreviewMID = UMaterialInstanceDynamic::Create(BaseMat, PlayerPreviewWorld);
		PlayerPreviewMID->SetTextureParameterValue(FName(TEXT("TheTexture")), PlayerPreviewTexture);
		PlayerPreviewBrush = new FSlateMaterialBrush(*PlayerPreviewMID, ViewportSize);
	}
	else
	{
		PlayerPreviewTexture = NULL;
		PlayerPreviewMID = NULL;
		PlayerPreviewBrush = new FSlateMaterialBrush(*UMaterial::GetDefaultMaterial(MD_Surface), ViewportSize);
	}

	PlayerPreviewTexture->TargetGamma = GEngine->GetDisplayGamma();
	PlayerPreviewTexture->InitCustomFormat(ViewportSize.X, ViewportSize.Y, PF_B8G8R8A8, false);
	PlayerPreviewTexture->UpdateResourceImmediate();

	FVector2D ResolutionScale(ViewportSize.X / 1280.0f, ViewportSize.Y / 720.0f);

	UUTGameUserSettings* Settings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	
	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[

		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFill)
				[
					SNew(SDragImage)
					.Image(PlayerPreviewBrush)
					.OnDrag(this, &SUWMatchSummary::DragPlayerPreview)
					.OnZoom(this, &SUWMatchSummary::ZoomPlayerPreview)
					.OnMove(this, &SUWMatchSummary::OnMouseMovePlayerPreview)
					.OnMousePressed(this, &SUWMatchSummary::OnMouseDownPlayerPreview)
				]
			]
			+ SOverlay::Slot()
			.VAlign(VAlign_Bottom)
			.HAlign(HAlign_Left)
			[
				SNew(SBox)
				.HeightOverride(140.0f)
				.WidthOverride(600.0f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SBorder)
						.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.95f))
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
			+ SOverlay::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Bottom)
				.HAlign(HAlign_Center)
				.Padding(0.0f,0.0f,0.0f,160.0f)
				[
					SNew(SBorder)
					.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f))
					.BorderImage(SUWindowsStyle::Get().GetBrush("UT.Background.Dark"))
					.Padding(0)
					.Visibility(this, &SUWMatchSummary::GetSwitcherVisibility)
					.Content()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.Padding(20.0f, 0.0f, 0.0f, 0.0f)
						.AutoWidth()
						[
							SNew(SButton)
							.HAlign(HAlign_Left)
							.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
							.ContentPadding(FMargin(20.0f, 5.0f, 20.0f, 5.0f))
							.Text(NSLOCTEXT("SUWMatchSummary", "PreviousPlayer", "<-"))
							.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
							.OnClicked(this, &SUWMatchSummary::OnSwitcherNext)
							.Visibility(this, &SUWMatchSummary::GetSwitcherButtonVisibility)
						]
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Fill)
						.FillWidth(1.0f)
						[
							SNew(SBox)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							[
								SNew(STextBlock)
								.Text(this, &SUWMatchSummary::GetSwitcherText)
								.ColorAndOpacity(this, &SUWMatchSummary::GetSwitcherColor)
								.TextStyle(SUWindowsStyle::Get(), "UT.Dialog.TitleTextStyle")
							]
						]
						+ SHorizontalBox::Slot()
						.Padding(0.0f, 0.0f, 20.0f, 0.0f)
						.AutoWidth()
						[
							SNew(SButton)
							.HAlign(HAlign_Right)
							.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
							.ContentPadding(FMargin(20.0f, 5.0f, 20.0f, 5.0f))
							.Text(NSLOCTEXT("SUWMatchSummary", "NextPlayer", "->"))
							.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
							.OnClicked(this, &SUWMatchSummary::OnSwitcherPrevious)
							.Visibility(this, &SUWMatchSummary::GetSwitcherButtonVisibility)
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
					.WidthOverride(TAttribute<FOptionalSize>::Create(TAttribute<FOptionalSize>::FGetter::CreateSP(this, &SUWMatchSummary::GetStatsWidth)))
					.Content()
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SBorder)
						]
						+SOverlay::Slot()
						[
							SNew(SImage)
							.Image(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
							.ColorAndOpacity(FLinearColor(0.0f,0.0f,0.0f,0.7f))
						]
						+ SOverlay::Slot()
						[
							SAssignNew(InfoPanel, SOverlay)
						]
					]
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SAssignNew(ChatPanel, SUInGameHomePanel, GetPlayerOwner())
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.HAlign(HAlign_Right)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
				.ContentPadding(FMargin(20.0f, 5.0f, 20.0f, 5.0f))
				.Text(NSLOCTEXT("SUWMatchSummary", "Close", "Close"))
				.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
				.OnClicked(this, &SUWMatchSummary::OnClose)
			]
		]
	];
	RecreateAllPlayers();

	//Set the camera state based on the game state
	if (GameState.IsValid())
	{
		if (GameState->GetMatchState() == MatchState::PlayerIntro)
		{
			//Play the intro music
			AUTPlayerController* UTPC = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);
			USoundBase* Music = LoadObject<USoundBase>(NULL, TEXT("/Game/RestrictedAssets/Audio/Music/FragCenterIntro.FragCenterIntro"), NULL, LOAD_NoWarn | LOAD_Quiet);
			if (UTPC != nullptr && Music != nullptr)
			{
				UTPC->ClientPlaySound(Music);
			}

			CameraState = CS_CamIntro;
			ViewAll();
			ViewedTeamNum = -1;
		}
		//View the winning team at the end of game
		else if(GameState->GetMatchState() == MatchState::WaitingPostMatch)
		{
			CameraState = CS_CamAuto;
			ViewTeam(GameState->WinningTeam != nullptr ? GameState->WinningTeam->GetTeamNum() : 0);
		}
		//View the local players team at halftime
		else if (GameState->GetMatchState() == MatchState::MatchEnteringHalftime
			|| GameState->GetMatchState() == MatchState::MatchIsAtHalftime)
		{
			CameraState = CS_CamAuto;
			ViewTeam(Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController)->GetTeamNum());
		}
		else
		{
			CameraState = CS_FreeCam;
			ViewTeam(0);
		}
	}

	// Turn on Screen Space Reflection max quality
	auto SSRQualityCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SSR.Quality"));
	OldSSRQuality = SSRQualityCVar->GetInt();
	SSRQualityCVar->Set(4, ECVF_SetByCode);
}

SUWMatchSummary::~SUWMatchSummary()
{
	// Reset Screen Space Reflection max quality, wish there was a cleaner way to reset the flags
	auto SSRQualityCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SSR.Quality"));
	EConsoleVariableFlags Flags = SSRQualityCVar->GetFlags();
	Flags = (EConsoleVariableFlags)(((uint32)Flags & ~ECVF_SetByMask) | ECVF_SetByScalability);
	SSRQualityCVar->Set(OldSSRQuality, ECVF_SetByCode);
	SSRQualityCVar->SetFlags(Flags);

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
	if (PlayerPreviewWorld != NULL)
	{
		PlayerPreviewWorld->DestroyWorld(true);
		GEngine->DestroyWorldContext(PlayerPreviewWorld);
		PlayerPreviewWorld = NULL;
	}
	ViewState.Destroy();
}

void SUWMatchSummary::OnTabButtonSelectionChanged(const FText& NewText)
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

void SUWMatchSummary::BuildInfoPanel()
{
	StatList.Empty();
	InfoPanel->ClearChildren();

	if (CameraState != CS_CamIntro)
	{
		InfoPanel->AddSlot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(TabWidget, SUTTabWidget)
				.OnTabButtonSelectionChanged(this, &SUWMatchSummary::OnTabButtonSelectionChanged)
				.TabTextStyle(SUWindowsStyle::Get(), "UT.MatchSummary.TabButton.TextStyle")
			];

		//Build all of the player stats
		AUTPlayerState* UTPS = ViewedChar.IsValid() ? Cast<AUTPlayerState>(ViewedChar->PlayerState) : nullptr;
		if (UTPS != nullptr && !UTPS->IsPendingKill() && UTPS->IsValidLowLevel())
		{
			AUTGameMode* DefaultGameMode = GetPlayerOwner()->GetWorld()->GetGameState()->GameModeClass->GetDefaultObject<AUTGameMode>();
			if (UTPS != nullptr && DefaultGameMode != nullptr)
			{
				//Build the highlights
				if (GameState->GetMatchState() != MatchState::PlayerIntro)
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

					TArray<FText> Highlights = DefaultGameMode->GetPlayerHighlights(UTPS);
					if (Highlights.Num() == 0)
					{
						Highlights.Add(NSLOCTEXT("AUTGameMode", "HighlightTopScore", "No Highlights"));
					}
					//Cap at 5 highlights
					else if (Highlights.Num() > 5)
					{
						Highlights.SetNum(5);
					}

					for (int32 i = 0; i < Highlights.Num(); i++)
					{
						VBox->AddSlot()
							.Padding(100, 20)
							.AutoHeight()
							[
								SNew(SBorder)
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
					}
				}

				UTPS->BuildPlayerInfo(TabWidget, StatList);

				//Build the player stats only if the match has started
				if (GameState.IsValid() && GameState->HasMatchStarted())
				{
					DefaultGameMode->BuildPlayerInfo(UTPS, TabWidget, StatList);
				}

				TabWidget->SelectTab(0);
			}
		}
	}
}

void SUWMatchSummary::UpdateChatText()
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

				if (Msg->Type == ChatDestinations::Friends)
				{
					Style = TEXT("UWindows.Chat.Text.Friends");
				}
				else if (Msg->Type == ChatDestinations::System || Msg->Type == ChatDestinations::MOTD)
				{
					Style = TEXT("UWindows.Chat.Text.Admin");
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
						Style = TEXT("UWindows.Chat.Text.Team.Red");;
					}
					else
					{
						Style = TEXT("UWindows.Chat.Text.Team.Blue");;
					}
				}
				else
				{
					Style = TEXT("UWindows.Chat.Text.Global");
				}

				RichText += FString::Printf(TEXT("<%s>[%s]%s</>"), *Style, *(ChatPanel->GetChatDestinationTag(Msg->Type).ToString()), *Msg->Message);
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

void SUWMatchSummary::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PlayerPreviewTexture);
	Collector.AddReferencedObject(PlayerPreviewMID);
	Collector.AddReferencedObject(PlayerPreviewWorld);
}

void SUWMatchSummary::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (PlayerPreviewWorld != nullptr)
	{
		PlayerPreviewWorld->Tick(LEVELTICK_All, InDeltaTime);
	}

	if ( PlayerPreviewTexture != nullptr )
	{
		PlayerPreviewTexture->UpdateResource();
	}

	// Force the preview mesh and weapons to put the highest mips into memory
	for (AUTCharacter* PlayerPreview : PlayerPreviewMeshs)
	{
		PlayerPreview->PrestreamTextures(1, true);
	}
	for (auto Weapon : PreviewWeapons)
	{
		Weapon->PrestreamTextures(1, true);
	}

	//Update the camera
	IntroTime += InDeltaTime;
	if (CameraState == CS_CamIntro)
	{
		UpdateIntroCam();
	}
	else if (CameraState == CS_CamAuto)
	{
		UpdateAutoCam();
	}

	if (ViewMode == VM_Team)
	{
		DesiredCameraTransform.Blend(TeamStartCamera, TeamEndCamera, TeamCamAlpha);
	}

	//Smooth the camera
	CameraTransform.SetLocation(FMath::VInterpTo(CameraTransform.GetLocation(), DesiredCameraTransform.GetLocation(), InDeltaTime, 5.0f));
	CameraTransform.SetRotation(FMath::RInterpTo(CameraTransform.Rotator(), DesiredCameraTransform.Rotator(), InDeltaTime, 5.0f).Quaternion());

	UpdateChatText();
}

void SUWMatchSummary::UpdateIntroCam()
{
	if (ViewedTeamNum < 0 && IntroTime > 0.9f)
	{
		ViewTeam(0);
		IntroTime = 0.0f;
	}
	else if (ViewedTeamNum >= 0)
	{
		int32 NumViewTeams = GameState->Teams.Num();
		if (NumViewTeams == 0)
		{
			NumViewTeams = 1;
		}
		//6 seconds for the team camera pan works well with the current song
		float TimePerTeam = 6.0f / NumViewTeams;
		TeamCamAlpha = IntroTime / TimePerTeam;

		if (IntroTime > TimePerTeam)
		{
			if (ViewedTeamNum + 1 < GameState->Teams.Num())
			{
				ViewTeam(ViewedTeamNum + 1);
				IntroTime = 0.0f;

				if (ViewedTeamNum > 0)
				{
					//Snap the camera backwards so it swoops down on the team
					CameraTransform.SetLocation(FVector(5000.0f, 0.0f, 35.0f));
					CameraTransform.SetRotation(FRotator(0.0f, 180.0f, 0.0f).Quaternion());
				}
			}
			else
			{
				ViewAll();
				CameraState = CS_FreeCam;
			}
		}
	}
}

void SUWMatchSummary::UpdateAutoCam()
{
	if (!ViewedChar.IsValid() && ViewMode == EViewMode::VM_Team)
	{
		if (IntroTime > 5.0f)
		{
			ViewAll();
			IntroTime = 0.0f;

			//Reset the scoreboard page and timers
			UUTScoreboard* Scoreboard = GetScoreboard();
			if (Scoreboard != nullptr)
			{
				Scoreboard->SetPage(0);
				Scoreboard->SetScoringPlaysTimer(true);
			}
		}
	}
	else if (!ViewedChar.IsValid() && ViewMode == EViewMode::VM_All)
	{
		if (IntroTime > 10.0f)
		{
			for (AUTCharacter* Char : PlayerPreviewMeshs)
			{
				if (Char->GetTeamNum() == ViewedTeamNum || Char->GetTeamNum() == 255)
				{
					ViewCharacter(Char);
					break;
				}
			}
			IntroTime = 0.0f;
		}
	}
	else if (IntroTime > 3.0f && (ViewMode == EViewMode::VM_All || ViewMode == EViewMode::VM_Player))
	{
		bool bFound = false;
		bool bFinished = true;
		for (AUTCharacter* Char : PlayerPreviewMeshs)
		{
			if (Char == ViewedChar)
			{
				bFound = true;
				continue;
			}

			if (bFound && (Char->GetTeamNum() == ViewedTeamNum || Char->GetTeamNum() == 255))
			{
				ViewCharacter(Char);
				bFinished = false;
				break;
			}
		}

		if (bFinished)
		{
			ViewTeam(ViewedTeamNum);
			CameraState = CS_FreeCam;
		}

		IntroTime = 0.0f;
	}
}

void SUWMatchSummary::RecreateAllPlayers()
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
	for (auto Flag : PreviewFlags)
	{
		Flag->Destroy();
	}
	PreviewFlags.Empty();
	for (auto Anchor : TeamAnchors)
	{
		Anchor->Destroy();
	}
	TeamAnchors.Empty();


	//Gather All of the playerstates
	TArray<TArray<class AUTPlayerState*> > TeamPlayerStates;
	if (GameState.IsValid())
	{
		for (TActorIterator<AUTPlayerState> It(GameState->GetWorld()); It; ++It)
		{
			AUTPlayerState* PS = *It;

			if (!PS->bOnlySpectator)
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

	for (int32 iTeam = 0; iTeam < TeamPlayerStates.Num(); iTeam++)
	{
		FVector PlayerLocation(0.0f, 0.0f, 0.0f);
		FVector TeamCenter = FVector(0.0f,(TeamPlayerStates[iTeam].Num() - 1) * PLAYER_SPACING * 0.5f, 0.0f);

		//Create an actor that all team actors will attach to for easy team manipulation
		AActor* TeamAnchor = PlayerPreviewWorld->SpawnActor<AActor>(ATargetPoint::StaticClass(), TeamCenter, FRotator::ZeroRotator);
		if (TeamAnchor != nullptr)
		{
			TeamAnchors.Add(TeamAnchor);

			//Spawn all of the characters for this team
			for (int32 iPlayer = 0; iPlayer < TeamPlayerStates[iTeam].Num(); iPlayer++)
			{
				AUTPlayerState* PS = TeamPlayerStates[iTeam][iPlayer];
				AUTCharacter* NewCharacter = RecreatePlayerPreview(PS, PlayerLocation, FRotator::ZeroRotator);
				NewCharacter->AttachRootComponentToActor(TeamAnchor, NAME_None, EAttachLocation::KeepWorldPosition);

				//Add the character to the team list
				if (!TeamPreviewMeshs.IsValidIndex(iTeam))
				{
					TeamPreviewMeshs.SetNum(iTeam + 1);
				}
				TeamPreviewMeshs[iTeam].Add(NewCharacter);

				PlayerLocation.Y += PLAYER_SPACING;
			}

			//Spawn a flag if its a ctf game TODO: Let the gamemode pick the mesh
			if (GameState.IsValid() && Cast<AUTCTFGameState>(GameState.Get()) != nullptr)
			{
				FVector FlagLocation = FVector(-200.0f, (TeamPlayerStates[iTeam].Num() - 1) * PLAYER_SPACING * 0.5f, 0.0f);

				USkeletalMesh* const FlagMesh = Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), NULL, TEXT("SkeletalMesh'/Game/RestrictedAssets/Proto/UT3_Pickups/Flag/S_CTF_Flag_IronGuard.S_CTF_Flag_IronGuard'"), NULL, LOAD_None, NULL));
				ASkeletalMeshActor* NewFlag = PlayerPreviewWorld->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), FlagLocation, FRotator(0.0f, 90.0f, 0.0f));
				if (NewFlag != nullptr && FlagMesh != nullptr)
				{
					UMaterialInstanceConstant* FlagMat = nullptr;
					if (iTeam == 0)
					{
						FlagMat = Cast<UMaterialInstanceConstant>(StaticLoadObject(UMaterialInstanceConstant::StaticClass(), NULL, TEXT("MaterialInstanceConstant'/Game/RestrictedAssets/Proto/UT3_Pickups/Flag/MI_CTF_RedFlag.MI_CTF_RedFlag'"), NULL, LOAD_None, NULL));
					}
					else
					{
						FlagMat = Cast<UMaterialInstanceConstant>(StaticLoadObject(UMaterialInstanceConstant::StaticClass(), NULL, TEXT("MaterialInstanceConstant'/Game/RestrictedAssets/Proto/UT3_Pickups/Flag/MI_CTF_BlueFlag.MI_CTF_BlueFlag'"), NULL, LOAD_None, NULL));
					}

					NewFlag->GetSkeletalMeshComponent()->SetSkeletalMesh(FlagMesh);
					NewFlag->GetSkeletalMeshComponent()->SetMaterial(1, FlagMat);
					NewFlag->SetActorScale3D(FVector(1.8f));
					NewFlag->AttachRootComponentToActor(TeamAnchor, NAME_None, EAttachLocation::KeepWorldPosition);
					PreviewFlags.Add(NewFlag);
				}
			}
		}
	}

	//Move the teams into position
	if (TeamAnchors.Num() == 1)
	{
		TeamAnchors[0]->SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	}
	else if (TeamAnchors.Num() == 2)
	{
		static const float TeamAngle = 50.0f;
		//Angle the teams a bit. TODO make this work for any number of teams
		{
			float Dist = (TeamPlayerStates[0].Num() - 1) * PLAYER_SPACING * 0.5 + 400.0f;

			FRotator Dir(0.0f, TeamAngle, 0.0f);
			FVector Location = Dir.Vector() * Dist;
			Dir.Yaw += -90.0f;
			TeamAnchors[0]->SetActorLocationAndRotation(Location, Dir);
		}
		{
			float Dist = (TeamPlayerStates[1].Num() - 1) * PLAYER_SPACING * 0.5 + 400.0f;
			
			FRotator Dir(0.0f, -TeamAngle, 0.0f);
			FVector Location = Dir.Vector() * Dist;
			Dir.Yaw += 90.0f;
			TeamAnchors[1]->SetActorLocationAndRotation(Location, Dir);
		}
	}
}

AUTCharacter* SUWMatchSummary::RecreatePlayerPreview(AUTPlayerState* NewPS, FVector Location, FRotator Rotation)
{
	AUTWeaponAttachment* PreviewWeapon = nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.bNoCollisionFail = true;
	TSubclassOf<class APawn> DefaultPawnClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *GetDefault<AUTGameMode>()->PlayerPawnObject.ToStringReference().AssetLongPathname, NULL, LOAD_NoWarn));

	AUTCharacter* PlayerPreviewMesh = PlayerPreviewWorld->SpawnActor<AUTCharacter>(DefaultPawnClass, Location, Rotation, SpawnParams);
	PlayerPreviewMesh->GetMesh()->SetAnimInstanceClass(PlayerPreviewAnimBlueprint);

	if (PlayerPreviewMesh)
	{
		PlayerPreviewMesh->PlayerState = NewPS; //PS needed for team colors
		PlayerPreviewMesh->Health = 100; //Set to 100 so the TacCom Overlay doesn't show damage
		PlayerPreviewMesh->DeactivateSpawnProtection();

		PlayerPreviewMesh->ApplyCharacterData(NewPS->GetSelectedCharacter());
		PlayerPreviewMesh->NotifyTeamChanged();

		PlayerPreviewMesh->SetHatClass(NewPS->HatClass);
		PlayerPreviewMesh->SetHatVariant(NewPS->HatVariant);
		PlayerPreviewMesh->SetEyewearClass(NewPS->EyewearClass);
		PlayerPreviewMesh->SetEyewearVariant(NewPS->EyewearVariant);

		// TODO TIM: Show the players best/favorite weapon
		UClass* PreviewAttachmentType = LoadClass<AUTWeaponAttachment>(NULL, TEXT("/Game/RestrictedAssets/Weapons/ShockRifle/ShockAttachment.ShockAttachment_C"), NULL, LOAD_None, NULL);
		if (PreviewAttachmentType != NULL)
		{
			PreviewWeapon = PlayerPreviewWorld->SpawnActor<AUTWeaponAttachment>(PreviewAttachmentType, FVector(0, 0, 0), FRotator(0, 0, 0));
			PreviewWeapon->Instigator = PlayerPreviewMesh;
		}

		if (PreviewWeapon)
		{
			PreviewWeapon->BeginPlay();
			PreviewWeapon->AttachToOwner();
		}

		PlayerPreviewMeshs.Add(PlayerPreviewMesh);
		PreviewWeapons.Add(PreviewWeapon);
		return PlayerPreviewMesh;
	}
	return nullptr;
}

void SUWMatchSummary::UpdatePlayerRender(UCanvas* C, int32 Width, int32 Height)
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

	//Check if the mouse is over a player and apply taccom effect
	if (CameraState != CS_CamAuto && (ViewMode == VM_Team || ViewMode == VM_All))
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
			else if (HighlightedChar == nullptr)
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
	if (ViewMode == VM_Team || GameState->GetMatchState() == MatchState::PlayerIntro)
	{
		//Helper for making sure player names don't overlap
		//TODO: do this better. Smooth the spacing of names when they overlap
		struct FPlayerName
		{
			FPlayerName(FString InPlayerName, FVector InLocation3D, FVector2D InLocation, FVector2D InSize)
				: PlayerName(InPlayerName), Location3D(InLocation3D), Location(InLocation), Size(InSize){}
			FString PlayerName;
			FVector Location3D;
			FVector2D Location;
			FVector2D Size;
		};


		UFont* Font = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->MediumFont;

		//Gather all of the player names
		TArray<FPlayerName> PlayerNames;
		for (AUTCharacter* UTC : PlayerPreviewMeshs)
		{
			if (UTC->PlayerState != nullptr && FVector::DotProduct(CameraTransform.Rotator().Vector(), (UTC->GetActorLocation() - CameraTransform.GetLocation())) > 0.0f)
			{
				FVector ActorLocation = UTC->GetActorLocation() + FVector(0.0f, 0.0f, 170.0f);
				FVector2D ScreenLoc;
				View->WorldToPixel(ActorLocation, ScreenLoc);

				float XL = 0, YL = 0;
				C->TextSize(Font, UTC->PlayerState->PlayerName, XL, YL);

				//center the text
				ScreenLoc.X -= XL * 0.5f;
				ScreenLoc.Y -= YL * 0.5f;

				PlayerNames.Add(FPlayerName(UTC->PlayerState->PlayerName, ActorLocation, ScreenLoc, FVector2D(XL, YL)));
			}
		}

		//Sort closest to farthest
		FVector CamLoc = CameraTransform.GetLocation();
		PlayerNames.Sort([CamLoc](const FPlayerName& A, const FPlayerName& B) -> bool
		{
			return  FVector::DistSquared(CamLoc, A.Location3D) < FVector::DistSquared(CamLoc, B.Location3D);
		});

		for (int32 i = 0; i < PlayerNames.Num(); i++)
		{
			//Draw the Player name
			FFontRenderInfo FontInfo;
			FontInfo.bEnableShadow = true;
			FontInfo.bClipText = true;
			C->DrawColor = FLinearColor::White;
			C->DrawText(Font, FText::FromString(PlayerNames[i].PlayerName), PlayerNames[i].Location.X, PlayerNames[i].Location.Y, 1.0f, 1.0f, FontInfo);

			//Move the remaining names out of the way
			for (int32 j = i + 1; j < PlayerNames.Num(); j++)
			{
				FPlayerName& A = PlayerNames[i];
				FPlayerName& B = PlayerNames[j];

				//if the names intersect, move it up along the Y axis
				if (A.Location.X < B.Location.X + B.Size.X && A.Location.X + A.Size.X > B.Location.X
					&& A.Location.Y < B.Location.Y + B.Size.Y && A.Location.Y + A.Size.Y > B.Location.Y)
				{
					B.Location.Y = A.Location.Y - (B.Size.Y * 1.5f);
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
		if (ShouldShowScoreboard())
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

FReply SUWMatchSummary::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
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
		if (UTInput->InputKey(InKeyEvent.GetKey(), EInputEvent::IE_Pressed, 1.0f, false))
		{
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FReply SUWMatchSummary::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
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

	//Pass the key event to PlayerInput 
	UUTPlayerInput* UTInput = Cast<UUTPlayerInput>(GetPlayerOwner()->PlayerController->PlayerInput);
	if (UTInput != nullptr)
	{
		if (UTInput->InputKey(InKeyEvent.GetKey(), EInputEvent::IE_Released, 1.0f, false))
		{
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FReply SUWMatchSummary::OnKeyChar(const FGeometry& InGeometry, const FCharacterEvent& InCharacterEvent)
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

void SUWMatchSummary::PlayTauntByIndex(AUTPlayerState* PS, int32 TauntIndex)
{
	AUTCharacter* UTC = FindCharacter(PS);
	if (UTC != nullptr)
	{
		UTC->PlayTauntByIndex(TauntIndex);
	}
}

void SUWMatchSummary::SetViewMode(EViewMode NewViewMode)
{
	if (NewViewMode != ViewMode)
	{
		ViewMode = NewViewMode;

		//Make sure all the characters are rotated in their proper team rotation
		for (int32 iTeam = 0; iTeam < TeamPreviewMeshs.Num(); iTeam++)
		{
			for (int32 iCharacter = 0; iCharacter < TeamPreviewMeshs[iTeam].Num(); iCharacter++)
			{
				if (TeamAnchors.IsValidIndex(iCharacter))
				{
					TeamPreviewMeshs[iTeam][iCharacter]->SetActorRotation(TeamAnchors[iTeam]->GetActorRotation());
				}
			}
		}
	}
}

AUTCharacter* SUWMatchSummary::FindCharacter(class AUTPlayerState* PS)
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

void SUWMatchSummary::ViewCharacter(AUTCharacter* NewChar)
{
	if (NewChar != nullptr)
	{
		NewChar->UpdateTacComMesh(false);
	}

	SetViewMode(VM_Player);
	ViewedChar = NewChar;

	ViewedTeamNum = ViewedChar->GetTeamNum() != 255 ? ViewedChar->GetTeamNum() : 0;

	FRotator Dir = ViewedChar->GetActorRotation();
	FVector Location = ViewedChar->GetActorLocation() + (Dir.Vector() * 300.0f);
	Location += Dir.Quaternion().GetAxisY() * -60.0f + FVector(0.0f, 0.0f, 45.0f);

	Dir.Yaw += 180.0f;
	DesiredCameraTransform.SetLocation(Location);
	DesiredCameraTransform.SetRotation(Dir.Quaternion());

	BuildInfoPanel();
}

void SUWMatchSummary::SelectPlayerState(AUTPlayerState* PS)
{
	AUTCharacter* UTC = FindCharacter(PS);
	if (UTC != nullptr)
	{
		ViewCharacter(UTC);
	}
}

void SUWMatchSummary::ViewTeam(int32 NewTeam)
{
	if (TeamAnchors.Num() == 0)
	{
		return;
	}

	SetViewMode(EViewMode::VM_Team);
	ViewedTeamNum = NewTeam;
	if (!TeamAnchors.IsValidIndex(ViewedTeamNum))
	{
		ViewedTeamNum = 0;
	}

	//Figure out the start and end camera tranforms for the team pan
	TArray<AUTCharacter*> &TeamCharacters = TeamPreviewMeshs[ViewedTeamNum];
	{
		if (TeamCharacters.Num() > 0)
		{
			AUTCharacter* StartChar = TeamCharacters[TeamCharacters.Num() - 1];
			FRotator Dir = StartChar->GetActorRotation();
			FVector Location = StartChar->GetActorLocation() + (Dir.Vector() * TEAM_CAMERA_OFFSET);

			Dir.Yaw += 180.0f;
			TeamStartCamera.SetLocation(Location);
			TeamStartCamera.SetRotation(Dir.Quaternion());
		}
	}
	if (TeamCharacters.Num() > 0)
	{
		AUTCharacter* EndChar = TeamCharacters[0];
		FRotator Dir = EndChar->GetActorRotation();
		FVector Location = EndChar->GetActorLocation() + (Dir.Vector() * TEAM_CAMERA_OFFSET);

		Dir.Yaw += 180.0f;
		TeamEndCamera.SetLocation(Location);
		TeamEndCamera.SetRotation(Dir.Quaternion());
	}
	TeamCamAlpha = 0.5f;

	BuildInfoPanel();
}

void SUWMatchSummary::ViewAll()
{
	SetViewMode(EViewMode::VM_All);
	DesiredCameraTransform.SetLocation(FVector(4000.0f, 0.0f, 900.0f));
	DesiredCameraTransform.SetRotation(FRotator(-15.0f, 180.0f, 0.0f).Quaternion());

	UUTScoreboard* Scoreboard = GetScoreboard();
	if (Scoreboard != nullptr)
	{
		Scoreboard->SetPage(0);
		Scoreboard->SetScoringPlaysTimer(false);
	}
}

UUTScoreboard* SUWMatchSummary::GetScoreboard()
{
	AUTPlayerController* UTPC = GetPlayerOwner().IsValid() ? Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController) : nullptr;
	return (UTPC != nullptr && UTPC->MyUTHUD != nullptr) ? UTPC->MyUTHUD->GetScoreboard() : nullptr;
}

void SUWMatchSummary::OnMouseDownPlayerPreview(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	MousePos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	//Try to Click the scoreboard first
	if (CanClickScoreboard())
	{
		UUTScoreboard* Scoreboard = GetScoreboard();
		if (Scoreboard != nullptr && Scoreboard->AttemptSelection(MousePos))
		{
			Scoreboard->SelectionClick();
			CameraState = CS_FreeCam;
			return;
		}
	}

	//Click a character
	if ((ViewMode == VM_Team || ViewMode == VM_All) && HighlightedChar.IsValid() && CameraState != CS_CamAuto)
	{
		CameraState = CS_FreeCam;
		ViewCharacter(HighlightedChar.Get());
	}
}

bool SUWMatchSummary::ShouldShowScoreboard()
{
	return ViewMode == EViewMode::VM_All && GameState->GetMatchState() != MatchState::PlayerIntro;
}
bool SUWMatchSummary::CanClickScoreboard()
{
	return ShouldShowScoreboard();
}

void SUWMatchSummary::OnMouseMovePlayerPreview(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	MousePos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	UUTScoreboard* Scoreboard = GetScoreboard();
	if (Scoreboard != nullptr)
	{
		Scoreboard->BecomeInteractive();
		Scoreboard->TrackMouseMovement(MousePos);
	}
}

void SUWMatchSummary::DragPlayerPreview(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (CameraState == CS_FreeCam)
	{
		if (ViewMode == VM_Team)
		{
			//Scroll the team left/right
			if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
			{
				float Dist = FVector::Dist(TeamStartCamera.GetLocation(), TeamEndCamera.GetLocation());

				if (Dist > 0.0f)
				{
					TeamCamAlpha -= MouseEvent.GetCursorDelta().X / Dist;
					TeamCamAlpha = FMath::Clamp(TeamCamAlpha, 0.0f, 1.0f);
				}
				else
				{
					TeamCamAlpha = 0.5f;
				}
			}
		}
		else if (ViewMode == VM_Player && ViewedChar.IsValid())
		{
			//Rotate the character
			if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
			{
				ViewedChar->SetActorRotation(ViewedChar->GetActorRotation() + FRotator(0, 0.2f * -MouseEvent.GetCursorDelta().X, 0.0f));
			}
			//Pan up and down
			else if (MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
			{
				FVector Location = DesiredCameraTransform.GetLocation();
				Location.Z = FMath::Clamp(Location.Z + MouseEvent.GetCursorDelta().Y, -80.0f, 80.0f);
				DesiredCameraTransform.SetLocation(Location);
			}
		}
	}
}

void SUWMatchSummary::ZoomPlayerPreview(float WheelDelta)
{
	if (CameraState != CS_CamAuto)
	{
		if (ViewMode == VM_Player && WheelDelta < 0.0f)
		{
			ViewTeam(ViewedTeamNum);
		}
		else if (ViewMode == VM_Team && WheelDelta < 0.0f)
		{
			ViewAll();
		}
	}
}

FReply SUWMatchSummary::OnSwitcherNext()
{
	if (ViewMode == VM_Team)
	{
		int32 Index = ViewedTeamNum + 1;
		ViewTeam(TeamAnchors.IsValidIndex(Index) ? Index : 0);
	}
	else if (ViewMode == VM_Player && ViewedChar.IsValid())
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

FReply SUWMatchSummary::OnSwitcherPrevious()
{
	if (ViewMode == VM_Team)
	{
		int32 Index = ViewedTeamNum - 1;
		ViewTeam(TeamAnchors.IsValidIndex(Index) ? Index : TeamAnchors.Num() - 1);
	}
	else if (ViewMode == VM_Player && ViewedChar.IsValid())
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

FText SUWMatchSummary::GetSwitcherText() const
{
	if (ViewMode == VM_Player && ViewedChar.IsValid() && ViewedChar->PlayerState != nullptr)
	{
		return FText::FromString(ViewedChar->PlayerState->PlayerName);
	}
	if (ViewMode == VM_Team && GameState.IsValid() && GameState->Teams.IsValidIndex(ViewedTeamNum))
	{
		return FText::Format(NSLOCTEXT("SUWMatchSummary", "Team", "{0} Team"), GameState->Teams[ViewedTeamNum]->TeamName);
	}
	return FText::GetEmpty();
}
FSlateColor SUWMatchSummary::GetSwitcherColor() const
{
	if (ViewMode == VM_Team && GameState.IsValid() && GameState->Teams.IsValidIndex(ViewedTeamNum))
	{
		return FMath::LerpStable(GameState->Teams[ViewedTeamNum]->TeamColor, FLinearColor::White, 0.3f);
	}
	return FLinearColor::White;
}

EVisibility SUWMatchSummary::GetSwitcherVisibility() const
{
	if (ViewMode != EViewMode::VM_All && !GetSwitcherText().IsEmpty())
	{
		return EVisibility::Visible;
	}
	return EVisibility::Hidden;
}

EVisibility SUWMatchSummary::GetSwitcherButtonVisibility() const
{
	if (CameraState == CS_FreeCam && ViewMode != EViewMode::VM_All)
	{
		return EVisibility::Visible;
	}
	return EVisibility::Hidden;
}

FOptionalSize SUWMatchSummary::GetStatsWidth() const
{
	float WantedWidth = 0.0f;
	
	if (CameraState != CS_CamIntro)
	{
		if (ViewMode == VM_Player)
		{
			WantedWidth = 1050.0f;
		}
	}
	StatsWidth = FMath::FInterpTo(StatsWidth, WantedWidth, GameState->GetWorld()->DeltaTimeSeconds, 10.0f);

	return FOptionalSize(StatsWidth);
}

FReply SUWMatchSummary::OnClose()
{
	GetPlayerOwner()->CloseMatchSummary();
	return FReply::Handled();
}

#endif
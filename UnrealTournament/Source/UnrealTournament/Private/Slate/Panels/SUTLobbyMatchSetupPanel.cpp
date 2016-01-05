
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTLobbyMatchSetupPanel.h"
#include "../SUWindowsStyle.h"
#include "../SUTStyle.h"
#include "../SUTUtils.h"
#include "../Dialogs/SUTGameSetupDialog.h"
#include "SUTMidGameInfoPanel.h"
#include "UTLobbyMatchInfo.h"
#include "UTGameEngine.h"
#include "UTLevelSummary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UTReplicatedMapInfo.h"
#include "UTLobbyPC.h"

#if !UE_SERVER

void SUTLobbyMatchSetupPanel::Construct(const FArguments& InArgs)
{
	DefaultLevelScreenshot = new FSlateDynamicImageBrush(Cast<UUTGameEngine>(GEngine)->DefaultLevelScreenshot, FVector2D(256.0, 128.0), NAME_None);

	BlinkyTimer = 0.0f;
	Dots=0;

	LastMapInfo.Reset();

	bIsHost = InArgs._bIsHost;
	MatchInfo = InArgs._MatchInfo;
	PlayerOwner = InArgs._PlayerOwner;

	OnMatchInfoUpdatedDelegate.BindSP(this, &SUTLobbyMatchSetupPanel::OnMatchInfoUpdated); 
	MatchInfo->OnMatchInfoUpdatedDelegate = OnMatchInfoUpdatedDelegate;

	OnRulesetUpdatedDelegate.BindSP(this, &SUTLobbyMatchSetupPanel::OnRulesetUpdated);
	MatchInfo->OnRulesetUpdatedDelegate = OnRulesetUpdatedDelegate;

	AUTLobbyGameState* LobbyGameState = GWorld->GetGameState<AUTLobbyGameState>();

	RulesetUpdatedDelegate = InArgs._OnRulesetUpdated;

	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SVerticalBox)

		// Host Options
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)												
			.HeightOverride(75)
			.WidthOverride(954)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SImage)
					.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
				]
				+SOverlay::Slot()
				[
					BuildHostOptionWidgets()
				]
			]
		]

		// Map
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.0f,10.0f,0.0f, 0.0f))
		[
			SNew(SBox).WidthOverride(954).HeightOverride(477)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SImage)
					.Image(this, &SUTLobbyMatchSetupPanel::GetMapImage)
				]
				+SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBox).HeightOverride(48)
						[
							SNew(SOverlay)
							+SOverlay::Slot()
							[
								SNew(SImage)
								.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.Shaded"))
							]
							+SOverlay::Slot()
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.HAlign(HAlign_Center)
								.Padding(FMargin(0.0f,5.0f,0.0f,0.0f))
								[
									SNew(STextBlock)
									.Text(this, &SUTLobbyMatchSetupPanel::GetMapName)
									.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Medium.Bold")
								]
							]
						]
					]
				]

			]
		]


		// Game Rules
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.0f,10.0f,0.0f, 0.0f))
		[
			SNew(SBox).WidthOverride(954).HeightOverride(376)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SImage)
					.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
				]
				+SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.Padding(FMargin(10.0f,10.0f,10.0f,0.0f))
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBox).WidthOverride(934).HeightOverride(300)
							[
								SNew(SScrollBox)
								.Style(SUWindowsStyle::Get(),"UT.ScrollBox.Borderless")
								+ SScrollBox::Slot()
								[
									SAssignNew(GameRulesPanel, SVerticalBox)
								]
							]

						]
					]
					+SVerticalBox::Slot()
					.Padding(FMargin(10.0f,10.0f,0.0f,10.0f))
					.AutoHeight()
					[
						SNew(SBox).HeightOverride(32)
						[
							SNew(SOverlay)
							+SOverlay::Slot()
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.FillWidth(1.0)
								[
									AddChangeButton()
								]
							]
							+SOverlay::Slot()
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.HAlign(HAlign_Right)
								.FillWidth(1.0)
								[
									AddActionButtons()
								]
							]
						]
					]
				]
			]
		]

		// Statusbar
		+SVerticalBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 0.0f)
		.AutoHeight()
		[
			SNew(SBox).HeightOverride(32)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SImage)
					.Image(SUWindowsStyle::Get().GetBrush("UWindows.Lobby.MatchBar.Background"))
				]

				+SOverlay::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.Padding(5.0,0.0,5.0,0.0)
					[
						SAssignNew(StatusTextBlock, STextBlock)
						.Text(this, &SUTLobbyMatchSetupPanel::GetStatusText )
						.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.OnClicked(this, &SUTLobbyMatchSetupPanel::CancelDownloadClicked)
						.Visibility(this, &SUTLobbyMatchSetupPanel::CancelButtonVisible)
						.ButtonStyle(SUTStyle::Get(), "UT.ContextMenu.Item")
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SUTLobbyMatchSetupPanel","CancelDownload","[Click to Cancel]"))
							.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Small")
						]
					]

				]
			]
		]
	];

	OnMatchInfoUpdated();
	OnRulesetUpdated();
}

FReply SUTLobbyMatchSetupPanel::CancelDownloadClicked()
{
	if (PlayerOwner.IsValid() && PlayerOwner->IsDownloadInProgress())
	{
		PlayerOwner->CancelDownload();
	}
	return FReply::Handled();
}

EVisibility SUTLobbyMatchSetupPanel::CancelButtonVisible() const
{
	return PlayerOwner->IsDownloadInProgress() ? EVisibility::Visible : EVisibility::Hidden;
}


void SUTLobbyMatchSetupPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if ( MatchInfo.IsValid())
	{
		if (MatchInfo->CurrentRuleset.IsValid())
		{
			if (MatchInfo->bRedirectsHaveChanged)
			{
				PlayerOwner->AccquireContent(MatchInfo->Redirects);
				MatchInfo->bRedirectsHaveChanged = false;
			}

		}
		else
		{
			if (MatchInfo->CurrentState == ELobbyMatchState::Setup)
			{
				MatchInfo->ServerMatchIsReadyForPlayers();
				ChooseGameClicked();
			}
		}

		if (MatchInfo.IsValid() && MatchInfo->CurrentState == ELobbyMatchState::Launching)
		{
			BlinkyTimer += InDeltaTime;
			if (BlinkyTimer > 0.25)
			{
				Dots++;
				if (Dots > 5) Dots = 0;
				BlinkyTimer = 0.0f;
			}
		}
	}
}

TSharedRef<SWidget> SUTLobbyMatchSetupPanel::BuildHostOptionWidgets()
{
	TSharedPtr<SVerticalBox> OuterContainer;
	TSharedPtr<SHorizontalBox> Container;

	SAssignNew(OuterContainer, SVerticalBox)
	+SVerticalBox::Slot()
	.Padding(10.0f, 0.0f, 20.0f, 0.0f )
	.AutoHeight()
	[
		// Server Rules Title
		SNew(STextBlock)
		.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Medium.Bold")
		.Text(this, &SUTLobbyMatchSetupPanel::GetMatchRulesTitle)
		.ColorAndOpacity(FLinearColor::Yellow)
	]
	+SVerticalBox::Slot()
	.AutoHeight()
	[
		SAssignNew(Container, SHorizontalBox)
	];


	if (Container.IsValid())
	{
		Container->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(10.0f, 0.0f, 20.0f, 0.0f )
		[
			SNew(SCheckBox)
			.IsChecked(this, &SUTLobbyMatchSetupPanel::GetLimitRankState)
			.IsEnabled(bIsHost)
			.OnCheckStateChanged(this, &SUTLobbyMatchSetupPanel::RankCeilingChanged)
			.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
			.Content()
			[
				SNew(STextBlock)
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
				.Text(NSLOCTEXT("SULobbySetup", "LimitSkill", "Limit Rank"))
			]
		];

		Container->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 20.0f, 0.0f )
		[
			SNew(SCheckBox)
			.IsChecked(this, &SUTLobbyMatchSetupPanel::GetAllowSpectatingState)
			.IsEnabled(bIsHost)
			.OnCheckStateChanged(this, &SUTLobbyMatchSetupPanel::AllowSpectatorChanged)
			.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
			.Content()
			[
				SNew(STextBlock)
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
				.Text(NSLOCTEXT("SULobbySetup", "AllowSpectators", "Allow Spectating"))
			]
		];

		Container->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 20.0f, 0.0f )
		[
			SNew(SCheckBox)
			.IsChecked(this, &SUTLobbyMatchSetupPanel::GetAllowJIPState)
			.IsEnabled(bIsHost)
			.OnCheckStateChanged(this, &SUTLobbyMatchSetupPanel::JoinAnyTimeChanged)
			.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
			.Content()
			[
				SNew(STextBlock)
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
				.Text(NSLOCTEXT("SULobbySetup", "AllowJoininProgress", "Allow Join-In-Progress"))
			]
		];

		Container->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 20.0f, 0.0f )
		[
			SNew(SCheckBox)
			.IsChecked(this, &SUTLobbyMatchSetupPanel::GetFriendsOnlyState)
			.IsEnabled(bIsHost)
			.OnCheckStateChanged(this, &SUTLobbyMatchSetupPanel::FriendsOnlyChanged)
			.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
			.Content()
			[
				SNew(STextBlock)
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
				.Text(NSLOCTEXT("SULobbySetup", "FriendsOnly", "Private Match"))
			]
		];


	}

	return OuterContainer.ToSharedRef();
}

void SUTLobbyMatchSetupPanel::BuildGameRulesPanel()
{
	if (GameRulesPanel.IsValid())
	{
		GameRulesPanel->ClearChildren();
		if (MatchInfo.IsValid() && MatchInfo->CurrentRuleset.IsValid())
		{
			GameRulesPanel->AddSlot()
			.AutoHeight()
			.Padding(0.0,10.0,0.0,10.0)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					// Show the Game type Icon
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Right)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.HAlign(HAlign_Right)
						.AutoWidth()
						[
							SNew(SBox)
							.WidthOverride(256)
							.HeightOverride(256)							
							[
								SNew(SImage)
								.Image(this, &SUTLobbyMatchSetupPanel::GetGameModeBadge)
								.ColorAndOpacity(FLinearColor(1.0f,1.0f,1.0f,0.18f))
							]
						]
					]
				]
				+SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SRichTextBlock)
						.TextStyle(SUWindowsStyle::Get(),"UT.Hub.RulesText")
						.Justification(ETextJustify::Left)
						.DecoratorStyleSet( &SUWindowsStyle::Get() )
						.AutoWrapText( true )
						.Text(this, &SUTLobbyMatchSetupPanel::GetMatchRulesDescription)
					]
				]
			];
		}
	}
}

TSharedRef<SWidget> SUTLobbyMatchSetupPanel::AddChangeButton()
{
	if (bIsHost)
	{
		return SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).WidthOverride(200).HeightOverride(32)
					[
						SNew(SButton)
						.ButtonStyle(SUTStyle::Get(),"UT.SimpleButton")
						.ContentPadding(FMargin(32.0f, 5.0f))
						.Text(NSLOCTEXT("HUB","ChangeRulesButton","Choose Game"))
						.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
						.IsEnabled(this, &SUTLobbyMatchSetupPanel::CanChooseGame)
						.OnClicked(this, &SUTLobbyMatchSetupPanel::ChooseGameClicked)
					]
				]
			];
	}
	else
	{
		return SNew(SCanvas);
	}
}

TSharedRef<SWidget> SUTLobbyMatchSetupPanel::AddActionButtons()
{
	if (bIsHost)
	{
		return SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox).WidthOverride(220).HeightOverride(32)
				[
					SNew(SButton)
					.ButtonStyle(SUTStyle::Get(),"UT.SimpleButton")
					.ContentPadding(FMargin(32.0f, 5.0f))
					.Text(this, &SUTLobbyMatchSetupPanel::GetStartMatchText)
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
					.OnClicked(this, &SUTLobbyMatchSetupPanel::StartMatchClicked)
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f,0.0f,10.0f,0.0f)
			[
				SNew(SBox).WidthOverride(220).HeightOverride(32)
				[
					SNew(SButton)
					.ButtonStyle(SUTStyle::Get(),"UT.SimpleButton")
					.ContentPadding(FMargin(32.0f, 5.0f))
					.Text(NSLOCTEXT("HUB","ExitMatch","Leave Match"))
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
					.OnClicked(this, &SUTLobbyMatchSetupPanel::LeaveMatchClicked)
				]
			]
		];

	}
	else
	{
		return SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().AutoWidth()
			.Padding(0.0,0.0,10.0,0.0)
			[
				SNew(SBox).WidthOverride(200).HeightOverride(32)
				[
					SNew(SButton)
					.ButtonStyle(SUTStyle::Get(),"UT.SimpleButton")
					.ContentPadding(FMargin(32.0f, 5.0f))
					.Text(NSLOCTEXT("HUB","ExitMatch","Leave Match"))
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
					.OnClicked(this, &SUTLobbyMatchSetupPanel::LeaveMatchClicked)
				]
			]
		];
	}

}

bool SUTLobbyMatchSetupPanel::CanChooseGame() const
{
	return (MatchInfo.IsValid() && MatchInfo->CurrentState == ELobbyMatchState::WaitingForPlayers);
}

FText SUTLobbyMatchSetupPanel::GetStartMatchText() const
{
	if (MatchInfo->CurrentState == ELobbyMatchState::WaitingForPlayers)
	{
		if (MatchInfo->CurrentRuleset.IsValid())
		{
			AUTLobbyGameState* LobbyGameState = GWorld->GetGameState<AUTLobbyGameState>();
			if (LobbyGameState && LobbyGameState->NumMatchesInProgress() > 0)
			{
				if (!MatchInfo->bJoinAnytime || !LobbyGameState->bAllowInstancesToStartWithBots || MatchInfo->BotSkillLevel < 0)
				{
					int32 NumPlayersNeeded = MatchInfo->CurrentRuleset->MinPlayersToStart - MatchInfo->Players.Num();
					if (NumPlayersNeeded > 0)
					{
						return FText::Format(NSLOCTEXT("HUB","StartMatchFormat"," Need {0} Player(s)"), FText::AsNumber(NumPlayersNeeded));
					}
				}
			}
		}

		return NSLOCTEXT("HUB","StartMatch","START MATCH");
	}

	return NSLOCTEXT("HUB","AbortMatch","ABORT MATCH");
}

FReply SUTLobbyMatchSetupPanel::LeaveMatchClicked()
{
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController)
	{
		AUTLobbyPlayerState* LobbyPlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
		if (LobbyPlayerState)
		{
			LobbyPlayerState->ServerDestroyOrLeaveMatch();
		}
	}

	return FReply::Handled();
}

FReply SUTLobbyMatchSetupPanel::StartMatchClicked()
{
	if (bIsHost)
	{
		if (MatchInfo->CurrentState == ELobbyMatchState::WaitingForPlayers)
		{
			MatchInfo->ServerStartMatch();
		}
		else
		{
			MatchInfo->ServerAbortMatch();
		}
	}

	return FReply::Handled();
}

FReply SUTLobbyMatchSetupPanel::ChooseGameClicked()
{
	AUTLobbyGameState* LobbyGameState = GWorld->GetGameState<AUTLobbyGameState>();
	if (PlayerOwner.IsValid() && !SetupDialog.IsValid() && LobbyGameState && LobbyGameState->AvailableGameRulesets.Num() > 0 )
	{
		SAssignNew(SetupDialog, SUTGameSetupDialog)
			.PlayerOwner(PlayerOwner)
			.GameRuleSets(LobbyGameState->AvailableGameRulesets)
			.OnDialogResult(this, &SUTLobbyMatchSetupPanel::OnGameChangeDialogResult);

		if ( SetupDialog.IsValid() )
		{
			LastMapInfo.Reset();
			if (MatchInfo.IsValid() && MatchInfo->CurrentRuleset.IsValid())
			{
				SetupDialog->ApplyCurrentRuleset(MatchInfo);
				SetupDialog->BotSkillLevel = MatchInfo->BotSkillLevel;
			}
			else
			{
				SetupDialog->BotSkillLevel = -1;
			}
			PlayerOwner->OpenDialog(SetupDialog.ToSharedRef(), 100);
		}
	
	}
	return FReply::Handled();
}

void SUTLobbyMatchSetupPanel::JoinAnyTimeChanged(ECheckBoxState NewState)
{
	if (MatchInfo.IsValid())
	{
		MatchInfo->SetAllowJoinInProgress(NewState == ESlateCheckBoxState::Checked);
	}
}

void SUTLobbyMatchSetupPanel::FriendsOnlyChanged(ECheckBoxState NewState)
{
	if (MatchInfo.IsValid())
	{
		MatchInfo->SetPrivateMatch(NewState == ESlateCheckBoxState::Checked);
	}
}


void SUTLobbyMatchSetupPanel::AllowSpectatorChanged(ECheckBoxState NewState)
{
	if (MatchInfo.IsValid())
	{
		MatchInfo->SetAllowSpectating(NewState == ESlateCheckBoxState::Checked);
	}

}

void SUTLobbyMatchSetupPanel::RankCeilingChanged(ECheckBoxState NewState)
{
	if (MatchInfo.IsValid())
	{
		MatchInfo->SetRankLocked(NewState == ESlateCheckBoxState::Checked);
	}
}

FText SUTLobbyMatchSetupPanel::GetStatusText() const
{
	if (MatchInfo.IsValid())
	{
		if (MatchInfo->CurrentRuleset.IsValid() && PlayerOwner->IsDownloadInProgress())
		{
			return PlayerOwner->GetDownloadStatusText();
		}
		else if (MatchInfo->CurrentState == ELobbyMatchState::WaitingForPlayers)
		{
			if (bIsHost)
			{
				return NSLOCTEXT("LobbyMessages","WaitingForPlayers","Set up your match and press Start Match when ready.");
			}

			return NSLOCTEXT("LobbyMessages","WaitingForHost","Waiting for the host to start...");
		}
		else if (MatchInfo->CurrentState == ELobbyMatchState::Launching)
		{
			FString Text = TEXT("");
			for (int32 i=0;i<Dots;i++)
			{
				Text = Text + TEXT(".");
			}

			return FText::Format(NSLOCTEXT("LobbyMessages","Launching","Launching Match, please wait..{0}"), FText::FromString(Text));
		}
		else if (MatchInfo->CurrentState == ELobbyMatchState::Aborting)
		{
			return NSLOCTEXT("LobbyMessages","Aborting","Aborting... please wait!");
		}
	}

	return NSLOCTEXT("LobbyMessages","Setup","Initializing....");
}

void SUTLobbyMatchSetupPanel::OnMatchInfoUpdated()
{
	if (MatchInfo->OwnerId.IsValid() && MatchInfo->CurrentRuleset.IsValid())
	{
		BuildGameRulesPanel();
	}
}

void SUTLobbyMatchSetupPanel::OnRulesetUpdated()
{
	if (RulesetUpdatedDelegate.IsBound())
	{
		RulesetUpdatedDelegate.Execute();
	}
}

FText SUTLobbyMatchSetupPanel::GetMatchRulesTitle() const
{
	return (MatchInfo.IsValid() && MatchInfo->CurrentRuleset.IsValid()) ? FText::FromString(FString::Printf(TEXT("GAME MODE: %s"), *MatchInfo->CurrentRuleset->Title)) : FText::GetEmpty();
}

FText SUTLobbyMatchSetupPanel::GetMatchRulesDescription() const
{
	if (MatchInfo.IsValid() && MatchInfo->CurrentRuleset.IsValid())
	{
		FString Desc = MatchInfo->CurrentRuleset->GetDescription();
		switch (MatchInfo->BotSkillLevel)
		{
			case 0 : Desc += TEXT("\nBotSkill: Novice"); break;
			case 1 : Desc += TEXT("\nBotSkill: Average"); break;
			case 2 : Desc += TEXT("\nBotSkill: Experienced"); break;
			case 3 : Desc += TEXT("\nBotSkill: Skilled"); break;
			case 4 : Desc += TEXT("\nBotSkill: Adept"); break;
			case 5 : Desc += TEXT("\nBotSkill: Masterful"); break;
			case 6 : Desc += TEXT("\nBotSkill: Inhuman"); break;
			case 7 : Desc += TEXT("\nBotSkill: Godlike"); break;
		}

		return FText::FromString(Desc);

	}
	return FText::GetEmpty();
}

ECheckBoxState SUTLobbyMatchSetupPanel::GetLimitRankState() const
{
	return (MatchInfo.IsValid() && MatchInfo->bRankLocked) ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
}

ECheckBoxState SUTLobbyMatchSetupPanel::GetAllowSpectatingState() const
{
	return (MatchInfo.IsValid() && MatchInfo->bSpectatable) ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
}

ECheckBoxState SUTLobbyMatchSetupPanel::GetAllowJIPState() const
{
	return (MatchInfo.IsValid() && MatchInfo->bJoinAnytime) ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
}

ECheckBoxState SUTLobbyMatchSetupPanel::GetFriendsOnlyState() const
{
	return (MatchInfo.IsValid() && MatchInfo->IsPrivateMatch() ) ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
}

void SUTLobbyMatchSetupPanel::OnGameChangeDialogResult(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonPressed)
{
	if (ButtonPressed == UTDIALOG_BUTTON_OK && MatchInfo.IsValid() && SetupDialog.IsValid() )
	{
		if (SetupDialog->IsCustomSettings())
		{
			FString GameMode;
			FString StartingMap;
			FString Description;
			TArray<FString> GameOptions;

			int32 DesiredPlayerCount = 0;
			int32 bTeamGame = 0;
			SetupDialog->GetCustomGameSettings(GameMode, StartingMap, Description, GameOptions, DesiredPlayerCount, bTeamGame);
			MatchInfo->ServerCreateCustomRule(GameMode, StartingMap, Description, GameOptions, SetupDialog->BotSkillLevel, DesiredPlayerCount, bTeamGame != 0);
		}

		else if (SetupDialog->SelectedRuleset.IsValid())
		{
			FString StartingMap = SetupDialog->GetSelectedMap();
			MatchInfo->ServerSetRules(SetupDialog->SelectedRuleset->UniqueTag, StartingMap, SetupDialog->BotSkillLevel);
		}
	}
	else if (ButtonPressed == UTDIALOG_BUTTON_CANCEL)
	{
		if (bIsHost && !MatchInfo->CurrentRuleset.IsValid())	
		{
			AUTLobbyPlayerState* PlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
			if (PlayerState)
			{
				PlayerState->ServerDestroyOrLeaveMatch();
			}
		}
	}

	SetupDialog.Reset();
}

const FSlateBrush* SUTLobbyMatchSetupPanel::GetGameModeBadge() const
{
	if (MatchInfo.IsValid() && MatchInfo->CurrentRuleset.IsValid())
	{
		return MatchInfo->CurrentRuleset->GetSlateBadge();
	}

	return SUWindowsStyle::Get().GetBrush("UWindows.Lobby.MatchBadge");
}

const FSlateBrush* SUTLobbyMatchSetupPanel::GetMapImage() const
{
	if (MatchInfo.IsValid() && MatchInfo->InitialMapInfo.IsValid() && MatchInfo->InitialMapInfo->MapBrush)
	{
		return MatchInfo->InitialMapInfo->MapBrush;
	}

	return DefaultLevelScreenshot;
}

FText SUTLobbyMatchSetupPanel::GetMapName() const
{
	if (MatchInfo.IsValid() && MatchInfo->InitialMapInfo.IsValid())
	{
		return FText::FromString(MatchInfo->InitialMapInfo->Title);
	}
	
	return FText::FromString(TEXT("???"));
}

#endif
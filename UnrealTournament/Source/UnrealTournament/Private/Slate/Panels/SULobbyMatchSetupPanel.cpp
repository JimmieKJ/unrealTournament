
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"
#include "../SUTUtils.h"
#include "../SUWGameSetupDialog.h"
#include "SUMidGameInfoPanel.h"
#include "SULobbyMatchSetupPanel.h"
#include "UTLobbyMatchInfo.h"
#include "UTGameEngine.h"
#include "UTLevelSummary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UTReplicatedMapInfo.h"


#if !UE_SERVER

void SULobbyMatchSetupPanel::Construct(const FArguments& InArgs)
{
	BlinkyTimer = 0.0f;
	Dots=0;

	LastMapInfo.Reset();

	bIsHost = InArgs._bIsHost;
	MatchInfo = InArgs._MatchInfo;
	PlayerOwner = InArgs._PlayerOwner;

	OnMatchInfoUpdatedDelegate.BindSP(this, &SULobbyMatchSetupPanel::OnMatchInfoUpdated); 
	MatchInfo->OnMatchInfoUpdatedDelegate = OnMatchInfoUpdatedDelegate;

	OnRulesetUpdatedDelegate.BindSP(this, &SULobbyMatchSetupPanel::OnRulesetUpdated);
	MatchInfo->OnRulesetUpdatedDelegate = OnRulesetUpdatedDelegate;

	AUTLobbyGameState* LobbyGameState = GWorld->GetGameState<AUTLobbyGameState>();

	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.AutoHeight()
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				SNew(SImage)
				.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.DarkBackground"))
			]
			+SOverlay::Slot()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding(15.0f,10.0f, 10.0f, 10.0f)
				.AutoWidth()
				[
					BuildHostOptionWidgets()
				]
			]
		]

		// Game Rules
		+SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Fill)
		[
			SAssignNew(GameRulesPanel, SVerticalBox)
		]

		// Player List
		+SVerticalBox::Slot()
		.Padding(5.0f, 5.0f, 5.0f, 0.0f)
		.AutoHeight()
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				SNew(SImage)
				.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.DarkBackground"))
			]
			+SOverlay::Slot()
			[
				SNew(SScrollBox)
				.Orientation(Orient_Horizontal)
				+SScrollBox::Slot()
				[
					SAssignNew(PlayerListBox, SHorizontalBox)
				]
			]

		]

		// Statusbar
		+SVerticalBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 0.0f)
		.AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(48)
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
						.Text(this, &SULobbyMatchSetupPanel::GetStatusText )
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.OnClicked(this, &SULobbyMatchSetupPanel::CancelDownloadClicked)
						.Visibility(this, &SULobbyMatchSetupPanel::CancelButtonVisible)
						.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SULobbyMatchSetupPanel","CancelDownload","[Click to Cancel]"))
							.TextStyle(SUWindowsStyle::Get(),"UT.Hub.RulesText_Small")
						]
					]

				]
			]
		]

	];

	OnMatchInfoUpdated();
	OnRulesetUpdated();

	if (MatchInfo.IsValid() && MatchInfo->InitialMapInfo.IsValid())
	{
		BuildMapList();
	}
}

FReply SULobbyMatchSetupPanel::CancelDownloadClicked()
{
	if (PlayerOwner.IsValid() && PlayerOwner->IsDownloadInProgress())
	{
		PlayerOwner->CancelDownload();
	}
	return FReply::Handled();
}

EVisibility SULobbyMatchSetupPanel::CancelButtonVisible() const
{
	return PlayerOwner->IsDownloadInProgress() ? EVisibility::Visible : EVisibility::Hidden;
}


void SULobbyMatchSetupPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if ( MatchInfo.IsValid())
	{
		if (MatchInfo->CurrentRuleset.IsValid())
		{
			MatchInfo->LoadInitialMapInfo();	
			if ( MatchInfo->InitialMapInfo.IsValid() )
			{
				if (!LastMapInfo.IsValid() || !MatchInfo->InitialMapInfo->MapPackageName.Equals(LastMapInfo->MapPackageName))
				{
					if (MatchInfo->InitialMapInfo.IsValid())
					{
						BuildMapList();
						LastMapInfo = MatchInfo->InitialMapInfo;
					}
				}
			}

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


		BuildPlayerList(InDeltaTime);

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



TSharedRef<SWidget> SULobbyMatchSetupPanel::BuildHostOptionWidgets()
{
	TSharedPtr<SHorizontalBox> Container;

	SNew(SVerticalBox)
	+SVerticalBox::Slot()
	.VAlign(VAlign_Center)
	[
		SAssignNew(Container, SHorizontalBox)
	];

	if (Container.IsValid())
	{

		Container->AddSlot()
		.AutoWidth()
		.Padding(10.0f, 0.0f, 20.0f, 0.0f )
		[
			SNew(STextBlock)
			.TextStyle(SUWindowsStyle::Get(),"UT.Hub.RulesTitle")
			.Text(this, &SULobbyMatchSetupPanel::GetMatchRulesTitle)
			.ColorAndOpacity(FLinearColor::Yellow)
		];

		Container->AddSlot()
		.FillWidth(1.0)
		[
			SNew(SCanvas)
		];

		Container->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(10.0f, 0.0f, 20.0f, 0.0f )
		[
			SNew(SCheckBox)
			.IsChecked(this, &SULobbyMatchSetupPanel::GetLimitRankState)
			.IsEnabled(bIsHost)
			.OnCheckStateChanged(this, &SULobbyMatchSetupPanel::RankCeilingChanged)
			.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
			.Content()
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				.Text(NSLOCTEXT("SULobbySetup", "LimitSkill", "Limit Rank"))
			]
		];

		Container->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 20.0f, 0.0f )
		[
			SNew(SCheckBox)
			.IsChecked(this, &SULobbyMatchSetupPanel::GetAllowSpectatingState)
			.IsEnabled(bIsHost)
			.OnCheckStateChanged(this, &SULobbyMatchSetupPanel::AllowSpectatorChanged)
			.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
			.Content()
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				.Text(NSLOCTEXT("SULobbySetup", "AllowSpectators", "Allow Spectating"))
			]
		];

		Container->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 20.0f, 0.0f )
		[
			SNew(SCheckBox)
			.IsChecked(this, &SULobbyMatchSetupPanel::GetAllowJIPState)
			.IsEnabled(bIsHost)
			.OnCheckStateChanged(this, &SULobbyMatchSetupPanel::JoinAnyTimeChanged)
			.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
			.Content()
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				.Text(NSLOCTEXT("SULobbySetup", "AllowJoininProgress", "Allow Join-In-Progress"))
			]
		];
	}

	return Container.ToSharedRef();
}

void SULobbyMatchSetupPanel::BuildGameRulesPanel()
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
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(10.0,0,0.0,0)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBox)
						.WidthOverride(256)
						.HeightOverride(256)							
						[
							SNew(SImage)
							.Image(this, &SULobbyMatchSetupPanel::GetGameModeBadge)
						]
					]
					+SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(SBox)
						.HeightOverride(42)
						[
							AddChangeButton()
						]
					]
				]
				+SHorizontalBox::Slot()
				.Padding(10.0,0,0.0,0)
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(700)
					.HeightOverride(370)
					[
						SNew(SScrollBox)
						.Style(SUWindowsStyle::Get(),"UT.ScrollBox.Borderless")
						+ SScrollBox::Slot()
						[
							SNew(SVerticalBox)

							+SVerticalBox::Slot()
							.FillHeight(1.0)
							[
								SNew(SRichTextBlock)
								.TextStyle(SUWindowsStyle::Get(),"UT.Hub.RulesText")
								.Justification(ETextJustify::Left)
								.DecoratorStyleSet( &SUWindowsStyle::Get() )
								.AutoWrapText( true )
								.Text(this, &SULobbyMatchSetupPanel::GetMatchRulesDescription)
							]
						]
					]
				]
				+SHorizontalBox::Slot()
				.Padding(30.0,0,20.0,0)
				.FillWidth(1.0)
				[
					SNew(SBox)
					.HeightOverride(370)
					[
						SNew(SScrollBox)
						.Style(SUWindowsStyle::Get(),"UT.ScrollBox.Borderless")
						+ SScrollBox::Slot()
						[
							SAssignNew(MapListPanel, SVerticalBox)
						]
					]
				]
			];
		}
	}
}

TSharedRef<SWidget> SULobbyMatchSetupPanel::AddChangeButton()
{
	if (bIsHost)
	{
		return SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("HUB","ChangeRulesButton","Choose Game"))
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
			.IsEnabled(this, &SULobbyMatchSetupPanel::CanChooseGame)
			.OnClicked(this, &SULobbyMatchSetupPanel::ChooseGameClicked);
	}
	else
	{
		return SNew(SCanvas);
	}
}

bool SULobbyMatchSetupPanel::CanChooseGame() const
{
	return (MatchInfo.IsValid() && MatchInfo->CurrentState == ELobbyMatchState::WaitingForPlayers);
}


FReply SULobbyMatchSetupPanel::ChooseGameClicked()
{
	AUTLobbyGameState* LobbyGameState = GWorld->GetGameState<AUTLobbyGameState>();
	if (PlayerOwner.IsValid() && !SetupDialog.IsValid() && LobbyGameState && LobbyGameState->AvailableGameRulesets.Num() > 0 )
	{
		SAssignNew(SetupDialog, SUWGameSetupDialog)
			.PlayerOwner(PlayerOwner)
			.GameRuleSets(LobbyGameState->AvailableGameRulesets)
			.OnDialogResult(this, &SULobbyMatchSetupPanel::OnGameChangeDialogResult);

		if ( SetupDialog.IsValid() )
		{
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

void SULobbyMatchSetupPanel::JoinAnyTimeChanged(ECheckBoxState NewState)
{
	if (MatchInfo.IsValid())
	{
		MatchInfo->SetAllowJoinInProgress(NewState == ESlateCheckBoxState::Checked);
	}
}

void SULobbyMatchSetupPanel::AllowSpectatorChanged(ECheckBoxState NewState)
{
	if (MatchInfo.IsValid())
	{
		MatchInfo->SetAllowSpectating(NewState == ESlateCheckBoxState::Checked);
	}

}

void SULobbyMatchSetupPanel::RankCeilingChanged(ECheckBoxState NewState)
{
	if (MatchInfo.IsValid())
	{
		MatchInfo->SetRankLocked(NewState == ESlateCheckBoxState::Checked);
	}
}


void SULobbyMatchSetupPanel::BuildPlayerList(float DeltaTime)
{
	if (PlayerListBox.IsValid() && MatchInfo.IsValid() && MatchInfo->CurrentRuleset.IsValid())
	{
		FString MenuOptions = MatchInfo->CurrentRuleset->bTeamGame ? TEXT("Change Team,Spectate,Kick,Ban") : TEXT("Spectate,Kick,Ban");

		bool bRequiresFullRefresh = false;
		bool bRequiresStatusRefresh = false;

		// Find any players who are no longer in the match list.
		for (int32 i=0; i < PlayerData.Num(); i++)
		{
			if (PlayerData[i].IsValid())
			{
				int32 Idx = (PlayerData[i]->PlayerState.IsValid()) ? MatchInfo->Players.Find( PlayerData[i]->PlayerState.Get() ) : INDEX_NONE;
				if (Idx == INDEX_NONE )
				{
					bRequiresFullRefresh = true;
					break;
				}
			}
		}

		// Look for new players to the match
		for (int32 i=0; i < MatchInfo->Players.Num(); i++)
		{
			int32 PlayerIndex = INDEX_NONE;
			for (int32 j=0; j < PlayerData.Num(); j++)
			{
				if (PlayerData[j]->PlayerState.IsValid() && PlayerData[j]->PlayerState.Get() == MatchInfo->Players[i].Get())
				{
					PlayerIndex = j;
					break;
				}
			}

			if (PlayerIndex == INDEX_NONE)
			{
				bRequiresFullRefresh = true;
				break;
			}
			else
			{
				// This player already exists, look to see if their team or status has changed
				if (PlayerIndex < PlayerData.Num() && PlayerData[PlayerIndex]->PlayerState.IsValid())
				{
					// If the team has changed, we have to rebuild the whole list
					if (PlayerData[PlayerIndex]->TeamNum != PlayerData[PlayerIndex]->PlayerState->DesiredTeamNum)
					{
						bRequiresFullRefresh = true;
						break;
					}
					if (PlayerData[PlayerIndex]->NeedsStatusRefresh())
					{
						bRequiresStatusRefresh = true;
					}
				}
				else
				{
					// We should never get here.  But if we do figure something went wrong and
					// rebuild the whole list.
					bRequiresFullRefresh = true;
					break;
				}
			}
		}

		if (bRequiresFullRefresh)
		{
			// First, build the player list and sort it by teams

			PlayerData.Empty();

			for (int32 i=0; i < MatchInfo->Players.Num(); i++)
			{
				if (MatchInfo->Players[i].IsValid())
				{
					int32 InsertIndex = 0;
					uint8 TeamNum = MatchInfo->Players[i]->DesiredTeamNum;
					while (InsertIndex < PlayerData.Num() && PlayerData[InsertIndex]->TeamNum <= TeamNum)
					{
						InsertIndex++;
					}
					PlayerData.Insert( FMatchPlayerData::Make(MatchInfo->Players[i]), InsertIndex );
				}
			}

			// Now build the actual list elements

			PlayerListBox->ClearChildren();
			for (int32 i=0; i < PlayerData.Num(); i++)
			{
				TSharedPtr<SWidget> Button;
				TSharedPtr<SUTComboButton> ComboButton;
				TSharedPtr<STextBlock> StatusText;

				FString SubMenu = TEXT("");

				if (bIsHost)
				{
					if ( PlayerData[i]->PlayerState->UniqueId != MatchInfo->OwnerId )
					{
						SubMenu = MenuOptions;
					}
					else if (MatchInfo->CurrentRuleset->bTeamGame)
					{
						SubMenu = TEXT("Change Team");
					}
				}

				static FName NAME_RedImage = FName(TEXT("UT.TeamBrush.Red"));
				static FName NAME_BlueImage = FName(TEXT("UT.TeamBrush.Blue"));
				static FName NAME_SpecImage = FName(TEXT("UT.TeamBrush.Spectator"));

				FName TeamImage;
				uint8 TeamNum = PlayerData[i]->PlayerState->DesiredTeamNum;
				switch (TeamNum)
				{
					case 0 : TeamImage = NAME_RedImage; break;
					case 1 : TeamImage = NAME_BlueImage; break;
					default: TeamImage = NAME_SpecImage; break;
				}

				PlayerListBox->AddSlot()
				[
					SAssignNew(Button, SBox)
					.WidthOverride(125)
					.HeightOverride(160)
					[
						SAssignNew(ComboButton, SUTComboButton)
						.ButtonStyle(SUWindowsStyle::Get(),"UWindows.Lobby.MatchBar.Button")
						.MenuButtonStyle(SUWindowsStyle::Get(),"UT.ContextMenu.Button")
						.MenuButtonTextStyle(SUWindowsStyle::Get(),"UT.ContextMenu.TextStyle")
						.HasDownArrow(false)
						.OnButtonSubMenuSelect(this, &SULobbyMatchSetupPanel::OnSubMenuSelect)
						.bRightClickOpensMenu(true)
						.MenuPlacement(MenuPlacement_MenuRight)
						.DefaultMenuItems(SubMenu)
						.ButtonContent()
						[
							SNew(SVerticalBox)

							// The player's Image - TODO: We need a way to download the portrait of a given player 
							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0,5.0,0.0,0.0)
							.HAlign(HAlign_Center)
							[
								SNew(SBox)
								.WidthOverride(102)
								.HeightOverride(128)							
								[
									SNew(SOverlay)
									+SOverlay::Slot()
									[
										SNew(SImage)
										.Image(SUWindowsStyle::Get().GetBrush("Testing.TestPortrait"))
									]
									+SOverlay::Slot()
									[
										SNew(SOverlay)

										// The Team Color background
										+SOverlay::Slot()
										[
											SNew(SVerticalBox)
											+SVerticalBox::Slot().AutoHeight()
											[
												SNew(SHorizontalBox)
												+SHorizontalBox::Slot().AutoWidth()
												[
													SNew(SBox).WidthOverride(102).HeightOverride(24)
													[
														SNew(SImage)
														.Image(SUWindowsStyle::Get().GetBrush(TeamImage))
													]
												]
											]
										]
										+SOverlay::Slot()
										[
											SNew(SHorizontalBox)
											+SHorizontalBox::Slot().HAlign(HAlign_Center)
											[
												SAssignNew(StatusText, STextBlock)
												.TextStyle(SUWindowsStyle::Get(), "UT.Hub.MapsText")
											]
										]

									]
									+SOverlay::Slot()
									.VAlign(VAlign_Bottom)
									.HAlign(HAlign_Right)
									[
										SNew(SHorizontalBox)
										+SHorizontalBox::Slot()
										.AutoWidth()
										[
											SNew(SVerticalBox)
											+SVerticalBox::Slot()
											.AutoHeight()
											[
												BuildELOBadgeForPlayer(MatchInfo->Players[i])
											]
										]
									]
								]
							]
							+SVerticalBox::Slot()
							.Padding(0.0,0.0,0.0,5.0)
							.AutoHeight()
							.HAlign(HAlign_Center)
							[
								SNew(STextBlock)
								.Text(FText::FromString(PlayerData[i]->PlayerState->PlayerName))
								.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.SmallButton.TextStyle")
							]

						]
					]
				];

				PlayerData[i]->Button = Button;
				PlayerData[i]->ComboButton = ComboButton;
				PlayerData[i]->StatusText = StatusText;
			}

		}

		if (bRequiresStatusRefresh || bRequiresFullRefresh)
		{
			for (int32 i=0; i< PlayerData.Num(); i++)
			{
				if (PlayerData[i].IsValid())
				{
					AUTLobbyPlayerState* PS = PlayerData[i].Get()->PlayerState.Get();	
					if (PS)
					{
						PlayerData[i].Get()->ComboButton->SetButtonStyle( !PS->bReadyToPlay ? &SUWindowsStyle::Get().GetWidgetStyle<FButtonStyle>("UWindows.Match.PlayerButton") : &SUWindowsStyle::Get().GetWidgetStyle<FButtonStyle>("UWindows.Match.ReadyPlayerButton") );

						if (PS->UniqueId == MatchInfo->OwnerId)
						{
							PlayerData[i]->StatusText->SetText(NSLOCTEXT("SULobbyMatchSetupPanel","Host","Host"));
						}
						else
						{
							if (PS->bReadyToPlay)
							{
								PlayerData[i]->StatusText->SetText(NSLOCTEXT("SULobbyMatchSetupPanel","Ready","Ready"));
							}
							else
							{
								PlayerData[i]->StatusText->SetText(NSLOCTEXT("SULobbyMatchSetupPanel","NotReady","Not Ready"));
							}
						
						}

						PlayerData[i]->bReadyToPlay = PS->bReadyToPlay;
					}
				}
			}
		}
	}
}

TSharedRef<SWidget> SULobbyMatchSetupPanel::BuildELOBadgeForPlayer(TWeakObjectPtr<AUTPlayerState> PlayerState)
{
	int32 Badge = 0;
	int32 Level = 0;

	if (PlayerOwner.IsValid() && MatchInfo.IsValid() && PlayerState.IsValid())
	{
		UUTLocalPlayer::GetBadgeFromELO(PlayerState->AverageRank, Badge, Level);
	}

	FString BadgeStr = FString::Printf(TEXT("UT.Badge.%i"), Badge);
	FString BadgeNumStr = FString::Printf(TEXT("UT.Badge.Numbers.%i"), Level+1);

	return SNew(SBox) 
		.WidthOverride(32)
		.HeightOverride(32)
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				SNew(SImage)
				.Image(SUWindowsStyle::Get().GetBrush(*BadgeStr))
			]
			+SOverlay::Slot()
			[
				SNew(SImage)
				.Image(SUWindowsStyle::Get().GetBrush(*BadgeNumStr))
			]
		];
}

void SULobbyMatchSetupPanel::OnSubMenuSelect(int32 MenuCmdId, TSharedPtr<SUTComboButton> Sender)
{
	if (bIsHost)
	{
		for (int32 i=0; i<PlayerData.Num(); i++)
		{
			if (PlayerData[i]->ComboButton.Get() == Sender.Get())
			{
				MatchInfo->ServerManageUser(MenuCmdId, PlayerData[i]->PlayerState.Get());
			}
		}
	}
}

FText SULobbyMatchSetupPanel::GetStatusText() const
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
				return NSLOCTEXT("LobbyMessages","WaitingForPlayers","Setup your match and press Start Match when ready.");
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

void SULobbyMatchSetupPanel::OnMatchInfoUpdated()
{
	if (MatchInfo->OwnerId.IsValid() && MatchInfo->CurrentRuleset.IsValid())
	{
		BuildGameRulesPanel();
	}
}

void SULobbyMatchSetupPanel::OnRulesetUpdated()
{
	if (MatchInfo->CurrentRuleset.IsValid())
	{
		PlayerData.Empty();
		BuildPlayerList(0);
	}
}

FText SULobbyMatchSetupPanel::GetMatchRulesTitle() const
{
	return (MatchInfo.IsValid() && MatchInfo->CurrentRuleset.IsValid()) ? FText::FromString(FString::Printf(TEXT("GAME MODE: %s"), *MatchInfo->CurrentRuleset->Title)) : FText::GetEmpty();
}

FText SULobbyMatchSetupPanel::GetMatchRulesDescription() const
{
	if (MatchInfo.IsValid() && MatchInfo->CurrentRuleset.IsValid())
	{
		FString Desc = MatchInfo->CurrentRuleset->Description;
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

void SULobbyMatchSetupPanel::BuildMapList()
{
	if (MapListPanel.IsValid() && MatchInfo.IsValid() && MatchInfo->CurrentRuleset.IsValid())
	{
		MapListPanel->ClearChildren();

		MapListPanel->AddSlot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("SULobbyMatchSetupPanel", "StartingMap","Starting Map..."))
			.TextStyle(SUWindowsStyle::Get(),"UT.Hub.MapsText")
			.ColorAndOpacity(FLinearColor::White)
		];

		MapListPanel->AddSlot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Text(FText::FromString(MatchInfo->InitialMapInfo->Title))
			.TextStyle(SUWindowsStyle::Get(),"UT.Hub.RulesTitle")
			.ColorAndOpacity(FLinearColor::White)
		];


		if (MatchInfo->InitialMapInfo->MapScreenshotReference!= TEXT(""))
		{
			UTexture2D* MapImage = LoadObject<UTexture2D>(nullptr, *MatchInfo->InitialMapInfo->MapScreenshotReference);
			MapScreenshot = new FSlateDynamicImageBrush(MapImage, FVector2D(256.0, 128.0), NAME_None);

		}
		else
		{
			MapScreenshot = new FSlateDynamicImageBrush(Cast<UUTGameEngine>(GEngine)->DefaultLevelScreenshot, FVector2D(256.0, 128.0), NAME_None);
		}

		MapListPanel->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.HeightOverride(256)
				.WidthOverride(512)
				[
					SNew(SImage)
					.Image(MapScreenshot)
				]
			]
		];

		MatchInfo->bMapChanged = false;

	}
}

ECheckBoxState SULobbyMatchSetupPanel::GetLimitRankState() const
{
	return (MatchInfo.IsValid() && MatchInfo->bRankLocked) ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
}

ECheckBoxState SULobbyMatchSetupPanel::GetAllowSpectatingState() const
{
	return (MatchInfo.IsValid() && MatchInfo->bSpectatable) ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
}

ECheckBoxState SULobbyMatchSetupPanel::GetAllowJIPState() const
{
	return (MatchInfo.IsValid() && MatchInfo->bJoinAnytime) ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
}


void SULobbyMatchSetupPanel::OnGameChangeDialogResult(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonPressed)
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

		// NOTE: The dialog closes itself... :)
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

const FSlateBrush* SULobbyMatchSetupPanel::GetGameModeBadge() const
{
	if (MatchInfo.IsValid() && MatchInfo->CurrentRuleset.IsValid() && MatchInfo->CurrentRuleset->SlateBadge.IsValid())
	{
		return MatchInfo->CurrentRuleset->SlateBadge.Get();
	}

	return SUWindowsStyle::Get().GetBrush("UWindows.Lobby.MatchBadge");
}


#endif
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"
#include "SUMidGameInfoPanel.h"
#include "SULobbyGameSettingsPanel.h"
#include "../Widgets/SUTComboButton.h"
#include "UTLobbyPC.h"
#include "UTGameEngine.h"
#include "UTLevelSummary.h"

#if !UE_SERVER

void SULobbyGameSettingsPanel::Construct(const FArguments& InArgs)
{
	// NOTE: leaks at the moment because Slate corrupts memory if you delete brushes
	LevelScreenshot = new FSlateDynamicImageBrush(Cast<UUTGameEngine>(GEngine)->DefaultLevelScreenshot, FVector2D(400.0f, 200.0f), FName(TEXT("LevelScreenshotLobby")));

	bIsHost = InArgs._bIsHost;
	MatchInfo = InArgs._MatchInfo;
	PlayerOwner = InArgs._PlayerOwner;

	BlinkyTimer = 0.0f;
	Dots=0;

	OnMatchMapChangedDelegate.BindSP(this, &SULobbyGameSettingsPanel::MapChanged);
	MatchInfo->OnMatchMapChanged = OnMatchMapChangedDelegate;

	OnMatchOptionsChangedDelegate.BindSP(this, &SULobbyGameSettingsPanel::OptionsChanged);
	MatchInfo->OnMatchOptionsChanged = OnMatchOptionsChangedDelegate;

	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SVerticalBox)
		// Settings panel

		+SVerticalBox::Slot()
		.Padding(5.0f, 5.0f, 5.0f, 5.0f)
		.AutoHeight()
		[
			SAssignNew(PanelContents, SOverlay)
			+SOverlay::Slot()
			[
				BuildGamePanel()
			]
		]

		+SVerticalBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 0.0f)
		.AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(32)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SImage)
					.Image(SUWindowsStyle::Get().GetBrush("UWindows.Lobby.MatchBar.Background"))
				]

				+SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("LobbyMessages","PlayersInMatch","- Players In Match -"))
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				]
			]
		]

		// Player List

		+SVerticalBox::Slot()
		.Padding(5.0f, 5.0f, 5.0f, 0.0f)
		.AutoHeight()
		[
			SNew(SScrollBox)
			.Orientation(Orient_Horizontal)
			+SScrollBox::Slot()
			[
				SAssignNew(PlayerListBox, SHorizontalBox)
			]
		]
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
						SAssignNew(StatusText,STextBlock)
						.Text(this, &SULobbyGameSettingsPanel::GetMatchMessage )
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
					]
				]
			]
		]
	];

}

void SULobbyGameSettingsPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
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


TSharedRef<SWidget> SULobbyGameSettingsPanel::BuildGamePanel()
{

	// Build the slate framework to hold everything.
	TSharedPtr<SWidget> Panel;

	SAssignNew(Panel, SBox)
		[	
			SNew(SHorizontalBox)	
			+SHorizontalBox::Slot()
			.FillWidth(0.5)
			.Padding(0.0,5.0,25.0,5.0)
			[
				SAssignNew(OptionsPanel, SVerticalBox)
			]
			+SHorizontalBox::Slot()
			.FillWidth(0.5)
			[
				SAssignNew(MapPanel, SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					BuildMapsPanel()
				]
			]
		];


	BuildOptionsFromData();

		// We are the host, say that this match is ready to go.
	if (bIsHost)
	{
		MatchInfo->ServerSetLobbyMatchState(ELobbyMatchState::WaitingForPlayers);
	}

	return Panel.ToSharedRef();

}

void SULobbyGameSettingsPanel::BuildOptionsFromData()
{
	if (OptionsPanel.IsValid())
	{
		GameConfigProps.Empty();
		if (MatchInfo.IsValid() && MatchInfo->CurrentGameModeData.IsValid() && MatchInfo->CurrentGameModeData->DefaultObject.IsValid())
		{
			MatchInfo->CurrentGameModeData->DefaultObject->CreateConfigWidgets(OptionsPanel, !bIsHost, GameConfigProps);
			// parse options data
			TArray<FString> Options;
			MatchInfo->MatchOptions.ParseIntoArray(&Options, TEXT("?"), true);
			for (const FString& Opt : Options)
			{
				FString Key, Value;
				Opt.Split(TEXT("="), &Key, &Value);
				for (TSharedPtr<TAttributePropertyBase> Prop : GameConfigProps)
				{
					if (Prop->GetURLKey() == Key)
					{
						Prop->SetFromString(Value);
					}
				}
			}
			if (bIsHost)
			{
				// set change delegates
				for (TSharedPtr<TAttributePropertyBase> Prop : GameConfigProps)
				{
					Prop->OnChange.BindSP(SharedThis(this), &SULobbyGameSettingsPanel::OnGameOptionChanged);
				}
			}
		}
	}
}

void SULobbyGameSettingsPanel::BuildPlayerList(float DeltaTime)
{
	if (PlayerListBox.IsValid() && MatchInfo.IsValid())
	{
		FString MenuOptions = bIsHost ? TEXT("Kick,Ban") : TEXT("");

		// Find any players who are no longer in the match list.
		for (int32 i=0; i < PlayerData.Num(); i++)
		{
			if (PlayerData[i].IsValid())
			{
				int32 Idx = (PlayerData[i]->PlayerState.IsValid()) ? MatchInfo->Players.Find( PlayerData[i]->PlayerState.Get() ) : INDEX_NONE;
				if (Idx == INDEX_NONE )
				
				{
					// Not found, remove this one.
					PlayerListBox->RemoveSlot(PlayerData[i]->Button.ToSharedRef());
					PlayerData.RemoveAt(i);
					i--;
				}
			}
		}

		// Find any players who are no longer in the match list.
		for (int32 i=0; i < MatchInfo->Players.Num(); i++)
		{
			if (MatchInfo->Players[i].IsValid())
			{
				bool bFound = false;
				for (int32 j=0; j<PlayerData.Num(); j++)
				{
					if (MatchInfo->Players[i] == PlayerData[j]->PlayerState.Get())
					{
						bFound = true;
						break;
					}
				}

				if (!bFound)
				{
					TSharedPtr<SWidget> Button;
					TSharedPtr<SUTComboButton> ComboButton;
					TSharedPtr<SImage> ReadyImage;

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
							.OnButtonSubMenuSelect(this, &SULobbyGameSettingsPanel::OnSubMenuSelect)
							.bRightClickOpensMenu(true)
							.MenuPlacement(MenuPlacement_MenuRight)
							.DefaultMenuItems(MenuOptions)
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
											SAssignNew(ReadyImage, SImage)
											.Image(SUWindowsStyle::Get().GetBrush("UWindows.Match.ReadyImage"))
											.Visibility(EVisibility::Hidden)
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
									.Text(MatchInfo->Players[i]->PlayerName)
									.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.SmallButton.TextStyle")
								]

							]
						]
					];

					PlayerData.Add( FMatchPlayerData::Make(MatchInfo->Players[i],Button, ComboButton, ReadyImage));
				}
			}
		}
	}

	for (int32 i=0; i< PlayerData.Num(); i++)
	{
		AUTLobbyPlayerState* PS = PlayerData[i].Get()->PlayerState.Get();	
		if (PS)
		{
			PlayerData[i].Get()->ComboButton->SetButtonStyle( !PS->bReadyToPlay ? &SUWindowsStyle::Get().GetWidgetStyle<FButtonStyle>("UWindows.Match.PlayerButton") : &SUWindowsStyle::Get().GetWidgetStyle<FButtonStyle>("UWindows.Match.ReadyPlayerButton") );
			PlayerData[i].Get()->ReadyImage->SetVisibility( PS->bReadyToPlay ? EVisibility::Visible : EVisibility::Hidden);
			if (PS->UniqueId == MatchInfo->OwnerId)
			{
				PlayerData[i].Get()->ReadyImage->SetVisibility( EVisibility::Visible);
				PlayerData[i].Get()->ReadyImage->SetImage(SUWindowsStyle::Get().GetBrush("UWindows.Match.HostImage"));
			}
		}
	}

}

TSharedRef<SWidget> SULobbyGameSettingsPanel::BuildELOBadgeForPlayer(TWeakObjectPtr<AUTPlayerState> PlayerState)
{
	int32 Badge = 0;
	int32 Level = 0;

	if (PlayerOwner.IsValid() && MatchInfo.IsValid())
	{
		PlayerOwner->GetBadgeFromELO(PlayerState->AverageRank, Badge, Level);
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

TSharedRef<SWidget> SULobbyGameSettingsPanel::ConstructContents()
{
	BuildGamePanel();
	return SNew(SCanvas);
}

void SULobbyGameSettingsPanel::MapChanged()
{
	UUTLevelSummary* Summary = UUTGameEngine::LoadLevelSummary(MatchInfo->MatchMap);
	if (Summary != NULL)
	{
		*LevelScreenshot = FSlateDynamicImageBrush(Summary->Screenshot != NULL ? Summary->Screenshot : Cast<UUTGameEngine>(GEngine)->DefaultLevelScreenshot, LevelScreenshot->ImageSize, LevelScreenshot->GetResourceName());
	}
	else
	{
		*LevelScreenshot = FSlateDynamicImageBrush(Cast<UUTGameEngine>(GEngine)->DefaultLevelScreenshot, LevelScreenshot->ImageSize, LevelScreenshot->GetResourceName());
	}
}

void SULobbyGameSettingsPanel::OptionsChanged()
{
	if (!bIsHost)
	{
		// Remove what was already there and rebuild
		OptionsPanel->ClearChildren();
		BuildOptionsFromData();
	}
}

void SULobbyGameSettingsPanel::CommitOptions()
{
}

void SULobbyGameSettingsPanel::OnSubMenuSelect(int32 MenuCmdId, TSharedPtr<SUTComboButton> Sender)
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

FText SULobbyGameSettingsPanel::GetMatchMessage() const
{
	if (MatchInfo.IsValid())
	{
		if (MatchInfo->CurrentState == ELobbyMatchState::WaitingForPlayers)
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

void SULobbyGameSettingsPanel::OnGameOptionChanged()
{
	if (GameConfigProps.Num() > 0)
	{
		FString NewOptions = GameConfigProps[0]->GetURLString();
		for (int32 i = 1; i < GameConfigProps.Num(); i++)
		{
			NewOptions += TEXT("?") + GameConfigProps[i]->GetURLString();
		}

		MatchInfo->ServerMatchOptionsChanged(NewOptions);
	}
}

TSharedRef<SWidget> SULobbyGameSettingsPanel::BuildMapsPanel()
{
	return SNew(SHorizontalBox)
	+SHorizontalBox::Slot()
	.AutoWidth()
	[
		SNew(SBox)
		.WidthOverride(400)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(5.0f,0.0f,0.0f,0.0f)
			.AutoHeight()
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("GameSettings","CurrentMap","- Map to Play -"))
				.TextStyle(SUWindowsStyle::Get(),"UT.Common.NormalText")
			]
			+SVerticalBox::Slot()
			[
				SNew(SBox)
				.HeightOverride(200)
				[
					BuildMapsList()
				]
			]
		]
	]
	+SHorizontalBox::Slot()
	.AutoWidth()
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.WidthOverride(512)
			.HeightOverride(256)							
			[
				SNew(SImage)
				.Image(LevelScreenshot)
			]
		]
	];


	if (MapList.IsValid())
	{
		MapList->SetSelection(MatchInfo->AvailableMaps[0]);
	}

}

TSharedRef<SWidget> SULobbyGameSettingsPanel::BuildMapsList()
{
	if (bIsHost)
	{
		return SAssignNew( MapList, SListView< TSharedPtr<FAllowedMapData> > )
			// List view items are this tall
			.ItemHeight(24)
			// Tell the list view where to get its source data
			.ListItemsSource(&MatchInfo->AvailableMaps)
			// When the list view needs to generate a widget for some data item, use this method
			.OnGenerateRow(this, &SULobbyGameSettingsPanel::GenerateMapListWidget)
			.OnSelectionChanged(this, &SULobbyGameSettingsPanel::OnMapListChanged)
			.SelectionMode(ESelectionMode::Single);
	}
	else
	{
		return SNew(STextBlock)
		.Text(this, &SULobbyGameSettingsPanel::GetMapText)
		.TextStyle(SUWindowsStyle::Get(),"UT.Common.ButtonText.White");
	}
}

TSharedRef<ITableRow> SULobbyGameSettingsPanel::GenerateMapListWidget(TSharedPtr<FAllowedMapData> InItem, const TSharedRef<STableViewBase>& OwningList)
{
	return SNew(STableRow<TSharedPtr<FAllowedMapData>>, OwningList)
		.Padding(5)
		[
			SNew(STextBlock)
			.Text(FText::FromString(InItem.Get()->MapName))
			.TextStyle(SUWindowsStyle::Get(),"UT.Common.ButtonText.White")
		];
}

void SULobbyGameSettingsPanel::OnMapListChanged(TSharedPtr<FAllowedMapData> SelectedItem, ESelectInfo::Type SelectInfo)
{

	if (MatchInfo.IsValid() && bIsHost)
	{
		MatchInfo->ServerMatchMapChanged(SelectedItem->MapName);
	}
}


FString SULobbyGameSettingsPanel::GetMapText() const
{
	if (MatchInfo.IsValid())
	{
		return MatchInfo->MatchMap;
	}
	return TEXT("None");
}


#endif

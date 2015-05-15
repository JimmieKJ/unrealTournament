// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWCreateGamePanel.h"
#include "../SUWindowsStyle.h"
#include "UTDMGameMode.h"
#include "AssetData.h"
#include "UTLevelSummary.h"
#include "../SUWScaleBox.h"
#include "UTMutator.h"
#include "../SUWBotConfigDialog.h"
#include "UTGameEngine.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "UTAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"

#if !UE_SERVER

#include "UserWidget.h"

void SUWCreateGamePanel::ConstructPanel(FVector2D ViewportSize)
{
	AUTGameState* GameState = GetPlayerOwner()->GetWorld()->GetGameState<AUTGameState>();
	if (GameState)
	{
		GameState->GetAvailableGameData(AllGametypes, MutatorListAvailable);
	}

	// Set the Initial Game Type, restoring the previous selection if possible.
	TSubclassOf<AUTGameMode> InitialSelectedGameClass = AUTDMGameMode::StaticClass();
	FString LastGametypePath;
	if (GConfig->GetString(TEXT("CreateGameDialog"), TEXT("LastGametypePath"), LastGametypePath, GGameIni) && LastGametypePath.Len() > 0)
	{
		for (UClass* GameType : AllGametypes)
		{
			if (GameType->GetPathName() == LastGametypePath)
			{
				InitialSelectedGameClass = GameType;
				break;
			}
		}
	}

	// Set the initial selected mutators if possible.
	TArray<FString> LastMutators;
	if (GConfig->GetArray(TEXT("CreateGameDialog"), TEXT("LastMutators"), LastMutators, GGameIni))
	{
		for (const FString& MutPath : LastMutators)
		{
			for (UClass* MutClass : MutatorListAvailable)
			{
				if (MutClass->GetPathName() == MutPath)
				{
					MutatorListEnabled.Add(MutClass);
					MutatorListAvailable.Remove(MutClass);
					break;
				}
			}
		}
	}

	TSharedPtr<SVerticalBox> MainBox;
	TSharedPtr<SUniformGridPanel> ButtonRow;

	// NOTE: leaks at the moment because Slate corrupts memory if you delete brushes
	LevelScreenshot = new FSlateDynamicImageBrush(GEngine->DefaultTexture, FVector2D(256.0f, 128.0f), FName(TEXT("LevelScreenshot")));

	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[

			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			[
				BuildGamePanel(InitialSelectedGameClass)
			]
		]
	];

}


TSharedRef<SWidget> SUWCreateGamePanel::BuildGamePanel(TSubclassOf<AUTGameMode> InitialSelectedGameClass)
{
	TSharedPtr<SGridPanel> MutatorGrid;

	SAssignNew(GamePanel,SVerticalBox)
	+ SVerticalBox::Slot()
	.Padding(FMargin(60.0f, 60.0f, 60.0f, 60.0f))
	[
		SNew(SVerticalBox)

		// Game type and map selection
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0, 0, 0, 50))
		[
			// Selection Controls
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(500)
				[
					// Game Type
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(0, 0, 0, 20))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.VAlign( VAlign_Center )
						.AutoWidth()
						[
							SNew(SBox)
							.WidthOverride(200)
							[
								SNew(STextBlock)
								.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
								.Text(NSLOCTEXT("SUWCreateGamePanel", "Gametype", "Game Type:"))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(FMargin(10.0f,0.0f,0.0f,0.0f))
						[
							SNew(SBox)
							.WidthOverride(290)
							[
								SAssignNew(GameList, SComboBox<UClass*>)
								.InitiallySelectedItem(InitialSelectedGameClass)
								.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
								.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
								.OptionsSource(&AllGametypes)
								.OnGenerateWidget(this, &SUWCreateGamePanel::GenerateGameNameWidget)
								.OnSelectionChanged(this, &SUWCreateGamePanel::OnGameSelected)
								.Content()
								[
									SAssignNew(SelectedGameName, STextBlock)
									.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
									.Text(InitialSelectedGameClass.GetDefaultObject()->DisplayName.ToString())
									.MinDesiredWidth(200.0f)
								]
							]
						]
					]

					// Map
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(0, 0, 0, 20))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.FillWidth(.4f)
						[
							SNew(SBox)
							.WidthOverride(200)
							[
								SNew(STextBlock)
								.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
								.Text(NSLOCTEXT("SUWCreateGamePanel", "Map", "Map:"))
							]
						]
						+ SHorizontalBox::Slot()
						.Padding(FMargin(10.0f,0.0f,0.0f,0.0f))
						.AutoWidth()
						[
							SNew(SBox)
							.WidthOverride(290)
							[
								SAssignNew(MapList, SComboBox< TSharedPtr<FMapListItem> >)
								.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
								.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
								.OptionsSource(&AllMaps)
								.OnGenerateWidget(this, &SUWCreateGamePanel::GenerateMapNameWidget)
								.OnSelectionChanged(this, &SUWCreateGamePanel::OnMapSelected)
								.Content()
								[
									SAssignNew(SelectedMap, STextBlock)
									.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
									.ColorAndOpacity(FSlateColor(FLinearColor(0, 0, 0, 1.0f)))
									.Text(NSLOCTEXT("SUWCreateGamePanel", "NoMaps", "No Maps Available").ToString())
									.MinDesiredWidth(200.0f)
								]
							]
						]
					]
					// Author
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin( 0, 0, 0, 5))
					[
						SAssignNew(MapAuthor, STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
						.Text(FText::Format(NSLOCTEXT("SUWCreateGamePanel", "Author", "Author: {0}"), FText::FromString(TEXT("-"))))
					]
					// Recommended players
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin( 0, 0, 0, 5))
					[
						SAssignNew(MapRecommendedPlayers, STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
					]
				]
			]
			// Map screenshot.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(20.0f,0.0f,0.0f,0.0f)
				[
					SNew(SBox)
					.WidthOverride(512)
					.HeightOverride(256)
					[
						SNew(SImage)
						.Image(LevelScreenshot)
					]
				]
			]

			+SHorizontalBox::Slot()
			.FillWidth(1.0)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(20, 0, 20, 0))
				[
					SNew(SBox)
					.HeightOverride(256)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SAssignNew(MapDesc, STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
							.Text(FText())
							.AutoWrapText(true)
						]
					]
				]
			]


		]

		// Game Settings and mutators
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.0f,30.0f,0.0f,0.0f))
		[
			SNew(SHorizontalBox)
			// Game Settings
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(0, 0, 60, 0))
			[
				SNew(SBox)
				.WidthOverride(700)
				[
					SNew(SVerticalBox)
					// Heading
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(0, 0, 0, 20))
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.BoldText")
						.Text(NSLOCTEXT("SUWCreateGamePanel", "GameSettings", "Game Settings:"))
					]
					// Game config panel
					+ SVerticalBox::Slot()
					[
						SAssignNew(GameConfigPanel, SVerticalBox)
					]
				]
			]
			// Mutators
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(30.0f,0.0f,0.0f,0.0f)
			[
				SNew(SBox)
				.WidthOverride(700)
				[

					SNew(SVerticalBox)
					// Heading
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(0, 0, 0, 20))
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.BoldText")
						.Text(NSLOCTEXT("SUWCreateGamePanel", "Mutators", "Mutators:"))
					]

					// Mutator enabler
					+ SVerticalBox::Slot()
					[
						SAssignNew(MutatorGrid, SGridPanel)
						// All mutators
						+ SGridPanel::Slot(0, 0)
						[
							SNew(SBorder)
							[
								SNew(SBorder)
								.BorderImage(SUWindowsStyle::Get().GetBrush("UT.Background.Dark"))
								.Padding(FMargin(5))
								[
									SAssignNew( AvailableMutators, SListView<UClass*> )
									.SelectionMode( ESelectionMode::Single )
									.ListItemsSource( &MutatorListAvailable )
									.OnGenerateRow( this, &SUWCreateGamePanel::GenerateMutatorListRow )
								]
							]
						]
						// Mutator switch buttons
						+ SGridPanel::Slot(1, 0)
						[
							SNew(SBox)
							.VAlign(VAlign_Center)
							.Padding(FMargin(10, 0, 10, 0))
							[
								SNew(SVerticalBox)
								// Add button
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(FMargin(0, 0, 0, 10))
								[
									SNew(SButton)
									.HAlign(HAlign_Left)
									.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
									.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
									.OnClicked(this, &SUWCreateGamePanel::AddMutator)
									[
										SNew(STextBlock)
										.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText.Black")
										.Text(NSLOCTEXT("SUWCreateGamePanel", "MutatorAdd", "-->").ToString())
									]
								]
								// Remove Button
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(SButton)
									.HAlign(HAlign_Left)
									.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
									.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
									.OnClicked(this, &SUWCreateGamePanel::RemoveMutator)
									[
										SNew(STextBlock)
										.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText.Black")
										.Text(NSLOCTEXT("SUWCreateGamePanel", "MutatorRemove", "<--").ToString())
									]
								]
							]
						]
						// Enabled mutators
						+ SGridPanel::Slot(2, 0)
						[
							SNew(SBorder)
							[
								SNew(SBorder)
								.BorderImage(SUWindowsStyle::Get().GetBrush("UT.Background.Dark"))
								.Padding(FMargin(5))
								[
									SAssignNew(EnabledMutators, SListView<UClass*>)
									.SelectionMode(ESelectionMode::Single)
									.ListItemsSource(&MutatorListEnabled)
									.OnGenerateRow(this, &SUWCreateGamePanel::GenerateMutatorListRow)
								]
							]
						]
						// Configure mutator button
						+ SGridPanel::Slot(2, 1)
						.Padding(FMargin(0, 5.0f, 0, 5.0f))
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
							.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
							.OnClicked(this, &SUWCreateGamePanel::ConfigureMutator)
							[
								SNew(STextBlock)
								.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
								.Text(NSLOCTEXT("SUWCreateGamePanel", "ConfigureMutator", "Configure Mutator").ToString())
							]
						]
					]
				]
			]
		]
	];

	MutatorGrid->SetRowFill(0, 1);
	MutatorGrid->SetColumnFill(0, .5);
	MutatorGrid->SetColumnFill(2, .5);

	OnGameSelected(InitialSelectedGameClass, ESelectInfo::Direct);

	return GamePanel.ToSharedRef();
}




void SUWCreateGamePanel::OnMapSelected(TSharedPtr<FMapListItem> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedMap->SetText(NewSelection.IsValid() ? FText::FromString(NewSelection.Get()->GetDisplayName()) : NSLOCTEXT("SUWCreateGamePanel", "NoMaps", "No Maps Available"));

	UUTLevelSummary* Summary = NewSelection.IsValid() ? UUTGameEngine::LoadLevelSummary(NewSelection.Get()->PackageName) : NULL;
	if (Summary != NULL)
	{
		int32 OptimalPlayerCount = SelectedGameClass.GetDefaultObject()->bTeamGame ? Summary->OptimalTeamPlayerCount : Summary->OptimalPlayerCount;
	
		MapAuthor->SetText(FText::Format(NSLOCTEXT("SUWCreateGamePanel", "Author", "Author: {0}"), FText::FromString(Summary->Author)));
		MapRecommendedPlayers->SetText(FText::Format(NSLOCTEXT("SUWCreateGamePanel", "OptimalPlayers", "Recommended Players: {0}"), FText::AsNumber(OptimalPlayerCount)));
		MapDesc->SetText(Summary->Description);
		*LevelScreenshot = FSlateDynamicImageBrush(Summary->Screenshot != NULL ? Summary->Screenshot : Cast<UUTGameEngine>(GEngine)->DefaultLevelScreenshot, LevelScreenshot->ImageSize, LevelScreenshot->GetResourceName());
	}
	else
	{
		MapAuthor->SetText(FText::Format(NSLOCTEXT("SUWCreateGamePanel", "Author", "Author: {0}"), FText::FromString(TEXT("Unknown"))));
		MapRecommendedPlayers->SetText(FText::Format(NSLOCTEXT("SUWCreateGamePanel", "OptimalPlayers", "Recommended Players: {0}"), FText::AsNumber(10)));
		MapDesc->SetText(FText());
		*LevelScreenshot = FSlateDynamicImageBrush(Cast<UUTGameEngine>(GEngine)->DefaultLevelScreenshot, LevelScreenshot->ImageSize, LevelScreenshot->GetResourceName());
	}
}

TSharedRef<SWidget> SUWCreateGamePanel::GenerateGameNameWidget(UClass* InItem)
{
	return SNew(SBox)
		.Padding(5)
		[
			SNew(STextBlock)
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
			.Text(InItem->GetDefaultObject<AUTGameMode>()->DisplayName.ToString())
		];
}

TSharedRef<SWidget> SUWCreateGamePanel::GenerateMapNameWidget(TSharedPtr<FMapListItem> InItem)
{
	return SNew(SBox)
		.Padding(5)
		[
			SNew(STextBlock)
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
			.Text(InItem.Get()->GetDisplayName())
		];
}

void SUWCreateGamePanel::OnGameSelected(UClass* NewSelection, ESelectInfo::Type SelectInfo)
{
	if (NewSelection != SelectedGameClass)
	{
		// clear existing game type info and cancel any changes that were made
		if (SelectedGameClass != NULL)
		{
			SelectedGameClass.GetDefaultObject()->ReloadConfig();
			GameConfigPanel->ClearChildren();
			GameConfigProps.Empty();
		}

		SelectedGameClass = NewSelection;
		SelectedGameClass.GetDefaultObject()->CreateConfigWidgets(GameConfigPanel, false, GameConfigProps);

		if (GetPlayerOwner()->GetWorld()->GetNetMode() != NM_Client)
		{
			TSharedPtr< TAttributePropertyBool > DemoRecAttr = MakeShareable(new TAttributePropertyBool(SelectedGameClass.GetDefaultObject(), &SelectedGameClass.GetDefaultObject()->bRecordDemo));
			GameConfigProps.Add(DemoRecAttr);
			GameConfigPanel->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(350)
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.Text(NSLOCTEXT("UTGameMode", "DemoRec", "Record Demo").ToString())
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(20.0f,0.0f,0.0f,0.0f)
				[
					SNew(SBox)
					.WidthOverride(330)
					[
						SNew(SCheckBox)
						.IsChecked(DemoRecAttr.ToSharedRef(), &TAttributePropertyBool::GetAsCheckBox)
						.OnCheckStateChanged(DemoRecAttr.ToSharedRef(), &TAttributePropertyBool::SetFromCheckBox)
						.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
						.Type(ESlateCheckBoxType::CheckBox)
					]
				]
			];

			// Configure bots button
			GameConfigPanel->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(350)
					[
						SNullWidget::NullWidget
					]
				]
				+ SHorizontalBox::Slot()
				.Padding(20.0f,0.0f,0.0f,0.0f)
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(320)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
						.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
						.OnClicked(this, &SUWCreateGamePanel::ConfigureBots)
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText.Black")
							.ColorAndOpacity(FSlateColor(FLinearColor(0, 0, 0, 1.0f)))
							.Text(NSLOCTEXT("SUWCreateGamePanel", "ConfigureBots", "Configure Bots").ToString())
						]
					]
				]
			];
		}


		SelectedGameName->SetText(SelectedGameClass.GetDefaultObject()->DisplayName.ToString());

		// generate map list
		const AUTGameMode* GameDefaults = SelectedGameClass.GetDefaultObject();
		AllMaps.Empty();

		AUTGameState* GameState = GetPlayerOwner()->GetWorld()->GetGameState<AUTGameState>();
		if (GameState)
		{
			GameState->GetAvailableMaps(GameDefaults, AllMaps);
		}

		AllMaps.Sort([](const TSharedPtr<FMapListItem>& A, const TSharedPtr<FMapListItem>& B)
					{
						bool bHasTitleA = !A.Get()->Title.IsEmpty();
						bool bHasTitleB = !B.Get()->Title.IsEmpty();
						if (bHasTitleA && !bHasTitleB)
						{
							return true;
						}
						else if (!bHasTitleA && bHasTitleB)
						{
							return false;
						}
						else
						{
							return A.Get()->GetDisplayName() < B.Get()->GetDisplayName();
						}
					});

		MapList->RefreshOptions();
		if (AllMaps.Num() > 0)
		{
			MapList->SetSelectedItem(AllMaps[0]);
			// remember last selection
			for (TSharedPtr<FMapListItem> TestMap : AllMaps)
			{
				if (TestMap.Get()->PackageName == SelectedGameClass.GetDefaultObject()->UILastStartingMap)
				{
					MapList->SetSelectedItem(TestMap);
					break;
				}
			}
		}
		else
		{
			MapList->SetSelectedItem(NULL);
		}
		OnMapSelected(MapList->GetSelectedItem(), ESelectInfo::Direct);
	}
}
void SUWCreateGamePanel::Cancel()
{
	// revert config changes
	SelectedGameClass.GetDefaultObject()->ReloadConfig();
	AUTGameState::StaticClass()->GetDefaultObject()->ReloadConfig();
}

TSharedRef<ITableRow> SUWCreateGamePanel::GenerateMutatorListRow(UClass* MutatorType, const TSharedRef<STableViewBase>& OwningList)
{
	checkSlow(MutatorType->IsChildOf(AUTMutator::StaticClass()));

	FString MutatorName = MutatorType->GetDefaultObject<AUTMutator>()->DisplayName.IsEmpty() ? MutatorType->GetName() : MutatorType->GetDefaultObject<AUTMutator>()->DisplayName.ToString();
	return SNew(STableRow<UClass*>, OwningList)
		.Padding(5)
		[
			SNew(STextBlock)
			.TextStyle(SUWindowsStyle::Get(),"UT.ContextMenu.TextStyle")
			.Text(MutatorName)
		]; 
}

FReply SUWCreateGamePanel::AddMutator()
{
	TArray<UClass*> Selection = AvailableMutators->GetSelectedItems();
	if (Selection.Num() > 0 && Selection[0] != NULL)
	{
		MutatorListEnabled.Add(Selection[0]);
		MutatorListAvailable.Remove(Selection[0]);
		AvailableMutators->RequestListRefresh();
		EnabledMutators->RequestListRefresh();
	}
	return FReply::Handled();
}
FReply SUWCreateGamePanel::RemoveMutator()
{
	TArray<UClass*> Selection = EnabledMutators->GetSelectedItems();
	if (Selection.Num() > 0 && Selection[0] != NULL)
	{
		MutatorListAvailable.Add(Selection[0]);
		MutatorListEnabled.Remove(Selection[0]);
		AvailableMutators->RequestListRefresh();
		EnabledMutators->RequestListRefresh();
	}
	return FReply::Handled();
}
FReply SUWCreateGamePanel::ConfigureMutator()
{
	if (!MutatorConfigMenu.IsValid() || MutatorConfigMenu->GetParent() == NULL)
	{
		TArray<UClass*> Selection = EnabledMutators->GetSelectedItems();
		if (Selection.Num() > 0 && Selection[0] != NULL)
		{
			checkSlow(Selection[0]->IsChildOf(AUTMutator::StaticClass()));
			AUTMutator* Mut = Selection[0]->GetDefaultObject<AUTMutator>();
			if (Mut->ConfigMenu != NULL)
			{
				MutatorConfigMenu = CreateWidget<UUserWidget>(GetPlayerOwner()->GetWorld(), Mut->ConfigMenu);
				if (MutatorConfigMenu != NULL)
				{
					MutatorConfigMenu->AddToViewport(400);
				}
			}
		}
	}
	return FReply::Handled();
}

FReply SUWCreateGamePanel::ConfigureBots()
{
	GetPlayerOwner()->OpenDialog(SNew(SUWBotConfigDialog).PlayerOwner(GetPlayerOwner()).GameClass(SelectedGameClass).NumBots(SelectedGameClass->GetDefaultObject<AUTGameMode>()->BotFillCount - 1));
	return FReply::Handled();
}

void SUWCreateGamePanel::GetCustomGameSettings(FString& GameMode, FString& StartingMap, TArray<FString>&GameOptions, int32 BotSkillLevel)
{
	StartingMap = MapList->GetSelectedItem().IsValid() ? MapList->GetSelectedItem().Get()->PackageName : TEXT("");
	AUTGameMode* DefaultGameMode = SelectedGameClass->GetDefaultObject<AUTGameMode>();
	if (DefaultGameMode)
	{
		// The TAttributeProperty's that link the options page to the game mode will have already set their values
		// so just save config on the default object and return the game.

		DefaultGameMode->UILastStartingMap = StartingMap;

		GameMode = SelectedGameClass->GetPathName();

		TArray<FString> LastMutators;
		if (MutatorListEnabled.Num() > 0)
		{
			FString MutatorOption = TEXT("");
			LastMutators.Add(MutatorListEnabled[0]->GetPathName());

			MutatorOption += FString::Printf(TEXT("?mutator=%s"), *MutatorListEnabled[0]->GetPathName());
			for (int32 i = 1; i < MutatorListEnabled.Num(); i++)
			{
				MutatorOption += TEXT(",") + MutatorListEnabled[i]->GetPathName();
				LastMutators.Add(MutatorListEnabled[i]->GetPathName());
			}

			GameOptions.Add(MutatorOption);
		}

		GConfig->SetArray(TEXT("CreateGameDialog"), TEXT("LastMutators"), LastMutators, GGameIni);
		DefaultGameMode->GetGameURLOptions(GameOptions);

		// If we don't want bots, clear BotFillCount

		if (BotSkillLevel >=0)
		{
			// Load the level summary of this map.
			UUTLevelSummary* Summary = UUTGameEngine::LoadLevelSummary(StartingMap);
			DefaultGameMode->BotFillCount = Summary->OptimalPlayerCount;
			GameOptions.Add(FString::Printf(TEXT("Difficulty=%i"), BotSkillLevel));
		}
		else
		{
			DefaultGameMode->BotFillCount = 0;
		}

		DefaultGameMode->SaveConfig();
	}
}

#endif


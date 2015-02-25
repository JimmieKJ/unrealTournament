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

#if !UE_SERVER

#include "UserWidget.h"

void SUWCreateGamePanel::ConstructPanel(FVector2D ViewportSize)
{
	for (TObjectIterator<UClass> It; It; ++It)
	{
		// non-native classes are detected by asset search even if they're loaded for consistency
		if (!It->HasAnyClassFlags(CLASS_Abstract | CLASS_HideDropDown) && It->HasAnyClassFlags(CLASS_Native))
		{
			if (It->IsChildOf(AUTGameMode::StaticClass()))
			{
				AllGametypes.Add(*It);
			}
			else if (It->IsChildOf(AUTMutator::StaticClass()) && !It->GetDefaultObject<AUTMutator>()->DisplayName.IsEmpty())
			{
				MutatorListAvailable.Add(*It);
			}
		}
	}

	{
		TArray<FAssetData> AssetList;
		GetAllBlueprintAssetData(AUTGameMode::StaticClass(), AssetList);
		for (const FAssetData& Asset : AssetList)
		{
			static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
			const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
			if (ClassPath != NULL)
			{
				UClass* TestClass = LoadObject<UClass>(NULL, **ClassPath);
				if (TestClass != NULL && !TestClass->HasAnyClassFlags(CLASS_Abstract) && TestClass->IsChildOf(AUTGameMode::StaticClass()))
				{
					AllGametypes.AddUnique(TestClass);
				}
			}
		}
	}

	{
		TArray<FAssetData> AssetList;
		GetAllBlueprintAssetData(AUTMutator::StaticClass(), AssetList);
		for (const FAssetData& Asset : AssetList)
		{
			static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
			const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
			if (ClassPath != NULL)
			{
				UClass* TestClass = LoadObject<UClass>(NULL, **ClassPath);
				if (TestClass != NULL && !TestClass->HasAnyClassFlags(CLASS_Abstract) && TestClass->IsChildOf(AUTMutator::StaticClass()) && !TestClass->GetDefaultObject<AUTMutator>()->DisplayName.IsEmpty())
				{
					MutatorListAvailable.AddUnique(TestClass);
				}
			}
		}
	}

	TSubclassOf<AUTGameMode> InitialSelectedGameClass = AUTDMGameMode::StaticClass();
	// restore previous selection if possible
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

	LevelScreenshot = new FSlateDynamicImageBrush(GEngine->DefaultTexture, FVector2D(256.0f, 128.0f), FName(TEXT("LevelScreenshot")));

	ChildSlot
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
						.Image(SUWindowsStyle::Get().GetBrush("UT.Backgrounds.BK01"))
					]
				]
			]
		]
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
						.Image(SUWindowsStyle::Get().GetBrush("UT.Backgrounds.Overlay"))
					]
				]
			]
		]

		+SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(0.0f, 0.0f, 0.0f, 5.0f)
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[

				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(56)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBox)
							.WidthOverride(25)
							[
								SNew(SImage)
								.Image(SUWindowsStyle::Get().GetBrush("UT.TopMenu.LightFill"))
							]
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SAssignNew(GameSettingsTabButton, SUTTabButton)
							.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.TabButton")
							.ClickMethod(EButtonClickMethod::MouseDown)
							.OnClicked(this, &SUWCreateGamePanel::GameSettingsClick)
							.ContentPadding(FMargin(25.0,0.0,60.0,5.0))
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								[
									SAssignNew(GameSettingsLabel, STextBlock)
									.Text(NSLOCTEXT("SUWCreateGamePanel","TabBar","Game Settings").ToString())
									.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
								]
							]
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SAssignNew(ServerSettingsTabButton,SUTTabButton)
							.ClickMethod(EButtonClickMethod::MouseDown)
							.OnClicked(this, &SUWCreateGamePanel::ServerSettingsClick)
							.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.TabButton")
							.ContentPadding(FMargin(25.0,0.0,60.0,5.0))
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								[
									SAssignNew(ServerSettingsLabel,STextBlock)
									.Text(NSLOCTEXT("SUWCreateGamePanel","TabBar","Server Settings").ToString())
									.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
								]
							]
						]
						+SHorizontalBox::Slot()
						.FillWidth(1.0)
						[
							SNew(SImage)
							.Image(SUWindowsStyle::Get().GetBrush("UT.TopMenu.LightFill"))
						]

					]
				]
				+SVerticalBox::Slot()
				.VAlign(VAlign_Fill)
				[
					SAssignNew(TabSwitcher, SWidgetSwitcher)
					+ SWidgetSwitcher::Slot()
					[
						BuildGamePanel(InitialSelectedGameClass)
					]
					+ SWidgetSwitcher::Slot()
					[
						BuildServerPanel()
					]
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Bottom)
			.HAlign(HAlign_Fill)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(8)
					[
						SNew(SImage)
						.Image(SUWindowsStyle::Get().GetBrush("UT.TopMenu.Bar"))
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					// Buttom Menu Bar
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.HAlign(HAlign_Fill)
						[
							SNew(SImage)
							.Image(SUWindowsStyle::Get().GetBrush("UT.TopMenu.TileBar"))
						]
					]
					+SOverlay::Slot()
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SBox)
							.HeightOverride(56)
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.HAlign(HAlign_Right)
								[
									BuildMenu()
								]
							]
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SBox)
							.HeightOverride(8)
							[
								SNew(SCanvas)
							]
						]

					]
				]
			]
		]
	];

	GameSettingsTabButton->BePressed();

}

TSharedRef<SWidget> SUWCreateGamePanel::BuildMenu()
{
	return SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding(0.0f,0.0f,5.0f,0.0f)
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
			.ContentPadding(FMargin(25.0,0.0,25.0,5.0))
			.OnClicked(this, &SUWCreateGamePanel::OfflineClick)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUWCreateGamePanel","MenuBar_OFFLINE","PLAY OFFLINE").ToString())
					.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
				]
			]
		]

		+SHorizontalBox::Slot()
		.Padding(0.0f,0.0f,5.0f,0.0f)
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
			.ContentPadding(FMargin(25.0,0.0,25.0,5.0))
			.OnClicked(this, &SUWCreateGamePanel::HostClick)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUWCreateGamePanel","MenuBar_ONLINE","START SERVER").ToString())
					.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
				]
			]
		];
}

TSharedRef<SWidget> SUWCreateGamePanel::BuildGamePanel(TSubclassOf<AUTGameMode> InitialSelectedGameClass)
{
	SAssignNew(GamePanel,SScrollBox)
	+ SScrollBox::Slot()
	.Padding(FMargin(0.0f, 5.0f, 0.0f, 5.0f))
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
		.AutoHeight()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.NormalText")
			.Text(NSLOCTEXT("SUWCreateGamePanel", "Gametype", "Gametype:"))
		]
		+ SVerticalBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 0.0f)
		.AutoHeight()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Center)
		[
			SAssignNew(GameList, SComboBox<UClass*>)
			.InitiallySelectedItem(InitialSelectedGameClass)
			.ComboBoxStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
			.OptionsSource(&AllGametypes)
			.OnGenerateWidget(this, &SUWCreateGamePanel::GenerateGameNameWidget)
			.OnSelectionChanged(this, &SUWCreateGamePanel::OnGameSelected)
			.Content()
			[
				SAssignNew(SelectedGameName, STextBlock)
				.Text(InitialSelectedGameClass.GetDefaultObject()->DisplayName.ToString())
				.MinDesiredWidth(200.0f)
			]
		]
		+ SVerticalBox::Slot()
			.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
			.AutoHeight()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.NormalText")
			.Text(NSLOCTEXT("SUWCreateGamePanel", "Map", "Map:"))
		]
		+ SVerticalBox::Slot()
			.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
			.AutoHeight()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Center)
		[
			SAssignNew(MapList, SComboBox< TSharedPtr<FString> >)
			.ComboBoxStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
			.OptionsSource(&AllMaps)
			.OnGenerateWidget(this, &SUWPanel::GenerateStringListWidget)
			.OnSelectionChanged(this, &SUWCreateGamePanel::OnMapSelected)
			.Content()
			[
				SAssignNew(SelectedMap, STextBlock)
				.Text(NSLOCTEXT("SUWCreateGamePanel", "NoMaps", "No Maps Available").ToString())
				.MinDesiredWidth(200.0f)
			]
		]
		+ SVerticalBox::Slot()
		.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
		.AutoHeight()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(5.0f, 5.0f, 5.0f, 5.0f)
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Fill)
			.FillWidth(0.5f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
				.AutoHeight()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Center)
				[
					SAssignNew(MapAuthor, STextBlock)
					.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.NormalText")
					.Text(FText::Format(NSLOCTEXT("SUWCreateGamePanel", "Author", "Author: {0}"), FText::FromString(TEXT("-"))))
				]
				+ SVerticalBox::Slot()
				.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
				.AutoHeight()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Center)
				[
					SAssignNew(MapRecommendedPlayers, STextBlock)
					.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.NormalText")
					.Text(FText::Format(NSLOCTEXT("SUWCreateGamePanel", "OptimalPlayers", "Recommended Players: {0} - {1}"), FText::AsNumber(8), FText::AsNumber(12)))
				]
				+ SVerticalBox::Slot()
				.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
				.AutoHeight()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Center)
				[
					SAssignNew(MapDesc, STextBlock)
					.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.NormalText")
					.Text(FText())
					.WrapTextAt(190.0f)
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(5.0f, 5.0f, 5.0f, 5.0f)
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Fill)
			.FillWidth(0.5f)
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					.Image(LevelScreenshot)
				]
			]
		]


		+ SVerticalBox::Slot()
		.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
		.HAlign(HAlign_Left)
		.AutoHeight()
		[
			SNew(STextBlock)
			.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.NormalText")
			.Text(NSLOCTEXT("SUWCreateGamePanel", "GameSettings", "Game Settings:"))
		]

		+ SVerticalBox::Slot()
		.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
		.AutoHeight()
		[
			SAssignNew(GameConfigPanel, SVerticalBox)
		]

		+ SVerticalBox::Slot()
		.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
		.AutoHeight()
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
			.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
			.Text(NSLOCTEXT("SUWCreateGamePanel", "ConfigureBots", "Configure Bots").ToString())
			.OnClicked(this, &SUWCreateGamePanel::ConfigureBots)
		]

		+ SVerticalBox::Slot()
		.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
		.HAlign(HAlign_Left)
		.AutoHeight()
		[
			SNew(STextBlock)
			.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.NormalText")
			.Text(NSLOCTEXT("SUWCreateGamePanel", "Mutators", "Mutators:"))
		]

		+ SVerticalBox::Slot()
		.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
		.HAlign(HAlign_Left)
		.AutoHeight()
		[
			SNew(SGridPanel)
			+ SGridPanel::Slot(0, 0)
			.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
			[
				SNew(SBox)
				.WidthOverride(200.0f)
				.HeightOverride(200.0f)
				[
					SNew(SBorder)
					.Content()
					[
						SAssignNew(AvailableMutators, SListView<UClass*>)
						.SelectionMode(ESelectionMode::Single)
						.ListItemsSource(&MutatorListAvailable)
						.OnGenerateRow(this, &SUWCreateGamePanel::GenerateMutatorListRow)
					]
				]
			]
			+ SGridPanel::Slot(1, 0)
				.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
				[
				SNew(SBox)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
					.HAlign(HAlign_Left)
					.AutoHeight()
					[
						SNew(SButton)
						.HAlign(HAlign_Left)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
						.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
						.Text(NSLOCTEXT("SUWCreateGamePanel", "MutatorAdd", "-->").ToString())
						.OnClicked(this, &SUWCreateGamePanel::AddMutator)
					]
					+ SVerticalBox::Slot()
						.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
						.HAlign(HAlign_Left)
					.AutoHeight()
					[
						SNew(SButton)
						.HAlign(HAlign_Left)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
						.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
						.Text(NSLOCTEXT("SUWCreateGamePanel", "MutatorRemove", "<--").ToString())
						.OnClicked(this, &SUWCreateGamePanel::RemoveMutator)
					]
				]
			]
			+ SGridPanel::Slot(2, 0)
				.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
				[
				SNew(SBox)
				.WidthOverride(200.0f)
				.HeightOverride(200.0f)
				[
					SNew(SBorder)
					.Content()
					[
						SAssignNew(EnabledMutators, SListView<UClass*>)
						.SelectionMode(ESelectionMode::Single)
						.ListItemsSource(&MutatorListEnabled)
						.OnGenerateRow(this, &SUWCreateGamePanel::GenerateMutatorListRow)
					]
				]
			]
			+ SGridPanel::Slot(2, 1)
				.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
				[
				SNew(SButton)
				.HAlign(HAlign_Left)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
				.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
				.Text(NSLOCTEXT("SUWCreateGamePanel", "ConfigureMutator", "Configure Mutator").ToString())
				.OnClicked(this, &SUWCreateGamePanel::ConfigureMutator)
			]
		]
	
	];

	OnGameSelected(InitialSelectedGameClass, ESelectInfo::Direct);

	return GamePanel.ToSharedRef();
}


TSharedRef<SWidget> SUWCreateGamePanel::BuildServerPanel()
{

	TSharedPtr<TAttributePropertyString> ServerNameProp = MakeShareable(new TAttributePropertyString(AUTGameState::StaticClass()->GetDefaultObject(), &AUTGameState::StaticClass()->GetDefaultObject<AUTGameState>()->ServerName));
	PropertyLinks.Add(ServerNameProp);
	TSharedPtr<TAttributePropertyString> ServerMOTDProp = MakeShareable(new TAttributePropertyString(AUTGameState::StaticClass()->GetDefaultObject(), &AUTGameState::StaticClass()->GetDefaultObject<AUTGameState>()->ServerMOTD));
	PropertyLinks.Add(ServerMOTDProp);

	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
		[
			SAssignNew(DedicatedServerCheckBox, SCheckBox)
			.ForegroundColor(FLinearColor::White)
			.IsChecked(ESlateCheckBoxState::Checked)
				
			.Content()
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
				.Text(NSLOCTEXT("SUWCreateGamePanel", "DedicatedServer", "Dedicated Server"))
			]
		]

		+SVerticalBox::Slot()
		.Padding(0.0f, 10.0f, 0.0f, 5.0f)
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.NormalText")
				.Text(NSLOCTEXT("SUWCreateGamePanel", "ServerName", "Server Name:"))
			]
			+ SHorizontalBox::Slot()
			.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
			.AutoWidth()
			[
				SNew(SEditableTextBox)
				.OnTextChanged(ServerNameProp.ToSharedRef(), &TAttributePropertyString::SetFromText)
				.MinDesiredWidth(300.0f)
				.Text(ServerNameProp.ToSharedRef(), &TAttributePropertyString::GetAsText)
			]
		]
		+SVerticalBox::Slot()
		.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
		.HAlign(HAlign_Left)
		.AutoHeight()
		[
			SNew(STextBlock)
			.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.NormalText")
			.Text(NSLOCTEXT("SUWCreateGamePanel", "ServerMOTD", "Server MOTD:"))
		]
		+SVerticalBox::Slot()
		.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
		.AutoHeight()
		[
			SNew(SEditableTextBox)
			.OnTextChanged(ServerMOTDProp.ToSharedRef(), &TAttributePropertyString::SetFromText)
			.MinDesiredWidth(400.0f)
			.Text(ServerMOTDProp.ToSharedRef(), &TAttributePropertyString::GetAsText)
		];
}
/*
	int32 ButtonID = 0;
	if (InArgs._IsOnline)
	{
		ButtonRow->AddSlot(ButtonID++, 0)
		[
			SNew(SButton)
			.HAlign(HAlign_Left)
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
			.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
			.Text(NSLOCTEXT("SUWCreateGamePanel", "StartDedicatedButton", "Start Dedicated").ToString())
			.OnClicked(this, &SUWCreateGamePanel::StartClick, SERVER_Dedicated)
		];
	}
	else
	{
		ButtonRow->AddSlot(ButtonID++, 0)
		[
			SNew(SButton)
			.HAlign(HAlign_Left)
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
			.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
			.Text(NSLOCTEXT("SUWCreateGamePanel", "StartButton", "Start").ToString())
			.OnClicked(this, &SUWCreateGamePanel::StartClick, SERVER_Standalone)
		];
	}
	ButtonRow->AddSlot(ButtonID++, 0)
	[
		SNew(SButton)
		.HAlign(HAlign_Left)
		.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
		.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
		.Text(NSLOCTEXT("SUWMessageBox", "CancelButton", "Cancel").ToString())
		.OnClicked(this, &SUWCreateGamePanel::CancelClick)
	];
}


	return SNew(SCanvas);
*/


void SUWCreateGamePanel::OnMapSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedMap->SetText(NewSelection.IsValid() ? *NewSelection.Get() : NSLOCTEXT("SUWCreateGamePanel", "NoMaps", "No Maps Available").ToString());

	UUTLevelSummary* Summary = NULL;
	FString MapFullName;
	if (NewSelection.IsValid() && FPackageName::SearchForPackageOnDisk(*NewSelection + FPackageName::GetMapPackageExtension(), &MapFullName))
	{
		static FName NAME_LevelSummary(TEXT("LevelSummary"));

		UPackage* Pkg = CreatePackage(NULL, *MapFullName);
		Summary = FindObject<UUTLevelSummary>(Pkg, *NAME_LevelSummary.ToString());
		if (Summary == NULL)
		{
			// LoadObject() actually forces the whole package to be loaded for some reason so we need to take the long way around
			BeginLoad();
			ULinkerLoad* Linker = GetPackageLinker(Pkg, NULL, LOAD_NoWarn | LOAD_Quiet, NULL, NULL);
			if (Linker != NULL)
			{
				//UUTLevelSummary* Summary = Cast<UUTLevelSummary>(Linker->Create(UUTLevelSummary::StaticClass(), FName(TEXT("LevelSummary")), Pkg, LOAD_NoWarn | LOAD_Quiet, false));
				// ULinkerLoad::Create() not exported... even more hard way :(
				FPackageIndex SummaryClassIndex, SummaryPackageIndex;
				if (Linker->FindImportClassAndPackage(FName(TEXT("UTLevelSummary")), SummaryClassIndex, SummaryPackageIndex) && SummaryPackageIndex.IsImport() && Linker->ImportMap[SummaryPackageIndex.ToImport()].ObjectName == AUTGameMode::StaticClass()->GetOutermost()->GetFName())
				{
					for (int32 Index = 0; Index < Linker->ExportMap.Num(); Index++)
					{
						FObjectExport& Export = Linker->ExportMap[Index];
						if (Export.ObjectName == NAME_LevelSummary && Export.ClassIndex == SummaryClassIndex)
						{
							Export.Object = StaticConstructObject(UUTLevelSummary::StaticClass(), Linker->LinkerRoot, Export.ObjectName, EObjectFlags(Export.ObjectFlags | RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_WasLoaded));
							Export.Object->SetLinker(Linker, Index);
							//GObjLoaded.Add(Export.Object);
							Linker->Preload(Export.Object);
							Export.Object->ConditionalPostLoad();
							Summary = Cast<UUTLevelSummary>(Export.Object);
							break;
						}
					}
				}
			}
			EndLoad();
		}
	}
	if (Summary != NULL)
	{
		MapAuthor->SetText(FText::Format(NSLOCTEXT("SUWCreateGamePanel", "Author", "Author: {0}"), FText::FromString(Summary->Author)));
		MapRecommendedPlayers->SetText(FText::Format(NSLOCTEXT("SUWCreateGamePanel", "OptimalPlayers", "Recommended Players: {0} - {1}"), FText::AsNumber(Summary->OptimalPlayerCount.X), FText::AsNumber(Summary->OptimalPlayerCount.Y)));
		MapDesc->SetText(Summary->Description);
		*LevelScreenshot = FSlateDynamicImageBrush(Summary->Screenshot != NULL ? Summary->Screenshot : GEngine->DefaultTexture, LevelScreenshot->ImageSize, LevelScreenshot->GetResourceName());
	}
	else
	{
		MapAuthor->SetText(FText::Format(NSLOCTEXT("SUWCreateGamePanel", "Author", "Author: {0}"), FText::FromString(TEXT("Unknown"))));
		MapRecommendedPlayers->SetText(FText::Format(NSLOCTEXT("SUWCreateGamePanel", "OptimalPlayers", "Recommended Players: {0} - {1}"), FText::AsNumber(8), FText::AsNumber(12)));
		MapDesc->SetText(FText());
		*LevelScreenshot = FSlateDynamicImageBrush(GEngine->DefaultTexture, LevelScreenshot->ImageSize, LevelScreenshot->GetResourceName());
	}
}

TSharedRef<SWidget> SUWCreateGamePanel::GenerateGameNameWidget(UClass* InItem)
{
	return SNew(SBox)
		.Padding(5)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FLinearColor::Black)
			.Text(InItem->GetDefaultObject<AUTGameMode>()->DisplayName.ToString())
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

		TSharedPtr< TAttributePropertyBool > DemoRecAttr = MakeShareable(new TAttributePropertyBool(SelectedGameClass.GetDefaultObject(), &SelectedGameClass.GetDefaultObject()->bRecordDemo));
		GameConfigProps.Add(DemoRecAttr);
		GameConfigPanel->AddSlot()
		.Padding(0.0f, 5.0f, 0.0f, 5.0f)
		.AutoHeight()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Left)
		[
			SNew(SCheckBox)
			.IsChecked(DemoRecAttr.ToSharedRef(), &TAttributePropertyBool::GetAsCheckBox)
			.OnCheckStateChanged(DemoRecAttr.ToSharedRef(), &TAttributePropertyBool::SetFromCheckBox)
			.Type(ESlateCheckBoxType::CheckBox)
			.Content()
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.NormalText")
				.Text(NSLOCTEXT("UTGameMode", "DemoRec", "Record Demo").ToString())
			]
		];

		SelectedGameName->SetText(SelectedGameClass.GetDefaultObject()->DisplayName.ToString());

		// generate map list
		const AUTGameMode* GameDefaults = SelectedGameClass.GetDefaultObject();
		AllMaps.Empty();
		TArray<FString> Paths;
		FPackageName::QueryRootContentPaths(Paths);
		int32 MapExtLen = FPackageName::GetMapPackageExtension().Len();
		for (int32 i = 0; i < Paths.Num(); i++)
		{
			// ignore /Engine/ as those aren't real gameplay maps
			if (!Paths[i].StartsWith(TEXT("/Engine/")))
			{
				TArray<FString> List;
				FPackageName::FindPackagesInDirectory(List, *FPackageName::LongPackageNameToFilename(Paths[i]));
				for (int32 j = 0; j < List.Num(); j++)
				{
					if (List[j].Right(MapExtLen) == FPackageName::GetMapPackageExtension())
					{
						TSharedPtr<FString> BaseName = MakeShareable(new FString(FPaths::GetBaseFilename(List[j])));
						if (GameDefaults->SupportsMap(*BaseName.Get()))
						{
							AllMaps.Add(BaseName);
						}
					}
				}
			}
		}

		MapList->RefreshOptions();
		if (AllMaps.Num() > 0)
		{
			MapList->SetSelectedItem(AllMaps[0]);
			// remember last selection
			for (TSharedPtr<FString> TestMap : AllMaps)
			{
				if (*TestMap.Get() == SelectedGameClass.GetDefaultObject()->UILastStartingMap)
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

FReply SUWCreateGamePanel::OfflineClick()
{
	StartGame(EServerStartMode::SERVER_Standalone);
	return FReply::Handled();
}
FReply SUWCreateGamePanel::HostClick()
{
	UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
	if (!UTEngine->IsCloudAndLocalContentInSync())
	{
		PlayerOwner->ShowMessage(NSLOCTEXT("SUWCreateGamePanel", "CloudSyncErrorCaption", "Cloud Not Synced"), NSLOCTEXT("SUWCreateGamePanel", "CloudSyncErrorMsg", "Some files are not up to date on your cloud storage. Would you like to start anyway?"), UTDIALOG_BUTTON_YES + UTDIALOG_BUTTON_NO, FDialogResultDelegate::CreateSP(this, &SUWCreateGamePanel::CloudOutOfSyncResult));
	}
	else
	{
		StartGame(DedicatedServerCheckBox->IsChecked() ? EServerStartMode::SERVER_Dedicated : EServerStartMode::SERVER_Listen);
	}

	return FReply::Handled();
}

void SUWCreateGamePanel::CloudOutOfSyncResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_YES)
	{
		StartGame(DedicatedServerCheckBox->IsChecked() ? EServerStartMode::SERVER_Dedicated : EServerStartMode::SERVER_Listen);
	}
}

FReply SUWCreateGamePanel::StartGame(EServerStartMode Mode)
{
	// save changes
	GConfig->SetString(TEXT("CreateGameDialog"), TEXT("LastGametypePath"), *SelectedGameClass->GetPathName(), GGameIni);
	SelectedGameClass.GetDefaultObject()->UILastStartingMap = SelectedMap->GetText().ToString();
	SelectedGameClass.GetDefaultObject()->SaveConfig();
	AUTGameState::StaticClass()->GetDefaultObject()->SaveConfig();

	FString NewURL = FString::Printf(TEXT("%s?game=%s"), *SelectedMap->GetText().ToString(), *SelectedGameClass->GetPathName());
	TArray<FString> LastMutators;
	if (MutatorListEnabled.Num() > 0)
	{
		LastMutators.Add(MutatorListEnabled[0]->GetPathName());
		NewURL += FString::Printf(TEXT("?mutator=%s"), *MutatorListEnabled[0]->GetPathName());
		for (int32 i = 1; i < MutatorListEnabled.Num(); i++)
		{
			NewURL += TEXT(",");
			NewURL += MutatorListEnabled[i]->GetPathName();
			LastMutators.Add(MutatorListEnabled[i]->GetPathName());
		}
	}
	GConfig->SetArray(TEXT("CreateGameDialog"), TEXT("LastMutators"), LastMutators, GGameIni);

	if (GetPlayerOwner()->DedicatedServerProcessHandle.IsValid())
	{
		if (FPlatformProcess::IsProcRunning(GetPlayerOwner()->DedicatedServerProcessHandle))
		{
			FPlatformProcess::TerminateProc(GetPlayerOwner()->DedicatedServerProcessHandle);
		}
		GetPlayerOwner()->DedicatedServerProcessHandle.Reset();
	}

	if (Mode == SERVER_Dedicated || Mode == SERVER_Listen)
	{
		GConfig->Flush(false);
		FString ExecPath = FPlatformProcess::GenerateApplicationPath(FApp::GetName(), FApp::GetBuildConfiguration());
		FString Options = FString::Printf(TEXT("unrealtournament %s -log -server"), *NewURL);

		IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
		if (OnlineSubsystem)
		{
			IOnlineIdentityPtr OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
			if (OnlineIdentityInterface.IsValid())
			{
				TSharedPtr<FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(GetPlayerOwner()->GetControllerId());
				if (UserId.IsValid())
				{
					Options += FString::Printf(TEXT(" -cloudID=%s"), *UserId->ToString());
				}
			}
		}

		FString McpConfigOverride;
		if (FParse::Value(FCommandLine::Get(), TEXT("MCPCONFIG="), McpConfigOverride))
		{
			Options += FString::Printf(TEXT(" -MCPCONFIG=%s"), *McpConfigOverride);
		}

		UE_LOG(UT, Log, TEXT("Run dedicated server with command line: %s %s"), *ExecPath, *Options);
		if (Mode == SERVER_Listen)
		{
			GetPlayerOwner()->DedicatedServerProcessHandle = FPlatformProcess::CreateProc(*ExecPath, *(Options + FString::Printf(TEXT(" -ClientProcID=%u"), FPlatformProcess::GetCurrentProcessId())), true, false, false, NULL, 0, NULL, NULL);

			if (GetPlayerOwner()->DedicatedServerProcessHandle.IsValid())
			{
				GEngine->SetClientTravel(GetPlayerOwner()->PlayerController->GetWorld(), TEXT("127.0.0.1"), TRAVEL_Absolute);

				PlayerOwner->HideMenu();
			}
		}
		else
		{
			FProcHandle ServerHandle = FPlatformProcess::CreateProc(*ExecPath, *Options, true, false, false, NULL, 0, NULL, NULL);
			if (ServerHandle.IsValid())
			{
				FPlatformMisc::RequestExit(false);
			}
		}
	}
	else if (GetPlayerOwner()->PlayerController != NULL)
	{
		GEngine->SetClientTravel(GetPlayerOwner()->PlayerController->GetWorld(), *NewURL, TRAVEL_Absolute);
		PlayerOwner->HideMenu();
	}

	return FReply::Handled();
}
FReply SUWCreateGamePanel::CancelClick()
{
	// revert config changes
	SelectedGameClass.GetDefaultObject()->ReloadConfig();
	AUTGameState::StaticClass()->GetDefaultObject()->ReloadConfig();

	//GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}

TSharedRef<ITableRow> SUWCreateGamePanel::GenerateMutatorListRow(UClass* MutatorType, const TSharedRef<STableViewBase>& OwningList)
{
	checkSlow(MutatorType->IsChildOf(AUTMutator::StaticClass()));

	FString MutatorName = MutatorType->GetDefaultObject<AUTMutator>()->DisplayName.IsEmpty() ? MutatorType->GetName() : MutatorType->GetDefaultObject<AUTMutator>()->DisplayName.ToString();
	return SNew(STableRow<UClass*>, OwningList)
		.Padding(5)
		[
			SNew(STextBlock)
			.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.NormalText")
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
	TArray<UClass*> Selection = EnabledMutators->GetSelectedItems();
	if (Selection.Num() > 0 && Selection[0] != NULL)
	{
		checkSlow(Selection[0]->IsChildOf(AUTMutator::StaticClass()));
		AUTMutator* Mut = Selection[0]->GetDefaultObject<AUTMutator>();
		if (Mut->ConfigMenu != NULL)
		{
			UUserWidget* Widget = CreateWidget<UUserWidget>(GetPlayerOwner()->GetWorld(), Mut->ConfigMenu);
			if (Widget != NULL)
			{
				Widget->AddToViewport();
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

FReply SUWCreateGamePanel::GameSettingsClick()
{
	const FTextBlockStyle* SelectedStyle = &(SUWindowsStyle::Get().GetWidgetStyle<FTextBlockStyle>("UT.TopMenu.Button.SmallTextStyle.Selected"));
	const FTextBlockStyle* NormalStyle = &(SUWindowsStyle::Get().GetWidgetStyle<FTextBlockStyle>("UT.TopMenu.Button.SmallTextStyle"));

	GameSettingsLabel->SetTextStyle(SelectedStyle);

	ServerSettingsTabButton->UnPressed();
	ServerSettingsLabel->SetTextStyle(NormalStyle);

	TabSwitcher->SetActiveWidgetIndex(0);
	return FReply::Handled();
}

FReply SUWCreateGamePanel::ServerSettingsClick()
{
	const FTextBlockStyle* SelectedStyle = &(SUWindowsStyle::Get().GetWidgetStyle<FTextBlockStyle>("UT.TopMenu.Button.SmallTextStyle.Selected"));
	const FTextBlockStyle* NormalStyle = &(SUWindowsStyle::Get().GetWidgetStyle<FTextBlockStyle>("UT.TopMenu.Button.SmallTextStyle"));

	ServerSettingsLabel->SetTextStyle(SelectedStyle);

	GameSettingsLabel->SetTextStyle(NormalStyle);
	GameSettingsTabButton->UnPressed();

	TabSwitcher->SetActiveWidgetIndex(1);
	return FReply::Handled();
}


#endif
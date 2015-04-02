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
	for (TObjectIterator<UClass> It; It; ++It)
	{
		// non-native classes are detected by asset search even if they're loaded for consistency
		if (!It->HasAnyClassFlags(CLASS_Abstract | CLASS_HideDropDown) && It->HasAnyClassFlags(CLASS_Native))
		{
			if (It->IsChildOf(AUTGameMode::StaticClass()))
			{
				if (!It->GetDefaultObject<AUTGameMode>()->bHideInUI)
				{
					AllGametypes.Add(*It);
				}
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
				if (TestClass != NULL && !TestClass->HasAnyClassFlags(CLASS_Abstract) && TestClass->IsChildOf(AUTGameMode::StaticClass()) && !TestClass->GetDefaultObject<AUTGameMode>()->bHideInUI)
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

	// NOTE: leaks at the moment because Slate corrupts memory if you delete brushes
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
				.VAlign(VAlign_Fill)
				[
					BuildGamePanel(InitialSelectedGameClass)
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

}

TSharedRef<SWidget> SUWCreateGamePanel::BuildMenu()
{
	TSharedPtr<SHorizontalBox> Box = SNew(SHorizontalBox);
	Box->AddSlot()
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
	];

#if PLATFORM_WINDOWS
	Box->AddSlot()
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
#endif

	return Box.ToSharedRef();
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
						.Text(FText::Format(NSLOCTEXT("SUWCreateGamePanel", "OptimalPlayers", "Recommended Players: {0} - {1}"), FText::AsNumber(8), FText::AsNumber(12)))
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
		MapAuthor->SetText(FText::Format(NSLOCTEXT("SUWCreateGamePanel", "Author", "Author: {0}"), FText::FromString(Summary->Author)));
		MapRecommendedPlayers->SetText(FText::Format(NSLOCTEXT("SUWCreateGamePanel", "OptimalPlayers", "Recommended Players: {0} - {1}"), FText::AsNumber(Summary->OptimalPlayerCount.X), FText::AsNumber(Summary->OptimalPlayerCount.Y)));
		MapDesc->SetText(Summary->Description);
		*LevelScreenshot = FSlateDynamicImageBrush(Summary->Screenshot != NULL ? Summary->Screenshot : Cast<UUTGameEngine>(GEngine)->DefaultLevelScreenshot, LevelScreenshot->ImageSize, LevelScreenshot->GetResourceName());
	}
	else
	{
		MapAuthor->SetText(FText::Format(NSLOCTEXT("SUWCreateGamePanel", "Author", "Author: {0}"), FText::FromString(TEXT("Unknown"))));
		MapRecommendedPlayers->SetText(FText::Format(NSLOCTEXT("SUWCreateGamePanel", "OptimalPlayers", "Recommended Players: {0} - {1}"), FText::AsNumber(8), FText::AsNumber(12)));
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

		SelectedGameName->SetText(SelectedGameClass.GetDefaultObject()->DisplayName.ToString());

		// generate map list
		const AUTGameMode* GameDefaults = SelectedGameClass.GetDefaultObject();
		AllMaps.Empty();
		TArray<FAssetData> MapAssets;
		GetAllAssetData(UWorld::StaticClass(), MapAssets);
		for (const FAssetData& Asset : MapAssets)
		{
			FString MapPackageName = Asset.PackageName.ToString();
			// ignore /Engine/ as those aren't real gameplay maps
			// make sure expected file is really there
			if ( GameDefaults->SupportsMap(Asset.AssetName.ToString()) && !MapPackageName.StartsWith(TEXT("/Engine/")) &&
				IFileManager::Get().FileSize(*FPackageName::LongPackageNameToFilename(MapPackageName, FPackageName::GetMapPackageExtension())) > 0 )
			{
				static FName NAME_Title(TEXT("Title"));
				const FString* Title = Asset.TagsAndValues.Find(NAME_Title);
				AllMaps.Add(MakeShareable(new FMapListItem(Asset.AssetName.ToString(), (Title != NULL) ? *Title : FString())));
			}
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
		UUTGameUserSettings* GameSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());

		bool bShowingLanWarning = false;
		if( !GameSettings->bShouldSuppressLanWarning )
		{
			TWeakObjectPtr<UUTLocalPlayer> PlayerOwner = GetPlayerOwner();

			auto OnDialogConfirmation = [PlayerOwner] (TSharedPtr<SCompoundWidget> Widget, uint16 Button)
			{
				if (PlayerOwner.IsValid() && PlayerOwner->PlayerController)
				{
					PlayerOwner->PlayerController->bShowMouseCursor = false;
					PlayerOwner->PlayerController->SetInputMode(FInputModeGameOnly());
				}

				UUTGameUserSettings* GameSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
				GameSettings->SaveConfig();
			};

			bool bCanBindAll = false;
			TSharedRef<FInternetAddr> Address = ISocketSubsystem::Get()->GetLocalHostAddr(*GWarn, bCanBindAll);
			if (Address->IsValid())
			{
				FString StringAddress = Address->ToString(false);

				// Note: This is an extremely basic way to test for local network and it only covers the common case
				if (StringAddress.StartsWith(TEXT("192.168.")))
				{
					if( PlayerOwner->PlayerController )
					{
						PlayerOwner->PlayerController->SetInputMode( FInputModeUIOnly() );
					}
					bShowingLanWarning = true;
					PlayerOwner->ShowSupressableConfirmation(
						NSLOCTEXT("SUWCreateGamePanel", "LocalNetworkWarningTitle", "Local Network Detected"),
						NSLOCTEXT("SUWCreateGamePanel", "LocalNetworkWarningDesc", "Make sure ports 7777 and 15000 are forwarded in your router to be visible to players outside your local network"),
						FVector2D(0, 0),
						GameSettings->bShouldSuppressLanWarning,
						FDialogResultDelegate::CreateLambda( OnDialogConfirmation ) );
				}

			}
		}
		StartGame(EServerStartMode::SERVER_Listen);

		if (PlayerOwner->PlayerController && bShowingLanWarning)
		{ 
			// ensure the user can click the warning.  The game will have tried to hide the cursor otherwise
			PlayerOwner->PlayerController->bShowMouseCursor = true;
			PlayerOwner->PlayerController->SetInputMode(FInputModeUIOnly());
		}
	}

	return FReply::Handled();
}

void SUWCreateGamePanel::CloudOutOfSyncResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_YES)
	{
		StartGame(EServerStartMode::SERVER_Listen);
	}
}

FReply SUWCreateGamePanel::StartGame(EServerStartMode Mode)
{
	if (FUTAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> ParamArray;
		if (Mode == EServerStartMode::SERVER_Dedicated)		ParamArray.Add(FAnalyticsEventAttribute(TEXT("StartGameMode"), TEXT("Dedicated")));
		if (Mode == EServerStartMode::SERVER_Listen)		ParamArray.Add(FAnalyticsEventAttribute(TEXT("StartGameMode"), TEXT("Listen")));
		if (Mode == EServerStartMode::SERVER_Standalone)	ParamArray.Add(FAnalyticsEventAttribute(TEXT("StartGameMode"), TEXT("Standalone")));
		
		FUTAnalytics::GetProvider().RecordEvent( TEXT("MenuStartGame"), ParamArray );
	}

	UUTGameEngine* Engine = Cast<UUTGameEngine>(GEngine);
	if (Mode != SERVER_Standalone && Engine != NULL && !Engine->IsCloudAndLocalContentInSync())
	{
		PendingStartMode = Mode;
		FText DialogText = NSLOCTEXT("UT", "ContentOutOfSyncWarning", "You have locally created custom content that is not in your cloud storage. Players may be unable to join your server. Are you sure?");
		FDialogResultDelegate Callback;
		Callback.BindSP(this, &SUWCreateGamePanel::StartGameWarningComplete);
		GetPlayerOwner()->ShowMessage(NSLOCTEXT("U1T", "ContentNotInSync", "Custom Content Out of Sync"), DialogText, UTDIALOG_BUTTON_YES | UTDIALOG_BUTTON_NO, Callback);
	}
	else
	{
		StartGameInternal(Mode);
	}

	return FReply::Handled();
}

void SUWCreateGamePanel::StartGameWarningComplete(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_YES || ButtonID == UTDIALOG_BUTTON_OK)
	{
		StartGameInternal(PendingStartMode);
	}
}

void SUWCreateGamePanel::StartGameInternal(EServerStartMode Mode)
{
	FString SelectedMapName = MapList->GetSelectedItem().IsValid() ? MapList->GetSelectedItem().Get()->PackageName : TEXT("");
	// save changes
	GConfig->SetString(TEXT("CreateGameDialog"), TEXT("LastGametypePath"), *SelectedGameClass->GetPathName(), GGameIni);
	SelectedGameClass.GetDefaultObject()->UILastStartingMap = SelectedMapName;
	SelectedGameClass.GetDefaultObject()->SaveConfig();
	AUTGameState::StaticClass()->GetDefaultObject()->SaveConfig();

	FString NewURL = FString::Printf(TEXT("%s?game=%s"), *SelectedMapName, *SelectedGameClass->GetPathName());
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


#endif
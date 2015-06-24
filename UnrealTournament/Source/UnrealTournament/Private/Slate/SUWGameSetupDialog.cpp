// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWGameSetupDialog.h"
#include "SUWindowsStyle.h"
#include "SUTUtils.h"
#include "UTGameEngine.h"
#include "UTLobbyMatchInfo.h"
#include "UTEpicDefaultRulesets.h"
#include "UTLobbyGameState.h"

#if !UE_SERVER


void SUWGameSetupDialog::Construct(const FArguments& InArgs)
{
	SUWDialog::Construct(SUWDialog::FArguments()
							.PlayerOwner(InArgs._PlayerOwner)
							.DialogTitle(InArgs._DialogTitle)
							.DialogSize(InArgs._DialogSize)
							.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
							.DialogPosition(InArgs._DialogPosition)
							.DialogAnchorPoint(InArgs._DialogAnchorPoint)
							.ContentPadding(InArgs._ContentPadding)
							.ButtonMask(InArgs._ButtonMask)
							.OnDialogResult(InArgs._OnDialogResult)
						);

	bHubMenu = GetPlayerOwner()->GetWorld()->GetGameState<AUTLobbyGameState>() != NULL;


	GameRulesets = InArgs._GameRuleSets;
	BotSkillLevel = 3;

	if (DialogContent.IsValid())
	{
		DialogContent->AddSlot()
		[

			SNew(SOverlay)
			+SOverlay::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(46)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SImage)
							.Image(SUWindowsStyle::Get().GetBrush("UT.TopMenu.MidFill"))
						]
					]
				]
			]
			+SOverlay::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(46)
					[
						SAssignNew(TabButtonPanel,SHorizontalBox)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(15.0f, 5.0f, 10.0f, 10.0f)
				.HAlign(HAlign_Fill)
				[
					SAssignNew(RulesPanel, SGridPanel)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(HideBox, SVerticalBox)
				]
			]
		];
	}

	BuildCategories();

	DisableButton(UTDIALOG_BUTTON_OK);
	DisableButton(UTDIALOG_BUTTON_PLAY);
	DisableButton(UTDIALOG_BUTTON_LAN);

}

TSharedRef<class SWidget> SUWGameSetupDialog::BuildCustomButtonBar()
{
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.AutoHeight()
		.Padding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(200)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					.Text(NSLOCTEXT("SUWGameSetupDialog","BotSkill","Bot Skill Level:"))
					.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUWGameSetupDialog","BotSkillTT","Configure how good the bots will be.")))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(BotSkillButton, SUTComboButton)
				.ButtonStyle(SUWindowsStyle::Get(),"UWindows.Standard.Button")
				.MenuButtonStyle(SUWindowsStyle::Get(),"UT.ContextMenu.Button")
				.MenuButtonTextStyle(SUWindowsStyle::Get(),"UT.ContextMenu.TextStyle")
				.HasDownArrow(false)
				.OnButtonSubMenuSelect(this, &SUWGameSetupDialog::OnBotMenuSelect)
				.bRightClickOpensMenu(false)
				.MenuPlacement(MenuPlacement_ComboBox)
				.DefaultMenuItems(TEXT("No Bots,Novice,Average,Experienced,Skilled,Adept,Masterful,Inhuman,Godlike"))
				.ButtonContent()
				[			
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					.Text(this, &SUWGameSetupDialog::GetBotSkillText)
				]
			]
		];
}

FText SUWGameSetupDialog::GetBotSkillText() const
{
	switch (BotSkillLevel)
	{
		case 0 :	return NSLOCTEXT("BotSkillLevels","Novice","Novice"); break;
		case 1 :	return NSLOCTEXT("BotSkillLevels","Average","Average"); break;
		case 2 :	return NSLOCTEXT("BotSkillLevels","Experienced","Experienced"); break;
		case 3 :	return NSLOCTEXT("BotSkillLevels","Skilled","Skilled"); break;
		case 4 :	return NSLOCTEXT("BotSkillLevels","Adept","Adept"); break;
		case 5 :	return NSLOCTEXT("BotSkillLevels","Masterful","Masterful"); break;
		case 6 :	return NSLOCTEXT("BotSkillLevels","Inhuman","Inhuman"); break;
		case 7 :	return NSLOCTEXT("BotSkillLevels","Godlike","Godlike"); break;
	}

	return NSLOCTEXT("BotSkillLevels","NoBots","No Bots");

}

void SUWGameSetupDialog::BuildCategories()
{
	TSharedPtr<SUTTabButton> Button;
	
	TArray<FName> Categories;
	for(int32 i=0; i < GameRulesets.Num(); i++)
	{
		for (int32 j=0; j < GameRulesets[i]->Categories.Num(); j++)
		{
			FName Cat = GameRulesets[i]->Categories[j];
			if (Categories.Find(Cat) == INDEX_NONE)
			{
				Categories.Add(Cat);
			}
		}
	}

	Categories.Add(FName(TEXT("Custom")));

	for (int32 i=0;i<Categories.Num(); i++)
	{
		TabButtonPanel->AddSlot()
		.Padding(FMargin(25.0f,0.0f,0.0f,0.0f))
		.AutoWidth()
		[
			SAssignNew(Button, SUTTabButton)
			.ContentPadding(FMargin(15.0f, 10.0f, 70.0f, 0.0f))
			.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.OptionTabButton")
			.ClickMethod(EButtonClickMethod::MouseDown)
			.Text(FText::FromString(Categories[i].ToString()))
			.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
			.OnClicked(this, &SUWGameSetupDialog::OnTabButtonClick, i)
		];
		Tabs.Add(FTabButtonInfo(Button, Categories[i]));
	}

	OnTabButtonClick(0);
}

FReply SUWGameSetupDialog::OnTabButtonClick(int32 ButtonIndex)
{
	// Toggle the state of the tabs
	for (int32 i=0; i < Tabs.Num(); i++)
	{
		if (i == ButtonIndex)
		{
			Tabs[i].Button->BePressed();
		}
		else
		{
			Tabs[i].Button->UnPressed();
		}
	}

	DisableButton(UTDIALOG_BUTTON_OK);
	DisableButton(UTDIALOG_BUTTON_PLAY);
	DisableButton(UTDIALOG_BUTTON_LAN);

	BuildRuleList(Tabs[ButtonIndex].Category);
	SelectedRuleset.Reset();
	if (MapBox.IsValid())
	{
		MapBox->ClearChildren();
	}

	// Build the Rules List
	return FReply::Handled();
}

void SUWGameSetupDialog::BuildRuleList(FName Category)
{
	RulesPanel->ClearChildren();
	RuleSubset.Empty();

	CurrentCategory = Category;

	if (Category == FName(TEXT("Custom")))
	{
		HideBox->ClearChildren();
		RulesPanel->AddSlot(0,0)
		[
			SAssignNew(CustomPanel, SUWCreateGamePanel, GetPlayerOwner())
		];

		return;	

	}
	CustomPanel.Reset();

	HideBox->ClearChildren();
	HideBox->AddSlot()
		.Padding(15.0f, 0.0f, 10.0f, 10.0f)
		.AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(220)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding(10.0,10.0,0.0,5.0)
				.FillWidth(1.0)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(),"UT.Hub.RulesTitle")
						.Text(this, &SUWGameSetupDialog::GetMatchRulesTitle)
						.ColorAndOpacity(FLinearColor::Yellow)
					]

					+SVerticalBox::Slot()
					.FillHeight(1.0)
					[
						SNew(SRichTextBlock)
						.TextStyle(SUWindowsStyle::Get(),"UT.Hub.RulesText")
						.Justification(ETextJustify::Left)
						.DecoratorStyleSet( &SUWindowsStyle::Get() )
						.AutoWrapText( true )
						.Text(this, &SUWGameSetupDialog::GetMatchRulesDescription)
					]
				]
			]
		];

	HideBox->AddSlot()
	.Padding(15.0f, 0.0f, 10.0f, 0.0f)
	.FillHeight(1.0)
	.HAlign(HAlign_Fill)
	[
		SAssignNew(MapBox, SVerticalBox)
	];

	int32 Cnt = 0;
	for (int32 i=0;i<GameRulesets.Num();i++)
	{
		if (GameRulesets[i].IsValid() && GameRulesets[i]->Categories.Find(Category) != INDEX_NONE)
		{
			int32 Row = Cnt / 6;
			int32 Col = Cnt % 6;

			FString Title = GameRulesets[i]->Title.IsEmpty() ? TEXT("") : GameRulesets[i]->Title;
			FString ToolTip = GameRulesets[i]->Tooltip.IsEmpty() ? TEXT("") : GameRulesets[i]->Tooltip;
			
			TSharedPtr<SUTTabButton> Button;

			RulesPanel->AddSlot(Col,Row).Padding(5.0,0.0,5.0,0.0)
			[
				SNew(SBox)
				.WidthOverride(196)
				.HeightOverride(230)							
				[
					SAssignNew(Button, SUTTabButton)
					.ButtonStyle(SUWindowsStyle::Get(), "UT.ComplexButton")
					.OnClicked(this, &SUWGameSetupDialog::OnRuleClick, Cnt)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SBox)
							.WidthOverride(196)
							.HeightOverride(196)							
							[
								SNew(SImage)
								.Image(GameRulesets[i]->GetSlateBadge())
								.ToolTip(SUTUtils::CreateTooltip(FText::FromString(ToolTip)))
							]

						]
						+SVerticalBox::Slot()
						.HAlign(HAlign_Center)
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(FText::FromString(Title))
							.TextStyle(SUWindowsStyle::Get(),"UT.Hub.MapsText")
							.ColorAndOpacity(FLinearColor::Black)
						]
					]
				]
			];
		
			RuleSubset.Add(FRuleSubsetInfo(GameRulesets[i], Button, false));
			Cnt++;
		}
	}
}

FReply SUWGameSetupDialog::OnRuleClick(int32 RuleIndex)
{
	if (RuleIndex >= 0 && RuleIndex <= RuleSubset.Num() && RuleSubset[RuleIndex].Ruleset.IsValid())
	{

		for (int32 i=0;i<RuleSubset.Num(); i++)
		{
			if (i == RuleIndex)
			{
				RuleSubset[i].Button->BePressed();
			}
			else
			{
				RuleSubset[i].Button->UnPressed();
			}
		}


		SelectedRuleset = RuleSubset[RuleIndex].Ruleset;

		BuildMapList();
	}

	return FReply::Handled();
}

void SUWGameSetupDialog::BuildMapList()
{
	AUTGameState* GameState = GetPlayerOwner()->GetWorld()->GetGameState<AUTGameState>();
	if (SelectedRuleset.IsValid() && GameState)
	{
		TArray<TSharedPtr<FMapListItem>> AllowedMapList;
		GameState->GetAvailableMaps(SelectedRuleset->MapPrefixes, AllowedMapList);
		MapPlayList.Empty();
		for (int32 i=0; i< AllowedMapList.Num(); i++)
		{
			// Look to see if this map fits the rules...

			int32 OptimalPlayerCount = SelectedRuleset->bTeamGame ? AllowedMapList[i]->OptimalTeamPlayerCount : AllowedMapList[i]->OptimalPlayerCount;
			if (OptimalPlayerCount >= SelectedRuleset->OptimalPlayers)
			{
				MapPlayList.Add(FMapPlayListInfo(AllowedMapList[i], false));
			}

			if (AllowedMapList[i]->Screenshot != TEXT(""))
			{
				FString Package = AllowedMapList[i]->Screenshot;
				const int32 Pos = Package.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart);
				if ( Pos != INDEX_NONE )
				{
					Package = Package.Left(Pos);
				}

				LoadPackageAsync(Package, FLoadPackageAsyncDelegate::CreateRaw(this, &SUWGameSetupDialog::TextureLoadComplete),0);
			}
		}
	}

	// Build the first panel
	BuildMapPanel();
}

void SUWGameSetupDialog::TextureLoadComplete(const FName& InPackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result)
{
	if (Result == EAsyncLoadingResult::Succeeded)
	{
		for (int32 i=0 ;i < MapPlayList.Num(); i++)
		{
			FString Screenshot = MapPlayList[i].MapInfo->Screenshot;
			FString PackageName = InPackageName.ToString();
			if (Screenshot != TEXT("") && Screenshot.Contains(PackageName))
			{
				UTexture2D* Tex = FindObject<UTexture2D>(nullptr, *Screenshot);
				if (Tex)
				{
					MapPlayList[i].MapTexture = Tex;
					MapPlayList[i].MapImage = new FSlateDynamicImageBrush(Tex, FVector2D(256.0, 128.0), NAME_None);
					MapPlayList[i].ImageWidget->SetImage(MapPlayList[i].MapImage);
				}
			}
		}
	}
}

void SUWGameSetupDialog::BuildMapPanel()
{
	if (SelectedRuleset.IsValid() && MapBox.IsValid() )
	{
		MapBox->ClearChildren();

		TSharedPtr<SGridPanel> MapPanel;
		MapBox->AddSlot().AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(56)
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
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.Padding(10.0f,0.0f,0.0f,0.0f)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("SUWGameSetupDialog","MapListInstructions","Select Starting Map..."))
						.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
					]
				]
			]
			
		];		

		MapBox->AddSlot().FillHeight(1.0)
		[
			SNew(SScrollBox)
			+SScrollBox::Slot()
			[
				SAssignNew(MapPanel, SGridPanel)
			]
		];

		if (MapPanel.IsValid())
		{
			MapPanel->ClearChildren();

			for (int32 i=0; i< MapPlayList.Num(); i++)
			{
				int32 Row = i / 6;
				int32 Col = i % 6;

				FString Title = MapPlayList[i].MapInfo->Title;
				FString ToolTip = MapPlayList[i].MapInfo->Description;;

				TSharedPtr<SUTComboButton> Button;
				TSharedPtr<SImage> CheckMark;
				TSharedPtr<SImage> ImageWidget;

				MapPanel->AddSlot(Col, Row).Padding(5.0,5.0,5.0,5.0)
				[
					SNew(SBox)
					.WidthOverride(256)
					.HeightOverride(158)							
					[
						SAssignNew(Button, SUTComboButton)
						.OnClicked(this, &SUWGameSetupDialog::OnMapClick, i)
						.IsToggleButton(true)
						.OnButtonSubMenuSelect(this, &SUWGameSetupDialog::OnSubMenuSelect)
						.ButtonStyle(SUWindowsStyle::Get(), "UT.ComplexButton")
						.MenuButtonStyle(SUWindowsStyle::Get(),"UT.ContextMenu.Button")
						.MenuButtonTextStyle(SUWindowsStyle::Get(),"UT.ContextMenu.TextStyle")
						.HasDownArrow(false)
						.bRightClickOpensMenu(true)
						.DefaultMenuItems(TEXT("Move To Top,Move Up,Move Down"))
						.MenuPlacement(MenuPlacement_MenuRight)
						.ToolTip(SUTUtils::CreateTooltip(FText::FromString(ToolTip)))
						.ButtonContent()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SBox)
								.WidthOverride(256)
								.HeightOverride(128)							
								[
									SNew(SOverlay)
									+SOverlay::Slot()
									[
										SAssignNew(ImageWidget,SImage)
										.Image(MapPlayList[i].MapImage)
									]
									+SOverlay::Slot()
									[
										SNew(SVerticalBox)
										+SVerticalBox::Slot()
										.AutoHeight()
										[
											SNew(SHorizontalBox)
											+SHorizontalBox::Slot()
											.AutoWidth()
											[
												SNew(SBox)
												.WidthOverride(32)
												.HeightOverride(32)							
												[
													SAssignNew(CheckMark, SImage)
													.Image(SUWindowsStyle::Get().GetBrush("UT.CheckMark"))
													.Visibility(EVisibility::Hidden)
												]
											]
										]
									]
								]

							]
							+SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(Title))
								.TextStyle(SUWindowsStyle::Get(),"UT.Hub.MapsText")
								.ColorAndOpacity(FLinearColor::Black)
							]
						]
					]
				];

				MapPlayList[i].SetWidgets(Button, ImageWidget, CheckMark);
			}
		}
	}
}

void SUWGameSetupDialog::OnBotMenuSelect(int32 MenuCmdId, TSharedPtr<SUTComboButton> Sender)
{
	BotSkillLevel = MenuCmdId-1;
}

void SUWGameSetupDialog::OnSubMenuSelect(int32 MenuCmdId, TSharedPtr<SUTComboButton> Sender)
{
	int32 MapIndex = INDEX_NONE;
	for (int32 i=0; i < MapPlayList.Num(); i++)
	{
		if (MapPlayList[i].Button.Get() == Sender.Get())
		{
			MapIndex  = i;
			break;
		}
	}

	if (MapIndex != INDEX_NONE)
	{
		if (MenuCmdId == 0)			// Move to Top...
		{
			FMapPlayListInfo MI = MapPlayList[MapIndex];
			MapPlayList.RemoveAt(MapIndex,1);
			MapPlayList.Insert(MI, 0);
			BuildMapPanel();
		}
		else if (MenuCmdId == 1)
		{
			if (MapIndex > 0)
			{
				MapPlayList.Swap(MapIndex, MapIndex-1);
				BuildMapPanel();
			}
		}
		else if (MenuCmdId == 2)
		{
			if (MapIndex < MapPlayList.Num()-1)
			{
				MapPlayList.Swap(MapIndex, MapIndex+1);
				BuildMapPanel();
			}
		}
	}
}


FReply SUWGameSetupDialog::OnMapClick(int32 MapIndex)
{
	int32 Cnt = 0;
	if (MapIndex >= 0 && MapIndex <= MapPlayList.Num())
	{
		for (int32 i = 0; i < MapPlayList.Num(); i++)
		{
			MapPlayList[i].bSelected = false;
			MapPlayList[i].Button->UnPressed();

		}

		MapPlayList[MapIndex].bSelected = true;
		MapPlayList[MapIndex].Button->BePressed();
	}

	// If we are a hub menu, then selecting the map is as good as starting.
	if (bHubMenu)
	{
		OnButtonClick(UTDIALOG_BUTTON_OK);
	}


	return FReply::Handled();
}


FText SUWGameSetupDialog::GetMatchRulesTitle() const
{
	return SelectedRuleset.IsValid() ? FText::FromString(FString::Printf(TEXT("GAME MODE: %s"), *SelectedRuleset.Get()->Title)) : NSLOCTEXT("SUWGameSetupDialog","PickGameMode","Choose your game mode....");
}

FText SUWGameSetupDialog::GetMatchRulesDescription() const
{
	return SelectedRuleset.IsValid() ? FText::FromString(SelectedRuleset.Get()->Description) : FText::GetEmpty();
}

void SUWGameSetupDialog::ApplyCurrentRuleset(TWeakObjectPtr<AUTLobbyMatchInfo> MatchInfo)
{
	if (!MatchInfo.IsValid() || !MatchInfo->CurrentRuleset.IsValid() || MatchInfo->CurrentRuleset->Categories.Num()<= 0) return;

	// Attempt to hook up a given ruleset.

	// Find the best tab...

	for (int32 i=0; i<Tabs.Num();i++)
	{
		if (Tabs[i].Category == MatchInfo->CurrentRuleset->Categories[0])
		{
			// Found it, switch to it's tab.
			OnTabButtonClick(i);

			// Now find the rule and pick it.

			for (int32 RuleIndex = 0; RuleIndex < RuleSubset.Num(); RuleIndex++)
			{
				if (RuleSubset[RuleIndex].Ruleset.IsValid() && RuleSubset[RuleIndex].Ruleset->UniqueTag.Equals(MatchInfo->CurrentRuleset->UniqueTag, ESearchCase::IgnoreCase))
				{
					// Select it.
					OnRuleClick(RuleIndex);
					BuildMapPanel();		
					return;
				}
			}
		}
	}
}

void SUWGameSetupDialog::GetCustomGameSettings(FString& GameMode, FString& StartingMap, FString& Description, TArray<FString>&GameOptions, int32& DesiredPlayerCount)
{
	if (CustomPanel.IsValid())
	{
		CustomPanel->GetCustomGameSettings(GameMode, StartingMap, Description, GameOptions, DesiredPlayerCount, BotSkillLevel);
	}
}

FString SUWGameSetupDialog::GetSelectedMap()
{
	FString StartingMap = TEXT("");
	for (int32 i=0; i< MapPlayList.Num(); i++ )
	{
		if (MapPlayList[i].bSelected)
		{

			return MapPlayList[i].MapInfo->PackageName;
		}
	}

	return TEXT("");
}

void SUWGameSetupDialog::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (IsCustomSettings())
	{
		if (CustomPanel->IsReadyToPlay())
		{
			EnableButton(UTDIALOG_BUTTON_OK);
			EnableButton(UTDIALOG_BUTTON_PLAY);
			EnableButton(UTDIALOG_BUTTON_LAN);
		}
		else
		{
			DisableButton(UTDIALOG_BUTTON_OK);
			DisableButton(UTDIALOG_BUTTON_PLAY);
			DisableButton(UTDIALOG_BUTTON_LAN);
		}
	}
	else
	{
		if (SelectedRuleset.IsValid() && GetSelectedMap() != TEXT(""))
		{
			EnableButton(UTDIALOG_BUTTON_OK);
			EnableButton(UTDIALOG_BUTTON_PLAY);
			EnableButton(UTDIALOG_BUTTON_LAN);
		}
		else
		{
			DisableButton(UTDIALOG_BUTTON_OK);
			DisableButton(UTDIALOG_BUTTON_PLAY);
			DisableButton(UTDIALOG_BUTTON_LAN);
		}

	}
}


#endif
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWGameSetupDialog.h"
#include "SUWindowsStyle.h"
#include "SUTUtils.h"
#include "UTGameEngine.h"
#include "UTLobbyMatchInfo.h"


#if !UE_SERVER


void SUWGameSetupDialog::Construct(const FArguments& InArgs)
{

	Categories.Add(FGameRuleCategories::Casual);		CategoryTexts.Add(NSLOCTEXT("SUWGameSetupDialog","CategoryText_Casual","Casual"));
	Categories.Add(FGameRuleCategories::TeamPlay);		CategoryTexts.Add(NSLOCTEXT("SUWGameSetupDialog","CategoryText_TeamPlay","Team Play"));
	Categories.Add(FGameRuleCategories::Big);	CategoryTexts.Add(NSLOCTEXT("SUWGameSetupDialog","CategoryText_Big","Big"));
	Categories.Add(FGameRuleCategories::Competitive);	CategoryTexts.Add(NSLOCTEXT("SUWGameSetupDialog","CategoryText_Competitive","Competitve"));
	Categories.Add(FGameRuleCategories::Custom);		CategoryTexts.Add(NSLOCTEXT("SUWGameSetupDialog","CategoryText_Custom","Custom"));

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
				]
				+SVerticalBox::Slot()
				.Padding(15.0f, 0.0f, 10.0f, 0.0f)
				.FillHeight(1.0)
				.HAlign(HAlign_Fill)
				[
					SAssignNew(MapBox, SVerticalBox)
				]

			]
		];
	}
	BuildCategories();
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

	for (int i=0;i<Categories.Num(); i++)
	{
		TabButtonPanel->AddSlot()
		.Padding(FMargin(25.0f,0.0f,0.0f,0.0f))
		.AutoWidth()
		[
			SAssignNew(Button, SUTTabButton)
			.ContentPadding(FMargin(15.0f, 10.0f, 70.0f, 0.0f))
			.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.OptionTabButton")
			.ClickMethod(EButtonClickMethod::MouseDown)
			.Text(CategoryTexts[i])
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

	BuildRuleList(Categories[ButtonIndex]);
	SelectedRuleset.Reset();
	MapBox->ClearChildren();

	// Build the Rules List
	return FReply::Handled();
}

void SUWGameSetupDialog::BuildRuleList(FName Category)
{
	RulesPanel->ClearChildren();
	RuleSubset.Empty();

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
							.Text(Title)
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
	if (SelectedRuleset.IsValid())
	{
		MapPlayList.Empty();
		for (int32 i=0; i< SelectedRuleset->MapPlaylist.Num(); i++)
		{
			// Pull the level Summary		
			TSharedPtr<FSlateDynamicImageBrush> Screenshot = MakeShareable(new FSlateDynamicImageBrush(Cast<UUTGameEngine>(GEngine)->DefaultLevelScreenshot, FVector2D(256.0, 128.0), FName(TEXT("HubMapListShot"))));
			UUTLevelSummary* Summary = UUTGameEngine::LoadLevelSummary(SelectedRuleset->MapPlaylist[i]);
			if (Summary != NULL)
			{
				*Screenshot = FSlateDynamicImageBrush(Summary->Screenshot != NULL ? Summary->Screenshot : Cast<UUTGameEngine>(GEngine)->DefaultLevelScreenshot, Screenshot->ImageSize, Screenshot->GetResourceName());
			}
			else
			{
				break;
			}	
			MapPlayList.Add(FMapPlayListInfo(SelectedRuleset->MapPlaylist[i], Screenshot, Summary,(i<SelectedRuleset->MapPlaylistSize)));
		}
	}

	// Build the first panel
	BuildMapPanel();
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
						.Text(FText::Format(NSLOCTEXT("SUWGameSetupDialog","MapListInstructions","Select up to {0} maps to play..."), FText::AsNumber(SelectedRuleset->MapPlaylistSize)))
						.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
					]
				]
				+SOverlay::Slot()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Bottom)
					.Padding(0.0f,0.0f,10.0f,10.0f)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("SUWGameSetupDialog","MapListInstructionsB","(right-click to reorder)"))
						.TextStyle(SUWindowsStyle::Get(), "UT.Hub.RulesText_Small")
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

			for (int32 i=0; i< SelectedRuleset->MapPlaylist.Num(); i++)
			{
				int32 Row = i / 5;
				int32 Col = i % 5;

				FString Title = MapPlayList[i].MapName;
				FString ToolTip = MapPlayList[i].MapName;
				
				if (MapPlayList[i].LevelSummary.IsValid())
				{
					Title = MapPlayList[i].LevelSummary->Title.IsEmpty() ? Title : MapPlayList[i].LevelSummary->Title;
					ToolTip = MapPlayList[i].LevelSummary->Description.IsEmpty() ? ToolTip : MapPlayList[i].LevelSummary->Description.ToString();
				}

				TSharedPtr<SUTComboButton> Button;
				TSharedPtr<SImage> CheckMark;

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
										SNew(SImage)
										.Image(MapPlayList[i].MapImage.Get())
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

				MapPlayList[i].SetWidgets(Button, CheckMark);
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
	if (MapIndex >= 0 && MapIndex <= MapPlayList.Num())
	{
		if (MapPlayList[MapIndex].bSelected)
		{
			MapPlayList[MapIndex].bSelected = false;
			MapPlayList[MapIndex].Button->UnPressed();
			MapPlayList[MapIndex].CheckMark->SetVisibility(EVisibility::Hidden);
		}
		else
		{
			int32 Cnt = 0;
			for (int32 i = 0; i < MapPlayList.Num(); i++)
			{
				if (MapPlayList[i].bSelected) Cnt++;
			}
		
			if (Cnt == SelectedRuleset->MapPlaylistSize)
			{
				MapPlayList[MapIndex].Button->UnPressed();
				GetPlayerOwner()->MessageBox(NSLOCTEXT("SUWGameSetupDialog","TooManyMapsTitle","Too many maps"), NSLOCTEXT("SUWGameSetupDialog","TooManyMaps","You can only have 6 maps in a play list at any given time.  Plesae unselect a map before adding another."));
			}
			else
			{
				MapPlayList[MapIndex].bSelected = true;
				MapPlayList[MapIndex].Button->BePressed();
				MapPlayList[MapIndex].CheckMark->SetVisibility(EVisibility::Visible);
			}
		}
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

	for (int32 i=0; i<Categories.Num();i++)
	{
		if (Categories[i] == MatchInfo->CurrentRuleset->Categories[0])
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

					// First, toggle all maps off
				
					for (int32 Ma = 0; Ma < MapPlayList.Num(); Ma++)
					{
						MapPlayList[Ma].bSelected = false;
						MapPlayList[Ma].Button->UnPressed();
						MapPlayList[Ma].CheckMark->SetVisibility(EVisibility::Hidden);
					}

					// Now, turn on and sort..

					for (int32 Mb = MatchInfo->MapList.Num() - 1 ; Mb >= 0; Mb--)
					{
						for (int32 Ma = 0; Ma <MapPlayList.Num(); Ma++ )
						{
							if (MapPlayList[Ma].MapName == MatchInfo->MapList[Mb])
							{
								// Found it...
								MapPlayList[Ma].bSelected = true;
								MapPlayList[Ma].Button->BePressed();
								MapPlayList[Ma].CheckMark->SetVisibility(EVisibility::Visible);
								
								// Now move it to the top.
					
								FMapPlayListInfo MPLI = MapPlayList[Ma];
								MapPlayList.RemoveAt(Ma);
								MapPlayList.Insert(MPLI, 0);
								break;
							}
						}
					}
					BuildMapPanel();		
					return;
				}
			}
		}
	}
}

#endif
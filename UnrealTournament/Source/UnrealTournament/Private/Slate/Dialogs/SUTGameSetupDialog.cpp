// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SUTGameSetupDialog.h"
#include "../SUWindowsStyle.h"
#include "../SUTUtils.h"
#include "UTGameEngine.h"
#include "UTLobbyMatchInfo.h"
#include "UTEpicDefaultRulesets.h"
#include "UTLobbyGameState.h"
#include "UTReplicatedMapInfo.h"
#include "PartyContext.h"
#include "BlueprintContextLibrary.h"

#if !UE_SERVER

void SUTGameSetupDialog::Construct(const FArguments& InArgs)
{
	bBeginnerMatch = false;
	bUserTurnedOffRankCheck = false;
	bHubMenu = InArgs._PlayerOwner->GetWorld()->GetGameState<AUTLobbyGameState>() != NULL;
	SUTDialogBase::Construct(SUTDialogBase::FArguments()
							.PlayerOwner(InArgs._PlayerOwner)
							.DialogTitle(InArgs._DialogTitle)
							.DialogSize(InArgs._DialogSize)
							.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
							 .IsScrollable(false)
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
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(52)
				[
					SNew(SBorder)
					.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
					[
						SAssignNew(TabButtonPanel,SHorizontalBox)
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(15.0f, 5.0f, 10.0f, 10.0f)
			.HAlign(HAlign_Fill)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox).MaxDesiredWidth(1400)
						[
							SAssignNew(RulesPanel, SGridPanel)
						]
					]
					+SHorizontalBox::Slot().FillWidth(1.0)
					[
						SAssignNew(RulesInfoBox, SVerticalBox)
					]
				]
				+SOverlay::Slot().VAlign(VAlign_Fill)
				[
					SAssignNew(CustomBox, SVerticalBox)
				]
			]
			+ SVerticalBox::Slot().Padding(15.0f, 25.0f, 10.0f, 25.0f).AutoHeight()
			[
				SNew(SBox).HeightOverride(46)
				[
					BuildBotSkill()
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SAssignNew(HideBox, SVerticalBox)
			]
		];
	}

	BuildCategories();

	DisableButton(UTDIALOG_BUTTON_OK);
	DisableButton(UTDIALOG_BUTTON_PLAY);
	DisableButton(UTDIALOG_BUTTON_LAN);

}

void SUTGameSetupDialog::BuildCategories()
{
	TSharedPtr<SUTTabButton> Button;
	
	TArray<FName> Categories;
	UUTEpicDefaultRulesets* DefaultRules = UUTEpicDefaultRulesets::StaticClass()->GetDefaultObject<UUTEpicDefaultRulesets>();
	if (DefaultRules != nullptr)
	{
		for (int32 i=0; i < DefaultRules->RuleCategories.Num(); i++)
		{
			Categories.Add(DefaultRules->RuleCategories[i].CategoryName);
			TabButtonPanel->AddSlot()
			.Padding(FMargin(25.0f,0.0f,0.0f,0.0f))
			.AutoWidth()
			[
				SAssignNew(Button, SUTTabButton)
				.ContentPadding(FMargin(15.0f, 10.0f, 15.0f, 0.0f))
				.Text(FText::FromString(DefaultRules->RuleCategories[i].CategoryButtonText))
				.ButtonStyle(SUTStyle::Get(), "UT.TabButton")
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
				.ClickMethod(EButtonClickMethod::MouseDown)
				.OnClicked(this, &SUTGameSetupDialog::OnTabButtonClick, i)
			];
			Tabs.Add(FTabButtonInfo(Button, DefaultRules->RuleCategories[i].CategoryName));
		}
	}

	FName CustomCategory = FName(TEXT("Custom"));
	Categories.Add(CustomCategory);

	TabButtonPanel->AddSlot()
	.Padding(FMargin(25.0f,0.0f,0.0f,0.0f))
	.FillWidth(1.0f)
	.HAlign(HAlign_Right)
	[
		SAssignNew(Button, SUTTabButton)
		.ContentPadding(FMargin(15.0f, 10.0f, 15.0f, 0.0f))
		.Text(NSLOCTEXT("SUTGameSetupDialog","CustomGameButtonText","Custom Game"))
		.ButtonStyle(SUTStyle::Get(), "UT.TabButton")
		.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
		.ClickMethod(EButtonClickMethod::MouseDown)
		.OnClicked(this, &SUTGameSetupDialog::OnTabButtonClick, Categories.Num() -1)
	];
	Tabs.Add(FTabButtonInfo(Button, CustomCategory));
	OnTabButtonClick(0);
}

FReply SUTGameSetupDialog::OnTabButtonClick(int32 ButtonIndex)
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

	SelectedRuleset.Reset();
	BuildRuleList(Tabs[ButtonIndex].Category);

	// Build the Rules List
	return FReply::Handled();
}

void SUTGameSetupDialog::BuildRuleList(FName Category)
{
	RulesPanel->ClearChildren();
	HideBox->ClearChildren();
	RulesInfoBox->ClearChildren();
	CustomBox->ClearChildren();

	RuleSubset.Empty();
	CurrentCategory = Category;

	if (Category == FName(TEXT("Custom")))
	{
		CustomBox->AddSlot().AutoHeight()
		[
			SNew(SBox).HeightOverride(800)
			[
				SAssignNew(CustomPanel, SUTCreateGamePanel, GetPlayerOwner())
			]
		];

		return;	
	}
	else
	{
		CustomPanel.Reset();
	}

	RulesInfoBox->AddSlot()
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
						.Text(this, &SUTGameSetupDialog::GetMatchRulesTitle)
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
						.Text(this, &SUTGameSetupDialog::GetMatchRulesDescription)
					]
				]
			]
		];

	HideBox->ClearChildren();
	HideBox->AddSlot()
	.Padding(15.0f, 0.0f, 10.0f, 0.0f)
	.FillHeight(1.0)
	.HAlign(HAlign_Fill)
	[
		SNew(SScrollBox)
		+SScrollBox::Slot()
		[
			SAssignNew(MapBox, SVerticalBox)
		]
	];

	int32 Cnt = 0;
	for (int32 i=0;i<GameRulesets.Num();i++)
	{
		if (GameRulesets[i] && GameRulesets[i]->Categories.Find(Category) != INDEX_NONE)
		{
			int32 Row = Cnt / 6;
			int32 Col = Cnt % 6;

			FString Title = GameRulesets[i]->Title.IsEmpty() ? TEXT("") : GameRulesets[i]->Title;
			FString NewToolTip = GameRulesets[i]->Tooltip.IsEmpty() ? TEXT("") : GameRulesets[i]->Tooltip;
			
			TSharedPtr<SUTTabButton> Button;

			RulesPanel->AddSlot(Col,Row).Padding(5.0,0.0,5.0,0.0)
			[
				SNew(SBox)
				.WidthOverride(196)
				.HeightOverride(230)							
				[
					SAssignNew(Button, SUTTabButton)
					.ButtonStyle(SUWindowsStyle::Get(), "UT.ComplexButton")
					.OnClicked(this, &SUTGameSetupDialog::OnRuleClick, Cnt)
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
								.ToolTip(SUTUtils::CreateTooltip(FText::FromString(NewToolTip)))
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

	OnRuleClick(0);
}

FReply SUTGameSetupDialog::OnRuleClick(int32 RuleIndex)
{
	if (RuleIndex >= 0 && RuleSubset.IsValidIndex(RuleIndex) && RuleIndex <= RuleSubset.Num() && RuleSubset[RuleIndex].Ruleset.IsValid())
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

		TWeakObjectPtr<AUTLobbyPlayerState> MatchOwner = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
		if (MatchOwner.IsValid() && MatchOwner->IsABeginner(SelectedRuleset.IsValid() ? SelectedRuleset->GetDefaultGameModeObject() : NULL))
		{
			bBeginnerMatch = true;

			// We are a beginner....
			if (!bUserTurnedOffRankCheck)
			{
				cbRankLocked->SetIsChecked(ECheckBoxState::Checked);
				MatchOwner->NotifyBeginnerAutoLock();
			}
		}
		else
		{
			bBeginnerMatch = false;
		}

		BuildMapList();
	}

	return FReply::Handled();
}

void SUTGameSetupDialog::BuildMapList()
{
	AUTGameState* GameState = GetPlayerOwner()->GetWorld()->GetGameState<AUTGameState>();
	if (SelectedRuleset.IsValid() && GameState)
	{
		MapPlayList.Empty();
		for (int32 i=0; i< SelectedRuleset->MapList.Num(); i++)
		{
			if (SelectedRuleset->MapList[i] != NULL)
			{
				MapPlayList.Add(FMapPlayListInfo(SelectedRuleset->MapList[i], false));

				if (SelectedRuleset->MapList[i]->MapScreenshotReference != TEXT(""))
				{
					FString Package = SelectedRuleset->MapList[i]->MapScreenshotReference;
					const int32 Pos = Package.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart);
					if (Pos != INDEX_NONE)
					{
						Package = Package.Left(Pos);
					}

					LoadPackageAsync(Package, FLoadPackageAsyncDelegate::CreateSP(this, &SUTGameSetupDialog::TextureLoadComplete), 0);
				}
			}
		}

	}

	// Build the first panel
	BuildMapPanel();
	OnMapClick(0);
}

void SUTGameSetupDialog::TextureLoadComplete(const FName& InPackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result)
{
	if (Result == EAsyncLoadingResult::Succeeded)
	{
		for (int32 i=0 ;i < MapPlayList.Num(); i++)
		{
			FString Screenshot = MapPlayList[i].MapInfo->MapScreenshotReference;
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

void SUTGameSetupDialog::BuildMapPanel()
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
						.Text(NSLOCTEXT("SUTGameSetupDialog","MapListInstructions","Select Starting Map"))
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
				FString NewToolTip = MapPlayList[i].MapInfo->Description;;

				int32 Pos = INDEX_NONE;
				if (Title.FindChar(TCHAR('-'), Pos))
				{
					if (Pos >=0 && Pos <= 4)
					{
						Title = Title.Right(Title.Len() - (Pos + 1));
					}
				}

				TSharedPtr<SUTComboButton> Button;
				TSharedPtr<SImage> CheckMark;
				TSharedPtr<SImage> ImageWidget;

				MapPanel->AddSlot(Col, Row).Padding(5.0,5.0,5.0,5.0)
				[
					SNew(SBox)
					.WidthOverride(268)
					.HeightOverride(158)							
					[
						SAssignNew(Button, SUTComboButton)
						.OnClicked(this, &SUTGameSetupDialog::OnMapClick, i)
						.IsToggleButton(true)
						.OnButtonSubMenuSelect(this, &SUTGameSetupDialog::OnSubMenuSelect)
						.ButtonStyle(SUWindowsStyle::Get(), "UT.ComplexButton")
						.MenuButtonStyle(SUTStyle::Get(), "UT.ContextMenu.Item")
						.MenuButtonTextStyle(SUTStyle::Get(), "UT.Font.ContextMenuItem")
						.HasDownArrow(false)
						.bRightClickOpensMenu(true)
						.DefaultMenuItems(TEXT("Move To Top,Move Up,Move Down"))
						.MenuPlacement(MenuPlacement_MenuRight)
						.ToolTip(SUTUtils::CreateTooltip(FText::FromString(NewToolTip)))
						.ButtonContent()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot().AutoWidth()
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
										+SOverlay::Slot()
										[
											MapPlayList[i].MapInfo->BuildMapOverlay(FVector2D(256.0f, 128.0f))
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


void SUTGameSetupDialog::OnSubMenuSelect(int32 MenuCmdId, TSharedPtr<SUTComboButton> Sender)
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

void SUTGameSetupDialog::SelectMap(int32 MapIndex)
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
	//if (bHubMenu)
	//{
	//	OnButtonClick(UTDIALOG_BUTTON_OK);
	//}


}

FReply SUTGameSetupDialog::OnMapClick(int32 MapIndex)
{
	if (!MapPlayList[MapIndex].MapInfo->bHasRights)
	{
		DesiredMapIndex = MapIndex;
		MapPlayList[MapIndex].Button->UnPressed();

		PlayerOwner->ShowMessage(
			NSLOCTEXT("SUTGameSetupDialog", "RightsTitle", "Epic Store"), 
			NSLOCTEXT("SUTGameSetupDialog", "RightText", "The map you have selected is available in the Epic Store.  Do you wish to load the store to accquire the map?"), 
			UTDIALOG_BUTTON_YES + UTDIALOG_BUTTON_NO, 
			FDialogResultDelegate::CreateRaw(this, &SUTGameSetupDialog::OnStoreDialogResult));								
	}

	else
	{
		SelectMap(MapIndex);
	}

	return FReply::Handled();
}

void SUTGameSetupDialog::OnStoreDialogResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_YES)
	{
		FString URL = TEXT("com.epicgames.launcher://ut/marketplace");
		FString Command = TEXT("");
		FString Error = TEXT("");
		FPlatformProcess::LaunchURL(*URL, *Command, &Error);
		FPlatformMisc::RequestMinimize();

		PlayerOwner->ShowMessage(
			NSLOCTEXT("SUTGameSetupDialog", "ReturnFromStore", "Returned from store..."), 
			NSLOCTEXT("SUT6GameSetupDialog", "StoreWait", "Click OK when ready."), 
			UTDIALOG_BUTTON_OK, 
			FDialogResultDelegate::CreateRaw(this, &SUTGameSetupDialog::OnStoreReturnResult), FVector2D(0.25f, 0.25f));								
	}
}

void SUTGameSetupDialog::OnStoreReturnResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	// TODO: Refresh entitlements and rebuild rights flags
	MapPlayList[DesiredMapIndex].MapInfo->bHasRights = true;
	BuildMapPanel();
}

FText SUTGameSetupDialog::GetMatchRulesTitle() const
{
	return SelectedRuleset.IsValid() ? FText::FromString(FString::Printf(TEXT("GAME MODE: %s"), *SelectedRuleset.Get()->Title)) : NSLOCTEXT("SUTGameSetupDialog","PickGameMode","Choose your game mode....");
}

FText SUTGameSetupDialog::GetMatchRulesDescription() const
{
	return SelectedRuleset.IsValid() ? FText::FromString(SelectedRuleset.Get()->GetDescription()) : FText::GetEmpty();
}

void SUTGameSetupDialog::ApplyCurrentRuleset(TWeakObjectPtr<AUTLobbyMatchInfo> MatchInfo)
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

void SUTGameSetupDialog::GetCustomGameSettings(FString& GameMode, FString& StartingMap, FString& Description, TArray<FString>&GameOptions, int32& DesiredPlayerCount, int32& bTeamGame)
{
	if (CustomPanel.IsValid())
	{
		CustomPanel->GetCustomGameSettings(GameMode, StartingMap, Description, GameOptions, DesiredPlayerCount, BotSkillLevel, bTeamGame);
	}
}

FString SUTGameSetupDialog::GetSelectedMap()
{
	FString StartingMap = TEXT("");
	for (int32 i=0; i< MapPlayList.Num(); i++ )
	{
		if (MapPlayList[i].bSelected)
		{

			return MapPlayList[i].MapInfo->MapPackageName;
		}
	}

	return TEXT("");
}

void SUTGameSetupDialog::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
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

FReply SUTGameSetupDialog::OnButtonClick(uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_CANCEL && CustomPanel.IsValid())
	{
		CustomPanel->Cancel();		
	}

	return SUTDialogBase::OnButtonClick(ButtonID);

}

TSharedRef<SWidget> SUTGameSetupDialog::BuildBotSkill()
{

	TSharedPtr<SHorizontalBox> Final;
	SAssignNew(Final, SHorizontalBox)
		+SHorizontalBox::Slot().AutoWidth().Padding(0.0f,0.0f,10.0f,0.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Tween")
			.Text(NSLOCTEXT("SUTGameSetupDialog","BotSkillCaption","Bot Skill Level:"))
		];

	TArray<FText> ButtonTexts;
	ButtonTexts.Add(NSLOCTEXT("BotSkillLevels","NoBots","No Bots"));
	ButtonTexts.Add(NSLOCTEXT("BotSkillLevels","Novice","Novice")); 
	ButtonTexts.Add(NSLOCTEXT("BotSkillLevels","Average","Average"));
	ButtonTexts.Add(NSLOCTEXT("BotSkillLevels","Experienced","Experienced"));
	ButtonTexts.Add(NSLOCTEXT("BotSkillLevels","Skilled","Skilled"));
	ButtonTexts.Add(NSLOCTEXT("BotSkillLevels","Adept","Adept"));
	ButtonTexts.Add(NSLOCTEXT("BotSkillLevels","Masterful","Masterful"));
	ButtonTexts.Add(NSLOCTEXT("BotSkillLevels","Inhuman","Inhuman"));
	ButtonTexts.Add(NSLOCTEXT("BotSkillLevels","Godlike","Godlike"));

	TSharedPtr<SUTTabButton> Button;
	BotSkillButtons.Empty();
	for (int32 i=0; i < ButtonTexts.Num(); i++)
	{
		Final->AddSlot().AutoWidth().Padding(10.0f,0.0f,0.0f,0.0f)
		[
			SAssignNew(Button,SUTTabButton)
			.ContentPadding(FMargin(15.0f, 10.0f, 70.0f, 0.0f))
			.Text(ButtonTexts[i])
			.ButtonStyle(SUTStyle::Get(), "UT.OptionButton")
		
			.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Tween.Bold")
			.ClickMethod(EButtonClickMethod::MouseDown)
			.OnClicked(this, &SUTGameSetupDialog::OnBotSkillClick, i)
		];

		BotSkillButtons.Add(Button);
	}

	OnBotSkillClick(3);
	return Final.ToSharedRef();
}


FReply SUTGameSetupDialog::OnBotSkillClick(int32 NewSkill)
{
	BotSkillLevel = NewSkill - 1;
	for (int32 i=0; i < BotSkillButtons.Num(); i++)
	{
		if (i == NewSkill)
		{
			BotSkillButtons[i]->BePressed();
		}
		else
		{
			BotSkillButtons[i]->UnPressed();
		}
	}

	return FReply::Handled();
}

void SUTGameSetupDialog::AddButtonsToLeftOfButtonBar(uint32& ButtonCount)
{
	if (!bHubMenu) return;

	ButtonBar->AddSlot(ButtonCount++, 0)
	.VAlign(VAlign_Center)
	[
		SAssignNew(cbRankLocked, SCheckBox)
		.IsChecked(ECheckBoxState::Unchecked)
		.Style(SUTStyle::Get(), "UT.CheckBox")
		.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTGameSetupDialog","LimitRankTT","If checked, then this match will be limited to people at your rank or lower")))
		.OnCheckStateChanged(this, &SUTGameSetupDialog::RankCheckChanged)
		.Content()
		[
			SNew(STextBlock)
			.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
			.Text(NSLOCTEXT("SULobbySetup", "LimitSkill", "Limit Rank"))
		]
	];

	ButtonBar->AddSlot(ButtonCount++, 0)
	.VAlign(VAlign_Center)
	[
		SAssignNew(cbSpectatable, SCheckBox)
		.IsChecked(ECheckBoxState::Checked)
		.Style(SUTStyle::Get(), "UT.CheckBox")
		.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTGameSetupDialog","AllowSpectTT","If checked, then spectators will be allowed to watch this match.  Uncheck to disable spectating.")))
		.Content()
		[
			SNew(STextBlock)
			.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
			.Text(NSLOCTEXT("SULobbySetup", "AllowSpectators", "Allow Spectating"))
		]
	];

	ButtonBar->AddSlot(ButtonCount++, 0)
	.VAlign(VAlign_Center)
	[
		SAssignNew(cbPrivateMatch, SCheckBox)
		.IsChecked(ECheckBoxState::Unchecked)
		.Style(SUTStyle::Get(), "UT.CheckBox")
		.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTGameSetupDialog","PrivateTT","If checked only friends that you personally invite in to this match will be able to join it.")))
		.Content()
		[
			SNew(STextBlock)
			.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
			.Text(NSLOCTEXT("SULobbySetup", "FriendsOnly", "Private Match"))
		]
	];

}

void SUTGameSetupDialog::ConfigureMatchInfo(TWeakObjectPtr<AUTLobbyMatchInfo> MatchInfo)
{

	bool bRankLocked = cbRankLocked.IsValid() ? cbRankLocked->IsChecked() : true;
	bool bSpectatable = cbSpectatable.IsValid() ? cbSpectatable->IsChecked() : true;
	bool bPrivateMatch = cbPrivateMatch.IsValid() ? cbPrivateMatch->IsChecked() : false;

	if ( IsCustomSettings() )
	{
		FString GameMode;
		FString StartingMap;
		FString Description;
		TArray<FString> GameOptions;

		int32 DesiredPlayerCount = 0;
		int32 bTeamGame = 0;
		GetCustomGameSettings(GameMode, StartingMap, Description, GameOptions, DesiredPlayerCount, bTeamGame);
		MatchInfo->ServerCreateCustomRule(GameMode, StartingMap, Description, GameOptions, BotSkillLevel, DesiredPlayerCount, bTeamGame != 0, bRankLocked, bSpectatable, bPrivateMatch, bBeginnerMatch);
	}

	else if (SelectedRuleset.IsValid())
	{
		bool bIsInParty = false;
		UPartyContext* PartyContext = Cast<UPartyContext>(UBlueprintContextLibrary::GetContext(PlayerOwner->GetWorld(), UPartyContext::StaticClass()));
		if (PartyContext)
		{
			bIsInParty = PartyContext->GetPartySize() > 1;
		}

		FString StartingMap = GetSelectedMap();
		MatchInfo->ServerSetRules(SelectedRuleset->UniqueTag, StartingMap, BotSkillLevel, bIsInParty, bRankLocked, bSpectatable, bPrivateMatch, bBeginnerMatch);
	}

}

void SUTGameSetupDialog::RankCheckChanged(ECheckBoxState NewState)
{
	if (NewState == ECheckBoxState::Unchecked) bUserTurnedOffRankCheck = true;
}

#endif
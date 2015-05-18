// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "SWidgetSwitcher.h"
#include "SUWDialog.h"
#include "Widgets/SUTTabButton.h"
#include "Widgets/SUTComboButton.h"
#include "UTLobbyMatchInfo.h"
#include "UTAudioSettings.h"
#include "UTLevelSummary.h"
#include "UTReplicatedGameRuleset.h"
#include "Panels/SUWCreateGamePanel.h"


#if !UE_SERVER

struct FTabButtonInfo
{
	TSharedPtr<SUTTabButton> Button;
	FName Category;

	FTabButtonInfo()
	{
		Button.Reset();
		Category = NAME_None;
	}

	FTabButtonInfo(TSharedPtr<SUTTabButton> InButton, FName InCategory)
	{
		Button = InButton;
		Category = InCategory;
	}
};

struct FMapPlayListInfo
{
	FString MapName;
	TSharedPtr<FSlateDynamicImageBrush> MapImage;
	TWeakObjectPtr<UUTLevelSummary> LevelSummary;
	TSharedPtr<SUTComboButton> Button;
	TSharedPtr<SImage> CheckMark;
	bool bSelected;

	FMapPlayListInfo()
	{
		MapName = TEXT("");
		MapImage.Reset();
		LevelSummary.Reset();
		CheckMark.Reset();
		Button.Reset();
		bSelected = false;
	}

	FMapPlayListInfo(FString InMapName, TSharedPtr<FSlateDynamicImageBrush> InMapImage, TWeakObjectPtr<UUTLevelSummary> InLevelSummary, bool bInitiallySelected )
	{
		MapName = InMapName;
		MapImage = InMapImage;
		LevelSummary = InLevelSummary;
		bSelected = bInitiallySelected;
		CheckMark.Reset();
		Button.Reset();
	}

	void SetWidgets(TSharedPtr<SUTComboButton> InButton, TSharedPtr<SImage> InCheckMark)
	{
		Button = InButton;
		CheckMark = InCheckMark;

		if (bSelected) 
		{
			Button->BePressed();
			CheckMark->SetVisibility(EVisibility::Visible);
		}
		else
		{
			Button->UnPressed();
			CheckMark->SetVisibility(EVisibility::Hidden);
		}
	}

};

struct FRuleSubsetInfo
{
	TWeakObjectPtr<class AUTReplicatedGameRuleset> Ruleset;
	TSharedPtr<SUTTabButton> Button;
	bool bSelected;

	FRuleSubsetInfo()
	{
		Ruleset.Reset();
		bSelected = false;
		Button.Reset();
	}

	FRuleSubsetInfo(TWeakObjectPtr<class AUTReplicatedGameRuleset> InRuleset, TSharedPtr<SUTTabButton> InButton, bool bInitiallySelected)
	{
		Ruleset = InRuleset;
		Button = InButton;
		bSelected = bInitiallySelected;
	}

};

class UNREALTOURNAMENT_API SUWGameSetupDialog : public SUWDialog
{
public:
	SLATE_BEGIN_ARGS(SUWGameSetupDialog)
	: _DialogTitle(NSLOCTEXT("SUWGameSetupDialog", "Title", "GAME SETTINGS"))
	, _DialogSize(FVector2D(1700, 1040))
	, _bDialogSizeIsRelative(false)
	, _DialogPosition(FVector2D(0.5f,0.5f))
	, _DialogAnchorPoint(FVector2D(0.5f,0.5f))
	, _ContentPadding(FVector2D(10.0f, 5.0f))
	, _ButtonMask(UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_CANCEL)
	{}
	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)			
	SLATE_ARGUMENT(TArray<TWeakObjectPtr<class AUTReplicatedGameRuleset>>, GameRuleSets)
	SLATE_ARGUMENT(FText, DialogTitle)											
	SLATE_ARGUMENT(FVector2D, DialogSize)										
	SLATE_ARGUMENT(bool, bDialogSizeIsRelative)									
	SLATE_ARGUMENT(FVector2D, DialogPosition)									
	SLATE_ARGUMENT(FVector2D, DialogAnchorPoint)								
	SLATE_ARGUMENT(FVector2D, ContentPadding)									
	SLATE_ARGUMENT(uint16, ButtonMask)
	SLATE_EVENT(FDialogResultDelegate, OnDialogResult)							
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TArray<FMapPlayListInfo> MapPlayList;
	TWeakObjectPtr<class AUTReplicatedGameRuleset> SelectedRuleset;

	void ApplyCurrentRuleset(TWeakObjectPtr<AUTLobbyMatchInfo> MatchInfo);
	int32 BotSkillLevel;

	// Will return true if this settings dialog is on the custom tab.  
	bool IsCustomSettings()
	{
		return CurrentCategory == FName(TEXT("Custom"));
	}

	void GetCustomGameSettings(FString& GameMode, FString& StartingMap, TArray<FString>&GameOptions, int32& DesiredPlayerCount);

protected:

	// Holds the list of categories to create.
	TArray<FName> Categories;
	TArray<FText> CategoryTexts;

	TArray<TWeakObjectPtr<class AUTReplicatedGameRuleset>> GameRulesets;

	// When created from a hub, this will be valid.
	TWeakObjectPtr<class AUTLobbyMatchInfo> TargetMatchInfo;
	
	TSharedPtr<class SSplitter> HorzSplitter;
	TSharedPtr<SHorizontalBox> TabButtonPanel;

	TSharedPtr<SGridPanel> RulesPanel;
	TArray<FTabButtonInfo> Tabs;

	FName CurrentCategory;

	TArray<FRuleSubsetInfo> RuleSubset;

	TSharedPtr<SButton> GameModeButtons;
	TSharedPtr<SButton> MapsButton;

	TSharedPtr<SVerticalBox> MapBox;
	TSharedPtr<SVerticalBox> HideBox;

	TSharedPtr<SUWCreateGamePanel> CustomPanel;

	FText GetMatchRulesTitle() const;
	FText GetMatchRulesDescription() const;

	void BuildCategories();
	void BuildRuleList(FName Category);
	TSharedRef<SWidget> BuildRuleTab(FName Tag);
	FReply OnTabButtonClick(int32 ButtonIndex);
	FReply OnRuleClick(int32 RuleIndex);
	FReply OnMapClick(int32 MapIndex);

	void BuildMapList();
	void BuildMapPanel();

	void OnSubMenuSelect(int32 MenuCmdId, TSharedPtr<SUTComboButton> Sender);

	TSharedPtr<SUTComboButton> BotSkillButton;
	virtual TSharedRef<class SWidget> BuildCustomButtonBar();
	FText GetBotSkillText() const;
	void OnBotMenuSelect(int32 MenuCmdId, TSharedPtr<SUTComboButton> Sender);

};

#endif
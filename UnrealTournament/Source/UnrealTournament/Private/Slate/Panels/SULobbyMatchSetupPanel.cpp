
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"
#include "SUMidGameInfoPanel.h"
#include "SULobbyMatchSetupPanel.h"
#include "UTLobbyMatchInfo.h"


#if !UE_SERVER

void SULobbyMatchSetupPanel::Construct(const FArguments& InArgs)
{
	bIsHost = InArgs._bIsHost;
	MatchInfo = InArgs._MatchInfo;
	PlayerOwner = InArgs._PlayerOwner;

	OnMatchInfoGameModeChangedDelegate.BindSP(this, &SULobbyMatchSetupPanel::OnMatchGameModeChanged); 
	MatchInfo->OnMatchGameModeChanged = OnMatchInfoGameModeChangedDelegate;

	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(0.0f,0.0f,10.0f,0.0f)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("SULobbySetup","GameMode","GAME MODE:"))
				.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
			]
			+SHorizontalBox::Slot()
			.Padding(0.0f,0.0f,10.0f,0.0f)
			.AutoWidth()
			[
				BuildGameModeWidget()
			]			
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Fill)
		[
			SAssignNew(SettingsPanel, SVerticalBox)
		]
	];

	OnGameModeChanged(CurrentGameModeData, ESelectInfo::Direct);
}

TSharedRef<SWidget> SULobbyMatchSetupPanel::BuildGameModeWidget()
{
	if (bIsHost)
	{

		FString FirstDisplayText = TEXT("None");
		if (MatchInfo.Get()->HostAvailbleGameModes.Num() > 0)
		{
			CurrentGameModeData =  MatchInfo.Get()->HostAvailbleGameModes[0];
			FirstDisplayText = MatchInfo.Get()->HostAvailbleGameModes[0]->DisplayName;
		}

		return SNew(SBox)
		.WidthOverride(300)
		[
			SNew(SComboBox< TSharedPtr<FAllowedGameModeData> >)
			.Method(SMenuAnchor::UseCurrentWindow)
			.InitiallySelectedItem(CurrentGameModeData)
			.ComboBoxStyle(SUWindowsStyle::Get(), "UWindows.Standard.ComboBox")
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
			.OptionsSource(&MatchInfo.Get()->HostAvailbleGameModes)
			.OnGenerateWidget(this, &SULobbyMatchSetupPanel::GenerateGameModeListWidget)
			.OnSelectionChanged(this, &SULobbyMatchSetupPanel::OnGameModeChanged)
			.Content()
			[
				SAssignNew(CurrentGameMode, STextBlock)
				.Text(FText::FromString(FirstDisplayText))
				.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.Options.TextStyle")
			]
		];
	}
	else
	{
		return SNew(STextBlock)
		.Text(this, &SULobbyMatchSetupPanel::GetGameModeText)
		.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle");
	}
}

FString SULobbyMatchSetupPanel::GetGameModeText() const
{
	return GameModeDisplayName;
}

TSharedRef<SWidget> SULobbyMatchSetupPanel::GenerateGameModeListWidget(TSharedPtr<FAllowedGameModeData> InItem)
{
	return SNew(SBox)
		.Padding(5)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FLinearColor::Black)
			.Text(FText::FromString(InItem.Get()->DisplayName))
		];
}

void SULobbyMatchSetupPanel::OnGameModeChanged(TSharedPtr<FAllowedGameModeData> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (bIsHost)
	{
		if ( NewSelection->DefaultObject.IsValid() )
		{
			MatchInfo->SetMatchGameMode(NewSelection->ClassName);
			ChangeGameModePanel(NewSelection->DefaultObject.Get());
		}
	}
	else
	{
		OnMatchGameModeChanged();
	}
}

void SULobbyMatchSetupPanel::ChangeGameModePanel(AUTGameMode* DefaultGameMode)
{
	if (SettingsPanel.IsValid() && DefaultGameMode)
	{
		SettingsPanel->ClearChildren();
		SettingsPanel->AddSlot()
			.HAlign(HAlign_Fill)
			.AutoHeight()
			[
				DefaultGameMode->CreateLobbyPanel(bIsHost, PlayerOwner, MatchInfo)
			];
	}
}

void SULobbyMatchSetupPanel::OnMatchGameModeChanged()
{
	if (!bIsHost && MatchInfo.IsValid())
	{
		AUTGameMode* DefaultGame = MatchInfo->GetGameModeDefaultObject(MatchInfo->MatchGameMode);
		if (DefaultGame)	
		{
			GameModeDisplayName = DefaultGame->DisplayName.ToString();
			ChangeGameModePanel(DefaultGame);
		}
	}
}

#endif
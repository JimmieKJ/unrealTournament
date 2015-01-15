
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

	AUTLobbyGameState* LobbyGameState = GWorld->GetGameState<AUTLobbyGameState>();
	MatchInfo->CurrentGameModeData = LobbyGameState->ResolveGameMode(MatchInfo->MatchGameMode);

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
			+SHorizontalBox::Slot()
			.Padding(0.0f,10.0f, 10.0f, 0.0f)
			.AutoWidth()
			[
				BuildHostOptionWidgets()
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Fill)
		[
			SAssignNew(GamePanel, SVerticalBox)
		]
	];

	// Trigger the first build.
	ChangeGameModePanel();

}

TSharedRef<SWidget> SULobbyMatchSetupPanel::BuildGameModeWidget()
{
	AUTLobbyGameState* LobbyGameState = GWorld->GetGameState<AUTLobbyGameState>();

	if (LobbyGameState && bIsHost)
	{
		return SNew(SBox)
		.WidthOverride(300)
		[
			SNew(SComboBox< TSharedPtr<FAllowedGameModeData> >)
			.InitiallySelectedItem(MatchInfo.Get()->CurrentGameModeData)
			.ComboBoxStyle(SUWindowsStyle::Get(), "UWindows.Standard.ComboBox")
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
			.OptionsSource(&LobbyGameState->ClientAvailbleGameModes)
			.OnGenerateWidget(this, &SULobbyMatchSetupPanel::GenerateGameModeListWidget)
			.OnSelectionChanged(this, &SULobbyMatchSetupPanel::OnGameModeChanged)
			.Content()
			[
				SAssignNew(CurrentGameMode, STextBlock)
				//.Text(FText::FromString(MatchInfo.Get()->CurrentGameModeData->DisplayName))
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

TSharedRef<SWidget> SULobbyMatchSetupPanel::BuildHostOptionWidgets()
{
	TSharedPtr<SHorizontalBox> Container;
	SAssignNew(Container, SHorizontalBox);

	if (Container.IsValid())
	{
		Container->AddSlot()
		.AutoWidth()
		.Padding(0.0,0.0,0.0,10.0)
		[
			SNew(SCheckBox)
			.IsChecked( (MatchInfo->CurrentGameModeData->DefaultObject->bLobbyDefaultsToAllowJoinInProgress ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked))
			//.OnCheckStateChanged(this, &FGameOption::CheckBoxChanged)
			.Content()
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
				.Text(NSLOCTEXT("SULobbySetup", "AllowJoininProgress", "Allow Join-In-Progress").ToString())
			]
		];

		Container->AddSlot()
		.AutoWidth()
		[
			SNew(SBox)
		];
	}

	return Container.ToSharedRef();
}

FString SULobbyMatchSetupPanel::GetGameModeText() const
{

	return (MatchInfo->CurrentGameModeData.IsValid() ? MatchInfo->CurrentGameModeData->DisplayName : TEXT("None"));
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
	AUTLobbyGameState* LobbyGameState = GWorld->GetGameState<AUTLobbyGameState>();
	if (LobbyGameState)
	{
		MatchInfo->MatchGameMode = NewSelection->ClassName;
		MatchInfo->ServerMatchGameModeChanged(MatchInfo->MatchGameMode);

		MatchInfo->CurrentGameModeData = LobbyGameState->ResolveGameMode(MatchInfo->MatchGameMode);
		// Reseed the local match info.
	
		MatchInfo->UpdateGameMode();
		ChangeGameModePanel();
	}
}


void SULobbyMatchSetupPanel::ChangeGameModePanel()
{

	AUTLobbyGameState* LobbyGameState = GWorld->GetGameState<AUTLobbyGameState>();
	if (LobbyGameState)
	{
		if (GamePanel.IsValid())
		{
			MatchInfo->BuildAllowedMapsList();
			if (bIsHost)
			{
				MatchInfo->ClientGetDefaultGameOptions();
			}

			if (CurrentGameMode.IsValid())
			{
				CurrentGameMode->SetText(MatchInfo->CurrentGameModeData->DisplayName);
			}

			GamePanel->ClearChildren();
			GamePanel->AddSlot()
				.HAlign(HAlign_Fill)
				.AutoHeight()
				[
					SNew(SULobbyGameSettingsPanel)
					.PlayerOwner(PlayerOwner)
					.MatchInfo(MatchInfo)
					.DefaultGameMode(MatchInfo.Get()->CurrentGameModeData->DefaultObject)
					.bIsHost(bIsHost)
				];
		}
	}
}


void SULobbyMatchSetupPanel::OnMatchGameModeChanged()
{
	if (!bIsHost && MatchInfo.IsValid())
	{
		ChangeGameModePanel();
	}
}

#endif
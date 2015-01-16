
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
		.Padding(0.0, 0.0, 0.0, 10.0)
		[
			SNew(SCheckBox)
			.IsChecked((MatchInfo->bJoinAnytime ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked))
			.OnCheckStateChanged(this, &SULobbyMatchSetupPanel::JoinAnyTimeChanged)
			.Content()
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.Dialog.TextStyle")
				.Text(NSLOCTEXT("SULobbySetup", "AllowJoininProgress", "Allow Join-In-Progress").ToString())
			]
		];

		float MaxPlayerValue = (float(MatchInfo->MaxPlayers) - 2.0f) / 30.0f;

		Container->AddSlot()
		.AutoWidth()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.Dialog.TextStyle")
				.Text(this, &SULobbyMatchSetupPanel::GetMaxPlayerLabel)
			]
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				SNew(SBox)
				.WidthOverride(100.0f)
				.Content()
				[
					SNew(SSlider)
					.Orientation(Orient_Horizontal)
					.Value(MaxPlayerValue)
					.OnValueChanged(this, &SULobbyMatchSetupPanel::OnMaxPlayersChanged)
				]
			]
		];
	}

	return Container.ToSharedRef();
}

FText SULobbyMatchSetupPanel::GetMaxPlayerLabel() const
{
	return FText::Format(NSLOCTEXT("LobbyMatchPanel","MaxPlayerFormat","Max Players: {0}  "), FText::AsNumber(MatchInfo->MaxPlayers));
}

void SULobbyMatchSetupPanel::OnMaxPlayersChanged(float NewValue)
{
	float NoPlayers = (30.0f * NewValue) + 2.0f;
	MatchInfo->SetMaxPlayers(int32(NoPlayers));
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

void SULobbyMatchSetupPanel::JoinAnyTimeChanged(ECheckBoxState NewState)
{
	if (MatchInfo.IsValid())
	{
		MatchInfo->bJoinAnytime = NewState == ESlateCheckBoxState::Checked;
	}
}

#endif
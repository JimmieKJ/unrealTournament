
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"
#include "SUMidGameInfoPanel.h"
#include "SULobbyGameSettingsPanel.h"
#include "../Widgets/SUTComboButton.h"
#include "UTLobbyPC.h"


#if !UE_SERVER

void SULobbyGameSettingsPanel::Construct(const FArguments& InArgs)
{
	bIsHost = InArgs._bIsHost;
	MatchInfo = InArgs._MatchInfo;
	PlayerOwner = InArgs._PlayerOwner;

	BlinkyTimer = 0.0f;
	Dots=0;

	OnMatchOptionsChangedDelegate.BindSP(this, &SULobbyGameSettingsPanel::RefreshOptions);
	MatchInfo->OnMatchOptionsChanged = OnMatchOptionsChangedDelegate;

	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SVerticalBox)
		// Settings panel

		+SVerticalBox::Slot()
		.Padding(5.0f, 5.0f, 5.0f, 5.0f)
		.AutoHeight()
		[
			SAssignNew(PanelContents, SOverlay)
			+SOverlay::Slot()
			[
				ConstructContents()
			]
		]

		+SVerticalBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 0.0f)
		.AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(24)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SImage)
					.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.DarkBackground"))
				]

				+SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("LobbyMessages","PlayersInMatch","- Players In Match -"))
					.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.SmallButton.TextStyle")
				]
			]
		]

		// Player List

		+SVerticalBox::Slot()
		.Padding(5.0f, 5.0f, 5.0f, 0.0f)
		.AutoHeight()
		[
			SNew(SScrollBox)
			.Orientation(Orient_Horizontal)
			+SScrollBox::Slot()
			[
				SAssignNew(PlayerListBox, SHorizontalBox)
			]
		]
		+SVerticalBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 0.0f)
		.AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(48)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SImage)
					.Image(SUWindowsStyle::Get().GetBrush("UWindows.Lobby.MatchBar.Background"))
				]

				+SOverlay::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.Padding(5.0,0.0,5.0,0.0)
					[
						SAssignNew(StatusText,STextBlock)
						.Text(this, &SULobbyGameSettingsPanel::GetMatchMessage )
						.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.SmallButton.TextStyle")
					]
				]


				+SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.HeightOverride(32)
					[
						SNew(SButton)
						.OnClicked(this, &SULobbyGameSettingsPanel::Ready)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Lobby.MatchBar.Button")
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.Padding(5.0,0.0,5.0,0.0)
							[
								SNew(STextBlock)
								.Text(this, &SULobbyGameSettingsPanel::GetReadyButtonText)
								.TextStyle(SUWindowsStyle::Get(), "UWindows.Lobby.MatchBar.Button.TextStyle")
							]
						]
					]
				]
			]
		]
	];

	RefreshOptions();
}

void SULobbyGameSettingsPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	BuildPlayerList(InDeltaTime);

	if (MatchInfo->CurrentState == ELobbyMatchState::Launching)
	{
		BlinkyTimer += InDeltaTime;
		if (BlinkyTimer > 0.25)
		{
			Dots++;
			if (Dots > 5) Dots = 0;
			BlinkyTimer = 0.0f;
		}
	}

}

void SULobbyGameSettingsPanel::BuildPlayerList(float DeltaTime)
{
	if (PlayerListBox.IsValid() && MatchInfo.IsValid())
	{
		FString MenuOptions = bIsHost ? TEXT("Kick,Ban") : TEXT("");

		// Find any players who are no longer in the match list.
		for (int32 i=0; i < PlayerData.Num(); i++)
		{
			if (PlayerData[i].IsValid())
			{
				int32 Idx = (PlayerData[i]->PlayerState.IsValid()) ? MatchInfo->Players.Find( PlayerData[i]->PlayerState.Get() ) : INDEX_NONE;
				if (Idx == INDEX_NONE )
				
				{
					// Not found, remove this one.
					PlayerListBox->RemoveSlot(PlayerData[i]->Button.ToSharedRef());
					PlayerData.RemoveAt(i);
					i--;
				}
			}
		}

		// Find any players who are no longer in the match list.
		for (int32 i=0; i < MatchInfo->Players.Num(); i++)
		{
			if (MatchInfo->Players[i])
			{
				bool bFound = false;
				for (int32 j=0; j<PlayerData.Num(); j++)
				{
					if (MatchInfo->Players[i] == PlayerData[j]->PlayerState.Get())
					{
						bFound = true;
						break;
					}
				}

				if (!bFound)
				{
					TSharedPtr<SWidget> Button;
					TSharedPtr<SUTComboButton> ComboButton;
					TSharedPtr<SImage> ReadyImage;
					PlayerListBox->AddSlot()
					[
						SAssignNew(Button, SBox)
						.WidthOverride(125)
						.HeightOverride(160)
						[
							SAssignNew(ComboButton, SUTComboButton)
							.ButtonStyle(SUWindowsStyle::Get(),"UWindows.Lobby.MatchBar.Button")
							.MenuButtonStyle(SUWindowsStyle::Get(),"UWindows.Standard.MenuList")
							.MenuButtonTextStyle(SUWindowsStyle::Get(),"UWindows.Standard.ContentText")
							.HasDownArrow(false)
							.OnButtonSubMenuSelect(this, &SULobbyGameSettingsPanel::OnSubMenuSelect)
							.bRightClickOpensMenu(true)
							.MenuPlacement(MenuPlacement_MenuRight)
							.Method(SMenuAnchor::UseCurrentWindow)
							.DefaultMenuItems(MenuOptions)
							.ButtonContent()
							[
								SNew(SVerticalBox)

								// The player's Image - TODO: We need a way to download the portrait of a given player 
								+SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0,5.0,0.0,0.0)
								.HAlign(HAlign_Center)
								[
									SNew(SBox)
									.WidthOverride(102)
									.HeightOverride(128)							
									[
										SNew(SOverlay)
										+SOverlay::Slot()
										[
											SNew(SImage)
											.Image(SUWindowsStyle::Get().GetBrush("Testing.TestPortrait"))
										]
										+SOverlay::Slot()
										[
											SAssignNew(ReadyImage, SImage)
											.Image(SUWindowsStyle::Get().GetBrush("UWindows.Match.ReadyImage"))
											.Visibility(EVisibility::Hidden)
										]
									]
								]
								+SVerticalBox::Slot()
								.Padding(0.0,0.0,0.0,5.0)
								.AutoHeight()
								.HAlign(HAlign_Center)
								[
									SNew(STextBlock)
									.Text(MatchInfo->Players[i]->PlayerName)
									.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.SmallButton.TextStyle")
								]

							]
						]
					];

					PlayerData.Add( FMatchPlayerData::Make(MatchInfo->Players[i],Button, ComboButton, ReadyImage));
				}
			}
		}
	}

	for (int32 i=0; i< PlayerData.Num(); i++)
	{
		AUTLobbyPlayerState* PS = PlayerData[i].Get()->PlayerState.Get();	
		if (PS)
		{
			PlayerData[i].Get()->ComboButton->SetButtonStyle( !PS->bReadyToPlay ? &SUWindowsStyle::Get().GetWidgetStyle<FButtonStyle>("UWindows.Match.PlayerButton") : &SUWindowsStyle::Get().GetWidgetStyle<FButtonStyle>("UWindows.Match.ReadyPlayerButton") );
			PlayerData[i].Get()->ReadyImage->SetVisibility( PS->bReadyToPlay ? EVisibility::Visible : EVisibility::Hidden);
		}
	}

}

TSharedRef<SWidget> SULobbyGameSettingsPanel::ConstructContents()
{
	return SNew(SCanvas);
}

void SULobbyGameSettingsPanel::RefreshOptions()
{
}

void SULobbyGameSettingsPanel::CommitOptions()
{
}

FReply SULobbyGameSettingsPanel::Ready()
{

	if (bIsHost || MatchInfo->CurrentState == ELobbyMatchState::WaitingForPlayers)
	{
		if (PlayerOwner.IsValid() && PlayerOwner->PlayerController)
		{
			AUTLobbyPC* PC= Cast<AUTLobbyPC>(PlayerOwner->PlayerController);

			if (PC)
			{
				if (bIsHost)
				{

					// Look to see if everyone is ready....
					bool bAllReady = true;
					if (PlayerOwner.IsValid() && PlayerOwner->PlayerController && PlayerOwner->PlayerController->PlayerState)
					{
						AUTLobbyPlayerState* PS = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
						if (PS)
						{
							for (int32 i=0; i < MatchInfo->Players.Num(); i++)
							{
								if (MatchInfo->Players[i] && MatchInfo->Players[i] != PS && !MatchInfo->Players[i]->bReadyToPlay)
								{
									bAllReady = false;
									break;
								}
							}
						}
					}

					if (!bAllReady)
					{
						PlayerOwner->ShowMessage(NSLOCTEXT("LobbyMessages","NotEveryoneReadyCaption","Not Ready"),NSLOCTEXT("LobbyMessages","NotEveryoneReadyMsg","Everone isn't ready to play.  Please wait for everyone before starting the match!"), UTDIALOG_BUTTON_OK);
						return FReply::Handled();
					}

					if (MatchInfo->CurrentState == ELobbyMatchState::WaitingForPlayers)
					{
						MatchInfo->ServerStartMatch();	
					}
					else
					{
						MatchInfo->ServerAbortMatch();
					}
				}
				else
				{
					PC->ServerSetReady(!PC->UTLobbyPlayerState->bReadyToPlay);
				}
			}
		}
	}


	return FReply::Handled();
}

FText SULobbyGameSettingsPanel::GetReadyButtonText() const
{
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController && PlayerOwner->PlayerController->PlayerState)
	{
		AUTLobbyPlayerState* PS = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
		if (PS)
		{
			if (bIsHost)
			{
				return !PS->bReadyToPlay ? NSLOCTEXT("Gerneric","StartMatch","Start Match") : NSLOCTEXT("Gerneric","AbortMatch","Abort Match");
			}

			return !PS->bReadyToPlay ? NSLOCTEXT("Gerneric","Ready","Ready") : NSLOCTEXT("Gerneric","NotReady","Not Ready");
		}
	}
	return NSLOCTEXT("Gerneric","NotReady","Not Ready");
}

void SULobbyGameSettingsPanel::OnSubMenuSelect(int32 MenuCmdId, TSharedPtr<SUTComboButton> Sender)
{
	if (bIsHost)
	{
		for (int32 i=0; i<PlayerData.Num(); i++)
		{
			if (PlayerData[i]->ComboButton.Get() == Sender.Get())
			{
				MatchInfo->ServerManageUser(MenuCmdId, PlayerData[i]->PlayerState.Get());
			}
		}
	}
}

FText SULobbyGameSettingsPanel::GetMatchMessage() const
{
	if (MatchInfo->CurrentState == ELobbyMatchState::WaitingForPlayers)
	{
		if (bIsHost)
		{
			return NSLOCTEXT("LobbyMessages","WaitingForPlayers","Setup your match and press PLAY when ready.");
		}

		return NSLOCTEXT("LobbyMessages","WaitingForHost","Waiting for the host to start...");
	}
	else if (MatchInfo->CurrentState == ELobbyMatchState::Launching)
	{
		FString Text = TEXT("");
		for (int32 i=0;i<Dots;i++)
		{
			Text = Text + TEXT(".");
		}

		return FText::Format(NSLOCTEXT("LobbyMessages","Launching","Launching Match, please wait..{0}"), FText::FromString(Text));
	}
	else if (MatchInfo->CurrentState == ELobbyMatchState::Aborting)
	{
		return NSLOCTEXT("LobbyMessages","Aborting","Aborting... please wait!");
	}

	return NSLOCTEXT("LobbyMessages","Setup","Initializing....");
}

#endif
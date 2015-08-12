
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "SlateExtras.h"
#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"
#include "../SUTStyle.h"
#include "SULobbyInfoPanel.h"
#include "UTLobbyPC.h"
#include "Engine/UserInterfaceSettings.h"
#include "../Public/UTLobbyHUD.h"



#if !UE_SERVER

void SULobbyInfoPanel::ConstructPanel(FVector2D ViewportSize)
{
	SUWPanel::ConstructPanel(ViewportSize);

	Tag = FName(TEXT("LobbyInfoPanel"));

	LastMatchState = ELobbyMatchState::Setup;

	AUTLobbyPlayerState* PlayerState = Cast<AUTLobbyPlayerState>(GetOwnerPlayerState());
	bShowingMatchBrowser = false;

	const FSlateBrush* NoBrush = FStyleDefaults::GetNoBrush();

	this->ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SAssignNew(MainOverlay,SOverlay)
		+SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot().FillHeight(1.0)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().FillWidth(1.0)
				[
					SNew(SImage)
					.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.Light"))
				]
			]

		]
		+SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(64.0,15.0,0.0,0.0)
				[
					SNew(SBox).WidthOverride(1792).HeightOverride(980)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0,0.0,6.0,0.0)
						[
							SNew(SBox).WidthOverride(954)
							[
								SAssignNew(LeftPanel,SVerticalBox)
							]
						]
						+SHorizontalBox::Slot()
						.Padding(6.0,0.0,0.0,0.0)
						.AutoWidth()
						[
							SNew(SBox).WidthOverride(826)
							[
								SAssignNew(RightPanel,SVerticalBox)
							]
						]
					]
				]		
			]
		]
	];

	ShowMatchBrowser();
}

void SULobbyInfoPanel::BuildChatAndPlayerList()
{
	if (!RightPanel.IsValid()) return; // Can't build a panel if the container isn't there.  Should never happen.
	
	RightPanel->ClearChildren();

	if (!PlayerListPanel.IsValid())
	{
		SAssignNew(PlayerListPanel, SUPlayerListPanel)
			.PlayerOwner(PlayerOwner)
			.OnPlayerClicked(FPlayerClicked::CreateSP(this, &SULobbyInfoPanel::PlayerClicked));
	}

	if (!TextChatPanel.IsValid())
	{
		SAssignNew(TextChatPanel, SUTextChatPanel)
			.PlayerOwner(PlayerOwner)
			.OnChatDestinationChanged(FChatDestinationChangedDelegate::CreateSP(this, &SULobbyInfoPanel::ChatDestionationChanged));

		if (TextChatPanel.IsValid())
		{
			TextChatPanel->AddDestination(NSLOCTEXT("LobbyChatDestinations","Everyone","Everyone"), ChatDestinations::Local,0.0,true);
			TextChatPanel->RouteBufferedChat();
		}
	}

	// Make sure the two panels are connected.
	if (PlayerListPanel.IsValid() && TextChatPanel.IsValid())
	{
		PlayerListPanel->ConnectedChatPanel = TextChatPanel;
		TextChatPanel->ConnectedPlayerListPanel = PlayerListPanel;
	}

	if (bShowingMatchBrowser)
	{

		if (TextChatPanel.IsValid())
		{
			TextChatPanel->RemoveDestination(ChatDestinations::Match);
		}

		RightPanel->AddSlot()
		.FillHeight(1.0)
		.HAlign(HAlign_Left)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox).WidthOverride(826).HeightOverride(195)
					[
						SNew(SImage)
						.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
					]
				]
			]
			+SVerticalBox::Slot()
			.FillHeight(1.0)
			.Padding(FMargin(0.0f,12.0f,0.0f,0.0f))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox).WidthOverride(826)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.FillWidth(1.0)
						[
							TextChatPanel.ToSharedRef()
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBox).WidthOverride(208)
							[
								PlayerListPanel.ToSharedRef()
							]
						]
					]
				]
			]
		];
	}
	else
	{

		if (TextChatPanel.IsValid())
		{
			TextChatPanel->AddDestination(NSLOCTEXT("LobbyChatDestinations","Match","Match"), ChatDestinations::Match,2.0,true);
		}

		RightPanel->AddSlot()
		.FillHeight(1.0)
		.HAlign(HAlign_Left)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1.0)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox).WidthOverride(826)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.FillWidth(1.0)
						[
							TextChatPanel.ToSharedRef()
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBox).WidthOverride(208)
							[
								PlayerListPanel.ToSharedRef()
							]
						]
					]
				]
			]
		];
	}

}


void SULobbyInfoPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	AUTLobbyPlayerState* PlayerState = Cast<AUTLobbyPlayerState>(GetOwnerPlayerState());
	if (!PlayerState) return;	// Quick out if we don't have an owner player.

	if (bShowingMatchBrowser)
	{
		// Look to see if we have been waiting for a valid GameModeData to replicate.  If we have, open the right panel.
		if (PlayerState && PlayerState->CurrentMatch && PlayerState->CurrentMatch->MatchIsReadyToJoin(PlayerState))
		{
			ShowMatchSetupPanel();
		}
	}
	else
	{
		// We are showing the Match Setup panel.. see if we should still be showing it...
		
		if (PlayerState->CurrentMatch == NULL || !PlayerState->CurrentMatch->MatchIsReadyToJoin(PlayerState) )
		{
			ShowMatchBrowser();
		}
	}
}


//TODO: Look at resusing the panels.

void SULobbyInfoPanel::ShowMatchSetupPanel()
{
	if (!LeftPanel.IsValid()) return; // Can't build a panel if the container isn't there.  Should never happen.

	AUTLobbyPlayerState* PlayerState = Cast<AUTLobbyPlayerState>(GetOwnerPlayerState());
	if (PlayerState && bShowingMatchBrowser)
	{
		bShowingMatchBrowser = false;
		MatchBrowser.Reset();
		LeftPanel->ClearChildren();

		LeftPanel->AddSlot().AutoHeight().HAlign(HAlign_Left)
		[
			SAssignNew(MatchSetup, SULobbyMatchSetupPanel)
			.PlayerOwner(PlayerOwner)
			.MatchInfo(PlayerState->CurrentMatch)
			.bIsHost(PlayerState->CurrentMatch->GetOwnerPlayerState() == PlayerState)
			.OnRulesetUpdated(FOnRulesetUpdated::CreateSP(this, &SULobbyInfoPanel::RulesetChanged))
		];

		BuildChatAndPlayerList();
	}

}

void SULobbyInfoPanel::ShowMatchBrowser()
{
	if (!LeftPanel.IsValid()) return; // Can't build a panel if the container isn't there.  Should never happen.

	AUTLobbyPlayerState* PlayerState = Cast<AUTLobbyPlayerState>(GetOwnerPlayerState());
	if (PlayerState && !bShowingMatchBrowser)
	{

		// Force remove the Match and Team Tabs if they still exist.
		if (TextChatPanel.IsValid())
		{
			TextChatPanel->RemoveDestination(ChatDestinations::Team);
			TextChatPanel->RemoveDestination(ChatDestinations::Match);
		}

		bShowingMatchBrowser = true;
		MatchSetup.Reset();
		LeftPanel->ClearChildren();

		LeftPanel->AddSlot().AutoHeight().HAlign(HAlign_Left)
		[
			SAssignNew(MatchBrowser, SUMatchPanel)
			.PlayerOwner(PlayerOwner)
		];

		BuildChatAndPlayerList();
	}
}

void SULobbyInfoPanel::ChatDestionationChanged(FName NewDestination)
{
	if (PlayerListPanel.IsValid())
	{
		PlayerListPanel->ForceRefresh();
	}
}

void SULobbyInfoPanel::RulesetChanged()
{
	AUTLobbyPlayerState* LobbyPlayerState = Cast<AUTLobbyPlayerState>(GetOwnerPlayerState());
	if (LobbyPlayerState && LobbyPlayerState->CurrentMatch && LobbyPlayerState->CurrentMatch->CurrentRuleset.IsValid() && LobbyPlayerState->CurrentMatch->CurrentRuleset->bTeamGame)
	{
		TextChatPanel->AddDestination(NSLOCTEXT("LobbyChatDestinations","Team","Team"), ChatDestinations::Team, 4.0, false);
		return;
	}
	
	TextChatPanel->RemoveDestination(ChatDestinations::Team);
	PlayerListPanel->ForceRefresh();
}

void SULobbyInfoPanel::PlayerClicked(FUniqueNetIdRepl PlayerId)
{
	AUTLobbyPC* LobbyPC = Cast<AUTLobbyPC>(PlayerOwner->PlayerController);
	if (LobbyPC && LobbyPC->UTLobbyPlayerState && LobbyPC->UTLobbyPlayerState->CurrentMatch && LobbyPC->UTLobbyPlayerState->UniqueId == PlayerId)
	{
		LobbyPC->ServerSetReady(!LobbyPC->UTLobbyPlayerState->bReadyToPlay);
	}
}

#endif

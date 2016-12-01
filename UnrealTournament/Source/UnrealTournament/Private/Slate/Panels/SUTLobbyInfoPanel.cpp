
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SlateBasics.h"
#include "SlateExtras.h"
#include "Slate/SlateGameResources.h"
#include "SUTLobbyInfoPanel.h"
#include "../SUWindowsStyle.h"
#include "../SUTStyle.h"
#include "UTLobbyPC.h"
#include "Engine/UserInterfaceSettings.h"
#include "UTLobbyHUD.h"
#include "../Dialogs/SUTGameSetupDialog.h"
#include "../UIWindows/SUTStartMatchWindow.h"


#if !UE_SERVER

void SUTLobbyInfoPanel::ConstructPanel(FVector2D ViewportSize)
{
	SUTPanelBase::ConstructPanel(ViewportSize);

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
				.Padding(15.0,15.0,0.0,0.0)
				[
					SNew(SBox).WidthOverride(1888).HeightOverride(980)
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
							SNew(SBox).WidthOverride(922)
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

void SUTLobbyInfoPanel::BuildChatAndPlayerList()
{
	if (!RightPanel.IsValid()) return; // Can't build a panel if the container isn't there.  Should never happen.
	
	RightPanel->ClearChildren();

	if (!PlayerListPanel.IsValid())
	{
		SAssignNew(PlayerListPanel, SUTPlayerListPanel)
			.PlayerOwner(PlayerOwner)
			.OnPlayerClicked(FPlayerClicked::CreateSP(this, &SUTLobbyInfoPanel::PlayerClicked));
	}

	if (!TextChatPanel.IsValid())
	{
		SAssignNew(TextChatPanel, SUTTextChatPanel)
			.PlayerOwner(PlayerOwner)
			.OnChatDestinationChanged(FChatDestinationChangedDelegate::CreateSP(this, &SUTLobbyInfoPanel::ChatDestionationChanged));

		if (TextChatPanel.IsValid())
		{
			TextChatPanel->AddDestination(NSLOCTEXT("LobbyChatDestinations","Global","HUB"), ChatDestinations::Local,0.0,true);
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
	}
	else
	{

		if (TextChatPanel.IsValid())
		{
			TextChatPanel->AddDestination(NSLOCTEXT("LobbyChatDestinations","Match","Match"), ChatDestinations::Match,2.0,true);
		}
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
				SNew(SBox).WidthOverride(922)
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


//TODO: Look at resusing the panels.

void SUTLobbyInfoPanel::ShowMatchSetupPanel()
{
	if (!LeftPanel.IsValid()) return; // Can't build a panel if the container isn't there.  Should never happen.

	AUTLobbyPlayerState* PlayerState = Cast<AUTLobbyPlayerState>(GetOwnerPlayerState());
	if (PlayerState && bShowingMatchBrowser)
	{

		PlayerOwner->bSuppressDownloadDialog = true;

		bShowingMatchBrowser = false;
		MatchBrowser.Reset();
		LeftPanel->ClearChildren();

		LeftPanel->AddSlot().AutoHeight().HAlign(HAlign_Left)
		[
			SAssignNew(MatchSetup, SUTLobbyMatchSetupPanel)
			.PlayerOwner(PlayerOwner)
			.MatchInfo(PlayerState->CurrentMatch)
			.bIsHost(PlayerState->CurrentMatch->GetOwnerPlayerState() == PlayerState)
			.OnRulesetUpdated(FOnRulesetUpdated::CreateSP(this, &SUTLobbyInfoPanel::RulesetChanged))
		];

		BuildChatAndPlayerList();
	}

}

void SUTLobbyInfoPanel::ShowMatchBrowser()
{
	if (!LeftPanel.IsValid()) return; // Can't build a panel if the container isn't there.  Should never happen.

	AUTLobbyPlayerState* PlayerState = Cast<AUTLobbyPlayerState>(GetOwnerPlayerState());
	if (PlayerState && !bShowingMatchBrowser)
	{
		PlayerOwner->bSuppressDownloadDialog = false;

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
			SAssignNew(MatchBrowser, SUTMatchPanel)
			.bExpectLiveData(true)
			.PlayerOwner(PlayerOwner)
			.OnStartMatchDelegate(this, &SUTLobbyInfoPanel::StartMatch)
		];

		BuildChatAndPlayerList();
	}
}

void SUTLobbyInfoPanel::ChatDestionationChanged(FName NewDestination)
{
	if (PlayerListPanel.IsValid())
	{
		PlayerListPanel->ForceRefresh();
	}
}

void SUTLobbyInfoPanel::RulesetChanged()
{
	AUTLobbyPlayerState* LobbyPlayerState = Cast<AUTLobbyPlayerState>(GetOwnerPlayerState());
	PlayerListPanel->ForceRefresh();
}

void SUTLobbyInfoPanel::PlayerClicked(FUniqueNetIdRepl PlayerId)
{
	AUTLobbyPC* LobbyPC = Cast<AUTLobbyPC>(PlayerOwner->PlayerController);
	if (LobbyPC && LobbyPC->UTLobbyPlayerState && LobbyPC->UTLobbyPlayerState->CurrentMatch && LobbyPC->UTLobbyPlayerState->UniqueId == PlayerId)
	{
		LobbyPC->ServerSetReady(!LobbyPC->UTLobbyPlayerState->bReadyToPlay);
	}
}

void SUTLobbyInfoPanel::OnShowPanel(TSharedPtr<SUTMenuBase> inParentWindow)
{
	SUTPanelBase::OnShowPanel(inParentWindow);
	if (TextChatPanel.IsValid()) TextChatPanel->OnShowPanel();
}
void SUTLobbyInfoPanel::OnHidePanel()
{
	PlayerOwner->bSuppressDownloadDialog = false;
	SUTPanelBase::OnHidePanel();
	if (TextChatPanel.IsValid()) TextChatPanel->OnHidePanel();
}


void SUTLobbyInfoPanel::StartMatch()
{
	AUTLobbyGameState* LobbyGameState = GWorld->GetGameState<AUTLobbyGameState>();
	if (PlayerOwner.IsValid() && !SetupDialog.IsValid() && LobbyGameState && LobbyGameState->AvailableGameRulesets.Num() > 0 )
	{
		SAssignNew(SetupDialog, SUTGameSetupDialog)
			.PlayerOwner(PlayerOwner)
			.GameRuleSets(LobbyGameState->AvailableGameRulesets)
			.OnDialogResult(this, &SUTLobbyInfoPanel::OnGameChangeDialogResult);

		if ( SetupDialog.IsValid() )
		{
			PlayerOwner->OpenDialog(SetupDialog.ToSharedRef(), 100);
		}
	}
}

void SUTLobbyInfoPanel::OnGameChangeDialogResult(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonPressed)
{
	if (ButtonPressed == UTDIALOG_BUTTON_OK)
	{
		// Looking to start a match...

		bool bIsInParty = false;
		UPartyContext* PartyContext = Cast<UPartyContext>(UBlueprintContextLibrary::GetContext(PlayerOwner->GetWorld(), UPartyContext::StaticClass()));
		if (PartyContext)
		{
			bIsInParty = PartyContext->GetPartySize() > 1;
		}

		AUTLobbyPlayerState* PlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
		if (PlayerState)
		{
			PlayerState->ServerCreateMatch(bIsInParty);
		}

		SAssignNew(StartMatchWindow, SUTStartMatchWindow, PlayerOwner)
			.bIsHost(true);
		 
		if (StartMatchWindow.IsValid())
		{
			StartMatchWindow->ParentPanel = SharedThis(this);
		}

		PlayerOwner->OpenWindow(StartMatchWindow);
	}
	else
	{
		SetupDialog.Reset();
	}
}

void SUTLobbyInfoPanel::CancelInstance()
{
	AUTLobbyPlayerState* LobbyPlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
	if (LobbyPlayerState)
	{	
		LobbyPlayerState->ServerDestroyOrLeaveMatch();
	}

	if (StartMatchWindow.IsValid())
	{
		PlayerOwner->CloseWindow(StartMatchWindow);
	}
	StartMatchWindow.Reset();
}

void SUTLobbyInfoPanel::ApplySetup(TWeakObjectPtr<AUTLobbyMatchInfo> MatchInfo)
{
	if (SetupDialog.IsValid() && MatchInfo.IsValid())
	{
		SetupDialog->ConfigureMatchInfo(MatchInfo);
		MatchInfo->ServerStartMatch();	
		PlayerOwner->CloseDialog(SetupDialog.ToSharedRef());
		SetupDialog.Reset();
	}
}

#endif


// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"
#include "UTGameEngine.h"
#include "UTLobbyGameState.h"
#include "UTLobbyMatchInfo.h"
#include "SUPlayerListPanel.h"
#include "UTLobbyPC.h"
#include "UTLobbyPlayerState.h"
#include "SUTextChatPanel.h"

#if !UE_SERVER

struct FPlayerCompare 
{
	FORCEINLINE bool operator()( const TSharedPtr< FTrackedPlayer > A, const TSharedPtr< FTrackedPlayer > B ) const 
	{
		if (A->EntryType == ETrackedPlayerType::MatchHeader) return true;
		if (B->EntryType == ETrackedPlayerType::MatchHeader) return false;

		int32 AValue = 0;
		if (A->EntryType == ETrackedPlayerType::EveryoneHeader) 
		{
			AValue = 100;
		}
		else if (A->EntryType == ETrackedPlayerType::InstancePlayer) 
		{
			AValue = 1000;
		}
		else
		{
			AValue = (A->bIsInMatch ? 10 : 150) + (A->bInInstance ? 2000 : 0) + (!A->bIsHost ? -5 : 0);
		}

		int32 BValue = 0;
		if (B->EntryType == ETrackedPlayerType::EveryoneHeader) 
		{
			BValue = 100;
		}
		else if (B->EntryType == ETrackedPlayerType::InstancePlayer) 
		{
			BValue = 1000;
		}
		else
		{
			BValue = (B->bIsInMatch ? 10 : 150) + (B->bInInstance ? 2000 : 0) + (!B->bIsHost ? -5 : 0);
		}

		UE_LOG(UT,Log,TEXT("A = %s (%i) B = %s (%i)"), (A->PlayerName.IsEmpty() ? TEXT("None") : *A->PlayerName), AValue, (B->PlayerName.IsEmpty() ? TEXT("None") : *B->PlayerName), BValue)
			
		return AValue < BValue;
	}
};


void SUPlayerListPanel::Construct(const FArguments& InArgs)
{
	PlayerOwner = InArgs._PlayerOwner;
	PlayerClickedDelegate = InArgs._OnPlayerClicked;

	bNeedsRefresh = false;

	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SOverlay)
		+SOverlay::Slot()
		[
			SNew(SImage)
			.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
		]
		+SOverlay::Slot()
		[
			SAssignNew( PlayerList, SListView< TSharedPtr<FTrackedPlayer> > )
			// List view items are this tall
			.ItemHeight(64)
			// Tell the list view where to get its source data
			.ListItemsSource( &TrackedPlayers)
			// When the list view needs to generate a widget for some data item, use this method
			.OnGenerateRow( this, &SUPlayerListPanel::OnGenerateWidgetForPlayerList)
			//.OnContextMenuOpening( this, &SUPlayerListPanel::OnGetContextMenuContent )
			//.OnMouseButtonDoubleClick(this, &SUPlayerListPanel::OnListSelect)
			.SelectionMode(ESelectionMode::Single)
		]
	];
}

void SUPlayerListPanel::CheckFlags(bool &bIsHost, bool &bIsTeamGame)
{
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController && PlayerOwner->PlayerController->PlayerState)
	{
		AUTLobbyPlayerState* LobbyPlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
		if (LobbyPlayerState)
		{
			bIsHost = (LobbyPlayerState->CurrentMatch && LobbyPlayerState->CurrentMatch->OwnerId == LobbyPlayerState->UniqueId);
			bIsTeamGame = (LobbyPlayerState->CurrentMatch && LobbyPlayerState->CurrentMatch->CurrentRuleset.IsValid() && LobbyPlayerState->CurrentMatch->CurrentRuleset->bTeamGame);
			return;
		}
	}

	bIsHost = false;
	AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	bIsTeamGame = (GameState && GameState->bTeamGame);
}


TSharedRef<ITableRow> SUPlayerListPanel::OnGenerateWidgetForPlayerList( TSharedPtr<FTrackedPlayer> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	// TODO: Add support for multiple avatars

	if (InItem->EntryType != ETrackedPlayerType::Player)
	{
		return SNew(STableRow<TSharedPtr<FTrackedPlayer>>, OwnerTable).ShowSelection(false)
			.Style(SUTStyle::Get(),"UT.PlayerList.Row")
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot().AutoHeight().VAlign(VAlign_Center).HAlign(HAlign_Center).Padding(0.0,5.0,0.0,15.0)
				[
					SNew(STextBlock)
						.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(InItem.Get(), &FTrackedPlayer::GetPlayerName)))
						.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tiny.Bold")
						.ColorAndOpacity(FLinearColor(0.3,0.3,0.3,1.0))
				]
			];
	}
	else
	{
		return SNew(STableRow<TSharedPtr<FTrackedPlayer>>, OwnerTable)
		.Style(SUTStyle::Get(),"UT.PlayerList.Row")
		[
			SNew(SUTMenuAnchor)
			.MenuButtonStyle(SUTStyle::Get(),"UT.SimpleButton.Dark")
			.MenuButtonTextStyle(SUTStyle::Get(),"UT.Font.NormalText.Small")
			.OnGetSubMenuOptions(this, &SUPlayerListPanel::GetMenuContent)
			.OnClicked(this, &SUPlayerListPanel::OnListSelect, InItem)
			.SearchTag(InItem->PlayerID.ToString())
			.OnSubMenuSelect(FUTSubMenuSelect::CreateSP(this, &SUPlayerListPanel::OnSubMenuSelect, InItem))
			.MenuPlacement(MenuPlacement_MenuRight)
			.ButtonContent()
			[			
				SNew(SVerticalBox)
				+SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot().AutoWidth().Padding(FMargin(5.0f ,0.0f ,0.0f ,0.0f))
					[
						SNew(SBox).WidthOverride(64).HeightOverride(64)
						[
							SNew(SImage)
							.Image(SUTStyle::Get().GetBrush("UT.Avatar.0"))
						]
					]
					+SHorizontalBox::Slot().Padding(FMargin(5.0f ,0.0f ,0.0f ,0.0f)).VAlign(VAlign_Top).FillWidth(1.0)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot().Padding(FMargin(0.0f,5.0f,0.0f,0.0f))
						[
							SNew(STextBlock)
							.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(InItem.Get(), &FTrackedPlayer::GetPlayerName)))
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
							.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(InItem.Get(), &FTrackedPlayer::GetNameColor)))
						]
						+SVerticalBox::Slot().Padding(FMargin(0.0f,5.0f,0.0f,0.0f))
						[
							SNew(STextBlock)
							.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(InItem.Get(), &FTrackedPlayer::GetLobbyStatusText)))
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tiny.Bold")
						]
					]
				]
			]
		];
	}
}

void SUPlayerListPanel::GetMenuContent(FString SearchTag, TArray<FMenuOptionData>& MenuOptions)
{
	int32 Idx = INDEX_NONE;
	for (int32 i=0; i < TrackedPlayers.Num(); i++)
	{
		if (SearchTag != TEXT("") && TrackedPlayers[i].IsValid() && TrackedPlayers[i]->EntryType == ETrackedPlayerType::Player && TrackedPlayers[i]->PlayerID.ToString() == SearchTag)
		{
			Idx = i;
			break;
		}
	}

	if (Idx == INDEX_NONE) return;

	MenuOptions.Empty();

	if (!TrackedPlayers[Idx]->bInInstance)
	{
		MenuOptions.Add(FMenuOptionData(NSLOCTEXT("PlayerListSubMenu","ShowPlayerCard","Player Card"), EPlayerListContentCommand::PlayerCard));
	}

	bool bIsHost = false;
	bool bTeamGame = false;
	CheckFlags(bIsHost, bTeamGame);

	if (bIsHost || (PlayerOwner.IsValid() && PlayerOwner->PlayerController && TrackedPlayers[Idx]->PlayerState.Get() == PlayerOwner->PlayerController->PlayerState))
	{
		if (TrackedPlayers[Idx]->bIsInMatch)
		{
			if (!TrackedPlayers[Idx]->bInInstance)
			{
				MenuOptions.Add(FMenuOptionData());
			}

			// Look to see if this player is in a team game...

			if (bTeamGame)
			{
				MenuOptions.Add(FMenuOptionData(NSLOCTEXT("PlayerListSubMenu","ChangeTeam","Change Team"), EPlayerListContentCommand::ChangeTeam));
			}

			MenuOptions.Add(FMenuOptionData(NSLOCTEXT("PlayerListSubMenu","Spectate","Spectate"), EPlayerListContentCommand::Spectate));

			if (bIsHost)
			{
				MenuOptions.Add(FMenuOptionData());
				MenuOptions.Add(FMenuOptionData(NSLOCTEXT("PlayerListSubMenu","Kick","Kick"), EPlayerListContentCommand::Kick));
				MenuOptions.Add(FMenuOptionData(NSLOCTEXT("PlayerListSubMenu","Ban","Ban"), EPlayerListContentCommand::Ban));
			}
		}
	}
}

int32 SUPlayerListPanel::IsTracked(const FUniqueNetIdRepl& PlayerID)
{
	for (int32 i = 0; i < TrackedPlayers.Num(); i++)
	{
		if (TrackedPlayers[i]->PlayerID == PlayerID)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

bool SUPlayerListPanel::IsInMatch(AUTPlayerState* PlayerState)
{
	AUTLobbyPlayerState* LobbyPlayerState = Cast<AUTLobbyPlayerState>(PlayerState);
	if (LobbyPlayerState && PlayerOwner.IsValid() && PlayerOwner->PlayerController && PlayerOwner->PlayerController->PlayerState )
	{
		AUTLobbyPlayerState* OwnerLobbyPlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
		if (OwnerLobbyPlayerState && OwnerLobbyPlayerState->CurrentMatch && OwnerLobbyPlayerState->CurrentMatch == LobbyPlayerState->CurrentMatch)
		{
			return true;
		}

		return false;
	}

	return true;
}

bool SUPlayerListPanel::ShouldShowPlayer(FUniqueNetIdRepl PlayerId, uint8 TeamNum, bool bIsInMatch)
{
	if (ConnectedChatPanel.IsValid())
	{
		if (ConnectedChatPanel->CurrentChatDestination == ChatDestinations::Friends)
		{
			// TODO: Look up the friends list and see if this player is on it
			return false;		
		}
		else if (ConnectedChatPanel->CurrentChatDestination == ChatDestinations::Team)
		{
			if (PlayerOwner.IsValid() && PlayerOwner->PlayerController && PlayerOwner->PlayerController->PlayerState)
			{
				AUTPlayerState* OwnerPlayerState = Cast<AUTPlayerState>(PlayerOwner->PlayerController->PlayerState);
				if (OwnerPlayerState)
				{
					return bIsInMatch && OwnerPlayerState->GetTeamNum() == TeamNum;
				}
			}

			return false;
		}
	}

	return true;
}


void SUPlayerListPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// Scan the PRI Array and Match infos for players and verify that the list is updated.  NOTE: We might want to consider 
	// moving this to a timer so it's not happening client-side every frame. 

	// Mark all players and Pendingkill.  
	for (int32 i=0; i < TrackedPlayers.Num(); i++)
	{
		if (TrackedPlayers[i]->EntryType == ETrackedPlayerType::Player)
		{
			TrackedPlayers[i]->bPendingKill = true;
		}
	}

	bool bListNeedsUpdate = false;

	AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	if (GameState)
	{
	
		if (PlayerOwner.IsValid() && PlayerOwner->PlayerController && PlayerOwner->PlayerController->PlayerState)
		{
			AUTLobbyPlayerState* OwnerPlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
			if (OwnerPlayerState && OwnerPlayerState->CurrentMatch)
			{
				// Owner is in a match.. add trhe headers...

				if (!MatchHeader.IsValid())
				{
					MatchHeader = FTrackedPlayer::MakeHeader(TEXT("- In Match - "), ETrackedPlayerType::MatchHeader);
					TrackedPlayers.Add(MatchHeader);

					bListNeedsUpdate = true;
				}

				if (!EveryoneHeader.IsValid())
				{
					EveryoneHeader = FTrackedPlayer::MakeHeader(TEXT("- Not in Match -"), ETrackedPlayerType::EveryoneHeader);
					TrackedPlayers.Add(EveryoneHeader);
					bListNeedsUpdate = true;
				}
			}
			else
			{
				if (MatchHeader.IsValid())
				{
					TrackedPlayers.Remove(MatchHeader);
					MatchHeader.Reset();
					bListNeedsUpdate = true;
				}

				if (EveryoneHeader.IsValid())
				{
					TrackedPlayers.Remove(EveryoneHeader);
					EveryoneHeader.Reset();
					bListNeedsUpdate = true;
				}
			}
		}
	

		// First go though all of the players on this server.
		for (int32 i=0; i < GameState->PlayerArray.Num(); i++)
		{
			AUTPlayerState* PlayerState = Cast<AUTPlayerState>(GameState->PlayerArray[i]);
			if (PlayerState)
			{
				bool bIsInMatch = IsInMatch(PlayerState);

				AUTLobbyPlayerState* LobbyPlayerState = Cast<AUTLobbyPlayerState>(PlayerState);
				bool bIsHost = (LobbyPlayerState && LobbyPlayerState->CurrentMatch && LobbyPlayerState->CurrentMatch->OwnerId == LobbyPlayerState->UniqueId);

				// Update the player's team info...

				uint8 TeamNum = PlayerState->GetTeamNum();

				if (ShouldShowPlayer(PlayerState->UniqueId, TeamNum, bIsInMatch))
				{
					int32 Idx = IsTracked(GameState->PlayerArray[i]->UniqueId);
					if (Idx != INDEX_NONE)
					{
						// This player lives to see another day
						TrackedPlayers[Idx]->bPendingKill = false;

						if (TrackedPlayers[Idx]->bIsInMatch != bIsInMatch)
						{
							bListNeedsUpdate = true;
							TrackedPlayers[Idx]->bIsInMatch = bIsInMatch;
						}
						
						if (TrackedPlayers[Idx]->TeamNum != TeamNum)
						{
							bListNeedsUpdate = true;
							TrackedPlayers[Idx]->TeamNum = TeamNum;
						}

						TrackedPlayers[Idx]->bIsHost = bIsHost;
					}
					else
					{
						bListNeedsUpdate = true;
						// This is a new player.. Add them.
						TrackedPlayers.Add(FTrackedPlayer::Make(PlayerState, PlayerState->UniqueId, PlayerState->PlayerName, TeamNum, 0, PlayerState == PlayerOwner->PlayerController->PlayerState,bIsHost));
					}


				}
			}
		}

		// If this is a lobby game, then pull players from the instanced arrays...
		AUTLobbyGameState* LobbyGameState = Cast<AUTLobbyGameState>(GameState);
		if (LobbyGameState)
		{
			for (int32 i = 0; i < LobbyGameState->AvailableMatches.Num(); i++)
			{
				AUTLobbyMatchInfo* MatchInfo = LobbyGameState->AvailableMatches[i];
				if (MatchInfo)			
				{
					for (int32 j = 0; j < MatchInfo->PlayersInMatchInstance.Num(); j++)					
					{
						if ( ShouldShowPlayer(MatchInfo->PlayersInMatchInstance[j].PlayerID, MatchInfo->PlayersInMatchInstance[j].TeamNum, false) ) 
						{
							int32 Idx = IsTracked(MatchInfo->PlayersInMatchInstance[j].PlayerID);
							if (Idx != INDEX_NONE)
							{
								TrackedPlayers[Idx]->bPendingKill = false;
							}
							else
							{
								bListNeedsUpdate = true;
								TrackedPlayers.Add(FTrackedPlayer::Make(nullptr, MatchInfo->PlayersInMatchInstance[j].PlayerID, 
																				MatchInfo->PlayersInMatchInstance[j].PlayerName,
																				MatchInfo->PlayersInMatchInstance[j].TeamNum, 0, false, false));
							}
						}
					}
				}
			}
		}
	}

	bool bNeedInstanceHeader = false;

	// Remove any entries that are pending kill
	for (int32 i = TrackedPlayers.Num() - 1; i >= 0; i--)
	{
		if (TrackedPlayers[i]->bPendingKill) 
		{
			bListNeedsUpdate = true;
			TrackedPlayers.RemoveAt(i);
		}
		else if (TrackedPlayers[i]->bInInstance) bNeedInstanceHeader = true;
	}

	if (bNeedInstanceHeader)
	{
		if (!InstanceHeader.IsValid())
		{
			InstanceHeader = FTrackedPlayer::MakeHeader(TEXT("- Playing -"), ETrackedPlayerType::EveryoneHeader);
			TrackedPlayers.Add(InstanceHeader);
			bListNeedsUpdate = true;
		}
	}
	else
	{
		if (InstanceHeader.IsValid())
		{
			TrackedPlayers.Remove(InstanceHeader);
			InstanceHeader.Reset();
			bListNeedsUpdate = true;
		}
	}


	if (bNeedsRefresh || bListNeedsUpdate)
	{
		// Sort the list...
		TrackedPlayers.Sort(FPlayerCompare());
		PlayerList->RequestListRefresh();
		bNeedsRefresh = false;
	}
}
 
void SUPlayerListPanel::ForceRefresh()
{
	bNeedsRefresh = true;
}

FReply SUPlayerListPanel::OnListSelect(TSharedPtr<FTrackedPlayer> Selected)
{
	if (PlayerClickedDelegate.IsBound() && Selected.IsValid())
	{
		PlayerClickedDelegate.Execute(Selected->PlayerID);
	}
	return FReply::Handled();
}

void SUPlayerListPanel::OnSubMenuSelect(FName Tag, TSharedPtr<FTrackedPlayer> InItem)
{

	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController && PlayerOwner->PlayerController->PlayerState)
	{
		AUTLobbyPlayerState* LobbyPlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
		AUTLobbyPlayerState* TargetPlayerState = Cast<AUTLobbyPlayerState>(InItem->PlayerState.Get());

		if (Tag == EPlayerListContentCommand::PlayerCard)
		{
			if (PlayerOwner.IsValid() && InItem->PlayerState.IsValid())
			{
				PlayerOwner->ShowPlayerInfo(InItem->PlayerState);
			}
		}
		else if (Tag == EPlayerListContentCommand::ChangeTeam)
		{
			if (LobbyPlayerState && TargetPlayerState && LobbyPlayerState->CurrentMatch)
			{
				LobbyPlayerState->CurrentMatch->ServerManageUser(0,TargetPlayerState);
			}
		}
		else if (Tag == EPlayerListContentCommand::Spectate)
		{
			if (LobbyPlayerState && TargetPlayerState && LobbyPlayerState->CurrentMatch)
			{
				LobbyPlayerState->CurrentMatch->ServerManageUser(1,TargetPlayerState);
			}
		}
		else if (Tag == EPlayerListContentCommand::Kick)
		{
			if (LobbyPlayerState && TargetPlayerState && LobbyPlayerState->CurrentMatch)
			{
				LobbyPlayerState->CurrentMatch->ServerManageUser(2,TargetPlayerState);
			}
		}
		else if (Tag == EPlayerListContentCommand::Ban)
		{
			if (LobbyPlayerState && TargetPlayerState && LobbyPlayerState->CurrentMatch)
			{
				LobbyPlayerState->CurrentMatch->ServerManageUser(3,TargetPlayerState);
			}
		}
	}
}

#endif
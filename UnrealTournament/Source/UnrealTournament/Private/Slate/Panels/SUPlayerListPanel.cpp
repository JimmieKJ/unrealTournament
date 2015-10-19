
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
		if (A->EntryType == ETrackedPlayerType::MatchHeader) 
		{
			return true;
		}
		if (B->EntryType == ETrackedPlayerType::MatchHeader) 
		{
			return false;
		}

		int32 AValue = 0;
		if (A->EntryType == ETrackedPlayerType::Spectator) 
		{
			AValue = 50;
		}
		else if (A->EntryType == ETrackedPlayerType::EveryoneHeader) 
		{
			AValue = 100;
		}
		else if (A->EntryType == ETrackedPlayerType::InstanceHeader)
		{
			AValue = 900;
		}
		else if (A->EntryType == ETrackedPlayerType::InstancePlayer) 
		{
			AValue = 1000;
		}
		else
		{
			AValue = (A->bIsInMatch ? 10 : 150) + (A->bInInstance ? 2000 : 0) + (A->bIsHost ? -5 : 0);
			AValue += (A->TeamNum == 0) ? 0 : 1;

			if (A->bIsSpectator) AValue += 51;
		}


		int32 BValue = 0;
		if (B->EntryType == ETrackedPlayerType::Spectator) 
		{
			BValue = 50;
		}
		else if (B->EntryType == ETrackedPlayerType::EveryoneHeader) 
		{
			BValue = 100;
		}
		else if (B->EntryType == ETrackedPlayerType::InstanceHeader)
		{
			BValue = 900;
		}
		else if (B->EntryType == ETrackedPlayerType::InstancePlayer) 
		{
			BValue = 1000;
		}
		else
		{
			BValue = (B->bIsInMatch ? 10 : 150) + (B->bInInstance ? 2000 : 0) + (B->bIsHost ? -5 : 0);
			BValue += (B->TeamNum == 0) ? 0 : 1;

			if (B->bIsSpectator) BValue += 51;

		}
/*
		UE_LOG(UT,Log,TEXT("Sort:  %s (%i) vs %s (%i) - %i < %i???"), *A->PlayerName, A->TeamNum, *B->PlayerName, B->TeamNum, AValue, BValue);			
*/
		return (AValue != BValue) ? AValue < BValue : (A->PlayerName < B->PlayerName);
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
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1.0)
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
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(InviteOverlay,SOverlay)
			]
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
							.Image(InItem.Get(), &FTrackedPlayer::GetAvatar)
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

	// Showing a player card requires a valid player state which players in an instance might not have.
	if (!TrackedPlayers[Idx]->bInInstance)
	{
		MenuOptions.Add(FMenuOptionData(NSLOCTEXT("PlayerListSubMenu","ShowPlayerCard","Player Card"), EPlayerListContentCommand::PlayerCard));
	}

	AUTPlayerState* OwnerPlayerState = Cast<AUTPlayerState>(PlayerOwner->PlayerController->PlayerState);

	bool bIsHost = false;
	bool bTeamGame = false;
	CheckFlags(bIsHost, bTeamGame);

	if (bIsHost || (PlayerOwner.IsValid() && PlayerOwner->PlayerController && TrackedPlayers[Idx]->PlayerState.Get() == PlayerOwner->PlayerController->PlayerState))
	{
		if (TrackedPlayers[Idx]->bIsInMatch)
		{

			// Look to see if this player is in a team game...

			if (bIsHost && (bTeamGame || TrackedPlayers[Idx]->bIsSpectator))
			{
				MenuOptions.Add(FMenuOptionData());
				MenuOptions.Add(FMenuOptionData(NSLOCTEXT("PlayerListSubMenu","ChangeTeam","Change Team"), EPlayerListContentCommand::ChangeTeam));
			}

			if (bIsHost && !TrackedPlayers[Idx]->bIsSpectator)
			{
				MenuOptions.Add(FMenuOptionData());
				MenuOptions.Add(FMenuOptionData(NSLOCTEXT("PlayerListSubMenu","Spectate","Spectate"), EPlayerListContentCommand::Spectate));
			}

			if (bIsHost && (TrackedPlayers[Idx]->PlayerState.Get() != PlayerOwner->PlayerController->PlayerState))
			{
				MenuOptions.Add(FMenuOptionData());
				MenuOptions.Add(FMenuOptionData(NSLOCTEXT("PlayerListSubMenu","Kick","Kick"), EPlayerListContentCommand::Kick));
				MenuOptions.Add(FMenuOptionData(NSLOCTEXT("PlayerListSubMenu","Ban","Ban"), EPlayerListContentCommand::Ban));
			}
		}
		else
		{
			AUTLobbyPlayerState* OwnerLobbyPlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
			AUTLobbyPlayerState* LobbyPlayerState = Cast<AUTLobbyPlayerState>(TrackedPlayers[Idx]->PlayerState.Get());
			if (OwnerLobbyPlayerState && LobbyPlayerState && OwnerLobbyPlayerState->CurrentMatch)
			{
				if ( OwnerLobbyPlayerState->CurrentMatch->AllowedPlayerList.Find(LobbyPlayerState->UniqueId.ToString()) == INDEX_NONE)
				{
					MenuOptions.Add(FMenuOptionData(NSLOCTEXT("PlayerListSubMenu","Invite","Invite to Match"), EPlayerListContentCommand::Invite));
				}
				else
				{
					MenuOptions.Add(FMenuOptionData(NSLOCTEXT("PlayerListSubMenu","Uninvite","Uninvite from Match"), EPlayerListContentCommand::UnInvite));
				}
			}
		
		}
	}

	if (OwnerPlayerState && OwnerPlayerState->bIsRconAdmin)
	{
		MenuOptions.Add(FMenuOptionData());
		MenuOptions.Add(FMenuOptionData(NSLOCTEXT("PlayerListSubMenu","ServerKick","Admin Kick"), EPlayerListContentCommand::ServerKick));
		MenuOptions.Add(FMenuOptionData(NSLOCTEXT("PlayerListSubMenu","ServerBan","Admin Ban"), EPlayerListContentCommand::ServerBan));
	}

	if (Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState) != NULL && TrackedPlayers[Idx]->bInInstance)
	{
		MenuOptions.Add(FMenuOptionData());
		MenuOptions.Add(FMenuOptionData(NSLOCTEXT("PlayerListSubMenu","Message","Send Message"), EPlayerListContentCommand::SendMessage));
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
	if (!PlayerId.IsValid())
	{
		return false;
	}

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
	AUTLobbyPlayerState* OwnerPlayerState = NULL;

	AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	if (GameState && PlayerOwner.IsValid() && PlayerOwner->PlayerController)
	{
		if (PlayerOwner.IsValid() && PlayerOwner->PlayerController && PlayerOwner->PlayerController->PlayerState)
		{
			OwnerPlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
			if (OwnerPlayerState && OwnerPlayerState->CurrentMatch)
			{
				// Owner is in a match.. add the headers...

				if (!MatchHeader.IsValid())
				{
					MatchHeader = FTrackedPlayer::MakeHeader(TEXT("- In Match - "), ETrackedPlayerType::MatchHeader);
					TrackedPlayers.Add(MatchHeader);

					bListNeedsUpdate = true;
				}

				if (!SpectatorHeader.IsValid())
				{
					SpectatorHeader = FTrackedPlayer::MakeHeader(TEXT("- Spectators - "), ETrackedPlayerType::Spectator);
					TrackedPlayers.Add(SpectatorHeader);
					bListNeedsUpdate = true;
				}

				if (!EveryoneHeader.IsValid())
				{
					//UE_LOG(UT,Log,TEXT("Adding Not In Match"));
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

				if (SpectatorHeader.IsValid())
				{
					TrackedPlayers.Remove(SpectatorHeader);
					SpectatorHeader.Reset();
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

/*
		UE_LOG(UT,Log,TEXT("==================================================[ Tick"));
		for (int32 i=0 ; i < TrackedPlayers.Num(); i++)
		{
			UE_LOG(UT,Log,TEXT("   TP %i : %s %s"), i, *TrackedPlayers[i]->PlayerName, *TrackedPlayers[i]->PlayerID.ToString());
		}

		UE_LOG(UT,Log,TEXT(""));
*/

		// First go though all of the players on this server.
		for (int32 i=0; i < GameState->PlayerArray.Num(); i++)
		{
//			FString DebugStr = FString::Printf(TEXT("  Testing %i "), i);

			AUTPlayerState* PlayerState = Cast<AUTPlayerState>(GameState->PlayerArray[i]);
			if (PlayerState)
			{
				bool bIsInMatch = IsInMatch(PlayerState);
				AUTLobbyPlayerState* LobbyPlayerState = Cast<AUTLobbyPlayerState>(PlayerState);
				bool bIsHost = (LobbyPlayerState && LobbyPlayerState->CurrentMatch && LobbyPlayerState->CurrentMatch->OwnerId == LobbyPlayerState->UniqueId);
				bool bIsInAnyMatch = (LobbyPlayerState && LobbyPlayerState->CurrentMatch);

				// Update the player's team info...

				uint8 TeamNum = PlayerState->GetTeamNum();

				if (LobbyPlayerState && LobbyPlayerState->CurrentMatch && LobbyPlayerState->CurrentMatch->CurrentRuleset.IsValid() && !LobbyPlayerState->CurrentMatch->CurrentRuleset->bTeamGame)
				{
					TeamNum = 255;
				}

				bool bIsSpec = (LobbyPlayerState && LobbyPlayerState->DesiredTeamNum == 255);
				
//				DebugStr = DebugStr + FString::Printf(TEXT("PlayerName = %s bIsInMAtch = %i  bIsHost = %i  bIsInAnyMatch = %i bIsSpec = %i (%s)"), *PlayerState->PlayerName, bIsInMatch, bIsHost, bIsInAnyMatch, bIsSpec, *LobbyPlayerState->UniqueId.ToString() );

				if (ShouldShowPlayer(PlayerState->UniqueId, TeamNum, bIsInMatch))
				{
					int32 Idx = IsTracked(GameState->PlayerArray[i]->UniqueId);

//					DebugStr = DebugStr + FString::Printf(TEXT(" TrackedIdx = %i"), Idx);

					if (Idx != INDEX_NONE && Idx < TrackedPlayers.Num())
					{
						// This player lives to see another day
						TrackedPlayers[Idx]->bPendingKill = false;

						if (TrackedPlayers[Idx]->bIsSpectator != bIsSpec)
						{
							TrackedPlayers[Idx]->bIsSpectator = bIsSpec;
							bListNeedsUpdate = true;
						}

						if (TrackedPlayers[Idx]->bIsInMatch != bIsInMatch)
						{
							bListNeedsUpdate = true;
							TrackedPlayers[Idx]->bIsInMatch = bIsInMatch;
						}
						
						if (TrackedPlayers[Idx]->bIsInAnyMatch != bIsInAnyMatch)
						{
							bListNeedsUpdate = true;
							TrackedPlayers[Idx]->bIsInAnyMatch = bIsInAnyMatch;
						}

						if (TrackedPlayers[Idx]->TeamNum != TeamNum)
						{
							bListNeedsUpdate = true;
							TrackedPlayers[Idx]->TeamNum = TeamNum;
						}

						TrackedPlayers[Idx]->bIsHost = bIsHost;

						AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(GameState->PlayerArray[i]);
						if (UTPlayerState)
						{
							TrackedPlayers[Idx]->Avatar = UTPlayerState->Avatar;
							TrackedPlayers[Idx]->bInInstance = false;
						}
					}
					else if (PlayerState && !PlayerState->bPendingKillPending)
					{
						bListNeedsUpdate = true;
						// This is a new player.. Add them.

						TrackedPlayers.Add(FTrackedPlayer::Make(PlayerState, PlayerState->UniqueId, PlayerState->PlayerName, TeamNum, PlayerState->Avatar, PlayerState == PlayerOwner->PlayerController->PlayerState,bIsHost, LobbyPlayerState ? (LobbyPlayerState->DesiredTeamNum == 255) : false));
					}
				}
			}

//			UE_LOG(UT,Log,TEXT("%s"),*DebugStr);
		}

		// If this is a lobby game, then pull players from the instanced arrays...
		AUTLobbyGameState* LobbyGameState = Cast<AUTLobbyGameState>(GameState);
		if (LobbyGameState)
		{
			for (int32 i = 0; i < LobbyGameState->AvailableMatches.Num(); i++)
			{
				AUTLobbyMatchInfo* MatchInfo = LobbyGameState->AvailableMatches[i];
				if (MatchInfo && MatchInfo->CurrentState != ELobbyMatchState::Returning && MatchInfo->CurrentState != ELobbyMatchState::Recycling)			
				{
					for (int32 j = 0; j < MatchInfo->PlayersInMatchInstance.Num(); j++)					
					{
						if ( ShouldShowPlayer(MatchInfo->PlayersInMatchInstance[j].PlayerID, MatchInfo->PlayersInMatchInstance[j].TeamNum, false) ) 
						{
							int32 Idx = IsTracked(MatchInfo->PlayersInMatchInstance[j].PlayerID);
							if (Idx != INDEX_NONE)
							{
								TrackedPlayers[Idx]->bPendingKill = false;
								TrackedPlayers[Idx]->Avatar = MatchInfo->PlayersInMatchInstance[j].Avatar;
							}
							else
							{
								bListNeedsUpdate = true;
								TrackedPlayers.Add(FTrackedPlayer::Make(nullptr, MatchInfo->PlayersInMatchInstance[j].PlayerID, 
																				MatchInfo->PlayersInMatchInstance[j].PlayerName,
																				MatchInfo->PlayersInMatchInstance[j].TeamNum,
																				MatchInfo->PlayersInMatchInstance[j].Avatar, false, false, false));
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

/*	
	UE_LOG(UT,Log,TEXT("-------------------------- %i || %i"), bNeedsRefresh,bListNeedsUpdate);
	UE_LOG(UT,Log,TEXT("  "));
*/

	if (bNeedInstanceHeader)
	{
		if (!InstanceHeader.IsValid())
		{
			InstanceHeader = FTrackedPlayer::MakeHeader(TEXT("- Playing -"), ETrackedPlayerType::InstanceHeader);
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

	if (OwnerPlayerState)
	{
		if (OwnerPlayerState->LastInvitedMatch)
		{
			if (!InviteInfo.IsValid() || InviteInfo.Get() != OwnerPlayerState->LastInvitedMatch)
			{
				InviteInfo = OwnerPlayerState->LastInvitedMatch;
				BuildInvite();
			}
		}
		else if (InviteBox.IsValid())
		{
			InviteInfo.Reset();
			BuildInvite();
		}
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
		else if (Tag == EPlayerListContentCommand::Invite)
		{
			if (LobbyPlayerState && TargetPlayerState && LobbyPlayerState->CurrentMatch)
			{
				LobbyPlayerState->CurrentMatch->ServerInvitePlayer(TargetPlayerState, true);
			}
		
		}
		else if (Tag == EPlayerListContentCommand::UnInvite)
		{
			if (LobbyPlayerState && TargetPlayerState && LobbyPlayerState->CurrentMatch)
			{
				LobbyPlayerState->CurrentMatch->ServerInvitePlayer(TargetPlayerState, false);
			}
		
		}
		else if ((Tag == EPlayerListContentCommand::ServerKick || Tag == EPlayerListContentCommand::ServerBan) && TargetPlayerState != NULL)
		{
			AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(PlayerOwner->PlayerController);
			if (PC)
			{
				PC->RconKick(TargetPlayerState->UniqueId.ToString(), Tag == EPlayerListContentCommand::ServerBan);
			}
		}
		else if ((Tag == EPlayerListContentCommand::SendMessage))
		{
			if (ConnectedChatPanel.IsValid())
			{
				ConnectedChatPanel->SetChatText(FString::Printf(TEXT("@%s "), *InItem->PlayerName));
			}
		}
	}
}

void SUPlayerListPanel::BuildInvite()
{
	InviteOverlay->ClearChildren();
	if (InviteInfo.IsValid())
	{

		FSlateApplication::Get().PlaySound(SUTStyle::MessageSound,0);

		TWeakObjectPtr<AUTLobbyPlayerState> InviteOwner = InviteInfo->GetOwnerPlayerState();
		if (InviteOwner.IsValid())
		{
			InviteOverlay->AddSlot()
			[
				SAssignNew(InviteBox,SVerticalBox)
				+SVerticalBox::Slot()
				.Padding(2.0,5.0,2.0,2.0)
				.AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperLight"))
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(InviteOwner->PlayerName))
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						[
							SNew(SRichTextBlock)
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tiny")
							.Justification(ETextJustify::Center)
							.DecoratorStyleSet( &SUTStyle::Get() )
							.AutoWrapText( true )
							.Text(NSLOCTEXT("SUPlayerListPanel","InviteText","has invited you to\nplay UT..."))
						]

						+SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						.Padding(0.0,5.0,0.0,5.0)
						[
							SNew(SGridPanel)
							+SGridPanel::Slot(0,0)
							.Padding(10.0,0.0,5.0,0.0)
							[
								SNew(SButton)
								.ButtonStyle(SUTStyle::Get(),"UT.SimpleButton")
								.OnClicked(this, &SUPlayerListPanel::OnMatchInviteAction, true)
								.ContentPadding(FMargin(5.0,0.0,5.0,0.0))
								[
									SNew(STextBlock)
									.Text(NSLOCTEXT("Generic","Accept","Accept"))
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
								]
							]
							+SGridPanel::Slot(1,0)
							.Padding(10.0,0.0,5.0,0.0)
							[
								SNew(SButton)
								.ButtonStyle(SUTStyle::Get(),"UT.SimpleButton")
								.OnClicked(this, &SUPlayerListPanel::OnMatchInviteAction,false)
								.ContentPadding(FMargin(5.0,0.0,5.0,0.0))
								[
									SNew(STextBlock)
									.Text(NSLOCTEXT("Generic","Ignore","Ignore"))
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
								]
							]
						]
					]
				]
			];


		}
	}
}

FReply SUPlayerListPanel::OnMatchInviteAction(bool bAccept)
{
	AUTLobbyPlayerState* LobbyPlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
	if (LobbyPlayerState)
	{
		if (bAccept && LobbyPlayerState->LastInvitedMatch)
		{
			LobbyPlayerState->ServerJoinMatch(LobbyPlayerState->LastInvitedMatch,false);
		}
		LobbyPlayerState->LastInvitedMatch = NULL;
	}

	return FReply::Handled();

}
#endif
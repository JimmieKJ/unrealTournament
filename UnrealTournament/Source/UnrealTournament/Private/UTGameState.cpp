// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTMultiKillMessage.h"
#include "UTSpreeMessage.h"
#include "UTRemoteRedeemer.h"
#include "Net/UnrealNetwork.h"
#include "UTTimerMessage.h"

AUTGameState::AUTGameState(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	MultiKillMessageClass = UUTMultiKillMessage::StaticClass();
	SpreeMessageClass = UUTSpreeMessage::StaticClass();
	MultiKillDelay = 4.0f;
	SpawnProtectionTime = 2.5f;
	bWeaponStay = true;
	bViewKillerOnDeath = true;
	bAllowTeamSwitches = true;
	bPlayerListsAreValid = false;

	ServerName = TEXT("My First Server");
	ServerMOTD = TEXT("Welcome!");
}

void AUTGameState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTGameState, RemainingMinute);
	DOREPLIFETIME(AUTGameState, WinnerPlayerState);
	DOREPLIFETIME(AUTGameState, WinningTeam);
	DOREPLIFETIME(AUTGameState, RespawnWaitTime);  // @TODO FIXMESTEVE why not initial only
	DOREPLIFETIME(AUTGameState, ForceRespawnTime);  // @TODO FIXMESTEVE why not initial only
	DOREPLIFETIME(AUTGameState, TimeLimit);  // @TODO FIXMESTEVE why not initial only
	DOREPLIFETIME(AUTGameState, bTeamGame);  // @TODO FIXMESTEVE why not initial only
	DOREPLIFETIME(AUTGameState, bOnlyTheStrongSurvive);  // @TODO FIXMESTEVE why not initial only
	DOREPLIFETIME(AUTGameState, bViewKillerOnDeath);  // @TODO FIXMESTEVE why not initial only
	DOREPLIFETIME(AUTGameState, TeamSwapSidesOffset);
	DOREPLIFETIME(AUTGameState, bIsInstanceServer);  // @TODO FIXMESTEVE why not initial only
	DOREPLIFETIME(AUTGameState, PlayersNeeded);  // FIXME only before match start

	DOREPLIFETIME_CONDITION(AUTGameState, bAllowTeamSwitches, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, bWeaponStay, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, GoalScore, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, RemainingTime, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, OverlayMaterials, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, SpawnProtectionTime, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, NumTeams, COND_InitialOnly);

	DOREPLIFETIME_CONDITION(AUTGameState, ServerName, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, ServerDescription, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, ServerMOTD, COND_InitialOnly);
}

void AUTGameState::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	NumTeams = Teams.Num();
	Super::PreReplication(ChangedPropertyTracker);
}

void AUTGameState::AddOverlayMaterial(UMaterialInterface* NewOverlay)
{
	if (NewOverlay != NULL && Role == ROLE_Authority)
	{
		if (NetUpdateTime > 0.0f)
		{
			UE_LOG(UT, Warning, TEXT("UTGameState::AddOverlayMaterial() called after startup; may not take effect on clients"));
		}
		for (int32 i = 0; i < ARRAY_COUNT(OverlayMaterials); i++)
		{
			if (OverlayMaterials[i] == NewOverlay)
			{
				return;
			}
			else if (OverlayMaterials[i] == NULL)
			{
				OverlayMaterials[i] = NewOverlay;
				return;
			}
		}
		UE_LOG(UT, Warning, TEXT("UTGameState::AddOverlayMaterial(): Ran out of slots, couldn't add %s"), *NewOverlay->GetFullName());
	}
}

void AUTGameState::BeginPlay()
{
	Super::BeginPlay();

	// HACK: temporary hack around config property replication bug; force to be different from defaults
	ServerName += TEXT(" ");
	ServerMOTD += TEXT(" ");

	// HACK: temporary workaround for replicated world gravity getting clobbered on client
	if (GetNetMode() == NM_Client)
	{
		if (GetWorld()->GetWorldSettings()->WorldGravityZ != 0.0f)
		{
			GetWorld()->GetWorldSettings()->bWorldGravitySet = true;
		}
		else
		{
			GetWorld()->GetWorldSettings()->GetGravityZ();
		}
	}

	if (GetNetMode() == NM_Client)
	{
		// hook up any TeamInfos that were received prior
		for (FActorIterator It(GetWorld()); It; ++It)
		{
			AUTTeamInfo* Team = Cast<AUTTeamInfo>(*It);
			if (Team != NULL)
			{
				Team->ReceivedTeamIndex();
			}
		}
	}
	else
	{
		TArray<UObject*> AllCharacters;
		GetObjectsOfClass(AUTCharacter::StaticClass(), AllCharacters, true, RF_NoFlags);
		for (int32 i = 0; i < AllCharacters.Num(); i++)
		{
			if (AllCharacters[i]->HasAnyFlags(RF_ClassDefaultObject))
			{
				checkSlow(AllCharacters[i]->IsA(AUTCharacter::StaticClass()));
				AddOverlayMaterial(((AUTCharacter*)AllCharacters[i])->TacComOverlayMaterial);
			}
		}

		TArray<UObject*> AllInventory;
		GetObjectsOfClass(AUTInventory::StaticClass(), AllInventory, true, RF_NoFlags);
		for (int32 i = 0; i < AllInventory.Num(); i++)
		{
			if (AllInventory[i]->HasAnyFlags(RF_ClassDefaultObject))
			{
				checkSlow(AllInventory[i]->IsA(AUTInventory::StaticClass()));
				((AUTInventory*)AllInventory[i])->AddOverlayMaterials(this);
			}
		}
	}
}

void AUTGameState::OnRep_RemainingTime()
{
	// if we received RemainingTime, it takes precedence
	// note that this relies on all variables being received prior to any notifies being called
	RemainingMinute = 0;
}

void AUTGameState::DefaultTimer()
{
	Super::DefaultTimer();

	if (GetWorld()->GetNetMode() == NM_Client)
	{
		if (RemainingMinute > 0)
		{
			RemainingTime = RemainingMinute;
			RemainingMinute = 0;
		}

		// might have been deferred while waiting for teams to replicate
		if (TeamSwapSidesOffset != PrevTeamSwapSidesOffset)
		{
			OnTeamSideSwap();
		}
	}

	if (RemainingTime > 0 && !bStopGameClock)
	{
		if (IsMatchInProgress())
		{
			RemainingTime--;
			if (GetWorld()->GetNetMode() != NM_Client)
			{
				int32 RepTimeInterval = (RemainingTime > 60) ? 60 : 12;
				if (RemainingTime % RepTimeInterval == 0)
				{
					RemainingMinute = RemainingTime;
				}
			}
		}
		else if (!HasMatchStarted())
		{
			AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
			if (Game == NULL || Game->NumPlayers + Game->NumBots > Game->MinPlayersToStart)
			{
				RemainingTime--;
				if (GetWorld()->GetNetMode() != NM_Client)
				{
					// during pre-match bandwidth isn't at a premium, let's be accurate
					RemainingMinute = RemainingTime;
				}
			}
		}
	}

	if (GetWorld()->GetNetMode() != NM_DedicatedServer && IsMatchInProgress() && !bStopGameClock)
	{
		int32 TimerMessageIndex = -1;
		switch (RemainingTime)
		{
			case 300 : TimerMessageIndex = 13; break;		// 5 mins remain
			case 180 : TimerMessageIndex = 12; break;		// 3 mins remain
			case 60  : TimerMessageIndex = 11; break;		// 1 min remains
			case 30  : TimerMessageIndex = 10; break;		// 30 seconds remain
			default:
				if (RemainingTime >0 && RemainingTime <= 10)
				{
					TimerMessageIndex = (RemainingTime -1);
				}
				break;
		}

		if (TimerMessageIndex >= 0)
		{
			TArray<APlayerController*> PlayerList;
			GEngine->GetAllLocalPlayerControllers(PlayerList);
			for (auto It = PlayerList.CreateIterator(); It; ++It)
			{
				AUTPlayerController* PC = Cast<AUTPlayerController>(*It);
				if (PC != NULL)
				{
					PC->ClientReceiveLocalizedMessage(UUTTimerMessage::StaticClass(), TimerMessageIndex);			
				}
			}
		}
	}
}

bool AUTGameState::OnSameTeam(const AActor* Actor1, const AActor* Actor2)
{
	const IUTTeamInterface* TeamInterface1 = Cast<IUTTeamInterface>(Actor1);
	const IUTTeamInterface* TeamInterface2 = Cast<IUTTeamInterface>(Actor2);
	if (TeamInterface1 == NULL || TeamInterface2 == NULL)
	{
		return false;
	}
	else if (TeamInterface1->IsFriendlyToAll() || TeamInterface2->IsFriendlyToAll())
	{
		return true;
	}
	else
	{
		uint8 TeamNum1 = TeamInterface1->GetTeamNum();
		uint8 TeamNum2 = TeamInterface2->GetTeamNum();

		if (TeamNum1 == 255 || TeamNum2 == 255)
		{
			return false;
		}
		else
		{
			return TeamNum1 == TeamNum2;
		}
	}
}

void AUTGameState::ChangeTeamSides(uint8 Offset)
{
	TeamSwapSidesOffset += Offset;
	OnTeamSideSwap();
}

void AUTGameState::OnTeamSideSwap()
{
	if (TeamSwapSidesOffset != PrevTeamSwapSidesOffset && Teams.Num() > 0 && (Role == ROLE_Authority || Teams.Num() == NumTeams))
	{
		uint8 TotalOffset;
		if (TeamSwapSidesOffset < PrevTeamSwapSidesOffset)
		{
			// rollover
			TotalOffset = uint8(uint32(TeamSwapSidesOffset + 255) - uint32(PrevTeamSwapSidesOffset));
		}
		else
		{
			TotalOffset = TeamSwapSidesOffset - PrevTeamSwapSidesOffset;
		}
		for (FActorIterator It(GetWorld()); It; ++It)
		{
			IUTTeamInterface* TeamObj = Cast<IUTTeamInterface>(*It);
			if (TeamObj != NULL)
			{
				uint8 Team = TeamObj->GetTeamNum();
				if (Team != 255)
				{
					TeamObj->Execute_SetTeamForSideSwap(*It, (Team + TotalOffset) % Teams.Num());
				}
			}
			// check for script interface
			else if (It->GetClass()->ImplementsInterface(UUTTeamInterface::StaticClass()))
			{
				// a little hackery to ignore if relevant functions haven't been implemented
				static FName NAME_ScriptGetTeamNum(TEXT("ScriptGetTeamNum"));
				UFunction* GetTeamNumFunc = It->GetClass()->FindFunctionByName(NAME_ScriptGetTeamNum);
				if (GetTeamNumFunc != NULL && GetTeamNumFunc->Script.Num() > 0)
				{
					uint8 Team = IUTTeamInterface::Execute_ScriptGetTeamNum(*It);
					if (Team != 255)
					{
						IUTTeamInterface::Execute_SetTeamForSideSwap(*It, (Team + TotalOffset) % Teams.Num());
					}
				}
				else
				{
					static FName NAME_SetTeamForSideSwap(TEXT("SetTeamForSideSwap"));
					UFunction* SwapFunc = It->GetClass()->FindFunctionByName(NAME_SetTeamForSideSwap);
					if (SwapFunc != NULL && SwapFunc->Script.Num() > 0)
					{
						UE_LOG(UT, Warning, TEXT("Unable to execute SetTeamForSideSwap() for %s because GetTeamNum() must also be implemented"), *It->GetName());
					}
				}
			}
		}
		if (Role == ROLE_Authority)
		{
			// re-initialize all AI squads, in case objectives have changed sides
			for (AUTTeamInfo* Team : Teams)
			{
				Team->ReinitSquads();
			}
		}

		TeamSideSwapDelegate.Broadcast(TotalOffset);

		PrevTeamSwapSidesOffset = TeamSwapSidesOffset;
	}
}

void AUTGameState::SetTimeLimit(uint32 NewTimeLimit)
{
	TimeLimit = NewTimeLimit;
	RemainingTime = TimeLimit;
	RemainingMinute = TimeLimit;

	ForceNetUpdate();
}

void AUTGameState::SetGoalScore(uint32 NewGoalScore)
{
	GoalScore = NewGoalScore;
	ForceNetUpdate();
}

void AUTGameState::SetWinner(AUTPlayerState* NewWinner)
{
	WinnerPlayerState = NewWinner;
	WinningTeam	= NewWinner != NULL ?  NewWinner->Team : 0;
	ForceNetUpdate();
}


/**
  * returns true if P1 should be sorted before P2
  */
bool AUTGameState::InOrder( AUTPlayerState* P1, AUTPlayerState* P2 )
{
	// spectators are sorted last
    if( P1->bOnlySpectator )
    {
		return P2->bOnlySpectator;
    }
    else if ( P2->bOnlySpectator )
	{
		return true;
	}

	// sort by Score
    if( P1->Score < P2->Score )
	{
		return false;
	}
    if( P1->Score == P2->Score )
    {
		// if score tied, use deaths to sort
		if ( P1->Deaths > P2->Deaths )
			return false;

		// keep local player highest on list
		if ( (P1->Deaths == P2->Deaths) && (Cast<APlayerController>(P2->GetOwner()) != NULL) )
		{
			ULocalPlayer* LP2 = Cast<ULocalPlayer>(Cast<APlayerController>(P2->GetOwner())->Player);
			if ( LP2 != NULL )
			{
				// make sure ordering is consistent for splitscreen players
				ULocalPlayer* LP1 = Cast<ULocalPlayer>(Cast<APlayerController>(P2->GetOwner())->Player);
				return ( LP1 != NULL );
			}
		}
	}
    return true;
}

/** 
  * Sort the PRI Array based on InOrder() prioritization
  */
void AUTGameState::SortPRIArray()
{
	for (int32 i=0; i<PlayerArray.Num()-1; i++)
	{
		AUTPlayerState* P1 = Cast<AUTPlayerState>(PlayerArray[i]);
		for (int32 j=i+1; j<PlayerArray.Num(); j++)
		{
			AUTPlayerState* P2 = Cast<AUTPlayerState>(PlayerArray[j]);
			if( !InOrder( P1, P2 ) )
			{
				PlayerArray[i] = P2;
				PlayerArray[j] = P1;
				P1 = P2;
			}
		}
	}
}

bool AUTGameState::IsMatchInCountdown() const
{
	return GetMatchState() == MatchState::CountdownToBegin;
}

bool AUTGameState::HasMatchStarted() const
{
	return Super::HasMatchStarted() && GetMatchState() != MatchState::CountdownToBegin;
}

bool AUTGameState::IsMatchInProgress() const
{
	FName MatchState = GetMatchState();
	if (MatchState == MatchState::InProgress || MatchState == MatchState::MatchIsInOvertime)
	{
		return true;
	}

	return false;
}

bool AUTGameState::IsMatchAtHalftime() const
{	
	return false;	
}

bool AUTGameState::IsMatchInSuddenDeath() const
{
	return false;
}

bool AUTGameState::IsMatchInOvertime() const
{
	FName MatchState = GetMatchState();
	if (MatchState == MatchState::MatchEnteringOvertime || MatchState == MatchState::MatchIsInOvertime)
	{
		return true;
	}

	return false;
}

void AUTGameState::OnWinnerReceived()
{
}

FName AUTGameState::OverrideCameraStyle(APlayerController* PCOwner, FName CurrentCameraStyle)
{
	if (HasMatchEnded())
	{
		return FName(TEXT("FreeCam"));
	}
	// FIXME: shouldn't this come from the Pawn?
	else if (Cast<AUTRemoteRedeemer>(PCOwner->GetPawn()) != nullptr)
	{
		return FName(TEXT("FirstPerson"));
	}
	else
	{
		return CurrentCameraStyle;
	}
}

FText AUTGameState::ServerRules()
{
	if (GameModeClass != NULL && GameModeClass->IsChildOf(AUTGameMode::StaticClass()))
	{
		return GameModeClass->GetDefaultObject<AUTGameMode>()->BuildServerRules(this);
	}
	else
	{
		return FText();
	}
}

void AUTGameState::ReceivedGameModeClass()
{
	Super::ReceivedGameModeClass();

	TSubclassOf<AUTGameMode> UTGameClass(*GameModeClass);
	if (UTGameClass != NULL)
	{
		// precache announcements
		for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
		{
			AUTPlayerController* UTPC = Cast<AUTPlayerController>(It->PlayerController);
			if (UTPC != NULL)
			{
				if (UTPC->RewardAnnouncer != NULL)
				{
					UTGameClass.GetDefaultObject()->PrecacheAnnouncements(UTPC->RewardAnnouncer);
				}
				if (UTPC->StatusAnnouncer != NULL)
				{
					UTGameClass.GetDefaultObject()->PrecacheAnnouncements(UTPC->StatusAnnouncer);
				}
			}
		}
	}
}

FText AUTGameState::GetGameStatusText()
{
	if (!IsMatchInProgress())
	{
		if (HasMatchEnded())
		{
			return NSLOCTEXT("UTGameState", "PostGame", "Game Over");
		}
		else
		{
			return NSLOCTEXT("UTGameState", "PreGame", "Pre-Game");
		}
	}

	return FText::GetEmpty();
}

void AUTGameState::OnRep_MatchState()
{
	Super::OnRep_MatchState();

	for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
	{
		if (It->PlayerController != NULL)
		{
			AUTHUD* Hud = Cast<AUTHUD>(It->PlayerController->MyHUD);
			if (Hud != NULL)
			{
				Hud->NotifyMatchStateChange();
			}
		}
	}
}

// By default, do nothing.  
void AUTGameState::OnRep_ServerName()
{
}

// By default, do nothing.  
void AUTGameState::OnRep_ServerMOTD()
{
}

void AUTGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);
	bPlayerListsAreValid = false;
}

void AUTGameState::RemovePlayerState(APlayerState* PlayerState)
{
	Super::RemovePlayerState(PlayerState);
	bPlayerListsAreValid = false;
}

// FIXME - move to UTGameState, and cache when PlayerArray changes to know when to refill.
void AUTGameState::FillPlayerLists()
{
	if (bPlayerListsAreValid)
	{
		return;
	}
	bPlayerListsAreValid = true;

	// fill blueplayerlist
	if (bTeamGame)
	{
		for (int32 i = 0; i < PlayerArray.Num() - 1; i++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerArray[i]);
			if (PS && (PS->Team->TeamIndex == 1))
			{
				bool bFoundPlayer = false;
				int32 FirstEmptySlot = -1;
				for (int32 j = 0; j < BluePlayerList.Num(); j++)
				{
					if (BluePlayerList[j] == PS)
					{
						bFoundPlayer = true;
					}
				}
				if (!bFoundPlayer)
				{
					// add player
					bool bFoundSlot = false;
					for (int32 j = 0; j < BluePlayerList.Num(); j++)
					{
						if (BluePlayerList[j] == NULL)
						{
							bFoundSlot = true;
							BluePlayerList[j] = PS;
						}
					}
					if (!bFoundSlot)
					{
						BluePlayerList.Add(PS);
					}
				}
			}
		}
	}

	// fill redplayerlist
	for (int32 i = 0; i < PlayerArray.Num() - 1; i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerArray[i]);
		if (PS && !PS->bOnlySpectator && (!bTeamGame || (PS->Team->TeamIndex == 0)))
		{
			bool bFoundPlayer = false;
			int32 FirstEmptySlot = -1;
			for (int32 j = 0; j < RedPlayerList.Num(); j++)
			{
				if (RedPlayerList[j] == PS)
				{
					bFoundPlayer = true;
				}
			}
			if (!bFoundPlayer)
			{
				// add player
				bool bFoundSlot = false;
				for (int32 j = 0; j < RedPlayerList.Num(); j++)
				{
					if (RedPlayerList[j] == NULL)
					{
						bFoundSlot = true;
						RedPlayerList[j] = PS;
					}
				}
				if (!bFoundSlot)
				{
					RedPlayerList.Add(PS);
				}
			}
		}
	}
}

AUTPlayerState* AUTGameState::GetRedPlayer(int32 Index)
{
	if ((RedPlayerList.Num() > Index) && (RedPlayerList[Index] != NULL) && !RedPlayerList[Index]->IsPendingKillPending())
	{
		return RedPlayerList[Index];
	}
	else
	{
		FillPlayerLists();
		if ((RedPlayerList.Num() > Index) && (RedPlayerList[Index] != NULL) && !RedPlayerList[Index]->IsPendingKillPending())
		{
			return RedPlayerList[Index];
		}
	}
	return NULL;
}

AUTPlayerState* AUTGameState::GetBluePlayer(int32 Index)
{
	if ((BluePlayerList.Num() > Index) && (BluePlayerList[Index] != NULL) && !BluePlayerList[Index]->IsPendingKillPending())
	{
		return BluePlayerList[Index];
	}
	else
	{
		FillPlayerLists();
		if ((BluePlayerList.Num() > Index) && (BluePlayerList[Index] != NULL) && !BluePlayerList[Index]->IsPendingKillPending())
		{
			return BluePlayerList[Index];
		}
	}
	return NULL;
}


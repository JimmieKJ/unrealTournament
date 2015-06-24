// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTMultiKillMessage.h"
#include "UTSpreeMessage.h"
#include "UTRemoteRedeemer.h"
#include "UTGameMessage.h"
#include "Net/UnrealNetwork.h"
#include "UTTimerMessage.h"
#include "UTReplicatedLoadoutInfo.h"
#include "UTMutator.h"
#include "UTReplicatedMapVoteInfo.h"
#include "UTPickup.h"
#include "UTArmor.h"
#include "StatNames.h"
#include "UTGameEngine.h"

AUTGameState::AUTGameState(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	MultiKillMessageClass = UUTMultiKillMessage::StaticClass();
	SpreeMessageClass = UUTSpreeMessage::StaticClass();
	MultiKillDelay = 3.0f;
	SpawnProtectionTime = 2.5f;
	bWeaponStay = true;
	bViewKillerOnDeath = true;
	bAllowTeamSwitches = true;

	KickThreshold=51.0;

	ServerName = TEXT("My First Server");
	ServerMOTD = TEXT("Welcome!");
	SecondaryAttackerStat = NAME_None;

	GameScoreStats.Add(NAME_AttackerScore);
	GameScoreStats.Add(NAME_DefenderScore);
	GameScoreStats.Add(NAME_SupporterScore);
	GameScoreStats.Add(NAME_Suicides);

	GameScoreStats.Add(NAME_UDamageTime);
	GameScoreStats.Add(NAME_BerserkTime);
	GameScoreStats.Add(NAME_InvisibilityTime);
	GameScoreStats.Add(NAME_BootJumps);
	GameScoreStats.Add(NAME_ShieldBeltCount);
	GameScoreStats.Add(NAME_ArmorVestCount);
	GameScoreStats.Add(NAME_ArmorPadsCount);
	GameScoreStats.Add(NAME_HelmetCount);

	TeamStats.Add(NAME_TeamKills);
	TeamStats.Add(NAME_UDamageTime);
	TeamStats.Add(NAME_BerserkTime);
	TeamStats.Add(NAME_InvisibilityTime);
	TeamStats.Add(NAME_BootJumps);
	TeamStats.Add(NAME_ShieldBeltCount);
	TeamStats.Add(NAME_ArmorVestCount);
	TeamStats.Add(NAME_ArmorPadsCount);
	TeamStats.Add(NAME_HelmetCount);

	WeaponStats.Add(NAME_ImpactHammerKills);
	WeaponStats.Add(NAME_EnforcerKills);
	WeaponStats.Add(NAME_BioRifleKills);
	WeaponStats.Add(NAME_ShockBeamKills);
	WeaponStats.Add(NAME_ShockCoreKills);
	WeaponStats.Add(NAME_ShockComboKills);
	WeaponStats.Add(NAME_LinkKills);
	WeaponStats.Add(NAME_LinkBeamKills);
	WeaponStats.Add(NAME_MinigunKills);
	WeaponStats.Add(NAME_MinigunShardKills);
	WeaponStats.Add(NAME_FlakShardKills);
	WeaponStats.Add(NAME_FlakShellKills);
	WeaponStats.Add(NAME_RocketKills);
	WeaponStats.Add(NAME_SniperKills);
	WeaponStats.Add(NAME_SniperHeadshotKills);
	WeaponStats.Add(NAME_RedeemerKills);
	WeaponStats.Add(NAME_InstagibKills);
	WeaponStats.Add(NAME_TelefragKills);

	WeaponStats.Add(NAME_ImpactHammerDeaths);
	WeaponStats.Add(NAME_EnforcerDeaths);
	WeaponStats.Add(NAME_BioRifleDeaths);
	WeaponStats.Add(NAME_ShockBeamDeaths);
	WeaponStats.Add(NAME_ShockCoreDeaths);
	WeaponStats.Add(NAME_ShockComboDeaths);
	WeaponStats.Add(NAME_LinkDeaths);
	WeaponStats.Add(NAME_LinkBeamDeaths);
	WeaponStats.Add(NAME_MinigunDeaths);
	WeaponStats.Add(NAME_MinigunShardDeaths);
	WeaponStats.Add(NAME_FlakShardDeaths);
	WeaponStats.Add(NAME_FlakShellDeaths);
	WeaponStats.Add(NAME_RocketDeaths);
	WeaponStats.Add(NAME_SniperDeaths);
	WeaponStats.Add(NAME_SniperHeadshotDeaths);
	WeaponStats.Add(NAME_RedeemerDeaths);
	WeaponStats.Add(NAME_InstagibDeaths);
	WeaponStats.Add(NAME_TelefragDeaths);

	WeaponStats.Add(NAME_BestShockCombo);
	WeaponStats.Add(NAME_AirRox);
	WeaponStats.Add(NAME_AmazingCombos);
	WeaponStats.Add(NAME_FlakShreds);
	WeaponStats.Add(NAME_AirSnot);

	RewardStats.Add(NAME_MultiKillLevel0);
	RewardStats.Add(NAME_MultiKillLevel1);
	RewardStats.Add(NAME_MultiKillLevel2);
	RewardStats.Add(NAME_MultiKillLevel3);

	RewardStats.Add(NAME_SpreeKillLevel0);
	RewardStats.Add(NAME_SpreeKillLevel1);
	RewardStats.Add(NAME_SpreeKillLevel2);
	RewardStats.Add(NAME_SpreeKillLevel3);
	RewardStats.Add(NAME_SpreeKillLevel4);
}

void AUTGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTGameState, RemainingMinute);
	DOREPLIFETIME(AUTGameState, WinnerPlayerState);
	DOREPLIFETIME(AUTGameState, WinningTeam);
	DOREPLIFETIME(AUTGameState, TimeLimit);  
	DOREPLIFETIME(AUTGameState, TimeLimit);  // @TODO FIXMESTEVE why not initial only
	DOREPLIFETIME_CONDITION(AUTGameState, RespawnWaitTime, COND_InitialOnly);  
	DOREPLIFETIME_CONDITION(AUTGameState, ForceRespawnTime, COND_InitialOnly);  
	DOREPLIFETIME_CONDITION(AUTGameState, bTeamGame, COND_InitialOnly);  
	DOREPLIFETIME_CONDITION(AUTGameState, bOnlyTheStrongSurvive, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, bViewKillerOnDeath, COND_InitialOnly);
	DOREPLIFETIME(AUTGameState, TeamSwapSidesOffset);
	DOREPLIFETIME_CONDITION(AUTGameState, bIsInstanceServer, COND_InitialOnly);
	DOREPLIFETIME(AUTGameState, PlayersNeeded);  // FIXME only before match start
	DOREPLIFETIME(AUTGameState, AvailableLoadout);
	DOREPLIFETIME(AUTGameState, HubGuid);

	DOREPLIFETIME_CONDITION(AUTGameState, RespawnWaitTime, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, ForceRespawnTime, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, bTeamGame, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, bOnlyTheStrongSurvive, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, bViewKillerOnDeath, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, bIsInstanceServer, COND_InitialOnly);  
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

	DOREPLIFETIME_CONDITION(AUTGameState, ServerSessionId, COND_InitialOnly);

	DOREPLIFETIME(AUTGameState, MapVoteList);
	DOREPLIFETIME(AUTGameState, VoteTimer);
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

float AUTGameState::GetClockTime()
{
	if (IsMatchInOvertime())
	{
		return ElapsedTime-TimeLimit;
	}
	return (TimeLimit > 0.f) ? RemainingTime : ElapsedTime;
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
	if (IsMatchAtHalftime())
	{
		// no elapsed time - it was incremented in super
		ElapsedTime--;
	}
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

	if ((RemainingTime > 0) && !bStopGameClock && TimeLimit > 0)
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

		if (GetWorld()->GetNetMode() != NM_DedicatedServer && IsMatchInProgress())
		{
			int32 TimerMessageIndex = -1;
			switch (RemainingTime)
			{
			case 300: TimerMessageIndex = 13; break;		// 5 mins remain
			case 180: TimerMessageIndex = 12; break;		// 3 mins remain
			case 60: TimerMessageIndex = 11; break;		// 1 min remains
			case 30: TimerMessageIndex = 10; break;		// 30 seconds remain
			default:
				if (RemainingTime <= 10)
				{
					TimerMessageIndex = RemainingTime - 1;
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

void AUTGameState::SetTimeLimit(int32 NewTimeLimit)
{
	TimeLimit = NewTimeLimit;
	RemainingTime = TimeLimit;
	RemainingMinute = TimeLimit;

	ForceNetUpdate();
}

void AUTGameState::SetGoalScore(int32 NewGoalScore)
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
			if (UTPC != NULL && UTPC->Announcer != NULL)
			{
				UTGameClass.GetDefaultObject()->PrecacheAnnouncements(UTPC->Announcer);
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
	// assign spectating ID to this player
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	// NOTE: in the case of a rejoining player, this function gets called for both the original and new PLayerStates
	//		we will migrate the initially selected ID to avoid unnecessary ID shuffling
	//		and thus need to check here if that has happened and avoid assigning a new one
	if (PS != NULL && !PS->bOnlySpectator && PS->SpectatingID == 0)
	{
		TArray<APlayerState*> PlayerArrayCopy = PlayerArray;
		PlayerArrayCopy.Sort([](const APlayerState& A, const APlayerState& B) -> bool
		{
			if (Cast<AUTPlayerState>(&A) == NULL)
			{
				return false;
			}
			else if (Cast<AUTPlayerState>(&B) == NULL)
			{
				return true;
			}
			else
			{
				return ((AUTPlayerState*)&A)->SpectatingID < ((AUTPlayerState*)&B)->SpectatingID;
			}
		});
		// find first gap in assignments from player leaving, give it to this player
		// if none found, assign PlayerArray.Num() + 1
		bool bFound = false;
		for (int32 i = 0; i < PlayerArrayCopy.Num(); i++)
		{
			AUTPlayerState* OtherPS = Cast<AUTPlayerState>(PlayerArrayCopy[i]);
			if (OtherPS == NULL || OtherPS->SpectatingID != uint8(i + 1))
			{
				PS->SpectatingID = uint8(i + 1);
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			PS->SpectatingID = uint8(PlayerArrayCopy.Num() + 1);
		}
	}

	Super::AddPlayerState(PlayerState);
}

void AUTGameState::CompactSpectatingIDs()
{
	if (Role == ROLE_Authority)
	{
		// get sorted list of UTPlayerStates that have been assigned an ID
		TArray<AUTPlayerState*> PlayerArrayCopy;
		for (APlayerState* PS : PlayerArray)
		{
			AUTPlayerState* UTPS = Cast<AUTPlayerState>(PS);
			if (UTPS != NULL && UTPS->SpectatingID > 0)
			{
				PlayerArrayCopy.Add(UTPS);
			}
		}
		PlayerArrayCopy.Sort([](const AUTPlayerState& A, const AUTPlayerState& B) -> bool
		{
			return A.SpectatingID < B.SpectatingID;
		});

		// fill in gaps from IDs at the end of the list
		for (int32 i = 0; i < PlayerArrayCopy.Num(); i++)
		{
			if (PlayerArrayCopy[i]->SpectatingID != uint8(i + 1))
			{
				AUTPlayerState* MovedPS = PlayerArrayCopy.Pop(false);
				MovedPS->SpectatingID = uint8(i + 1);
				PlayerArrayCopy.Insert(MovedPS, i);
			}
		}

		// now do the same for SpectatingIDTeam
		for (AUTTeamInfo* Team : Teams)
		{
			PlayerArrayCopy.Reset();
			const TArray<AController*> Members = Team->GetTeamMembers();
			for (AController* C : Members)
			{
				AUTPlayerState* UTPS = Cast<AUTPlayerState>(C->PlayerState);
				if (UTPS != NULL && UTPS->SpectatingIDTeam)
				{
					PlayerArrayCopy.Add(UTPS);
				}
			}
			PlayerArrayCopy.Sort([](const AUTPlayerState& A, const AUTPlayerState& B) -> bool
			{
				return A.SpectatingIDTeam < B.SpectatingIDTeam;
			});

			for (int32 i = 0; i < PlayerArrayCopy.Num(); i++)
			{
				if (PlayerArrayCopy[i]->SpectatingIDTeam != uint8(i + 1))
				{
					AUTPlayerState* MovedPS = PlayerArrayCopy.Pop(false);
					MovedPS->SpectatingIDTeam = uint8(i + 1);
					PlayerArrayCopy.Insert(MovedPS, i);
				}
			}
		}
	}
}

int32 AUTGameState::GetMaxSpectatingId()
{
	int32 MaxSpectatingID = 0;
	for (int32 i = 0; i<PlayerArray.Num() - 1; i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerArray[i]);
		if (PS && (PS->SpectatingID > MaxSpectatingID))
		{
			MaxSpectatingID = PS->SpectatingID;
		}
	}
	return MaxSpectatingID;
}

int32 AUTGameState::GetMaxTeamSpectatingId(int32 TeamNum)
{
	int32 MaxSpectatingID = 0;
	for (int32 i = 0; i<PlayerArray.Num() - 1; i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerArray[i]);
		if (PS && (PS->GetTeamNum() == TeamNum) && (PS->SpectatingIDTeam > MaxSpectatingID))
		{
			MaxSpectatingID = PS->SpectatingIDTeam;
		}
	}
	return MaxSpectatingID;

}

void AUTGameState::AddLoadoutItem(const FLoadoutInfo& Item)
{
	FActorSpawnParameters Params;
	Params.Owner = this;
	AUTReplicatedLoadoutInfo* NewLoadoutInfo = GetWorld()->SpawnActor<AUTReplicatedLoadoutInfo>(Params);
	if (NewLoadoutInfo)
	{
		NewLoadoutInfo->ItemClass = Item.ItemClass;	
		NewLoadoutInfo->RoundMask = Item.RoundMask;
		NewLoadoutInfo->CurrentCost = Item.InitialCost;
		NewLoadoutInfo->bDefaultInclude = Item.bDefaultInclude;
		NewLoadoutInfo->bPurchaseOnly = Item.bPurchaseOnly;

		AvailableLoadout.Add(NewLoadoutInfo);
	}
}

void AUTGameState::AdjustLoadoutCost(TSubclassOf<AUTInventory> ItemClass, float NewCost)
{
	for (int32 i=0; i < AvailableLoadout.Num(); i++)
	{
		if (AvailableLoadout[i]->ItemClass == ItemClass)
		{
			AvailableLoadout[i]->CurrentCost = NewCost;
			return;
		}
	}
}

bool AUTGameState::IsTempBanned(const TSharedPtr<class FUniqueNetId>& UniqueId)
{
	for (int32 i=0; i< TempBans.Num(); i++)
	{
		if (TempBans[i]->ToString() == UniqueId->ToString())
		{
			return true;
		}
	}
	return false;
}

void AUTGameState::VoteForTempBan(AUTPlayerState* BadGuy, AUTPlayerState* Voter)
{
	AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (Game && Game->NumPlayers > 0)
	{
		BadGuy->LogBanRequest(Voter);
		Game->BroadcastLocalized(Voter, UUTGameMessage::StaticClass(), 13, Voter, BadGuy);

		float Perc = (float(BadGuy->CountBanVotes()) / float(Game->NumPlayers)) * 100.0f;
		BadGuy->KickPercent = int8(Perc);
		UE_LOG(UT,Log,TEXT("VoteForTempBan %f %i %i %i"),Perc,BadGuy->KickPercent,BadGuy->CountBanVotes(),Game->NumPlayers);

		if ( Perc >=  KickThreshold )
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(BadGuy->GetOwner());
			if (PC)
			{
				AUTGameSession* GS = Cast<AUTGameSession>(Game->GameSession);
				if (GS)
				{
					GS->KickPlayer(PC,NSLOCTEXT("UTGameState","TempKickBan","The players on this server have decided you need to leave."));
					TempBans.Add(BadGuy->UniqueId.GetUniqueNetId());				
				}
			}
		}
	}
}

void AUTGameState::GetAvailableGameData(TArray<UClass*>& GameModes, TArray<UClass*>& MutatorList)
{
	for (TObjectIterator<UClass> It; It; ++It)
	{
		// non-native classes are detected by asset search even if they're loaded for consistency
		if (!It->HasAnyClassFlags(CLASS_Abstract | CLASS_HideDropDown) && It->HasAnyClassFlags(CLASS_Native))
		{
			if (It->IsChildOf(AUTGameMode::StaticClass()))
			{
				if (!It->GetDefaultObject<AUTGameMode>()->bHideInUI)
				{
					GameModes.Add(*It);
				}
			}
			else if (It->IsChildOf(AUTMutator::StaticClass()) && !It->GetDefaultObject<AUTMutator>()->DisplayName.IsEmpty())
			{
				MutatorList.Add(*It);
			}
		}
	}

	{
		TArray<FAssetData> AssetList;
		GetAllBlueprintAssetData(AUTGameMode::StaticClass(), AssetList);
		for (const FAssetData& Asset : AssetList)
		{
			static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
			const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
			if (ClassPath != NULL)
			{
				UClass* TestClass = LoadObject<UClass>(NULL, **ClassPath);
				if (TestClass != NULL && !TestClass->HasAnyClassFlags(CLASS_Abstract) && TestClass->IsChildOf(AUTGameMode::StaticClass()) && !TestClass->GetDefaultObject<AUTGameMode>()->bHideInUI)
				{
					GameModes.AddUnique(TestClass);
				}
			}
		}
	}

	{
		TArray<FAssetData> AssetList;
		GetAllBlueprintAssetData(AUTMutator::StaticClass(), AssetList);
		for (const FAssetData& Asset : AssetList)
		{
			static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
			const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
			if (ClassPath != NULL)
			{
				UClass* TestClass = LoadObject<UClass>(NULL, **ClassPath);
				if (TestClass != NULL && !TestClass->HasAnyClassFlags(CLASS_Abstract) && TestClass->IsChildOf(AUTMutator::StaticClass()) && !TestClass->GetDefaultObject<AUTMutator>()->DisplayName.IsEmpty())
				{
					MutatorList.AddUnique(TestClass);
				}
			}
		}
	}
}

void AUTGameState::GetAvailableMaps(const TArray<FString>& AllowedMapPrefixes, TArray<TSharedPtr<FMapListItem>>& MapList)
{
	TArray<FAssetData> MapAssets;
	GetAllAssetData(UWorld::StaticClass(), MapAssets);
	for (const FAssetData& Asset : MapAssets)
	{
		FString MapPackageName = Asset.PackageName.ToString();
		// ignore /Engine/ as those aren't real gameplay maps
		// make sure expected file is really there
		if ( !MapPackageName.StartsWith(TEXT("/Engine/")) && IFileManager::Get().FileSize(*FPackageName::LongPackageNameToFilename(MapPackageName, FPackageName::GetMapPackageExtension())) > 0 )
		{
			// Look to see if this is allowed.
			bool bMapIsAllowed = AllowedMapPrefixes.Num() == 0;
			for (int32 i=0; i<AllowedMapPrefixes.Num();i++)
			{
				if ( Asset.AssetName.ToString().StartsWith(AllowedMapPrefixes[i] + TEXT("-")) )
				{
					bMapIsAllowed = true;
					break;
				}
			}

			if (bMapIsAllowed)
			{
				const FString* Title = Asset.TagsAndValues.Find(NAME_MapInfo_Title); 
				const FString* Author = Asset.TagsAndValues.Find(NAME_MapInfo_Author);
				const FString* Description = Asset.TagsAndValues.Find(NAME_MapInfo_Description);
				const FString* Screenshot = Asset.TagsAndValues.Find(NAME_MapInfo_ScreenshotReference);

				const FString* OptimalPlayerCountStr = Asset.TagsAndValues.Find(NAME_MapInfo_OptimalPlayerCount);
				int32 OptimalPlayerCount = 6;
				if (OptimalPlayerCountStr != NULL)
				{
					OptimalPlayerCount = FCString::Atoi(**OptimalPlayerCountStr);
				}

				const FString* OptimalTeamPlayerCountStr = Asset.TagsAndValues.Find(NAME_MapInfo_OptimalTeamPlayerCount);
				int32 OptimalTeamPlayerCount = 10;
				if (OptimalTeamPlayerCountStr != NULL)
				{
					OptimalTeamPlayerCount = FCString::Atoi(**OptimalTeamPlayerCountStr);
				}

				MapList.Add(MakeShareable(new FMapListItem( Asset.PackageName.ToString(), (Title != NULL && !Title->IsEmpty()) ? *Title : *Asset.AssetName.ToString(), (Author != NULL) ? *Author : FString(),
												(Description != NULL) ? *Description : FString(), (Screenshot != NULL) ? *Screenshot : FString(), OptimalPlayerCount, OptimalTeamPlayerCount)));
			}

		}
	}
}

void AUTGameState::CreateMapVoteInfo(const FString& MapPackage,const FString& MapTitle, const FString& MapScreenshotReference)
{
	FActorSpawnParameters Params;
	Params.Owner = this;
	AUTReplicatedMapVoteInfo* MapVoteInfo = GetWorld()->SpawnActor<AUTReplicatedMapVoteInfo>(Params);
	if (MapVoteInfo)
	{
		MapVoteInfo->MapPackage = MapPackage;
		MapVoteInfo->MapTitle = MapTitle;
		MapVoteInfo->MapScreenshotReference = MapScreenshotReference;
		MapVoteList.Add(MapVoteInfo);
	}
}

void AUTGameState::SortVotes()
{
	for (int32 i=0; i<MapVoteList.Num()-1; i++)
	{
		AUTReplicatedMapVoteInfo* V1 = Cast<AUTReplicatedMapVoteInfo>(MapVoteList[i]);
		for (int32 j=i+1; j<MapVoteList.Num(); j++)
		{
			AUTReplicatedMapVoteInfo* V2 = Cast<AUTReplicatedMapVoteInfo>(MapVoteList[j]);
			if( V2->VoteCount > V1->VoteCount )
			{
				MapVoteList[i] = V2;
				MapVoteList[j] = V1;
				V1 = V2;
			}
		}
	}
}

bool AUTGameState::GetImportantPickups_Implementation(TArray<AUTPickup*>& PickupList)
{
	for (FActorIterator It(GetWorld()); It; ++It)
	{
		AUTPickup* Pickup = Cast<AUTPickup>(*It);
		AUTPickupInventory* PickupInventory = Cast<AUTPickupInventory>(*It);

		if ((PickupInventory && PickupInventory->GetInventoryType() && PickupInventory->GetInventoryType()->GetDefaultObject<AUTInventory>()->bShowPowerupTimer
			&& ((PickupInventory->GetInventoryType()->GetDefaultObject<AUTInventory>()->HUDIcon.Texture != NULL) || PickupInventory->GetInventoryType()->IsChildOf(AUTArmor::StaticClass())))
			|| (Pickup && Pickup->bDelayedSpawn && Pickup->RespawnTime >= 0.0f && Pickup->HUDIcon.Texture != nullptr))
		{
			if (!Pickup->bOverride_TeamSide)
			{
				Pickup->TeamSide = NearestTeamSide(Pickup);
			}
			PickupList.Add(Pickup);
		}
	}

	//Sort the list by by respawn time 
	//TODO: powerup priority so different armors sort properly
	PickupList.Sort([](const AUTPickup& A, const AUTPickup& B) -> bool
	{
		return A.RespawnTime > B.RespawnTime;
	});

	return true;
}

void AUTGameState::OnRep_ServerSessionId()
{
	UUTGameEngine* GameEngine = Cast<UUTGameEngine>(GEngine);
	if (GameEngine && ServerSessionId != TEXT(""))
	{
		for(auto It = GameEngine->GetLocalPlayerIterator(GetWorld()); It; ++It)
		{
			UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(*It);
			if (LocalPlayer)
			{
				LocalPlayer->VerifyGameSession(ServerSessionId);
			}
		}
	}

}

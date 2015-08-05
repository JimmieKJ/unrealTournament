// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTShowdownGame.h"
#include "UTTimerMessage.h"
#include "UTShowdownGameMessage.h"
#include "UTHUD_Showdown.h"
#include "UTShowdownGameState.h"

AUTShowdownGame::AUTShowdownGame(const FObjectInitializer& OI)
: Super(OI)
{
	ExtraHealth = 100;
	bForceRespawn = false;
	DisplayName = NSLOCTEXT("UTGameMode", "Showdown", "Showdown");
	TimeLimit = 2.0f; // per round
	GoalScore = 5;
	SpawnSelectionTime = 5;
	PowerupDuration = 15.0f;
	bHasRespawnChoices = false; // unique system
	HUDClass = AUTHUD_Showdown::StaticClass();
	GameStateClass = AUTShowdownGameState::StaticClass();
}

void AUTShowdownGame::StartNewRound()
{
	LastRoundWinner = NULL;
	// reset everything
	for (FActorIterator It(GetWorld()); It; ++It)
	{
		// pickups spawn at start, don't respawn
		AUTPickup* Pickup = Cast<AUTPickup>(*It);
		if (Pickup != NULL)
		{
			Pickup->bDelayedSpawn = false;
			Pickup->RespawnTime = 0.0f;
		}
		if (It->GetClass()->ImplementsInterface(UUTResetInterface::StaticClass()))
		{
			IUTResetInterface::Execute_Reset(*It);
		}
	}

	// respawn players
	bAllowPlayerRespawns = true;
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		AController* C = It->Get();
		if (C != NULL && C->GetPawn() == NULL && C->PlayerState != NULL && !C->PlayerState->bOnlySpectator)
		{
			RestartPlayer(*It);
		}
	}
	bAllowPlayerRespawns = false;

	UTGameState->SetTimeLimit(TimeLimit);

	AnnounceMatchStart();
}

void AUTShowdownGame::HandleMatchHasStarted()
{
	if (UTGameState != NULL)
	{
		UTGameState->CompactSpectatingIDs();
	}
	UTGameState->SetTimeLimit(TimeLimit);
	bFirstBloodOccurred = false;

	GameSession->HandleMatchHasStarted();

	// Make sure level streaming is up to date before triggering NotifyMatchStarted
	GEngine->BlockTillLevelStreamingCompleted(GetWorld());

	// First fire BeginPlay, if we haven't already in waiting to start match
	GetWorldSettings()->NotifyBeginPlay();

	// Then fire off match started
	GetWorldSettings()->NotifyMatchStarted();

	if (IsHandlingReplays() && GetGameInstance() != nullptr)
	{
		GetGameInstance()->StartRecordingReplay(GetWorld()->GetMapName(), GetWorld()->GetMapName());
	}

	SetMatchState(MatchState::MatchIntermission);
	Cast<AUTShowdownGameState>(GameState)->IntermissionStageTime = 1;
}

void AUTShowdownGame::SetPlayerDefaults(APawn* PlayerPawn)
{
	AUTCharacter* UTC = Cast<AUTCharacter>(PlayerPawn);
	if (UTC != NULL)
	{
		UTC->HealthMax = UTC->GetClass()->GetDefaultObject<AUTCharacter>()->HealthMax + ExtraHealth;
		UTC->SuperHealthMax = UTC->GetClass()->GetDefaultObject<AUTCharacter>()->SuperHealthMax + ExtraHealth;
	}
	Super::SetPlayerDefaults(PlayerPawn);
}

void AUTShowdownGame::ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
	if (GetMatchState() != MatchState::MatchIntermission && (TimeLimit <= 0 || UTGameState->RemainingTime > 0))
	{
		if (Other != NULL)
		{
			AUTPlayerState* OtherPS = Cast<AUTPlayerState>(Other->PlayerState);
			if (OtherPS != NULL && OtherPS->Team != NULL)
			{
				AUTPlayerState* KillerPS = (Killer != NULL && Killer != Other) ? Cast<AUTPlayerState>(Killer->PlayerState) : NULL;
				AUTTeamInfo* KillerTeam = (KillerPS != NULL) ? KillerPS->Team : Teams[1 - FMath::Min<int32>(1, OtherPS->Team->TeamIndex)];
				KillerTeam->Score += 1;

				if (LastRoundWinner == NULL)
				{
					LastRoundWinner = KillerTeam;
				}
				else if (LastRoundWinner != KillerTeam)
				{
					LastRoundWinner = NULL; // both teams got a point so nobody won
				}

				// this is delayed so mutual kills can happen
				SetTimerUFunc(this, FName(TEXT("StartIntermission")), 1.0f, false);
			}
		}
		AUTTeamGameMode::ScoreKill_Implementation(Killer, Other, KilledPawn, DamageType);
	}
}

void AUTShowdownGame::CheckGameTime()
{
	static FName NAME_StartIntermission(TEXT("StartIntermission"));
	if (IsMatchInProgress() && !HasMatchEnded() && TimeLimit > 0 && UTGameState->RemainingTime <= 0 && !IsTimerActiveUFunc(this, NAME_StartIntermission))
	{
		// end round; player with highest health + armor wins
		TArray< AUTPlayerState*, TInlineAllocator<2> > AlivePlayers;
		AUTPlayerState* RoundWinner = NULL;
		int32 BestTotalHealth = 0;
		bool bTied = false;
		for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
		{
			AUTCharacter* UTC = Cast<AUTCharacter>(It->Get());
			if (UTC != NULL && !UTC->IsDead())
			{
				AUTPlayerState* PS = Cast<AUTPlayerState>(UTC->PlayerState);
				if (PS != NULL)
				{
					AlivePlayers.Add(PS);
					if (UTC->Health > BestTotalHealth)
					{
						RoundWinner = PS;
						BestTotalHealth = UTC->Health;
					}
					else if (UTC->Health == BestTotalHealth)
					{
						bTied = true;
					}
				}
			}
		}
		if (bTied)
		{
			// both players score a point
			for (AUTPlayerState* PS : AlivePlayers)
			{
				if (PS != NULL)
				{
					PS->Score += 1.0f;
					if (PS->Team != NULL)
					{
						PS->Team->Score += 1.0f;
					}
				}
			}
			LastRoundWinner = NULL;
			BroadcastLocalized(NULL, UUTShowdownGameMessage::StaticClass(), 1);
		}
		else
		{
			RoundWinner->Score += 1.0f;
			if (RoundWinner->Team != NULL)
			{
				RoundWinner->Team->Score += 1.0f;
			}
			LastRoundWinner = RoundWinner->Team;
			BroadcastLocalized(NULL, UUTShowdownGameMessage::StaticClass(), 0, RoundWinner);
		}
		SetTimerUFunc(this, NAME_StartIntermission, 2.0f, false);
	}
}

void AUTShowdownGame::StartIntermission()
{
	ClearTimerUFunc(this, FName(TEXT("StartIntermission")));
	if (!HasMatchEnded())
	{
		// if there's not enough time for a new round to work, then award victory now
		uint32 bTied = 0;
		AUTPlayerState* Winner = NULL;
		Winner = IsThereAWinner(bTied);
		if (Winner != NULL && !bTied && ((Winner->Team == NULL) ? (Winner->Score >= GoalScore) : (Winner->Team->Score >= GoalScore)))
		{
			EndGame(Winner, TEXT("scorelimit"));
		}
		else
		{
			SetMatchState(MatchState::MatchIntermission);
		}
	}
}

AActor* AUTShowdownGame::ChoosePlayerStart_Implementation(AController* Player)
{
	// if they pre-selected, apply it
	AUTPlayerState* UTPS = Cast<AUTPlayerState>((Player != NULL) ? Player->PlayerState : NULL);
	if (UTPS != NULL && UTPS->RespawnChoiceA != nullptr)
	{
		return UTPS->RespawnChoiceA;
	}
	else
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}
}

void AUTShowdownGame::HandleMatchIntermission()
{
	AUTShowdownGameState* GS = Cast<AUTShowdownGameState>(GameState);

	GS->IntermissionStageTime = 3;
	GS->bFinalIntermissionDelay = false;
	RemainingPicks.Empty();
	GS->SpawnSelector = NULL;
	GS->RemainingMinute = 0;
	GS->RemainingTime = 0;
	// reset timer for consistency
	GetWorldTimerManager().SetTimer(TimerHandle_DefaultTimer, this, &AUTGameMode::DefaultTimer, GetWorldSettings()->GetEffectiveTimeDilation() / GetWorldSettings()->DemoPlayTimeDilation, true);

	// reset pickups in advance so they show up on the spawn previews
	for (FActorIterator It(GetWorld()); It; ++It)
	{
		AUTPickup* Pickup = Cast<AUTPickup>(*It);
		if (Pickup != NULL)
		{
			checkSlow(Pickup->GetClass()->ImplementsInterface(UUTResetInterface::StaticClass()));
			IUTResetInterface::Execute_Reset(Pickup);
		}
	}

	// give players spawn point selection
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		AController* C = It->Get();
		if (C != NULL)
		{
			APawn* P = C->GetPawn();
			if (P != NULL)
			{
				C->UnPossess();
				P->Destroy();
			}
			AUTPlayerState* PS = Cast<AUTPlayerState>(C->PlayerState);
			if (PS != NULL && !PS->bOnlySpectator && PS->Team != NULL)
			{
				PS->RespawnChoiceA = NULL;
				PS->RespawnChoiceB = NULL;
				RemainingPicks.Add(PS);
			}
		}
	}
	if (LastRoundWinner != NULL)
	{
		RemainingPicks.Sort([=](const AUTPlayerState& A, const AUTPlayerState& B){ return A.Team != LastRoundWinner || B.Team == LastRoundWinner; });
	}
	else
	{
		// if last round didn't have a winner, pick based on current score
		RemainingPicks.Sort([=](const AUTPlayerState& A, const AUTPlayerState& B){ return A.Team->Score > B.Team->Score; });
	}
}

void AUTShowdownGame::CallMatchStateChangeNotify()
{
	if (GetMatchState() == MatchState::MatchIntermission)
	{
		HandleMatchIntermission();
	}
	else if (GetMatchState() == MatchState::InProgress && GetWorld()->bMatchStarted)
	{
		StartNewRound();
	}
	else
	{
		Super::CallMatchStateChangeNotify();
	}
}

void AUTShowdownGame::DefaultTimer()
{
	if (GetMatchState() == MatchState::MatchIntermission)
	{
		AUTShowdownGameState* GS = Cast<AUTShowdownGameState>(GameState);

		if (GS->SpawnSelector != NULL && GS->SpawnSelector->RespawnChoiceA != NULL)
		{
			GS->IntermissionStageTime = 0;
		}
		else if (GS->IntermissionStageTime > 0)
		{
			GS->IntermissionStageTime--;
		}
		if (GS->IntermissionStageTime == 0)
		{
			if (GS->SpawnSelector != NULL)
			{
				if (GS->SpawnSelector->RespawnChoiceA == NULL)
				{
					GS->SpawnSelector->RespawnChoiceA = Cast<APlayerStart>(FindPlayerStart(Cast<AController>(GS->SpawnSelector->GetOwner())));
				}
				RemainingPicks.Remove(GS->SpawnSelector);
				GS->SpawnSelector = NULL;
			}
			// make sure we don't have any stale entries from quitters
			for (int32 i = RemainingPicks.Num() - 1; i >= 0; i--)
			{
				if (RemainingPicks[i] == NULL || RemainingPicks[i]->bPendingKillPending)
				{
					RemainingPicks.RemoveAt(i);
				}
			}
			if (RemainingPicks.Num() > 0)
			{
				GS->SpawnSelector = RemainingPicks[0];
				GS->IntermissionStageTime = FMath::Max<uint8>(1, SpawnSelectionTime);
			}
			else if (!GS->bFinalIntermissionDelay)
			{
				GS->bFinalIntermissionDelay = true;
				GS->IntermissionStageTime = 3;
			}
			else
			{
				GS->bFinalIntermissionDelay = false;
				SetMatchState(MatchState::InProgress);
			}
		}
		if (GS->bFinalIntermissionDelay)
		{
			BroadcastLocalized(NULL, UUTTimerMessage::StaticClass(), int32(GS->IntermissionStageTime) - 1);
		}
	}
	else
	{
		Super::DefaultTimer();
	}
}
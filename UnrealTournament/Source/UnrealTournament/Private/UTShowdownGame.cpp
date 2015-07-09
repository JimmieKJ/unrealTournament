// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTShowdownGame.h"
#include "UTTimerMessage.h"
#include "UTShowdownGameMessage.h"

AUTShowdownGame::AUTShowdownGame(const FObjectInitializer& OI)
: Super(OI)
{
	ExtraHealth = 100;
	bForceRespawn = false;
	DisplayName = NSLOCTEXT("UTGameMode", "Showdown", "Showdown");
	TimeLimit = 2.0f; // per round
	GoalScore = 5;
	PowerupDuration = 15.0f;
}

void AUTShowdownGame::StartNewRound()
{
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

	bHasRespawnChoices = false;

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

	StartNewRound();
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
		bHasRespawnChoices = false; // make sure this is set so choices aren't displayed early
		if (Other != NULL)
		{
			AUTPlayerState* OtherPS = Cast<AUTPlayerState>(Other->PlayerState);
			if (OtherPS != NULL && OtherPS->Team != NULL)
			{
				AUTPlayerState* KillerPS = (Killer != NULL && Killer != Other) ? Cast<AUTPlayerState>(Killer->PlayerState) : NULL;
				AUTTeamInfo* KillerTeam = (KillerPS != NULL) ? KillerPS->Team : Teams[1 - FMath::Min<int32>(1, OtherPS->Team->TeamIndex)];
				KillerTeam->Score += 1;

				// this is delayed so mutual kills can happen
				SetTimerUFunc(this, FName(TEXT("StartIntermission")), 1.0f, false);
			}
		}
		AUTTeamGameMode::ScoreKill_Implementation(Killer, Other, KilledPawn, DamageType);
	}
}

void AUTShowdownGame::CheckGameTime()
{
	if (IsMatchInProgress() && !HasMatchEnded() && TimeLimit > 0 && UTGameState->RemainingTime <= 0)
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
			BroadcastLocalized(NULL, UUTShowdownGameMessage::StaticClass(), 1);
		}
		else
		{
			RoundWinner->Score += 1.0f;
			if (RoundWinner->Team != NULL)
			{
				RoundWinner->Team->Score += 1.0f;
			}
			BroadcastLocalized(NULL, UUTShowdownGameMessage::StaticClass(), 0, RoundWinner);
		}
		SetTimerUFunc(this, FName(TEXT("StartIntermission")), 2.0f, false);
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
	AUTPlayerState* UTPS = Cast<AUTPlayerState>(Player->PlayerState);
	if (bHasRespawnChoices && UTPS->RespawnChoiceA != nullptr && UTPS->RespawnChoiceB != nullptr)
	{
		if (UTPS->bChosePrimaryRespawnChoice)
		{
			return UTPS->RespawnChoiceA;
		}
		else
		{
			return UTPS->RespawnChoiceB;
		}
	}

	// since we only allow respawning between rounds, skip all the traces and just give a random unique spawn point not in any other player's respawn choices

	TArray<APlayerStart*> PlayerStarts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		PlayerStarts.Add(*It);
	}

	if (PlayerStarts.Num() < NumPlayers * 2) // min to not have dupes
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}
	else
	{
		APlayerStart* Pick = NULL;
		bool bTaken;
		int32 Tries = 0; // just in case
		do
		{
			bTaken = false;
			Pick = PlayerStarts[FMath::RandHelper(PlayerStarts.Num())];
			for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
			{
				AController* C = It->Get();
				if (C != NULL)
				{
					AUTPlayerState* PS = Cast<AUTPlayerState>(C->PlayerState);
					if (PS != NULL && !PS->bOnlySpectator && (PS->RespawnChoiceA == Pick || PS->RespawnChoiceB == Pick))
					{
						bTaken = true;
						break;
					}
				}
			}
		} while (bTaken && ++Tries < 100);

		return Pick;
	}
}

void AUTShowdownGame::HandleMatchIntermission()
{
	IntermissionTimeRemaining = 6;
	// reset timer for consistency
	GetWorldTimerManager().SetTimer(TimerHandle_DefaultTimer, this, &AUTGameMode::DefaultTimer, GetWorldSettings()->GetEffectiveTimeDilation(), true);

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
	bHasRespawnChoices = true;
	TArray<AController*> Players;
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
			if (PS != NULL && !PS->bOnlySpectator)
			{
				PS->RespawnChoiceA = nullptr;
				PS->RespawnChoiceB = nullptr;
				PS->bChosePrimaryRespawnChoice = true;
				Players.Add(C);
			}
		}
	}

	// TODO: better idea: player who died gets to choose first, then second player chooses from what remains, both players know spawn points in advance
	for (AController* C : Players)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(C->PlayerState);
		PS->RespawnChoiceA = Cast<APlayerStart>(ChoosePlayerStart(C));
	}
	for (AController* C : Players)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(C->PlayerState);
		PS->RespawnChoiceB = Cast<APlayerStart>(ChoosePlayerStart(C));
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
		IntermissionTimeRemaining--;
		if (IntermissionTimeRemaining <= 0)
		{
			SetMatchState(MatchState::InProgress);
		}
		else if (IntermissionTimeRemaining <= 5)
		{
			BroadcastLocalized(NULL, UUTTimerMessage::StaticClass(), IntermissionTimeRemaining - 1);
		}
	}
	else
	{
		Super::DefaultTimer();
	}
}
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/GameMode.h"
#include "UTDeathMessage.h"
#include "UTGameMessage.h"
#include "UTVictoryMessage.h"
#include "UTTimedPowerup.h"
#include "UTCountDownMessage.h"
#include "UTFirstBloodMessage.h"
#include "UTMutator.h"
#include "UTScoreboard.h"
#include "Slate.h"
#include "UTAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"

UUTResetInterface::UUTResetInterface(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{}

namespace MatchState
{
	const FName CountdownToBegin = FName(TEXT("CountdownToBegin"));
	const FName MatchEnteringOvertime = FName(TEXT("MatchEnteringOvertime"));
	const FName MatchIsInOvertime = FName(TEXT("MatchIsInOvertime"));
}

AUTGameMode::AUTGameMode(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	// set default pawn class to our Blueprinted character

	static ConstructorHelpers::FObjectFinder<UClass> PlayerPawnObject(TEXT("Class'/Game/RestrictedAssets/Blueprints/WIP/Steve/SteveUTCharacter.SteveUTCharacter_C'"));
	
	if (PlayerPawnObject.Object != NULL)
	{
		DefaultPawnClass = (UClass*)PlayerPawnObject.Object;
	}

	// use our custom HUD class
	HUDClass = AUTHUD::StaticClass();

	GameStateClass = AUTGameState::StaticClass();
	PlayerStateClass = AUTPlayerState::StaticClass();

	PlayerControllerClass = AUTPlayerController::StaticClass();
	MinRespawnDelay = 1.5f;
	bUseSeamlessTravel = false;
	CountDown = 4;
	bPauseable = false;
	RespawnWaitTime = 1.5f;
	bPlayersMustBeReady = false;
	MinPlayersToStart = 1;
	EndScoreboardDelay = 2.0f;
	VictoryMessageClass=UUTVictoryMessage::StaticClass();
	DeathMessageClass=UUTDeathMessage::StaticClass();
	GameMessageClass=UUTGameMessage::StaticClass();

	DefaultPlayerName = FString("Malcolm");
	MapPrefix = TEXT("DM");
}


void AUTGameMode::BeginPlayMutatorHack(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;

	// WARNING: 'this' is actually an AActor! Only do AActor things!
	if (!IsA(ALevelScriptActor::StaticClass()) && !IsA(AUTMutator::StaticClass()) &&
		(RootComponent == NULL || RootComponent->Mobility != EComponentMobility::Static || (!IsA(AStaticMeshActor::StaticClass()) && !IsA(ALight::StaticClass()))) )
	{
		AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
		// a few type checks being AFTER the CheckRelevance() call is intentional; want mutators to be able to modify, but not outright destroy
		if (Game != NULL && Game != this && !Game->CheckRelevance((AActor*)this) && !IsA(APlayerController::StaticClass()))
		{
			Destroy();
		}
	}
}

// Parse options for this game...
void AUTGameMode::InitGame( const FString& MapName, const FString& Options, FString& ErrorMessage )
{
	// HACK: workaround to inject CheckRelevance() into the BeginPlay sequence
	UFunction* Func = AActor::GetClass()->FindFunctionByName(FName(TEXT("ReceiveBeginPlay")));
	Func->FunctionFlags |= FUNC_Native;
	Func->SetNativeFunc((Native)&AUTGameMode::BeginPlayMutatorHack);

	UE_LOG(UT,Log,TEXT("==============="));
	UE_LOG(UT,Log,TEXT("  Init Game"));
	UE_LOG(UT,Log,TEXT("==============="));

	Super::InitGame(MapName, Options, ErrorMessage);

	GameDifficulty = FMath::Max(0, GetIntOption(Options, TEXT("Difficulty"), GameDifficulty));

	FString InOpt = ParseOption(Options, TEXT("ForceRespawn"));
	bForceRespawn = EvalBoolOptions(InOpt, bForceRespawn);

	InOpt = ParseOption(Options, TEXT("MustBeReady"));
	bPlayersMustBeReady = EvalBoolOptions(InOpt, bPlayersMustBeReady);

	TimeLimit = FMath::Max(0,GetIntOption( Options, TEXT("TimeLimit"), TimeLimit ));
	TimeLimit *= 60;

	// Set goal score to end match.
	GoalScore = FMath::Max(0,GetIntOption( Options, TEXT("GoalScore"), GoalScore ));

	MinPlayersToStart = FMath::Max(1, GetIntOption( Options, TEXT("MinPlayers"), MinPlayersToStart));

	RespawnWaitTime = FMath::Max(0,GetIntOption( Options, TEXT("RespawnWait"), RespawnWaitTime ));

	InOpt = ParseOption(Options, TEXT("bRequirePassword"));
	bRequirePassword= EvalBoolOptions(InOpt, bRequirePassword);
	if (bRequirePassword)
	{
		InOpt = ParseOption(Options, TEXT("ServerPassword"));
		if (!InOpt.IsEmpty())
		{
			ServerPassword = InOpt;
		}

		if (ServerPassword.IsEmpty())
		{
			bRequirePassword = false;
		}
	}

	UE_LOG(UT,Log,TEXT("Password: %i %s"), bRequirePassword, ServerPassword.IsEmpty() ? TEXT("NONE") : *ServerPassword)

	for (int32 i = 0; i < BuiltInMutators.Num(); i++)
	{
		AddMutatorClass(BuiltInMutators[i]);
	}
	
	for (int32 i = 0; i < ConfigMutators.Num(); i++)
	{
		AddMutator(ConfigMutators[i]);
	}

	InOpt = ParseOption(Options, TEXT("Mutator"));
	if (InOpt.Len() > 0)
	{
		UE_LOG(UT, Log, TEXT("Mutators: %s"), *InOpt);
		while (InOpt.Len() > 0)
		{
			FString LeftOpt;
			int32 Pos = InOpt.Find(TEXT(","));
			if (Pos > 0)
			{
				LeftOpt = InOpt.Left(Pos);
				InOpt = InOpt.Right(InOpt.Len() - Pos - 1);
			}
			else
			{
				LeftOpt = InOpt;
				InOpt.Empty();
			}
			AddMutator(LeftOpt);
		}
	}

	InOpt = ParseOption(Options, TEXT("Playerclass"));
	if (InOpt.Len() > 0)
	{
		UE_LOG(UT, Log, TEXT("Set PlayerClass: %s"), *InOpt);

		// Note that to specify the normal SteveUTCharacter, the command is ?Playerclass=/game/restrictedassets/blueprints/wip/steve/SteveUTCharacter.SteveUTCharacter_c
		// allow remapping using the GameModeClassName array in Game.ini
		InOpt = AGameMode::StaticGetFullGameClassName(InOpt);
		TSubclassOf<AUTCharacter> CharClass = LoadClass<AUTCharacter>(NULL, *InOpt, NULL, 0, NULL);
		if (CharClass)
		{
			UE_LOG(UT, Log, TEXT("FOUND PlayerClass: %s"), *InOpt);
			DefaultPawnClass = CharClass;
		}
	}

	PostInitGame(Options);
}

void AUTGameMode::AddMutator(const FString& MutatorPath)
{
	int32 PeriodIndex = INDEX_NONE;
	if (MutatorPath.Right(2) != FString(TEXT("_C")) && MutatorPath.FindChar(TCHAR('.'), PeriodIndex))
	{
		FName MutatorModuleName = FName(*MutatorPath.Left(PeriodIndex));
		if (!FModuleManager::Get().IsModuleLoaded(MutatorModuleName))
		{
			if (!FModuleManager::Get().LoadModule(MutatorModuleName).IsValid())
			{
				UE_LOG(UT, Warning, TEXT("Failed to load module for mutator %s"), *MutatorModuleName.ToString());
			}
		}
	}
	AddMutatorClass(LoadClass<AUTMutator>(NULL, *MutatorPath, NULL, 0, NULL));
}

void AUTGameMode::AddMutatorClass(TSubclassOf<AUTMutator> MutClass)
{
	if (MutClass != NULL && AllowMutator(MutClass))
	{
		AUTMutator* NewMut = GetWorld()->SpawnActor<AUTMutator>(MutClass);
		if (NewMut != NULL)
		{
			NewMut->Init(OptionsString);
			if (BaseMutator == NULL)
			{
				BaseMutator = NewMut;
			}
			else
			{
				BaseMutator->AddMutator(NewMut);
			}
		}
	}
}

bool AUTGameMode::AllowMutator(TSubclassOf<AUTMutator> MutClass)
{
	const AUTMutator* DefaultMut = MutClass.GetDefaultObject();

	for (AUTMutator* Mut = BaseMutator; Mut != NULL; Mut = Mut->NextMutator)
	{
		if (Mut->GetClass() == MutClass)
		{
			// already have this exact mutator
			UE_LOG(UT, Log, TEXT("Rejected mutator %s - already have one"), *MutClass->GetPathName());
			return false;
		}
		for (int32 i = 0; i < Mut->GroupNames.Num(); i++)
		{
			for (int32 j = 0; j < DefaultMut->GroupNames.Num(); j++)
			{
				if (Mut->GroupNames[i] == DefaultMut->GroupNames[j])
				{
					// group conflict
					UE_LOG(UT, Log, TEXT("Rejected mutator %s - already have mutator %s with group %s"), *MutClass->GetPathName(), *Mut->GetPathName(), *Mut->GroupNames[i].ToString());
					return false;
				}
			}
		}
	}

	return true;
}

void AUTGameMode::InitGameState()
{
	Super::InitGameState();

	UTGameState = Cast<AUTGameState>(GameState);
	if (UTGameState != NULL)
	{
		UTGameState->SetGoalScore(GoalScore);
		UTGameState->SetTimeLimit(0);
		UTGameState->RespawnWaitTime = RespawnWaitTime;
		UTGameState->bPlayerMustBeReady = bPlayersMustBeReady;
		UTGameState->bTeamGame = bTeamGame;
	}
	else
	{
		UE_LOG(UT,Error, TEXT("UTGameState is NULL %s"), *GameStateClass->GetFullName());
	}

	if (GameSession != NULL && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		GameSession->RegisterServer();
	}
}

void AUTGameMode::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	// Because of the behavior changes to PostBeginPlay() this really has to go here as PreInitializeCompoennts is sort of the UE4 PBP even
	// though PBP still exists.  It can't go in InitGame() or InitGameState() because team info needed for team locked GameObjectives are not
	// setup at that point.

	for (TActorIterator<AUTGameObjective> ObjIt(GetWorld()); ObjIt; ++ObjIt)
	{
		ObjIt->InitializeObjective();	
		GameObjectiveInitialized(*ObjIt);
	}
}

void AUTGameMode::GameObjectiveInitialized(AUTGameObjective* Obj)
{
	// Allow subclasses to track game objectives as they are initialized
}


APlayerController* AUTGameMode::Login(class UPlayer* NewPlayer, const FString& Portal, const FString& Options, const TSharedPtr<class FUniqueNetId>& UniqueId, FString& ErrorMessage)
{
	FString ModdedPortal = Portal;
	FString ModdedOptions = Options;
	if (BaseMutator != NULL)
	{
		BaseMutator->ModifyLogin(ModdedPortal, ModdedOptions);
	}
	return Super::Login(NewPlayer, Portal, Options, UniqueId, ErrorMessage);
}

/**
 *	DefaultTimer is called once per second and is useful for consistent timed events that don't require to be 
 *  done every frame.
 **/
void AUTGameMode::DefaultTimer()
{	
	// Let the game see if it's time to end the match
	CheckGameTime();

	if (bForceRespawn)
	{
		for (auto It = GetWorld()->GetControllerIterator(); It; ++It)
		{
			AController* Controller = *It;
			if (Controller->IsInState(NAME_Inactive))
			{
				AUTPlayerState* PS = Cast<AUTPlayerState>(Controller->PlayerState);
				if (PS != NULL && PS->RespawnTime <= 0.0f)
				{
					RestartPlayer(Controller);
				}
			}
		}
	}
}


void AUTGameMode::Reset()
{
	Super::Reset();

	bGameEnded = false;

	//now respawn all the players
	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AController* Controller = *Iterator;
		if (Controller->PlayerState != NULL && !Controller->PlayerState->bOnlySpectator)
		{
			RestartPlayer(Controller);
		}
	}

	UTGameState->SetTimeLimit(0);
}

void AUTGameMode::RestartGame()
{
	if (HasMatchStarted())
	{
		Super::RestartGame();
	}
}

bool AUTGameMode::IsEnemy(AController * First, AController* Second)
{
	// In DM - Everyone is an enemy
	return First != Second;
}

void AUTGameMode::Killed(AController* Killer, AController* KilledPlayer, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
	// Ignore all killing when entering overtime as we kill off players and don't want it affecting their score.
	if (GetMatchState() != MatchState::MatchEnteringOvertime)
	{
		AUTPlayerState* const KillerPlayerState = Killer ? Cast<AUTPlayerState>(Killer->PlayerState) : NULL;
		AUTPlayerState* const KilledPlayerState = KilledPlayer ? Cast<AUTPlayerState>(KilledPlayer->PlayerState) : NULL;

		UE_LOG(UT, Log, TEXT("Player Killed: %s killed %s"), (KillerPlayerState != NULL ? *KillerPlayerState->PlayerName : TEXT("NULL")), (KilledPlayerState != NULL ? *KilledPlayerState->PlayerName : TEXT("NULL")));

		bool const bEnemyKill = IsEnemy(Killer, KilledPlayer);

		if (KilledPlayerState != NULL)
		{
			KilledPlayerState->LastKillerPlayerState = KillerPlayerState;

			KilledPlayerState->IncrementDeaths(KillerPlayerState);
			TSubclassOf<UUTDamageType> UTDamage(*DamageType);
			if (UTDamage != NULL)
			{
				UTDamage.GetDefaultObject()->ScoreKill(KillerPlayerState, KilledPlayerState, KilledPawn);
			}

			ScoreKill(Killer, KilledPlayer, DamageType);
			BroadcastDeathMessage(Killer, KilledPlayer, DamageType);
		}

		DiscardInventory(KilledPawn, Killer);
		NotifyKilled(Killer, KilledPlayer, KilledPawn, DamageType);
	}
}

void AUTGameMode::NotifyKilled(AController* Killer, AController* Killed, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
}

void AUTGameMode::ScorePickup(AUTPickup* Pickup, AUTPlayerState* PickedUpBy, AUTPlayerState* LastPickedUpBy)
{
}

void AUTGameMode::ScoreDamage(int DamageAmount, AController* Victim, AController* Attacker)
{
}

void AUTGameMode::ScoreKill(AController* Killer, AController* Other, TSubclassOf<UDamageType> DamageType)
{
	if( (Killer == Other) || (Killer == NULL) )
	{
		// If it's a suicide, subtract a kill from the player...

		if (Other != NULL && Other->PlayerState != NULL && Cast<AUTPlayerState>(Other->PlayerState) != NULL)
		{
			Cast<AUTPlayerState>(Other->PlayerState)->AdjustScore(-1);
		}
	}
	else 
	{
		AUTPlayerState * KillerPlayerState = Cast<AUTPlayerState>(Killer->PlayerState);
		if ( KillerPlayerState != NULL )
		{
			KillerPlayerState->AdjustScore(+1);
			KillerPlayerState->IncrementKills(true);
			CheckScore(KillerPlayerState);
		}

		if (!bFirstBloodOccurred)
		{
			BroadcastLocalized(this, UUTFirstBloodMessage::StaticClass(), 0, KillerPlayerState, NULL, NULL);
			bFirstBloodOccurred = true;
		}
	}
}

void AUTGameMode::DiscardInventory(APawn* Other, AController* Killer)
{
	AUTCharacter* UTC = Cast<AUTCharacter>(Other);
	if (UTC != NULL)
	{
		// toss weapon
		if (UTC->GetWeapon() != NULL)
		{
			UTC->TossInventory(UTC->GetWeapon());
		}
		// toss all powerups
		AUTInventory* Inv = UTC->GetInventory();
		while (Inv != NULL)
		{
			AUTInventory* NextInv = Inv->GetNext();
			if (Inv->bAlwaysDropOnDeath)
			{
				UTC->TossInventory(Inv, FVector(FMath::FRandRange(0.0f, 200.0f), FMath::FRandRange(-400.0f, 400.0f), FMath::FRandRange(0.0f, 200.0f)));
			}
			Inv = NextInv;
		}
		// delete the rest
		UTC->DiscardAllInventory();
	}
}

bool AUTGameMode::CheckScore(AUTPlayerState* Scorer)
{
	if ( Scorer != NULL )
	{
		if ( (GoalScore > 0) && (Scorer->Score >= GoalScore) )
		{
			EndGame(Scorer,FName(TEXT("fraglimit")));
		}
	}
	return true;
}


void AUTGameMode::StartMatch()
{
	if (HasMatchStarted())
	{
		// Already started
		return;
	}
	if (GetWorld()->IsPlayInEditor())
	{
		SetMatchState(MatchState::InProgress);
	}
	else
	{
		SetMatchState(MatchState::CountdownToBegin);	
	}

	if (FUTAnalytics::IsAvailable())
	{
		if (GetWorld()->GetNetMode() != NM_Standalone)
		{
			TArray<FAnalyticsEventAttribute> ParamArray;
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("MapName"), GetWorld()->GetMapName()));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("GameName"), GetNameSafe(this)));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("GoalScore"), GoalScore));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("TimeLimit"), TimeLimit));
			FUTAnalytics::GetProvider().RecordEvent( TEXT("NewMatch"), ParamArray );
		}
	}

	if (GameSession != NULL)
	{
		AUTGameSession* UTGameSession = Cast<AUTGameSession>(GameSession);
		if (UTGameSession != NULL)
		{
			UTGameSession->StartMatch();
		}
	}

}

void AUTGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	// reset things, relevant for any kind of warmup mode and to make sure placed Actors like pickups are in their initial state no matter how much time has passed in pregame
	for (FActorIterator It(GetWorld()); It; ++It)
	{
		IUTResetInterface* ResetActor = InterfaceCast<IUTResetInterface>(*It);
		if (ResetActor != NULL)
		{
			ResetActor->Execute_Reset(*It);
		}
	}

	UTGameState->SetTimeLimit(TimeLimit);

	bFirstBloodOccurred = false;
	BroadcastLocalized( this, UUTGameMessage::StaticClass(), 0, NULL, NULL, NULL);
}

void AUTGameMode::BeginGame()
{
	UE_LOG(UT,Log,TEXT("--------------------------"));
	UE_LOG(UT,Log,TEXT("Game Has Begun "));
	UE_LOG(UT,Log,TEXT("--------------------------"));

	UE_LOG(UT,Log,TEXT("GameType: %s"), *GetNameSafe(this));
	UE_LOG(UT,Log,TEXT("Difficulty: %i"), GameDifficulty);
	UE_LOG(UT,Log,TEXT("GoalScore: %i"), GoalScore);
	UE_LOG(UT,Log,TEXT("TimeLimit: %i"), TimeLimit);
	UE_LOG(UT,Log,TEXT("Min # of Players: %i"), MinPlayersToStart);
	UE_LOG(UT,Log,TEXT("End Delays %f / %f"), EndTimeDelay, EndScoreboardDelay);

	for (FActorIterator It(GetWorld()); It; ++It)
	{
		AActor* TestActor = *It;
		if (TestActor &&
			!TestActor->IsPendingKill() &&
			TestActor->IsA<APlayerState>())
		{
			Cast<APlayerState>(TestActor)->StartTime = 0;
		}
	}

	GameState->ElapsedTime = 0;

	//Let the game session override the StartMatch function, in case it wants to wait for arbitration

	if (GameSession->HandleStartMatchRequest())
	{
		return;
	}

	SetMatchState(MatchState::InProgress);
}

void AUTGameMode::EndMatch()
{
	Super::EndMatch();
	GetWorldTimerManager().SetTimer(this, &AUTGameMode::PlayEndOfMatchMessage, 1.0f);

	for (FConstPawnIterator Iterator = GetWorld()->GetPawnIterator(); Iterator; ++Iterator )
	{
		// If a pawn is marked pending kill, *Iterator will be NULL
		APawn* Pawn = *Iterator;
		if (Pawn)
		{
			Pawn->TurnOff();
		}
	}

	if (GameSession != NULL)
	{
		AUTGameSession* UTGameSession = Cast<AUTGameSession>(GameSession);
		if (UTGameSession != NULL)
		{
			UTGameSession->EndMatch();
		}
	}

}

void AUTGameMode::EndGame(AUTPlayerState* Winner, FName Reason )
{

	// If we don't have a winner, then go and find one
	if (Winner == NULL)
	{
		for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
		{
			AController* Controller = *Iterator;
			AUTPlayerState* CPS = Cast<AUTPlayerState> (Controller->PlayerState);
			if ( CPS && ((Winner == NULL) || (CPS->Score >= Winner->Score)) )
			{
				Winner = CPS;
			}
		}
	}

	UTGameState->SetWinner(Winner);
	EndTime = GetWorld()->TimeSeconds;

	SetEndGameFocus(Winner);

	// Allow replication to happen before reporting scores, stats, etc.
	GetWorldTimerManager().SetTimer(this, &AUTGameMode::HandleMatchHasEnded, 1.5f);
	bGameEnded = true;

	// Setup a timer to pop up the final scoreboard on everyone
	GetWorldTimerManager().SetTimer(this, &AUTGameMode::ShowFinalScoreboard, EndScoreboardDelay);

	// Setup a timer to continue to the next map.

	EndTime = GetWorld()->TimeSeconds;
	GetWorldTimerManager().SetTimer(this, &AUTGameMode::TravelToNextMap, EndTimeDelay);

	EndMatch();
}

/**
 *	NOTE: This is a really simple map list.  It doesn't support multiple maps in the list, etc and is really dumb.  But it
 *  will work for now.
 **/
void AUTGameMode::TravelToNextMap()
{
	FString CurrentMapName = GetWorld()->GetMapName();

	int32 MapIndex = -1;
	for (int i=0;i<MapRotation.Num();i++)
	{
		if (MapRotation[i].EndsWith(CurrentMapName))
		{
			MapIndex = i;
			break;
		}
	}

	if (MapRotation.Num() > 0)
	{
		MapIndex = (MapIndex + 1) % MapRotation.Num();
		if (MapIndex >=0 && MapIndex < MapRotation.Num())
		{
			GetWorld()->ServerTravel(MapRotation[MapIndex],false);
			return;
		}
	}

	RestartGame();	
}

void AUTGameMode::ShowFinalScoreboard()
{
	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
		if (PC != NULL)
		{
			PC->ClientToggleScoreboard(true);
		}
	}
}

void AUTGameMode::SetEndGameFocus(AUTPlayerState* Winner)
{
	if (Winner == NULL) return; // It's possible to call this with Winner == NULL if timelimit is hit with noone on the server

	EndGameFocus = Cast<AController>(Winner->GetOwner())->GetPawn();
	if ( (EndGameFocus == NULL) && (Cast<AController>(Winner->GetOwner()) != NULL) )
	{
		// If the controller of the winner does not have a pawn, give him one.
		RestartPlayer(Cast<AController>(Winner->GetOwner()));
		EndGameFocus = Cast<AController>(Winner->GetOwner())->GetPawn();
	}

	if ( EndGameFocus != NULL )
	{
		EndGameFocus->bAlwaysRelevant = true;
	}

	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AController* Controller = *Iterator;
		Controller->GameHasEnded(EndGameFocus, (Controller->PlayerState != NULL) && (Controller->PlayerState == Winner) );
	}
}


void AUTGameMode::BroadcastDeathMessage(AController* Killer, AController* Other, TSubclassOf<UDamageType> DamageType)
{
	if (DeathMessageClass != NULL)
	{
		if ( (Killer == Other) || (Killer == NULL) )
		{
			BroadcastLocalized(this, DeathMessageClass, 1, NULL, Other->PlayerState, DamageType);
		}
		else
		{
			BroadcastLocalized(this, DeathMessageClass, 0, Killer->PlayerState, Other->PlayerState, DamageType);
		}
	}
}


bool AUTGameMode::IsAWinner(AUTPlayerController* PC)
{
	return ( PC != NULL && PC->UTPlayerState != NULL && ( PC->UTPlayerState->bOnlySpectator || PC->UTPlayerState == UTGameState->WinnerPlayerState));
}

void AUTGameMode::PlayEndOfMatchMessage()
{
	for( FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		APlayerController* Controller = *Iterator;
		if (Controller->IsA(AUTPlayerController::StaticClass()))
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(Controller);
			if ( (PC->PlayerState != NULL) && !PC->PlayerState->bOnlySpectator )
			{
				PC->ClientReceiveLocalizedMessage(VictoryMessageClass, IsAWinner(PC) ? 0 : 1);
			}
		}
	}
}

// workaround for call chain from engine, SetPlayerDefaults() could be called while pawn is alive to reset its values but we don't want it to spawn inventory unless it's called from RestartPlayer()
static bool bSetPlayerDefaultsSpawnInventory = false;

void AUTGameMode::RestartPlayer(AController* aPlayer)
{
	if (aPlayer == NULL)
	{
		UE_LOG(UT,Log,TEXT("[BAD] RestartPlayer with a bad player"));
		return;
	}
	else if (aPlayer->PlayerState == NULL)
	{
		UE_LOG(UT,Log,TEXT("[BAD] RestartPlayer with bad player state"));
		return;
	}
	else if (aPlayer->PlayerState->PlayerName.IsEmpty())
	{
		UE_LOG(UT,Log,TEXT("[BAD] RestartPlayer with an empty player name"));
		return;
	}
	

	if ( !UTGameState->HasMatchStarted() && bPlayersMustBeReady )
	{
		// If we are in the pre-game stage then flag the player as ready to play.  The game starting will be handled in the DefaultTimer() event
		Cast<AUTPlayerState>(aPlayer->PlayerState)->bReadyToPlay = true;
		return;
	}

	if (!IsMatchInProgress())
	{
		return;
	}

	if (aPlayer->PlayerState->bOnlySpectator)
	{
		return;
	}

	bSetPlayerDefaultsSpawnInventory = true;
	Super::RestartPlayer(aPlayer);
	bSetPlayerDefaultsSpawnInventory = false;
}

void AUTGameMode::GiveDefaultInventory(APawn* PlayerPawn)
{
	AUTCharacter* UTCharacter = Cast<AUTCharacter>(PlayerPawn);
	if (UTCharacter != NULL && UTCharacter->GetInventory() == NULL)
	{
		UTCharacter->AddDefaultInventory(DefaultInventory);
	}
}

/* 
  Make sure pawn properties are back to default
  Also add default inventory
*/
void AUTGameMode::SetPlayerDefaults(APawn* PlayerPawn)
{
	Super::SetPlayerDefaults(PlayerPawn);

	if (BaseMutator != NULL)
	{
		BaseMutator->ModifyPlayer(PlayerPawn);
	}

	if (bSetPlayerDefaultsSpawnInventory)
	{
		GiveDefaultInventory(PlayerPawn);
	}
}

void AUTGameMode::ChangeName(AController* Other, const FString& S, bool bNameChange)
{
	// Cap player name's at 15 characters...
	FString SMod = S;
	if (SMod.Len()>15)
	{
		SMod = SMod.Left(15);
	}

    if ( !Other->PlayerState|| FCString::Stricmp(*Other->PlayerState->PlayerName, *SMod) == 0 )
    {
		return;
	}

	// Look to see if someone else is using the the new name
	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AController* Controller = *Iterator;
		if (Controller->PlayerState && FCString::Stricmp(*Controller->PlayerState->PlayerName, *SMod) == 0)
		{
			if ( Cast<APlayerController>(Other) != NULL )
			{
					Cast<APlayerController>(Other)->ClientReceiveLocalizedMessage( GameMessageClass, 5 );
					if ( FCString::Stricmp(*Other->PlayerState->PlayerName, *DefaultPlayerName) == 0 )
					{
						Other->PlayerState->SetPlayerName(FString::Printf(TEXT("%s%i"), *DefaultPlayerName, Other->PlayerState->PlayerId));
					}
				return;
			}
		}
	}

    Other->PlayerState->SetPlayerName(SMod);
}

bool AUTGameMode::ShouldSpawnAtStartSpot(AController* Player)
{
	if ( Player && Cast<APlayerStartPIE>(Player->StartSpot.Get()) )
	{
		return true;
	}

	return ( GetWorld()->GetNetMode() == NM_Standalone && Player != NULL && Player->StartSpot.IsValid() &&
		(GetMatchState() == MatchState::WaitingToStart || (Player->PlayerState != NULL && Cast<AUTPlayerState>(Player->PlayerState)->bWaitingPlayer))
		 && (RatePlayerStart(Cast<APlayerStart>(Player->StartSpot.Get()), Player) >= 0.f) );
}


AActor* AUTGameMode::FindPlayerStart(AController* Player, const FString& IncomingName)
{
	AActor* const Best = Super::FindPlayerStart(Player, IncomingName);
	if (Best)
	{
		LastStartSpot = Best;
	}

	return Best;
}

AActor* AUTGameMode::ChoosePlayerStart( AController* Player )
{
	// Start by choosing a random start
	int32 RandStart = FMath::RandHelper(PlayerStarts.Num());

	float BestRating = 0.f;
	APlayerStart* BestStart = NULL;
	for ( int32 i=RandStart; i<PlayerStarts.Num(); i++ )
	{
		APlayerStart* P = PlayerStarts[i];

		if (P->IsA(APlayerStartPIE::StaticClass()))
		{
			// Always prefer the first "Play from Here" PlayerStart, if we find one while in PIE mode
			return P;
		}

		float NewRating = RatePlayerStart(P,Player);

		if (NewRating >= 30.0f && GetWorld()->WorldType != EWorldType::PIE)
		{
			// this PlayerStart is good enough
			return P;
		}
		if ( NewRating > BestRating )
		{
			BestRating = NewRating;
			BestStart = P;
		}
	}
	for ( int32 i=0; i<RandStart; i++ )
	{
		APlayerStart* P = PlayerStarts[i];

		if (P->IsA(APlayerStartPIE::StaticClass()))
		{
			// Always prefer the first "Play from Here" PlayerStart, if we find one while in PIE mode
			return P;
		}

		float NewRating = RatePlayerStart(P,Player);

		if (NewRating >= 30.0f && GetWorld()->WorldType != EWorldType::PIE)
		{
			// this PlayerStart is good enough
			return P;
		}
		if ( NewRating > BestRating )
		{
			BestRating = NewRating;
			BestStart = P;
		}
	}
	if (BestRating < 20.0f)
	{
		UE_LOG(UT, Log, TEXT("ChoosePlayerStart(): No ideal PlayerStart found for %s: Best was %s rating %f"), *GetNameSafe(Player), *GetNameSafe(BestStart), BestRating);
	}
	return (BestStart != NULL) ? BestStart : Super::ChoosePlayerStart(Player);
}

float AUTGameMode::RatePlayerStart(APlayerStart* P, AController* Player)
{
	float Score = 30.0f;

	AActor* LastSpot = (Player != NULL && Player->StartSpot.IsValid()) ? Player->StartSpot.Get() : NULL;
	if (P == LastStartSpot || (LastSpot != NULL && P == LastSpot))
	{
		// avoid re-using starts
		Score -= 15.0f;
	}

	bool bTwoPlayerGame = (NumPlayers + NumBots == 2);

	if (Player != NULL)
	{
		FVector StartLoc = P->GetActorLocation();
		for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
		{
			AController* OtherController = *Iterator;
			ACharacter* OtherCharacter = Cast<ACharacter>( OtherController->GetPawn());

			if ( OtherCharacter && OtherCharacter->PlayerState )
			{
				if ( FMath::Abs(StartLoc.Z - OtherCharacter->GetActorLocation().Z) < P->CapsuleComponent->GetScaledCapsuleHalfHeight() + OtherCharacter->CapsuleComponent->GetScaledCapsuleHalfHeight()
					&& (StartLoc - OtherCharacter->GetActorLocation()).Size2D() < P->CapsuleComponent->GetScaledCapsuleRadius() + OtherCharacter->CapsuleComponent->GetScaledCapsuleRadius() )
				{
					// overlapping - would telefrag
					return -10.f;
				}

				float NextDist = (OtherCharacter->GetActorLocation() - StartLoc).Size();
				static FName NAME_RatePlayerStart = FName(TEXT("RatePlayerStart"));

				if ( NextDist < 6000.0f && !UTGameState->OnSameTeam(Player, OtherController) &&
					!GetWorld()->LineTraceTest(OtherCharacter->GetActorLocation() + FVector(0.f, 0.f, OtherCharacter->CapsuleComponent->GetScaledCapsuleHalfHeight()), StartLoc, ECC_Visibility, FCollisionQueryParams(NAME_RatePlayerStart, false)) )
				{
					// Avoid the last person that killed me
					if (OtherCharacter->PlayerState == Cast<AUTPlayerState>(Player->PlayerState)->LastKillerPlayerState)
					{
						Score -= 7.f;
					}

					Score -= (5.f - 0.0006f * NextDist);
				}
				else if (NextDist < 3000.0f && OtherCharacter->PlayerState == Cast<AUTPlayerState>(Player->PlayerState)->LastKillerPlayerState)
				{
					// Avoid the last person that killed me
					Score -= 7.f;
				}
				else if (bTwoPlayerGame)
				{
					// in 2 player game, look for any visibility
					Score += FMath::Min(2.f,0.0008f*NextDist);
					if (!GetWorld()->LineTraceTest(OtherCharacter->GetActorLocation(), StartLoc, ECC_Visibility, FCollisionQueryParams(NAME_RatePlayerStart, false, this)))
					{
						Score -= 5.f;
					}
				}
			}
		}
	}
	return FMath::Max(Score, 0.2f);
}

/**
 *	We are going to duplicate GameMode's StartNewPlayer because we need to replicate the scoreboard class along with the hud class.  
 *  We are doing this here like this because we are trying to not change engine.  Ultimately the code to create the hud should be
 *  moved to it's own easy to override function instead of being hard-coded in StartNewPlayer
 **/
void AUTGameMode::StartNewPlayer(APlayerController* NewPlayer)
{
	AUTPlayerController* UTNewPlayer = Cast<AUTPlayerController>(NewPlayer);
	if (UTNewPlayer != NULL)
	{
		// tell client what hud class to use

		TSubclassOf<UUTScoreboard> ScoreboardClass = LoadClass<UUTScoreboard>(NULL, *ScoreboardClassName.AssetLongPathname, NULL, LOAD_None, NULL);
		UTNewPlayer->ClientSetHUDAndScoreboard(HUDClass, ScoreboardClass);

		if (!bDelayedStart)
		{
			// start match, or let player enter, immediately
			if (UTGameState->HasMatchStarted())
			{
				RestartPlayer(NewPlayer);
			}

			if (NewPlayer->GetPawn() != NULL)
			{
				NewPlayer->GetPawn()->ClientSetRotation(NewPlayer->GetPawn()->GetActorRotation());
			}
		}
	}
	else
	{
		Super::StartNewPlayer(NewPlayer);
	}
}

bool AUTGameMode::ReadyToStartMatch()
{
	// If bDelayed Start is set, wait for a manual match start
	if (bDelayedStart)
	{
		return false;
	}

	if (GetWorld()->IsPlayInEditor()) return true;	// PIE is always ready to start.

	// By default start when we have > 0 players
	if (GetMatchState() == MatchState::WaitingToStart)
	{
		if (NumPlayers + NumBots >= MinPlayersToStart)
		{
			if (bPlayersMustBeReady)
			{
				for (int i=0;i<UTGameState->PlayerArray.Num();i++)
				{
					AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
					if (PS != NULL && !PS->bOnlySpectator && !PS->bReadyToPlay)
					{
						return false;					
					}
				}
			}

			return true;
		}
	}
	return false;
}

/**
 *	Overwriting all of these functions to work the way I think it should.  Really, the match state should
 *  only be in 1 place otherwise it's prone to mismatch errors.  I'm chosen the GameState because it's
 *  replicated and will be available on clients.
 **/

bool AUTGameMode::HasMatchStarted() const
{
	return UTGameState->HasMatchStarted();
}

bool AUTGameMode::IsMatchInProgress() const
{
	return UTGameState->IsMatchInProgress();
}

bool AUTGameMode::HasMatchEnded() const
{
	return UTGameState->HasMatchEnded();
}

/**
 *	I needed to rework the ordering of SetMatchState until it can be corrected in the engine.
 **/
void AUTGameMode::SetMatchState(FName NewState)
{
	if (MatchState == NewState)
	{
		return;
	}

	MatchState = NewState;
	if (GameState)
	{
		GameState->SetMatchState(NewState);
	}

	// Call change callbacks

	if (MatchState == MatchState::WaitingToStart)
	{
		HandleMatchIsWaitingToStart();
	}
	else if (MatchState == MatchState::CountdownToBegin)
	{
		HandleCountdownToBegin();
	}
	else if (MatchState == MatchState::InProgress)
	{
		HandleMatchHasStarted();
	}
	else if (MatchState == MatchState::WaitingPostMatch)
	{
		HandleMatchHasEnded();
	}
	else if (MatchState == MatchState::LeavingMap)
	{
		HandleLeavingMap();
	}
	else if (MatchState == MatchState::Aborted)
	{
		HandleMatchAborted();
	}
	else if (MatchState == MatchState::MatchEnteringOvertime)
	{
		HandleEnteringOvertime();
	}
	else if (MatchState == MatchState::MatchIsInOvertime)
	{
		HandleMatchInOvertime();
	}
}

void AUTGameMode::HandleEnteringOvertime()
{
	// We are entering overtime, kill off anyone not at the top of the leader board....

	AUTPlayerState* BestPlayer = NULL;
	AUTPlayerState* KillPlayer = NULL;
	float BestScore = 0.0;

	for (int PlayerIdx=0; PlayerIdx < UTGameState->PlayerArray.Num();PlayerIdx++)
	{
		if (UTGameState->PlayerArray[PlayerIdx] != NULL)
		{
			if (BestPlayer == NULL || UTGameState->PlayerArray[PlayerIdx]->Score > BestScore)
			{
				if (BestPlayer != NULL)
				{
					KillPlayer = BestPlayer;
				}
				BestPlayer = Cast<AUTPlayerState>(UTGameState->PlayerArray[PlayerIdx]);
				BestScore = BestPlayer->Score;
			}
			else if (UTGameState->PlayerArray[PlayerIdx]->Score < BestScore)
			{
				KillPlayer = Cast<AUTPlayerState>(UTGameState->PlayerArray[PlayerIdx]);
			}
		}

		if (KillPlayer != NULL)
		{
			// No longer the best.. kill him.. KILL HIM NOW!!!!!
			AController* COwner = Cast<AController> (KillPlayer->GetOwner());
			if (COwner != NULL && COwner->GetPawn() != NULL)
			{
				AUTCharacter* UTChar = Cast<AUTCharacter>(COwner->GetPawn());
				if (UTChar != NULL)
				{
					UE_LOG(UT,Log,TEXT("    -- Calling Died"));
					// Kill off the pawn...
					UTChar->Died(NULL,FDamageEvent(UUTDamageType::StaticClass()));
					// Send this character a message/taunt about not making the cut....
				}
			}

			KillPlayer = NULL;
		}
	}
	SetMatchState(MatchState::MatchIsInOvertime);
}

void AUTGameMode::HandleMatchInOvertime()
{
	// Send the overtime message....
	BroadcastLocalized( this, UUTGameMessage::StaticClass(), 1, NULL, NULL, NULL);
}

void AUTGameMode::HandleCountdownToBegin()
{
	CountDown = 5;
	CheckCountDown();
}

void AUTGameMode::CheckCountDown()
{
	if (CountDown >0)
	{
		// Broadcast the localized message saying when the game begins.
		BroadcastLocalized( this, UUTCountDownMessage::StaticClass(), CountDown, NULL, NULL, NULL);
		GetWorldTimerManager().SetTimer(this, &AUTGameMode::CheckCountDown, 1.0,false);
		CountDown--;
	}
	else
	{
		BeginGame();
	}
}

void AUTGameMode::CheckGameTime()
{
	if ( IsMatchInProgress() && !HasMatchEnded() && TimeLimit > 0 && UTGameState->RemainingTime <= 0)
	{
		// Game should be over.. look to see if we need to go in to overtime....	

		uint32 bTied = 0;
		AUTPlayerState* Winner = IsThereAWinner(bTied);

		if (!bAllowOvertime || !bTied)
		{
			EndGame(Winner, FName(TEXT("TimeLimit")));			
		}
		else if (bAllowOvertime && !UTGameState->IsMatchInOvertime())
		{
			// Stop the clock in Overtime. 
			UTGameState->bStopGameClock = true;
			SetMatchState(MatchState::MatchEnteringOvertime);
		}
	}
}

/**
 *	Look though the player states and see if we have a winner.  If there is a tie, we return
 *  NULL so that we can enter overtime.
 **/
AUTPlayerState* AUTGameMode::IsThereAWinner(uint32& bTied)
{
	AUTPlayerState* BestPlayer = NULL;
	float BestScore = 0.0;

	for (int PlayerIdx=0; PlayerIdx < UTGameState->PlayerArray.Num();PlayerIdx++)
	{
		if (UTGameState->PlayerArray[PlayerIdx] != NULL)
		{
			if (BestPlayer == NULL || UTGameState->PlayerArray[PlayerIdx]->Score > BestScore)
			{
				BestPlayer = Cast<AUTPlayerState>(UTGameState->PlayerArray[PlayerIdx]);
				BestScore = BestPlayer->Score;
				bTied = 0;
			}
			else if (UTGameState->PlayerArray[PlayerIdx]->Score == BestScore)
			{
				bTied = 1;
			}
		}
	}

	return BestPlayer;
}

void AUTGameMode::OverridePlayerState(APlayerController* PC, APlayerState* OldPlayerState)
{
	Super::OverridePlayerState(PC, OldPlayerState);

	// if we're in this function GameMode swapped PlayerState objects so we need to update the precasted copy
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(PC);
	if (UTPC != NULL)
	{
		UTPC->UTPlayerState = Cast<AUTPlayerState>(UTPC->PlayerState);
	}
}

void AUTGameMode::GenericPlayerInitialization(AController* C)
{
	Super::GenericPlayerInitialization(C);

	if (BaseMutator != NULL)
	{
		BaseMutator->PostPlayerInit(C);
	}
}

void AUTGameMode::PostLogin( APlayerController* NewPlayer )
{
	Super::PostLogin(NewPlayer);

	if (GameSession != NULL)
	{
		AUTGameSession* UTGameSession = Cast<AUTGameSession>(GameSession);
		if (UTGameSession != NULL)
		{
			UTGameSession->UpdateGameState();
		}
	}
}


void AUTGameMode::Logout(AController* Exiting)
{
	if (BaseMutator != NULL)
	{
		BaseMutator->NotifyLogout(Exiting);
	}

	// Let's Analytics know how long this player has been online....

	AUTPlayerState* PS = Cast<AUTPlayerState>(Exiting->PlayerState);
		
	if (PS != NULL && FUTAnalytics::IsAvailable())
	{
		float TotalTimeOnline = GameState->ElapsedTime - PS->StartTime;
		TArray<FAnalyticsEventAttribute> ParamArray;
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("ID"), PS->StatsID));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("PlayerName"), PS->PlayerName));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("TimeOnline"), TotalTimeOnline));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("Kills"), PS->Deaths));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("Deaths"), PS->Kills));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("Score"), PS->Score));
		FUTAnalytics::GetProvider().RecordEvent( TEXT("PlayerLogoutStat"), ParamArray );
	}

	Super::Logout(Exiting);

	if (GameSession != NULL)
	{
		AUTGameSession* UTGameSession = Cast<AUTGameSession>(GameSession);
		if (UTGameSession != NULL)
		{
			UTGameSession->UpdateGameState();
		}
	}


}

bool AUTGameMode::PlayerCanRestart( APlayerController* Player )
{
	// Can't restart in overtime
	if (UTGameState->IsMatchInOvertime())
	{
		return false;
	}

	return Super::PlayerCanRestart(Player);
}

void AUTGameMode::ModifyDamage_Implementation(int32& Damage, FVector& Momentum, APawn* Injured, AController* InstigatedBy, const FHitResult& HitInfo, AActor* DamageCauser)
{
	AUTCharacter* InjuredChar = Cast<AUTCharacter>(Injured);
	if (InjuredChar != NULL && InjuredChar->bSpawnProtectionEligible && InstigatedBy != NULL && InstigatedBy != Injured->Controller && GetWorld()->TimeSeconds - Injured->CreationTime < UTGameState->SpawnProtectionTime)
	{
		Damage = 0;
	}
}

bool AUTGameMode::CheckRelevance_Implementation(AActor* Other)
{
	if (BaseMutator == NULL)
	{
		return true;
	}
	else
	{
		bool bPreventModify = false;
		bool bForceKeep = BaseMutator->AlwaysKeep(Other, bPreventModify);
		if (bForceKeep && bPreventModify)
		{
			return true;
		}
		else
		{
			return (BaseMutator->CheckRelevance(Other) || bForceKeep);
		}
	}
}

void AUTGameMode::SetWorldGravity(float NewGravity)
{
	AWorldSettings* Settings = GetWorld()->GetWorldSettings();
	Settings->bWorldGravitySet = true;
	Settings->WorldGravityZ = NewGravity;
}

bool AUTGameMode::ChangeTeam(AController* Player, uint8 NewTeam, bool bBroadcast)
{
	// By default, we don't do anything.
	return true;
}

TSubclassOf<AGameSession> AUTGameMode::GetGameSessionClass() const
{
	return AUTGameSession::StaticClass();
}

void AUTGameMode::ScoreObject(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason)
{
	UE_LOG(UT,Log,TEXT("ScoreObject %s by %s (%s) %s"), *GetNameSafe(GameObject), *GetNameSafe(HolderPawn), *GetNameSafe(Holder), *Reason.ToString());
}

void AUTGameMode::GetSeamlessTravelActorList(bool bToEntry, TArray<AActor*>& ActorList)
{
	Super::GetSeamlessTravelActorList(bToEntry, ActorList);

	for (AUTMutator* Mut = BaseMutator; Mut != NULL; Mut = Mut->NextMutator)
	{
		Mut->GetSeamlessTravelActorList(bToEntry, ActorList);
	}
}

void AUTGameMode::CreateConfigWidgets(TSharedPtr<class SVerticalBox> MenuSpace, TArray< TSharedPtr<TAttributePropertyBase> >& ConfigProps)
{
	TSharedPtr< TAttributeProperty<int32> > TimeLimitAttr = MakeShareable(new TAttributeProperty<int32>(this, &TimeLimit));
	ConfigProps.Add(TimeLimitAttr);
	TSharedPtr< TAttributeProperty<int32> > GoalScoreAttr = MakeShareable(new TAttributeProperty<int32>(this, &GoalScore));
	ConfigProps.Add(GoalScoreAttr);
	TSharedPtr< TAttributePropertyBool > ForceRespawnAttr = MakeShareable(new TAttributePropertyBool(this, &bForceRespawn));
	ConfigProps.Add(ForceRespawnAttr);

	MenuSpace->AddSlot()
	.Padding(10.0f, 5.0f, 10.0f, 5.0f)
	.AutoHeight()
	.VAlign(VAlign_Top)
	.HAlign(HAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(150.0f)
		.Content()
		[
			SNew(SNumericEntryBox<int32>)
			.LabelPadding(FMargin(10.0f, 0.0f))
			.Value(GoalScoreAttr.ToSharedRef(), &TAttributeProperty<int32>::GetOptional)
			.OnValueChanged(GoalScoreAttr.ToSharedRef(), &TAttributeProperty<int32>::Set)
			.AllowSpin(true)
			.Delta(1)
			.MinValue(0)
			.MaxValue(999)
			.MinSliderValue(0)
			.MaxSliderValue(99)
			.Label()
			[
				SNew(STextBlock)
				.ColorAndOpacity(FLinearColor::Black)
				.Text(NSLOCTEXT("UTGameMode", "GoalScore", "Goal Score"))
			]
		]
	];
	MenuSpace->AddSlot()
	.Padding(10.0f, 5.0f, 10.0f, 5.0f)
	.AutoHeight()
	.VAlign(VAlign_Top)
	.HAlign(HAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(150.0f)
		.Content()
		[
			SNew(SNumericEntryBox<int32>)
			.LabelPadding(FMargin(10.0f, 0.0f))
			.Value(TimeLimitAttr.ToSharedRef(), &TAttributeProperty<int32>::GetOptional)
			.OnValueChanged(TimeLimitAttr.ToSharedRef(), &TAttributeProperty<int32>::Set)
			.AllowSpin(true)
			.Delta(1)
			.MinValue(0)
			.MaxValue(999)
			.MinSliderValue(0)
			.MaxSliderValue(60)
			.Label()
			[
				SNew(STextBlock)
				.ColorAndOpacity(FLinearColor::Black)
				.Text(NSLOCTEXT("UTGameMode", "TimeLimit", "Time Limit"))
			]
		]
	];
	MenuSpace->AddSlot()
		.Padding(0.0f, 5.0f, 0.0f, 5.0f)
		.AutoHeight()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Center)
		[
			SNew(SCheckBox)
			.IsChecked(ForceRespawnAttr.ToSharedRef(), &TAttributePropertyBool::GetAsCheckBox)
			.OnCheckStateChanged(ForceRespawnAttr.ToSharedRef(), &TAttributePropertyBool::SetFromCheckBox)
			.Type(ESlateCheckBoxType::CheckBox)
			.Content()
			[
				SNew(STextBlock)
				.ColorAndOpacity(FLinearColor::Black)
				.Text(NSLOCTEXT("UTGameMode", "ForceRespawn", "Force Respawn").ToString())
			]
		];
}

void AUTGameMode::ProcessServerTravel(const FString& URL, bool bAbsolute)
{
	if (GameSession != NULL)
	{
		AUTGameSession* UTGameSession = Cast<AUTGameSession>(GameSession);
		if (UTGameSession != NULL)
		{
			UTGameSession->UnRegisterServer();
		}
	}

	Super::ProcessServerTravel(URL, bAbsolute);
}

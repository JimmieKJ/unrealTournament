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
#include "UTBot.h"
#include "UTSquadAI.h"
#include "Slate/Panels/SUDuelSettings.h"
#include "Slate/Panels/SULobbyMatchSetupPanel.h"

UUTResetInterface::UUTResetInterface(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{}

namespace MatchState
{
	const FName CountdownToBegin = FName(TEXT("CountdownToBegin"));
	const FName MatchEnteringOvertime = FName(TEXT("MatchEnteringOvertime"));
	const FName MatchIsInOvertime = FName(TEXT("MatchIsInOvertime"));
}

AUTGameMode::AUTGameMode(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
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
	bOnlyTheStrongSurvive = true;
	EndScoreboardDelay = 2.0f;
	GameDifficulty = 3.0f;
	BotFillCount = 0;
	VictoryMessageClass = UUTVictoryMessage::StaticClass();
	DeathMessageClass = UUTDeathMessage::StaticClass();
	GameMessageClass = UUTGameMessage::StaticClass();
	SquadType = AUTSquadAI::StaticClass();
	MaxSquadSize = 3;

	DefaultPlayerName = FString("Malcolm");
	MapPrefix = TEXT("DM");

	//LobbySetupPanelClass = SUDuelSettings::StaticClass();

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

	// HACK: workaround for cached inventory flags getting blown away by blueprint recompile cycle
	for (TObjectIterator<AUTInventory> It(RF_NoFlags); It; ++It)
	{
		if (It->IsTemplate(RF_ClassDefaultObject))
		{
			It->PostInitProperties();
		}
	}

	UE_LOG(UT,Log,TEXT("==============="));
	UE_LOG(UT,Log,TEXT("  Init Game"));
	UE_LOG(UT,Log,TEXT("==============="));

	Super::InitGame(MapName, Options, ErrorMessage);

	GameDifficulty = FMath::Max(0, GetIntOption(Options, TEXT("Difficulty"), GameDifficulty));

	FString InOpt = ParseOption(Options, TEXT("ForceRespawn"));
	bForceRespawn = EvalBoolOptions(InOpt, bForceRespawn);

	InOpt = ParseOption(Options, TEXT("OnlyStrong"));
	bOnlyTheStrongSurvive = EvalBoolOptions(InOpt, bOnlyTheStrongSurvive);

	InOpt = ParseOption(Options, TEXT("MustBeReady"));
	bPlayersMustBeReady = EvalBoolOptions(InOpt, bPlayersMustBeReady);

	TimeLimit = FMath::Max(0,GetIntOption( Options, TEXT("TimeLimit"), TimeLimit ));
	TimeLimit *= 60;

	// Set goal score to end match.
	GoalScore = FMath::Max(0,GetIntOption( Options, TEXT("GoalScore"), GoalScore ));

	MinPlayersToStart = FMath::Max(1, GetIntOption( Options, TEXT("MinPlayers"), MinPlayersToStart));

	RespawnWaitTime = FMath::Max(0,GetIntOption( Options, TEXT("RespawnWait"), RespawnWaitTime ));

	// alias for testing convenience
	if (HasOption(Options, TEXT("Bots")))
	{
		BotFillCount = GetIntOption(Options, TEXT("BotFill"), BotFillCount) + 1;
	}
	else
	{
		BotFillCount = GetIntOption(Options, TEXT("BotFill"), BotFillCount);
	}

	ServerPassword = TEXT("");
	ServerPassword = ParseOption(Options, TEXT("ServerPassword"));
	bRequirePassword = !ServerPassword.IsEmpty();

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

	InOpt = ParseOption(Options, TEXT("WeaponStay"));
	bWeaponStayActive = EvalBoolOptions(InOpt, true);
	UE_LOG(UT, Log, TEXT("WeaponStay %d"), bWeaponStayActive);

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
		UTGameState->bWeaponStay = bWeaponStayActive;
	}
	else
	{
		UE_LOG(UT,Error, TEXT("UTGameState is NULL %s"), *GameStateClass->GetFullName());
	}

	if (GameSession != NULL && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		GameSession->RegisterServer();
		GetWorldTimerManager().SetTimer(this, &AUTGameMode::UpdateOnlineServer, 60.0f);	
	}
}

void AUTGameMode::UpdateOnlineServer()
{
	if (GameSession &&  GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		AUTGameSession* GS = Cast<AUTGameSession>(GameSession);
		if (GS)
		{
			GS->UpdateGameState();
		}
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

AUTBot* AUTGameMode::AddBot(uint8 TeamNum)
{
	AUTBot* NewBot = GetWorld()->SpawnActor<AUTBot>(AUTBot::StaticClass());
	if (NewBot != NULL)
	{
		// pick bot character
		if (BotCharacters.Num() == 0)
		{
			UE_LOG(UT, Warning, TEXT("AddBot(): No BotCharacters defined"));
			static int32 NameCount = 0;
			NewBot->PlayerState->SetPlayerName(FString(TEXT("TestBot")) + ((NameCount > 0) ? FString::Printf(TEXT("_%i"), NameCount) : TEXT("")));
			NameCount++;
		}
		else
		{
			int32 NumEligible = 0;
			FBotCharacter* BestChar = NULL;
			uint8 BestSelectCount = MAX_uint8;
			for (FBotCharacter& Character : BotCharacters)
			{
				if (Character.SelectCount < BestSelectCount)
				{
					NumEligible = 1;
					BestChar = &Character;
					BestSelectCount = Character.SelectCount;
				}
				else if (Character.SelectCount == BestSelectCount)
				{
					NumEligible++;
					if (FMath::FRand() < 1.0f / float(NumEligible))
					{
						BestChar = &Character;
					}
				}
			}
			BestChar->SelectCount++;
			NewBot->Personality = *BestChar;
			NewBot->PlayerState->SetPlayerName(BestChar->PlayerName);
		}

		NewBot->InitializeSkill(GameDifficulty);
		NumBots++;
		ChangeTeam(NewBot, TeamNum);
		GenericPlayerInitialization(NewBot);
	}
	return NewBot;
}

AUTBot* AUTGameMode::ForceAddBot(uint8 TeamNum)
{
	BotFillCount = FMath::Max<int32>(BotFillCount, NumPlayers + NumBots + 1);
	return AddBot(TeamNum);
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

	if (NumPlayers + NumBots > BotFillCount)
	{
		for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
		{
			if (It->Get()->GetPawn() == NULL)
			{
				AUTBot* B = Cast<AUTBot>(It->Get());
				if (B != NULL)
				{
					B->Destroy();
				}
			}
		}
	}
	else while (NumPlayers + NumBots < BotFillCount)
	{
		AddBot();
	}

	// Look to see if we should restart the game due to server inactivity
	if (GetNumPlayers() <= 0 && NumSpectators <= 0 && HasMatchStarted())
	{
		EmptyServerTime++;
		if (EmptyServerTime >= AutoRestartTime)
		{
			TravelToNextMap();
		}
	}
	else
	{
		EmptyServerTime = 0;
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

		if (UTGameState->IsMatchInOvertime() && UTGameState->bOnlyTheStrongSurvive)
		{
			KilledPlayer->ChangeState(NAME_Spectating);
		}
	}
	NotifyKilled(Killer, KilledPlayer, KilledPawn, DamageType);
}

void AUTGameMode::NotifyKilled(AController* Killer, AController* Killed, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		if (It->IsValid())
		{
			AUTBot* B = Cast<AUTBot>(It->Get());
			if (B != NULL)
			{
				B->UTNotifyKilled(Killer, Killed, KilledPawn, DamageType.GetDefaultObject());
			}
		}
	}
}

void AUTGameMode::ScorePickup(AUTPickup* Pickup, AUTPlayerState* PickedUpBy, AUTPlayerState* LastPickedUpBy)
{
}

void AUTGameMode::ScoreDamage(int32 DamageAmount, AController* Victim, AController* Attacker)
{
	if (BaseMutator != NULL)
	{
		BaseMutator->ScoreDamage(DamageAmount, Victim, Attacker);
	}
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
			FindAndMarkHighScorer();
			CheckScore(KillerPlayerState);
		}

		if (!bFirstBloodOccurred)
		{
			BroadcastLocalized(this, UUTFirstBloodMessage::StaticClass(), 0, KillerPlayerState, NULL, NULL);
			bFirstBloodOccurred = true;
		}
	}

	if (BaseMutator != NULL)
	{
		BaseMutator->ScoreKill(Killer, Other, DamageType);
	}
}

bool AUTGameMode::OverridePickupQuery_Implementation(APawn* Other, TSubclassOf<AUTInventory> ItemClass, AActor* Pickup, bool& bAllowPickup)
{
	return (BaseMutator != NULL && BaseMutator->OverridePickupQuery(Other, ItemClass, Pickup, bAllowPickup));
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
		for (TInventoryIterator<> It(UTC); It; ++It)
		{
			if (It->bAlwaysDropOnDeath)
			{
				UTC->TossInventory(*It, FVector(FMath::FRandRange(0.0f, 200.0f), FMath::FRandRange(-400.0f, 400.0f), FMath::FRandRange(0.0f, 200.0f)));
			}
		}
		// delete the rest
		UTC->DiscardAllInventory();
	}
}

void AUTGameMode::FindAndMarkHighScorer()
{
	int32 BestScore = 0;
	for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
	{
		AUTPlayerState *PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (PS != nullptr)
		{
			// Clear previous high scores
			PS->bHasHighScore = false;
			AController *C = Cast<AController>(PS->GetOwner());
			if (C != nullptr)
			{
				AUTCharacter *UTChar = Cast<AUTCharacter>(C->GetPawn());
				if (UTChar)
				{
					UTChar->bHasHighScore = false;
				}
			}

			if (BestScore == 0 || PS->Score > BestScore)
			{
				BestScore = PS->Score;
			}
		}
	}

	for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
	{
		AUTPlayerState *PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (PS != nullptr && PS->Score == BestScore)
		{
			PS->bHasHighScore = true;
			AController *C = Cast<AController>(PS->GetOwner());
			if (C != nullptr)
			{
				AUTCharacter *UTChar = Cast<AUTCharacter>(C->GetPawn());
				if (UTChar)
				{
					UTChar->bHasHighScore = true;
				}
			}
		}
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
}

void AUTGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	// reset things, relevant for any kind of warmup mode and to make sure placed Actors like pickups are in their initial state no matter how much time has passed in pregame
	for (FActorIterator It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->ImplementsInterface(UUTResetInterface::StaticClass()))
		{
			IUTResetInterface::Execute_Reset(*It);
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
	// Dont ever end the game in PIE
	if (GetWorld()->WorldType == EWorldType::PIE) return;

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
	if (!RconNextMapName.IsEmpty())
	{
		GetWorld()->ServerTravel(RconNextMapName, false);
		return;
	}

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

void AUTGameMode::PlayEndOfMatchMessage()
{
	int32 IsFlawlessVictory = 1;
	for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
	{
		AController* Controller = *Iterator;
		if (Controller->PlayerState != NULL && !Controller->PlayerState->bOnlySpectator && (Controller->PlayerState->Score > 0.f) & (Controller->PlayerState != UTGameState->WinnerPlayerState))
		{
			IsFlawlessVictory = 0;
			break;
		}
	}

	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* Controller = *Iterator;
		if (Controller->IsA(AUTPlayerController::StaticClass()))
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(Controller);
			if ((PC->PlayerState != NULL) && !PC->PlayerState->bOnlySpectator)
			{
				PC->ClientReceiveLocalizedMessage(VictoryMessageClass, 2*IsFlawlessVictory + ((UTGameState->WinnerPlayerState == PC->PlayerState) ? 1 : 0), UTGameState->WinnerPlayerState, PC->PlayerState, NULL);
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
		UE_LOG(UT, Warning, TEXT("RestartPlayer with a bad player"));
		return;
	}
	else if (aPlayer->PlayerState == NULL)
	{
		UE_LOG(UT, Warning, TEXT("RestartPlayer with bad player state"));
		return;
	}
	else if (aPlayer->PlayerState->PlayerName.IsEmpty())
	{
		UE_LOG(UT, Warning, TEXT("RestartPlayer with an empty player name"));
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
	if (UTCharacter != NULL)
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

AActor* AUTGameMode::ChoosePlayerStart(AController* Player)
{
	if (PlayerStarts.Num() == 0)
	{
		return Super::ChoosePlayerStart(Player);
	}
	if (GetWorld()->WorldType == EWorldType::PIE)
	{
		for (int32 i = 0; i < PlayerStarts.Num(); i++)
		{
			APlayerStart* P = PlayerStarts[i];

			if (P->IsA(APlayerStartPIE::StaticClass()))
			{
				// Always prefer the first "Play from Here" PlayerStart, if we find one while in PIE mode
				return P;
			}
		}
	}
	
	// Always randomize the list order a bit to prevent groups of bad starts from permanently making the next decent start overused
	for (int i = 0; i < 2; i++)
	{
		int32 RandIndexOne = FMath::RandHelper(PlayerStarts.Num());
		int32 RandIndexTwo = FMath::RandHelper(PlayerStarts.Num());
		APlayerStart* SavedStart = PlayerStarts[RandIndexOne]; 
		PlayerStarts[RandIndexOne] = PlayerStarts[RandIndexTwo];
		PlayerStarts[RandIndexTwo] = SavedStart;
	}

	// Start by choosing a random start
	int32 RandStart = FMath::RandHelper(PlayerStarts.Num());

	float BestRating = 0.f;
	APlayerStart* BestStart = NULL;
	for ( int32 i=RandStart; i<PlayerStarts.Num(); i++ )
	{
		APlayerStart* P = PlayerStarts[i];

		float NewRating = RatePlayerStart(P,Player);

		if (NewRating >= 30.0f)
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

		float NewRating = RatePlayerStart(P,Player);

		if (NewRating >= 30.0f)
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
		FVector StartLoc = P->GetActorLocation() + AUTCharacter::StaticClass()->GetDefaultObject<AUTCharacter>()->BaseEyeHeight ;
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
				bool bIsLastKiller = (OtherCharacter->PlayerState == Cast<AUTPlayerState>(Player->PlayerState)->LastKillerPlayerState);

				if (((NextDist < 8000.0f) || bTwoPlayerGame) && !UTGameState->OnSameTeam(Player, OtherController))
				{
					if (!GetWorld()->LineTraceTest(StartLoc, OtherCharacter->GetActorLocation() + FVector(0.f, 0.f, OtherCharacter->CapsuleComponent->GetScaledCapsuleHalfHeight()), ECC_Visibility, FCollisionQueryParams(NAME_RatePlayerStart, false)))
					{
						// Avoid the last person that killed me
						if (bIsLastKiller)
						{
							Score -= 7.f;
						}

						Score -= (5.f - 0.0003f * NextDist);
					}
					else if (NextDist < 4000.0f)
					{
						// Avoid the last person that killed me
						Score -= bIsLastKiller ? 5.f : 0.0005f * (5000.f - NextDist);

						if (!GetWorld()->LineTraceTest(StartLoc, OtherCharacter->GetActorLocation(), ECC_Visibility, FCollisionQueryParams(NAME_RatePlayerStart, false, this)))
						{
							Score -= 2.f;
						}
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
		if (NumPlayers + NumBots >= MinPlayersToStart && NumPlayers + NumSpectators > 0)
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
	if (bOnlyTheStrongSurvive)
	{
		UTGameState->bOnlyTheStrongSurvive = true;

		// We are entering overtime, kill off anyone not at the top of the leader board....

		AUTPlayerState* BestPlayer = NULL;
		AUTPlayerState* KillPlayer = NULL;
		float BestScore = 0.0;

		for (int PlayerIdx = 0; PlayerIdx < UTGameState->PlayerArray.Num(); PlayerIdx++)
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
				AController* COwner = Cast<AController>(KillPlayer->GetOwner());
				if (COwner != NULL )
				{
					if (COwner->GetPawn() != NULL)
					{
						AUTCharacter* UTChar = Cast<AUTCharacter>(COwner->GetPawn());
						if (UTChar != NULL)
						{
							UE_LOG(UT, Log, TEXT("    -- Calling Died"));
							// Kill off the pawn...
							UTChar->Died(NULL, FDamageEvent(UUTDamageType::StaticClass()));
							// Send this character a message/taunt about not making the cut....
						}
					}

					// Tell the player they didn't make the cut
					AUTPlayerController* PC = Cast<AUTPlayerController>(COwner);
					if (PC)
					{
						PC->ClientReceiveLocalizedMessage(UUTGameMessage::StaticClass(), 8);					
						PC->ChangeState(NAME_Spectating);
					}
				}


				KillPlayer = NULL;
			}
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
	GetWorldTimerManager().SetTimer(this, &AUTGameMode::CheckCountDown, 1.0, false);
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

	if (Cast<AUTBot>(Exiting) != NULL)
	{
		NumBots--;
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
	if (bOnlyTheStrongSurvive && UTGameState->IsMatchInOvertime())
	{
		return false;
	}

	return Super::PlayerCanRestart(Player);
}

void AUTGameMode::ModifyDamage_Implementation(int32& Damage, FVector& Momentum, APawn* Injured, AController* InstigatedBy, const FHitResult& HitInfo, AActor* DamageCauser, TSubclassOf<UDamageType> DamageType)
{
	AUTCharacter* InjuredChar = Cast<AUTCharacter>(Injured);
	if (InjuredChar != NULL && InjuredChar->bSpawnProtectionEligible && InstigatedBy != NULL && InstigatedBy != Injured->Controller && GetWorld()->TimeSeconds - Injured->CreationTime < UTGameState->SpawnProtectionTime)
	{
		Damage = 0;
	}

	if (BaseMutator != NULL)
	{
		BaseMutator->ModifyDamage(Damage, Momentum, Injured, InstigatedBy, HitInfo, DamageCauser, DamageType);
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
	if (BaseMutator != NULL)
	{
		BaseMutator->ScoreObject(GameObject, HolderPawn, Holder, Reason);
	}
}

void AUTGameMode::GetSeamlessTravelActorList(bool bToEntry, TArray<AActor*>& ActorList)
{
	Super::GetSeamlessTravelActorList(bToEntry, ActorList);

	for (AUTMutator* Mut = BaseMutator; Mut != NULL; Mut = Mut->NextMutator)
	{
		Mut->GetSeamlessTravelActorList(bToEntry, ActorList);
	}
}

#if !UE_SERVER
void AUTGameMode::CreateConfigWidgets(TSharedPtr<class SVerticalBox> MenuSpace, TArray< TSharedPtr<TAttributePropertyBase> >& ConfigProps)
{
	TSharedPtr< TAttributeProperty<int32> > TimeLimitAttr = MakeShareable(new TAttributeProperty<int32>(this, &TimeLimit));
	ConfigProps.Add(TimeLimitAttr);
	TSharedPtr< TAttributeProperty<int32> > GoalScoreAttr = MakeShareable(new TAttributeProperty<int32>(this, &GoalScore));
	ConfigProps.Add(GoalScoreAttr);
	TSharedPtr< TAttributePropertyBool > ForceRespawnAttr = MakeShareable(new TAttributePropertyBool(this, &bForceRespawn));
	ConfigProps.Add(ForceRespawnAttr);
	TSharedPtr< TAttributeProperty<int32> > CombatantsAttr = MakeShareable(new TAttributeProperty<int32>(this, &BotFillCount));
	ConfigProps.Add(CombatantsAttr);
	TSharedPtr< TAttributeProperty<float> > BotSkillAttr = MakeShareable(new TAttributeProperty<float>(this, &GameDifficulty));
	ConfigProps.Add(BotSkillAttr);

	MenuSpace->AddSlot()
	.Padding(10.0f, 5.0f, 10.0f, 5.0f)
	.AutoHeight()
	.VAlign(VAlign_Top)
	.HAlign(HAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(200.0f)
		.Content()
		[
			SNew(SNumericEntryBox<int32>)
			.LabelPadding(FMargin(10.0f, 0.0f))
			.Value(CombatantsAttr.ToSharedRef(), &TAttributeProperty<int32>::GetOptional)
			.OnValueChanged(CombatantsAttr.ToSharedRef(), &TAttributeProperty<int32>::Set)
			.AllowSpin(true)
			.Delta(1)
			.MinValue(1)
			.MaxValue(32)
			.MinSliderValue(1)
			.MaxSliderValue(32)
			.Label()
			[
				SNew(STextBlock)
				.ColorAndOpacity(FLinearColor::Black)
				.Text(NSLOCTEXT("UTGameMode", "NumCombatants", "Number of Combatants"))
			]
		]
	];
	// TODO: BotSkill should be a list box with the usual items; this is a simple placeholder
	MenuSpace->AddSlot()
	.Padding(10.0f, 5.0f, 10.0f, 5.0f)
	.AutoHeight()
	.VAlign(VAlign_Top)
	.HAlign(HAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(200.0f)
		.Content()
		[
			SNew(SNumericEntryBox<float>)
			.LabelPadding(FMargin(10.0f, 0.0f))
			.Value(BotSkillAttr.ToSharedRef(), &TAttributeProperty<float>::GetOptional)
			.OnValueChanged(BotSkillAttr.ToSharedRef(), &TAttributeProperty<float>::Set)
			.AllowSpin(true)
			.Delta(1)
			.MinValue(0)
			.MaxValue(7)
			.MinSliderValue(0)
			.MaxSliderValue(7)
			.Label()
			[
				SNew(STextBlock)
				.ColorAndOpacity(FLinearColor::Black)
				.Text(NSLOCTEXT("UTGameMode", "BotSkill", "Bot Skill"))
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
#endif

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

FText AUTGameMode::BuildServerRules(AUTGameState* GameState)
{
	// TODO: should return game class path in addition to game name so client can display in local language if available
	return FText::Format(NSLOCTEXT("UTGameMode","GameRules","{0} - GoalScore: {1}  Time Limit: {2}"), DisplayName, FText::AsNumber(GameState->GoalScore), FText::AsNumber(uint32(GameState->TimeLimit * 60)));
}

void AUTGameMode::BuildServerResponseRules(FString& OutRules)
{
	OutRules += FString::Printf(TEXT("GoalScore\t%i\t"), GoalScore);
	OutRules += FString::Printf(TEXT("TimeLimit\t%i\t"), int32(TimeLimit/60.0));
	OutRules += FString::Printf(TEXT("Allow Overtime\t%s\t"), bAllowOvertime ? TEXT("True") : TEXT("False"));
	OutRules += FString::Printf(TEXT("Forced Respawn\t%s\t"), bForceRespawn ?  TEXT("True") : TEXT("False"));
	OutRules += FString::Printf(TEXT("Only The Strong\t%s\t"), bOnlyTheStrongSurvive ? TEXT("True") : TEXT("False"));
	OutRules += FString::Printf(TEXT("Players Must Be Ready\t%s\t"), bPlayersMustBeReady ?  TEXT("True") : TEXT("False"));

	AUTMutator* Mut = BaseMutator;
	while (Mut)
	{
		OutRules += FString::Printf(TEXT("Mutator\t%s\t"), *Mut->DisplayName.ToString());
		Mut = Mut->NextMutator;
	}
}

void AUTGameMode::BlueprintBroadcastLocalized( AActor* Sender, TSubclassOf<ULocalMessage> Message, int32 Switch, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject)
{
	BroadcastLocalized(Sender, Message, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}

void AUTGameMode::BlueprintSendLocalized( AActor* Sender, AUTPlayerController* Receiver, TSubclassOf<ULocalMessage> Message, int32 Switch, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject)
{
	Receiver->ClientReceiveLocalizedMessage(Message, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}

void AUTGameMode::AssignDefaultSquadFor(AController* C)
{
	if (C != NULL)
	{
		if (SquadType == NULL)
		{
			UE_LOG(UT, Warning, TEXT("Game mode %s missing SquadType"), *GetName());
			SquadType = AUTSquadAI::StaticClass();
		}
		AUTPlayerState* PS = Cast<AUTPlayerState>(C->PlayerState);
		if (PS != NULL && PS->Team != NULL)
		{
			PS->Team->AssignDefaultSquadFor(C);
		}
		else
		{
			// default is to just spawn a squad for each individual
			AUTBot* B = Cast<AUTBot>(C);
			if (B != NULL)
			{
				B->SetSquad(GetWorld()->SpawnActor<AUTSquadAI>(SquadType));
			}
		}
	}
}

bool AUTGameMode::RestrictPlayerSpawns()
{
	return (bOnlyTheStrongSurvive && UTGameState->IsMatchInOvertime());
}

#if !UE_SERVER
TSharedRef<SWidget> AUTGameMode::CreateLobbyPanel(bool inIsHost, TWeakObjectPtr<class UUTLocalPlayer> inPlayerOwner, TWeakObjectPtr<AUTLobbyMatchInfo> inMatchInfo) const
{
	// Return just an empty panel
	return SNew(SCanvas);
}
#endif

FString AUTGameMode::GetDefaultLobbyOptions() const
{
	return TEXT("");
}
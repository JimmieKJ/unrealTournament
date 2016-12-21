// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamGameMode.h"
#include "UTHUD_CTF.h"
#include "UTCTFRoundGame.h"
#include "UTCTFGameMessage.h"
#include "UTCTFRoleMessage.h"
#include "UTCTFRewardMessage.h"
#include "UTCTFMajorMessage.h"
#include "UTFirstBloodMessage.h"
#include "UTPickup.h"
#include "UTGameMessage.h"
#include "UTMutator.h"
#include "UTCTFSquadAI.h"
#include "UTWorldSettings.h"
#include "Widgets/SUTTabWidget.h"
#include "Dialogs/SUTPlayerInfoDialog.h"
#include "StatNames.h"
#include "Engine/DemoNetDriver.h"
#include "UTCTFScoreboard.h"
#include "UTShowdownGameMessage.h"
#include "UTShowdownRewardMessage.h"
#include "UTShowdownStatusMessage.h"
#include "UTPlayerStart.h"
#include "UTTimedPowerup.h"
#include "UTPlayerState.h"
#include "UTFlagRunHUD.h"
#include "UTGhostFlag.h"
#include "UTCTFRoundGameState.h"
#include "UTAsymCTFSquadAI.h"
#include "UTAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "UTLineUpHelper.h"
#include "UTLineUpZone.h"
#include "UTProjectile.h"
#include "UTRemoteRedeemer.h"

AUTCTFRoundGame::AUTCTFRoundGame(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	TimeLimit = 5;
	QuickPlayersToStart = 10;
	IntermissionDuration = 28.f;
	RoundLives = 5;
	bPerPlayerLives = true;
	bNeedFiveKillsMessage = true;
	FlagCapScore = 1;
	UnlimitedRespawnWaitTime = 2.f;
	bForceRespawn = true;
	bCarryOwnFlag = true;
	bNoFlagReturn = true;
	bFirstRoundInitialized = false;
	FlagPickupDelay = 10;
	HUDClass = AUTFlagRunHUD::StaticClass();
	GameStateClass = AUTCTFRoundGameState::StaticClass();
	SquadType = AUTAsymCTFSquadAI::StaticClass();
	NumRounds = 6;
	bHideInUI = true;

	InitialBoostCount = 0;
	MaxTimeScoreBonus = 180;

	bGameHasTranslocator = false;

	RollingAttackerRespawnDelay = 5.f;
	LastAttackerSpawnTime = 0.f;
	RollingSpawnStartTime = 0.f;
	bRollingAttackerSpawns = true;
	ForceRespawnTime = 0.1f;
	LimitedRespawnWaitTime = 6.f;

	MainScoreboardDisplayTime = 7.5f;
	EndScoreboardDelay = 8.7f;

	bSitOutDuringRound = false;
	bSlowFlagCarrier = false;
	EndOfMatchMessageDelay = 2.5f;
	bUseLevelTiming = true;
}

void AUTCTFRoundGame::PostLogin(APlayerController* NewPlayer)
{
	if (NewPlayer)
	{
		InitPlayerForRound(Cast<AUTPlayerState>(NewPlayer->PlayerState));
	}
	Super::PostLogin(NewPlayer);
}

void AUTCTFRoundGame::CreateGameURLOptions(TArray<TSharedPtr<TAttributePropertyBase>>& MenuProps)
{
	MenuProps.Empty();
	MenuProps.Add(MakeShareable(new TAttributeProperty<int32>(this, &BotFillCount, TEXT("BotFill"))));
	MenuProps.Add(MakeShareable(new TAttributePropertyBool(this, &bBalanceTeams, TEXT("BalanceTeams"))));
}

void AUTCTFRoundGame::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (!UGameplayStatics::HasOption(Options, TEXT("TimeLimit")))
	{
		AUTWorldSettings* WorldSettings = bUseLevelTiming ? Cast<AUTWorldSettings>(GetWorldSettings()) : nullptr;
		TimeLimit = WorldSettings ? WorldSettings->DefaultRoundLength : TimeLimit;
	}

	// key options are ?RoundLives=xx?Dash=xx?Asymm=xx?PerPlayerLives=xx?OffKillsForPowerup=xx?DefKillsForPowerup=xx?AllowPrototypePowerups=xx?DelayRally=xxx?Boost=xx
	RoundLives = FMath::Max(1, UGameplayStatics::GetIntOption(Options, TEXT("RoundLives"), RoundLives));

	FString InOpt = UGameplayStatics::ParseOption(Options, TEXT("OwnFlag"));
	bCarryOwnFlag = EvalBoolOptions(InOpt, bCarryOwnFlag);

	InOpt = UGameplayStatics::ParseOption(Options, TEXT("FlagReturn"));
	bNoFlagReturn = EvalBoolOptions(InOpt, bNoFlagReturn);

	InOpt = UGameplayStatics::ParseOption(Options, TEXT("PerPlayerLives"));
	bPerPlayerLives = EvalBoolOptions(InOpt, bPerPlayerLives);

	FlagPickupDelay = FMath::Max(1, UGameplayStatics::GetIntOption(Options, TEXT("FlagDelay"), FlagPickupDelay));

	InOpt = UGameplayStatics::ParseOption(Options, TEXT("SlowFC"));
	bSlowFlagCarrier = EvalBoolOptions(InOpt, bSlowFlagCarrier);
}

bool AUTCTFRoundGame::SkipPlacement(AUTCharacter* UTChar)
{
	return false; // (UTChar && FlagScorer && (UTChar->PlayerState == FlagScorer));
}

void AUTCTFRoundGame::RemoveLosers(int32 LoserTeam, int32 FlagTeam)
{
	// remove all dead or loser pawns
	for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
	{
		// Detach all controllers from their pawns
		AController* Controller = Iterator->Get();
		AUTPlayerState* PS = Controller ? Cast<AUTPlayerState>(Controller->PlayerState) : nullptr;
		if (Controller && Controller->GetPawn() && (!PS || !PS->Team || (PS->Team->TeamIndex == LoserTeam)))
		{
			Controller->PawnPendingDestroy(Controller->GetPawn());
			Controller->UnPossess();
		}
	}

	TArray<APawn*> PawnsToDestroy;
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		APawn* Pawn = It->Get();
		if (Pawn && !Pawn->GetController())
		{
			PawnsToDestroy.Add(Pawn);
		}
		else
		{
			AUTCharacter* Char = Cast<AUTCharacter>(*It);
			if (Char)
			{
				Char->SetAmbientSound(NULL);
				Char->SetLocalAmbientSound(NULL);
			}
		}
	}

	for (int32 i = 0; i<PawnsToDestroy.Num(); i++)
	{
		APawn* Pawn = PawnsToDestroy[i];
		if (Pawn != NULL && !Pawn->IsPendingKill())
		{
			Pawn->Destroy();
		}
	}

	for (FActorIterator It(GetWorld()); It; ++It)
	{
		AActor* TestActor = *It;
		if (TestActor && !TestActor->IsPendingKill() && TestActor->IsA<AUTProjectile>())
		{
			TestActor->Destroy();
		}
	}
}

void AUTCTFRoundGame::BeginGame()
{
	UE_LOG(UT, Log, TEXT("BEGIN GAME GameType: %s"), *GetNameSafe(this));
	UE_LOG(UT, Log, TEXT("Difficulty: %f GoalScore: %i TimeLimit (sec): %i"), GameDifficulty, GoalScore, TimeLimit);

	for (FActorIterator It(GetWorld()); It; ++It)
	{
		AActor* TestActor = *It;
		if (TestActor && !TestActor->IsPendingKill() && TestActor->IsA<AUTPlayerState>())
		{
			Cast<AUTPlayerState>(TestActor)->StartTime = 0;
			Cast<AUTPlayerState>(TestActor)->bSentLogoutAnalytics = false;
		}
		else if (TestActor && !TestActor->IsPendingKill() && TestActor->IsA<AUTProjectile>())
		{
			TestActor->Destroy();
		}
	}
	if (CTFGameState)
	{
		CTFGameState->ElapsedTime = 0;
	}

	//Let the game session override the StartMatch function, in case it wants to wait for arbitration
	if (GameSession->HandleStartMatchRequest())
	{
		return;
	}
	if (CTFGameState)
	{
		CTFGameState->CTFRound = 1;
		CTFGameState->NumRounds = NumRounds;
		CTFGameState->HalftimeScoreDelay = 0.5f;
	}
	float RealIntermissionDuration = IntermissionDuration;
	IntermissionDuration = 10.f;
	SetMatchState(MatchState::MatchIntermission);
	IntermissionDuration = RealIntermissionDuration;
	
	if ((!GetWorld() || !GetWorld()->GetGameState<AUTGameState>() || !GetWorld()->GetGameState<AUTGameState>()->LineUpHelper || !GetWorld()->GetGameState<AUTGameState>()->LineUpHelper->bIsActive))
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(It->Get());
			if (PC)
			{
				PC->ViewStartSpot();
			}
		}
	}
}

AActor* AUTCTFRoundGame::SetIntermissionCameras(uint32 TeamToWatch)
{
	PlacePlayersAroundFlagBase(TeamToWatch, TeamToWatch);
	return CTFGameState->FlagBases[TeamToWatch];
}

void AUTCTFRoundGame::HandleMatchIntermission()
{
	if (bFirstRoundInitialized)
	{
		// view defender base, with last team to score around it
		int32 TeamToWatch = IntermissionTeamToView(nullptr);

		if ((CTFGameState == NULL) || (TeamToWatch >= CTFGameState->FlagBases.Num()) || (CTFGameState->FlagBases[TeamToWatch] == NULL))
		{
			return;
		}

		UTGameState->PrepareForIntermission();

		AActor* IntermissionFocus = SetIntermissionCameras(TeamToWatch);
		// Tell the controllers to look at defender base
		for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
			if (PC != NULL)
			{
				PC->ClientPrepareForIntermission();
				PC->SetViewTarget(IntermissionFocus);
			}
		}
	}

	if (CTFGameState)
	{
		CTFGameState->bIsAtIntermission = true;
		CTFGameState->OnIntermissionChanged();
		CTFGameState->bStopGameClock = true;
		Cast<AUTCTFRoundGameState>(CTFGameState)->IntermissionTime = IntermissionDuration;
	}
}

float AUTCTFRoundGame::AdjustNearbyPlayerStartScore(const AController* Player, const AController* OtherController, const ACharacter* OtherCharacter, const FVector& StartLoc, const APlayerStart* P)
{
	float ScoreAdjust = 0.f;
	float NextDist = (OtherCharacter->GetActorLocation() - StartLoc).Size();

	if (NextDist < 8000.0f)
	{
		if (!UTGameState->OnSameTeam(Player, OtherController))
		{
			static FName NAME_RatePlayerStart = FName(TEXT("RatePlayerStart"));
			bool bIsLastKiller = (OtherCharacter->PlayerState == Cast<AUTPlayerState>(Player->PlayerState)->LastKillerPlayerState);
			if (!GetWorld()->LineTraceTestByChannel(StartLoc, OtherCharacter->GetActorLocation() + FVector(0.f, 0.f, OtherCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), ECC_Visibility, FCollisionQueryParams(NAME_RatePlayerStart, false)))
			{
				// Avoid the last person that killed me
				if (bIsLastKiller)
				{
					ScoreAdjust -= 7.f;
				}

				ScoreAdjust -= (5.f - 0.0003f * NextDist);
			}
			else if (NextDist < 4000.0f)
			{
				// Avoid the last person that killed me
				ScoreAdjust -= bIsLastKiller ? 5.f : 0.0005f * (5000.f - NextDist);

				if (!GetWorld()->LineTraceTestByChannel(StartLoc, OtherCharacter->GetActorLocation(), ECC_Visibility, FCollisionQueryParams(NAME_RatePlayerStart, false, this)))
				{
					ScoreAdjust -= 2.f;
				}
			}
		}
		else if ((NextDist < 3000.f) && Player && Cast<AUTPlayerState>(Player->PlayerState) && IsPlayerOnLifeLimitedTeam(Cast<AUTPlayerState>(Player->PlayerState)))
		{
			ScoreAdjust += (3000.f - NextDist) / 1000.f;
		}
	}
	return ScoreAdjust;
}

bool AUTCTFRoundGame::CheckScore_Implementation(AUTPlayerState* Scorer)
{
	CheckForWinner(Scorer->Team);
	return true;
}

void AUTCTFRoundGame::PlayEndOfMatchMessage()
{
	if (UTGameState && UTGameState->WinningTeam)
	{
		int32 IsFlawlessVictory = (UTGameState->WinningTeam->Score == 12);
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* Controller = Iterator->Get();
			if (Controller && Controller->IsA(AUTPlayerController::StaticClass()))
			{
				AUTPlayerController* PC = Cast<AUTPlayerController>(Controller);
				if (PC && Cast<AUTPlayerState>(PC->PlayerState))
				{
					if (bSecondaryWin)
					{
						PC->ClientReceiveLocalizedMessage(VictoryMessageClass, 4 + ((UTGameState->WinningTeam == Cast<AUTPlayerState>(PC->PlayerState)->Team) ? 1 : 0), UTGameState->WinnerPlayerState, PC->PlayerState, UTGameState->WinningTeam);
					}
					else
					{
						PC->ClientReceiveLocalizedMessage(VictoryMessageClass, 2 * IsFlawlessVictory + ((UTGameState->WinningTeam == Cast<AUTPlayerState>(PC->PlayerState)->Team) ? 1 : 0), UTGameState->WinnerPlayerState, PC->PlayerState, UTGameState->WinningTeam);
					}
				}
			}
		}
	}
}

void AUTCTFRoundGame::EndTeamGame(AUTTeamInfo* Winner, FName Reason)
{
	// Dont ever end the game in PIE
	if (GetWorld()->WorldType == EWorldType::PIE) return;

	APlayerController* LocalPC = GEngine->GetFirstLocalPlayerController(GetWorld());
	UUTLocalPlayer* LP = LocalPC ? Cast<UUTLocalPlayer>(LocalPC->Player) : NULL;
	if (LP)
	{
		LP->EarnedStars = 0;
		LP->RosterUpgradeText = FText::GetEmpty();
		if (bOfflineChallenge && PlayerWonChallenge())
		{
			LP->ChallengeCompleted(ChallengeTag, ChallengeDifficulty + 1);
		}
	}

	UTGameState->WinningTeam = Winner;
	EndTime = GetWorld()->TimeSeconds;

	if (IsGameInstanceServer() && LobbyBeacon)
	{
		FString MatchStats = FString::Printf(TEXT("%i"), CTFGameState->ElapsedTime);

		FMatchUpdate MatchUpdate;
		MatchUpdate.GameTime = UTGameState->ElapsedTime;
		MatchUpdate.NumPlayers = NumPlayers;
		MatchUpdate.NumSpectators = NumSpectators;
		MatchUpdate.MatchState = MatchState;
		MatchUpdate.bMatchHasBegun = HasMatchStarted();
		MatchUpdate.bMatchHasEnded = HasMatchEnded();

		UpdateLobbyScore(MatchUpdate);
		LobbyBeacon->EndGame(MatchUpdate);
	}

	// SETENDGAMEFOCUS
	EndMatch();
	AActor* EndMatchFocus = SetIntermissionCameras(Winner->TeamIndex);

	AUTCTFFlagBase* WinningBase =Cast<AUTCTFFlagBase>(EndMatchFocus);
	for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
	{
		AUTPlayerController* Controller = Cast<AUTPlayerController>(*Iterator);
		if (Controller && Controller->UTPlayerState)
		{
			AUTCTFFlagBase* BaseToView = WinningBase;
			// If we don't have a winner, view my base
			if (BaseToView == NULL)
			{
				AUTTeamInfo* MyTeam = Controller->UTPlayerState->Team;
				if (MyTeam)
				{
					BaseToView = CTFGameState->FlagBases[MyTeam->GetTeamNum()];
				}
			}

			if (BaseToView)
			{
				if (UTGameState->LineUpHelper && UTGameState->LineUpHelper->bIsActive)
				{
					Controller->GameHasEnded(Controller->GetPawn(), (Controller->UTPlayerState->Team && (Controller->UTPlayerState->Team->TeamIndex == Winner->TeamIndex)));
				}
				else
				{
					Controller->GameHasEnded(BaseToView, (Controller->UTPlayerState->Team && (Controller->UTPlayerState->Team->TeamIndex == Winner->TeamIndex)));
				}
			}
		}
	}

	// Allow replication to happen before reporting scores, stats, etc.
	FTimerHandle TempHandle;
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTGameMode::HandleMatchHasEnded, 1.5f);
	bGameEnded = true;

	// Setup a timer to pop up the final scoreboard on everyone
	FTimerHandle TempHandle2;
	GetWorldTimerManager().SetTimer(TempHandle2, this, &AUTGameMode::ShowFinalScoreboard, EndScoreboardDelay*GetActorTimeDilation());

	// Setup a timer to continue to the next map.  Need enough time for match summaries
	EndTime = GetWorld()->TimeSeconds;
	float TravelDelay = GetTravelDelay();
	FTimerHandle TempHandle3;
	GetWorldTimerManager().SetTimer(TempHandle3, this, &AUTGameMode::TravelToNextMap, TravelDelay*GetActorTimeDilation());

	FTimerHandle TempHandle4;
	float EndReplayDelay = TravelDelay - 10.f;
	GetWorldTimerManager().SetTimer(TempHandle4, this, &AUTCTFRoundGame::StopRCTFReplayRecording, EndReplayDelay);

	SendEndOfGameStats(Reason);
}

void AUTCTFRoundGame::StopRCTFReplayRecording()
{
	if (Super::UTIsHandlingReplays() && GetGameInstance() != nullptr)
	{
		GetGameInstance()->StopRecordingReplay();
	}
}


void AUTCTFRoundGame::ScoreObject_Implementation(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason)
{
	for (int32 i = 0; i < Teams.Num(); i++)
	{
		if (Teams[i])
		{
			Teams[i]->RoundBonus = 0;
		}
	}
	if (Reason == FName("FlagCapture"))
	{
		if (UTGameState)
		{
			// force replication of server clock time
			UTGameState->SetRemainingTime(UTGameState->GetRemainingTime());
			if (Holder && Holder->Team)
			{
				Holder->Team->RoundBonus = FMath::Min(MaxTimeScoreBonus, UTGameState->GetRemainingTime());
				UpdateTiebreak(Holder->Team->RoundBonus, Holder->Team->TeamIndex);
			}
		}
	}

	Super::ScoreObject_Implementation(GameObject, HolderPawn, Holder, Reason);
}

void AUTCTFRoundGame::UpdateTiebreak(int32 Bonus, int32 TeamIndex)
{
	AUTCTFRoundGameState* RCTFGameState = Cast<AUTCTFRoundGameState>(CTFGameState);
	if (RCTFGameState)
	{
		if (Bonus == MaxTimeScoreBonus)
		{
			Bonus = 60;
		}
		else
		{
			while (Bonus > 59)
			{
				Bonus -= 60;
			}
		}
		if (TeamIndex == 0)
		{
			RCTFGameState->TiebreakValue += Bonus;
		}
		else
		{
			RCTFGameState->TiebreakValue -= Bonus;
		}
	}
}

void AUTCTFRoundGame::HandleFlagCapture(AUTCharacter* HolderPawn, AUTPlayerState* Holder)
{
	FlagScorer = Holder;
	CheckScore(Holder);
	if (UTGameState && UTGameState->IsMatchInProgress())
	{
		Holder->AddCoolFactorEvent(400.0f);

		SetMatchState(MatchState::MatchIntermission);
	}
}

int32 AUTCTFRoundGame::IntermissionTeamToView(AUTPlayerController* PC)
{
	if (LastTeamToScore)
	{
		return LastTeamToScore->TeamIndex;
	}
	return Super::IntermissionTeamToView(PC);
}

void AUTCTFRoundGame::BuildServerResponseRules(FString& OutRules)
{
	OutRules += FString::Printf(TEXT("Goal Score\t%i\t"), GoalScore);

	AUTMutator* Mut = BaseMutator;
	while (Mut)
	{
		OutRules += FString::Printf(TEXT("Mutator\t%s\t"), *Mut->DisplayName.ToString());
		Mut = Mut->NextMutator;
	}
}

void AUTCTFRoundGame::IntermissionSwapSides()
{
	// swap sides, if desired
	AUTWorldSettings* Settings = Cast<AUTWorldSettings>(GetWorld()->GetWorldSettings());
	if (Settings != NULL && Settings->bAllowSideSwitching)
	{
		CTFGameState->ChangeTeamSides(1);
	}
}

void AUTCTFRoundGame::InitGameState()
{
	Super::InitGameState();

	CTFGameState->CTFRound = 1;
	CTFGameState->NumRounds = NumRounds;
}

void AUTCTFRoundGame::HandleExitingIntermission()
{
	CTFGameState->bStopGameClock = false;
	CTFGameState->HalftimeScoreDelay = 3.f;
	RemoveAllPawns();

	if (bFirstRoundInitialized)
	{
		IntermissionSwapSides();
	}
	else
	{
		CTFGameState->CTFRound = 0;
	}

	InitRound();
	if (!bFirstRoundInitialized)
	{
		bFirstRoundInitialized = true;
		if (Super::UTIsHandlingReplays() && GetGameInstance() != nullptr)
		{
			GetGameInstance()->StartRecordingReplay(TEXT(""), GetWorld()->GetMapName());
		}
	}

	//now respawn all the players
	for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
	{
		AController* Controller = Iterator->Get();
		if (Controller->PlayerState != NULL && !Controller->PlayerState->bOnlySpectator)
		{
			RestartPlayer(Controller);

			// Reset group taunt
			AUTPlayerState* PS = Cast<AUTPlayerState>(Controller->PlayerState);
			if (PS)
			{
				PS->ActiveGroupTaunt = nullptr;
				PS->ClearRoundStats();
			}
		}
	}

	// Send all flags home..
	CTFGameState->ResetFlags();
	CTFGameState->bIsAtIntermission = false;
	CTFGameState->OnIntermissionChanged();
	CTFGameState->SetTimeLimit(TimeLimit);		// Reset the GameClock for the second time.
	SetMatchState(MatchState::InProgress);
}

void AUTCTFRoundGame::InitFlagForRound(AUTCarriedObject* Flag)
{
	if (Flag != nullptr)
	{
		Flag->AutoReturnTime = 8.f;
		Flag->bGradualAutoReturn = true;
		Flag->bDisplayHolderTrail = true;
		Flag->bShouldPingFlag = true;
		Flag->bSlowsMovement = bSlowFlagCarrier;
		Flag->ClearGhostFlags();
		Flag->bSendHomeOnScore = false;
		Flag->bEnemyCanPickup = !bCarryOwnFlag;
		Flag->bFriendlyCanPickup = bCarryOwnFlag;
		Flag->bTeamPickupSendsHome = !Flag->bFriendlyCanPickup && !bNoFlagReturn;
		Flag->bEnemyPickupSendsHome = !Flag->bEnemyCanPickup && !bNoFlagReturn;
	}
}

void AUTCTFRoundGame::InitFlags()
{
	for (AUTCTFFlagBase* Base : CTFGameState->FlagBases)
	{
		if (Base != NULL && Base->MyFlag)
		{
			InitFlagForRound(Base->MyFlag);

			// check for flag carrier already here waiting
			TArray<AActor*> Overlapping;
			Base->MyFlag->GetOverlappingActors(Overlapping, AUTCharacter::StaticClass());
			for (AActor* A : Overlapping)
			{
				AUTCharacter* Character = Cast<AUTCharacter>(A);
				if (Character != NULL)
				{
					if (!GetWorld()->LineTraceTestByChannel(Character->GetActorLocation(), Base->MyFlag->GetActorLocation(), ECC_Pawn, FCollisionQueryParams(), WorldResponseParams))
					{
						Base->MyFlag->TryPickup(Character);
					}
				}
			}
		}
	}
}

void AUTCTFRoundGame::FlagCountDown()
{
	AUTCTFRoundGameState* RCTFGameState = Cast<AUTCTFRoundGameState>(CTFGameState);
	if (RCTFGameState && IsMatchInProgress() && (MatchState != MatchState::MatchIntermission))
	{
		RCTFGameState->RemainingPickupDelay--;
		if (RCTFGameState->RemainingPickupDelay > 0)
		{
			FTimerHandle TempHandle;
			GetWorldTimerManager().SetTimer(TempHandle, this, &AUTCTFRoundGame::FlagCountDown, 1.f*GetActorTimeDilation(), false);
		}
		else
		{
			FlagsAreReady();
		}
	}
}

void AUTCTFRoundGame::FlagsAreReady()
{
	BroadcastLocalized(this, UUTCTFMajorMessage::StaticClass(), 21, NULL, NULL, NULL);
	InitFlags();
}

void AUTCTFRoundGame::InitGameStateForRound()
{
	AUTCTFRoundGameState* RCTFGameState = Cast<AUTCTFRoundGameState>(CTFGameState);
	if (RCTFGameState)
	{
		RCTFGameState->CTFRound++;
		if (!bPerPlayerLives)
		{
			RCTFGameState->RedLivesRemaining = RoundLives;
			RCTFGameState->BlueLivesRemaining = RoundLives;
		}
		if (CTFGameState->FlagBases.Num() > 1)
		{
			RCTFGameState->RedLivesRemaining += CTFGameState->FlagBases[0] ? CTFGameState->FlagBases[0]->RoundLivesAdjustment : 0;
			RCTFGameState->BlueLivesRemaining += CTFGameState->FlagBases[1] ? CTFGameState->FlagBases[0]->RoundLivesAdjustment : 0;
		}

		RCTFGameState->RemainingPickupDelay = FlagPickupDelay;
	}
}

void AUTCTFRoundGame::InitRound()
{
	FlagScorer = nullptr;
	bFirstBloodOccurred = false;
	bLastManOccurred = false;
	bNeedFiveKillsMessage = true;
	InitGameStateForRound();
	ResetFlags();
	if (FlagPickupDelay > 0)
	{
		for (AUTCTFFlagBase* Base : CTFGameState->FlagBases)
		{
			if (Base != NULL && Base->MyFlag)
			{
				InitDelayedFlag(Base->MyFlag);
			}
		}
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTCTFRoundGame::FlagCountDown, 1.f*GetActorTimeDilation(), false);
	}
	else
	{
		InitFlags();
	}

	for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		InitPlayerForRound(PS);

	}
	CTFGameState->SetTimeLimit(TimeLimit);

	// re-initialize all AI squads, in case objectives have changed sides
	for (AUTTeamInfo* Team : Teams)
	{
		Team->ReinitSquads();
	}
}

void AUTCTFRoundGame::InitDelayedFlag(AUTCarriedObject* Flag)
{
	if (Flag != nullptr)
	{
		Flag->bEnemyCanPickup = false;
		Flag->bFriendlyCanPickup = false;
		Flag->bTeamPickupSendsHome = false;
		Flag->bEnemyPickupSendsHome = false;
	}
}

bool AUTCTFRoundGame::CheckForWinner(AUTTeamInfo* ScoringTeam)
{
	if (ScoringTeam && CTFGameState && (CTFGameState->CTFRound >= NumRounds) && (CTFGameState->CTFRound % 2 == 0))
	{
		AUTTeamInfo* BestTeam = ScoringTeam;
		bool bHaveTie = false;

		// Check if team with highest score has reached goal score
		for (AUTTeamInfo* Team : Teams)
		{
			if (Team->Score > BestTeam->Score)
			{
				BestTeam = Team;
				bHaveTie = false;
				bSecondaryWin = false;
			}
			else if ((Team != BestTeam) && (Team->Score == BestTeam->Score))
			{
				bHaveTie = true;
				AUTCTFRoundGameState* GS = Cast<AUTCTFRoundGameState>(UTGameState);
				if (GS && (GS->TiebreakValue != 0))
				{
					BestTeam = (GS->TiebreakValue > 0) ? Teams[0] : Teams[1];
					bHaveTie = false;
					bSecondaryWin = true;
				}
			}
		}
		if (!bHaveTie)
		{
			EndTeamGame(BestTeam, FName(TEXT("scorelimit")));
			return true;
		}
	}
	return false;
}

void AUTCTFRoundGame::ResetFlags()
{
	for (AUTCTFFlagBase* Base : CTFGameState->FlagBases)
	{
		if (Base != NULL && Base->MyFlag)
		{
			Base->MyFlag->SetActorHiddenInGame(true);
			Base->ClearDefenseEffect();
			if (IsTeamOnDefense(Base->MyFlag->GetTeamNum()))
			{
				Base->SpawnDefenseEffect();
			}
		}
	}
}

void AUTCTFRoundGame::SetPlayerStateInactive(APlayerState* NewPlayerState)
{
	Super::SetPlayerStateInactive(NewPlayerState);
	AUTPlayerState* PS = Cast<AUTPlayerState>(NewPlayerState);
	if (PS && !PS->bOnlySpectator && UTGameState && UTGameState->IsMatchIntermission())
	{
		PS->RemainingLives = RoundLives;
	}
	if (PS)
	{
		PS->ClearRoundStats();
	}
}

void AUTCTFRoundGame::InitPlayerForRound(AUTPlayerState* PS)
{
	if (PS)
	{
		PS->bHasLifeLimit = bPerPlayerLives;
		PS->RoundKills = 0;
		PS->RoundKillAssists = 0;
		PS->NextRallyTime = GetWorld()->GetTimeSeconds();
		PS->RespawnWaitTime = IsPlayerOnLifeLimitedTeam(PS) ? LimitedRespawnWaitTime : UnlimitedRespawnWaitTime;
		PS->SetRemainingBoosts(InitialBoostCount);
		PS->bSpecialTeamPlayer = false;
		PS->bSpecialPlayer = false;
		if (GetNetMode() != NM_DedicatedServer)
		{
			PS->OnRepSpecialPlayer();
			PS->OnRepSpecialTeamPlayer();
		}
		if (PS && (!PS->Team || PS->bOnlySpectator))
		{
			PS->RemainingLives = 0;
			PS->SetOutOfLives(true);
		}
		else if (PS)
		{
			PS->RemainingLives = RoundLives;
			PS->bHasLifeLimit = IsPlayerOnLifeLimitedTeam(PS);
			PS->SetOutOfLives(false);
		}
		PS->ForceNetUpdate();
	}
}

void AUTCTFRoundGame::HandleTeamChange(AUTPlayerState* PS, AUTTeamInfo* OldTeam)
{
	if ((GetWorld()->WorldType == EWorldType::PIE) || bDevServer || !PS || !UTGameState || (UTGameState->GetMatchState() != MatchState::InProgress))
	{
		return;
	}
	PS->bHasLifeLimit = IsPlayerOnLifeLimitedTeam(PS);
	if (bSitOutDuringRound)
	{
		PS->RemainingLives = 0;
	}
	if (PS->RemainingLives == 0 && IsPlayerOnLifeLimitedTeam(PS))
	{
		PS->SetOutOfLives(true);
		PS->ForceRespawnTime = 1.f;
	}

	// verify that OldTeam and New team still have live players
	AUTTeamInfo* NewTeam = PS->Team;
	bool bOldTeamHasPlayers = false;
	bool bNewTeamHasPlayers = false;
	for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
	{
		AUTPlayerState* OtherPS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (OtherPS && !OtherPS->bOutOfLives && !OtherPS->bIsInactive)
		{
			if (OldTeam && (OtherPS->Team == OldTeam))
			{
				bOldTeamHasPlayers = true;
			}
			if (NewTeam && (OtherPS->Team == NewTeam))
			{
				bNewTeamHasPlayers = true;
			}
		}
	}
	if (!bOldTeamHasPlayers && OldTeam)
	{
		ScoreAlternateWin((OldTeam->TeamIndex == 0) ? 1 : 0);
	}
	else if (!bNewTeamHasPlayers && NewTeam)
	{
		ScoreAlternateWin((NewTeam->TeamIndex == 0) ? 1 : 0);
	}
}

bool AUTCTFRoundGame::ChangeTeam(AController* Player, uint8 NewTeamIndex, bool bBroadcast)
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(Player->PlayerState);
	AUTTeamInfo* OldTeam = PS->Team;
	bool bResult = Super::ChangeTeam(Player, NewTeamIndex, bBroadcast);
	if (bResult && (GetMatchState() == MatchState::InProgress))
	{
		HandleTeamChange(PS, OldTeam);
	}
	return bResult;
}

void AUTCTFRoundGame::SendRestartNotifications(AUTPlayerState* PS, AUTPlayerController* PC)
{
}

void AUTCTFRoundGame::RestartPlayer(AController* aPlayer)
{
	if ((!IsMatchInProgress() && bPlacingPlayersAtIntermission) || (GetMatchState() == MatchState::MatchIntermission) || (UTGameState && UTGameState->LineUpHelper && UTGameState->LineUpHelper->bIsActive && UTGameState->LineUpHelper->bIsPlacingPlayers))
	{
		// placing players during intermission
		if (bPlacingPlayersAtIntermission || (UTGameState && UTGameState->LineUpHelper && UTGameState->LineUpHelper->bIsActive && UTGameState->LineUpHelper->bIsPlacingPlayers))
		{
			AGameMode::RestartPlayer(aPlayer);
		}
		return;
	}
	AUTPlayerState* PS = Cast<AUTPlayerState>(aPlayer->PlayerState);
	AUTPlayerController* PC = Cast<AUTPlayerController>(aPlayer);
	if (bPerPlayerLives && PS && PS->Team && HasMatchStarted())
	{
		if (IsPlayerOnLifeLimitedTeam(PS) && (PS->RemainingLives == 0) && (GetMatchState() == MatchState::InProgress))
		{
			// failsafe for player that leaves match before RemainingLives are set and then rejoins
			PS->SetOutOfLives(true);
		}
		if (PS->bOutOfLives)
		{
			if (PC != NULL)
			{
				PC->ChangeState(NAME_Spectating);
				PC->ClientGotoState(NAME_Spectating);

				for (AController* Member : PS->Team->GetTeamMembers())
				{
					if (Member->GetPawn() != NULL)
					{
						PC->ServerViewPlayerState(Member->PlayerState);
						break;
					}
				}
			}
			return;
		}
		if (IsPlayerOnLifeLimitedTeam(PS))
		{
			if ((PS->RemainingLives > 0) && IsMatchInProgress() && (GetMatchState() != MatchState::MatchIntermission))
			{
				if (PS->RemainingLives == 1)
				{
					if (PC)
					{
						PC->ClientReceiveLocalizedMessage(UUTShowdownStatusMessage::StaticClass(), 5, PS, NULL, NULL);
					}
					PS->AnnounceStatus(StatusMessage::LastLife, 0, true);
					PS->RespawnWaitTime = 0.5f;
					PS->ForceNetUpdate();
					PS->OnRespawnWaitReceived();
				}
				else if (PC)
				{
					PC->ClientReceiveLocalizedMessage(UUTShowdownStatusMessage::StaticClass(), 30+PS->RemainingLives, PS, NULL, NULL);
				}
			}
			else
			{
				return;
			}
		}
	}
	SendRestartNotifications(PS, PC);
	Super::RestartPlayer(aPlayer);

	AUTCTFRoundGameState* RCTFGameState = Cast<AUTCTFRoundGameState>(CTFGameState);
	if (aPlayer->GetPawn() && !bPerPlayerLives && (RoundLives > 0) && PS && PS->Team && RCTFGameState && RCTFGameState->IsMatchInProgress())
	{
		if ((PS->Team->TeamIndex == 0) && IsPlayerOnLifeLimitedTeam(PS))
		{
			RCTFGameState->RedLivesRemaining--;
			if (RCTFGameState->RedLivesRemaining <= 0)
			{
				RCTFGameState->RedLivesRemaining = 0;
				ScoreAlternateWin(1);
				return;
			}
			else if (bNeedFiveKillsMessage && (RCTFGameState->RedLivesRemaining == 5))
			{
				bNeedFiveKillsMessage = false;
				BroadcastLocalized(NULL, UUTShowdownGameMessage::StaticClass(), 7);
			}
		}
		else if ((PS->Team->TeamIndex == 1) && IsPlayerOnLifeLimitedTeam(PS))
		{
			RCTFGameState->BlueLivesRemaining--;
			if (RCTFGameState->BlueLivesRemaining <= 0)
			{
				RCTFGameState->BlueLivesRemaining = 0;
				ScoreAlternateWin(0);
				return;
			}
			else if (bNeedFiveKillsMessage && (RCTFGameState->BlueLivesRemaining == 5))
			{
				bNeedFiveKillsMessage = false;
				BroadcastLocalized(NULL, UUTShowdownGameMessage::StaticClass(), 7);
			}
		}
	}
}

void AUTCTFRoundGame::HandleRollingAttackerRespawn(AUTPlayerState* OtherPS)
{
	if (GetWorld()->GetTimeSeconds() - LastAttackerSpawnTime < 1.f)
	{
		OtherPS->RespawnWaitTime = 1.f;
	}
	else
	{
		if (GetWorld()->GetTimeSeconds() - RollingSpawnStartTime < RollingAttackerRespawnDelay)
		{
			OtherPS->RespawnWaitTime = RollingAttackerRespawnDelay - GetWorld()->GetTimeSeconds() + RollingSpawnStartTime;
		}
		else
		{
			RollingSpawnStartTime = GetWorld()->GetTimeSeconds();
			OtherPS->RespawnWaitTime = RollingAttackerRespawnDelay;
		}
		// if friendly hanging near flag base, respawn right away
		AUTCTFFlagBase* TeamBase = CTFGameState->FlagBases[OtherPS->Team->TeamIndex];
		for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
		{
			AUTCharacter* Teammate = Cast<AUTCharacter>(*It);
			if (Teammate && !Teammate->IsDead() && CTFGameState && CTFGameState->OnSameTeam(OtherPS, Teammate) && ((Teammate->GetActorLocation() - TeamBase->GetActorLocation()).SizeSquared() < 4000000.f))
			{
				OtherPS->RespawnWaitTime = 1.f;
				break;
			}
		}
	}
}

void AUTCTFRoundGame::ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
	Super::ScoreKill_Implementation(Killer, Other, KilledPawn, DamageType);

	AUTPlayerState* OtherPS = Other ? Cast<AUTPlayerState>(Other->PlayerState) : nullptr;
	if (OtherPS && OtherPS->Team && IsTeamOnOffense(OtherPS->Team->TeamIndex) && bRollingAttackerSpawns)
	{
		HandleRollingAttackerRespawn(OtherPS);
		OtherPS->RespawnWaitTime += 0.01f * FMath::FRand();
		OtherPS->ForceNetUpdate();
		OtherPS->OnRespawnWaitReceived();
	}
	if (OtherPS && IsPlayerOnLifeLimitedTeam(OtherPS) && (OtherPS->RemainingLives > 0))
	{
		OtherPS->RemainingLives--;
		bool bEliminated = false;
		if (OtherPS->RemainingLives == 0)
		{
			// this player is out of lives
			OtherPS->SetOutOfLives(true);
			bEliminated = true;
			bool bFoundTeammate = false;
			for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
			{
				AUTPlayerState* TeamPS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
				if (TeamPS && (OtherPS->Team == TeamPS->Team) && !TeamPS->bOutOfLives && !TeamPS->bIsInactive)
				{
					// found a live teammate, so round isn't over - notify about termination though
					if (IsMatchInProgress() && (GetMatchState() != MatchState::MatchIntermission))
					{
						BroadcastLocalized(NULL, UUTShowdownRewardMessage::StaticClass(), 3, OtherPS);
					}
					bFoundTeammate = true;
					break;
				}
			}
			if (!bFoundTeammate)
			{
				BroadcastLocalized(NULL, UUTShowdownRewardMessage::StaticClass(), 4);
				CTFGameState->bStopGameClock = true;

				if (OtherPS->Team->TeamIndex == 0)
				{
					FTimerHandle TempHandle;
					GetWorldTimerManager().SetTimer(TempHandle, this, &AUTCTFRoundGame::ScoreBlueAlternateWin, 1.f);
				}
				else
				{
					FTimerHandle TempHandle;
					GetWorldTimerManager().SetTimer(TempHandle, this, &AUTCTFRoundGame::ScoreRedAlternateWin, 1.f);

				}
			}
		}

		int32 RemainingDefenders = 0;
		int32 RemainingLives = 0;

		// check if just transitioned to last man
		bool bWasAlreadyLastMan = bLastManOccurred;
		for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
			if (PS && (OtherPS->Team == PS->Team) && !PS->bOutOfLives && !PS->bIsInactive)
			{
				RemainingDefenders++;
				RemainingLives += PS->RemainingLives;
			}
		}
		bLastManOccurred = (RemainingDefenders == 1);
		if (bLastManOccurred && !bWasAlreadyLastMan)
		{
			for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
			{
				AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
				AUTPlayerController* PC = PS ? Cast<AUTPlayerController>(PS->GetOwner()) : nullptr;
				if (PC)
				{
					int32 MessageType = (OtherPS->Team == PS->Team) ? 1 : 0;
					PC->ClientReceiveLocalizedMessage(UUTShowdownRewardMessage::StaticClass(), MessageType, PS, NULL, NULL);
				}
			}
		}
		else if (((RemainingDefenders == 3) && bEliminated) || (RemainingLives < 10))
		{
			// find player on other team to speak message
			AUTPlayerState* Speaker = nullptr;
			for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
			{
				AUTPlayerState* TeamPS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
				if (TeamPS && TeamPS->Team && (OtherPS->Team != TeamPS->Team) && !TeamPS->bOutOfLives && !TeamPS->bIsInactive)
				{
					Speaker = TeamPS;
					break;
				}
			}
			if (Speaker != nullptr)
			{
				if ((RemainingDefenders == 3) && bEliminated)
				{
					Speaker->AnnounceStatus(StatusMessage::EnemyThreePlayers);
				}
				else if (RemainingLives == 9)
				{
					Speaker->AnnounceStatus(StatusMessage::EnemyLowLives);
				}
			}
		}

		if (OtherPS->RemainingLives > 0)
		{
			OtherPS->RespawnWaitTime = FMath::Max(1.f, float(RemainingDefenders));
			if (UTGameState && UTGameState->GetRemainingTime() > 180)
			{
				OtherPS->RespawnWaitTime = FMath::Min(OtherPS->RespawnWaitTime, 2.f);
			}
			OtherPS->RespawnWaitTime += 0.01f * FMath::FRand();
			OtherPS->ForceNetUpdate();
			OtherPS->OnRespawnWaitReceived();
		}
	}
}

void AUTCTFRoundGame::ScoreRedAlternateWin()
{
	if (IsMatchInProgress() && (GetMatchState() != MatchState::MatchIntermission))
	{
		ScoreAlternateWin(0);
	}
}

void AUTCTFRoundGame::ScoreBlueAlternateWin()
{
	if (IsMatchInProgress() && (GetMatchState() != MatchState::MatchIntermission))
	{
		ScoreAlternateWin(1);
	}
}

//Special markup for Analytics event so they show up properly in grafana. Should be eventually moved to UTAnalytics.
/*
* @EventName RCTFRoundResult
* @Trigger Sent when a round ends in an RCTF game through Score Alternate Win
* @Type Sent by the Server
* @EventParam FlagCapScore int32 Always 0, just shows that someone caped flag
* @Comments
*/

void AUTCTFRoundGame::AnnounceWin(AUTTeamInfo* WinningTeam, APlayerState* ScoringPlayer, uint8 Reason)
{
	BroadcastLocalized(NULL, UUTShowdownGameMessage::StaticClass(), 3 + WinningTeam->TeamIndex);
}

int32 AUTCTFRoundGame::GetDefenseScore()
{
	return 1;
}

void AUTCTFRoundGame::ScoreAlternateWin(int32 WinningTeamIndex, uint8 Reason)
{
	FindAndMarkHighScorer();
	AUTTeamInfo* WinningTeam = (Teams.Num() > WinningTeamIndex) ? Teams[WinningTeamIndex] : NULL;
	if (WinningTeam)
	{
		if (Reason != 2)
		{
			WinningTeam->Score += IsTeamOnOffense(WinningTeamIndex) ? GetFlagCapScore() : GetDefenseScore();
		}
		if (CTFGameState)
		{
			for (int32 i = 0; i < Teams.Num(); i++)
			{
				if (Teams[i])
				{
					Teams[i]->RoundBonus = 0;
				}
			}
			if (Reason != 2)
			{
				WinningTeam->RoundBonus = FMath::Min(MaxTimeScoreBonus, CTFGameState->GetRemainingTime());
				UpdateTiebreak(WinningTeam->RoundBonus, WinningTeam->TeamIndex);
			}

			FCTFScoringPlay NewScoringPlay;
			NewScoringPlay.Team = WinningTeam;
			NewScoringPlay.bDefenseWon = !IsTeamOnOffense(WinningTeamIndex);
			NewScoringPlay.Period = CTFGameState->CTFRound;
			NewScoringPlay.bAnnihilation = (Reason == 0);
			NewScoringPlay.TeamScores[0] = CTFGameState->Teams[0] ? CTFGameState->Teams[0]->Score : 0;
			NewScoringPlay.TeamScores[1] = CTFGameState->Teams[1] ? CTFGameState->Teams[1]->Score : 0;
			NewScoringPlay.RemainingTime = CTFGameState->GetRemainingTime();
			NewScoringPlay.RedBonus = CTFGameState->Teams[0] ? CTFGameState->Teams[0]->RoundBonus : 0;
			NewScoringPlay.BlueBonus = CTFGameState->Teams[1] ? CTFGameState->Teams[1]->RoundBonus : 0;
			CTFGameState->AddScoringPlay(NewScoringPlay);

			// force replication of server clock time
			CTFGameState->SetRemainingTime(CTFGameState->GetRemainingTime());
		}

		WinningTeam->ForceNetUpdate();
		LastTeamToScore = WinningTeam;
		AnnounceWin(WinningTeam, nullptr, Reason);
		CheckForWinner(LastTeamToScore);
		if (UTGameState->IsMatchInProgress())
		{
			SetMatchState(MatchState::MatchIntermission);
		}

		if (FUTAnalytics::IsAvailable())
		{
			if (GetWorld()->GetNetMode() != NM_Standalone)
			{
				TArray<FAnalyticsEventAttribute> ParamArray;
				ParamArray.Add(FAnalyticsEventAttribute(TEXT("FlagCapScore"), 0));
				FUTAnalytics::GetProvider().RecordEvent(TEXT("RCTFRoundResult"), ParamArray);
			}
		}
	}
}

void AUTCTFRoundGame::EndPlayerIntro()
{
	BeginGame();
}

void AUTCTFRoundGame::CheckRoundTimeVictory()
{
}

void AUTCTFRoundGame::CheckGameTime()
{
	AUTCTFRoundGameState* RCTFGameState = Cast<AUTCTFRoundGameState>(CTFGameState);
	if (CTFGameState->IsMatchIntermission())
	{
		if (RCTFGameState && (RCTFGameState->IntermissionTime <= 0))
		{
			SetMatchState(MatchState::MatchExitingIntermission);
		}
	}
	else if ((GetMatchState() == MatchState::InProgress) && TimeLimit > 0)
	{
		CheckRoundTimeVictory();
	}
	else
	{
		Super::CheckGameTime();
	}
}

bool AUTCTFRoundGame::IsTeamOnOffense(int32 TeamNumber) const
{
	return true;
}

bool AUTCTFRoundGame::IsTeamOnDefense(int32 TeamNumber) const
{
	return !IsTeamOnOffense(TeamNumber);
}

bool AUTCTFRoundGame::IsPlayerOnLifeLimitedTeam(AUTPlayerState* PlayerState) const
{
	AUTCTFRoundGameState* RCTFGameState = Cast<AUTCTFRoundGameState>(CTFGameState);
	if (RCTFGameState == nullptr)
	{
		return false;
	}
	return PlayerState && PlayerState->Team && IsTeamOnOffense(PlayerState->Team->TeamIndex) ? RCTFGameState->bAttackerLivesLimited : RCTFGameState->bDefenderLivesLimited;
}


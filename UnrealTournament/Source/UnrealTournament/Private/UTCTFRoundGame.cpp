// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamGameMode.h"
#include "UTHUD_CTF.h"
#include "UTCTFRoundGame.h"
#include "UTCTFGameMessage.h"
#include "UTCTFRoleMessage.h"
#include "UTCTFRewardMessage.h"
#include "UTFirstBloodMessage.h"
#include "UTCountDownMessage.h"
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
#include "UTPlayerStart.h"
#include "UTSkullPickup.h"

AUTCTFRoundGame::AUTCTFRoundGame(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	GoalScore = 5;
	TimeLimit = 5;
	DisplayName = NSLOCTEXT("UTGameMode", "CTFR", "Round based CTF");
	RoundLives = 5;
	bPerPlayerLives = true;
	bNeedFiveKillsMessage = true;
	FlagCapScore = 2;
	UnlimitedRespawnWaitTime = 5.f;
	bForceRespawn = true;
	bUseDash = false;
	bAsymmetricVictoryConditions = true;
	bCarryOwnFlag = true;
	bNoFlagReturn = true;
	bFirstRoundInitialized = false;
	ExtraHealth = 0;
	FlagPickupDelay = 10;
	RemainingPickupDelay = 0;

	// remove translocator - fixmesteve make this an option
	TranslocatorObject = nullptr;
}

void AUTCTFRoundGame::CreateGameURLOptions(TArray<TSharedPtr<TAttributePropertyBase>>& MenuProps)
{
	MenuProps.Add(MakeShareable(new TAttributeProperty<int32>(this, &GoalScore, TEXT("GoalScore"))));
	MenuProps.Add(MakeShareable(new TAttributeProperty<int32>(this, &BotFillCount, TEXT("BotFill"))));
	MenuProps.Add(MakeShareable(new TAttributePropertyBool(this, &bBalanceTeams, TEXT("BalanceTeams"))));
	MenuProps.Add(MakeShareable(new TAttributeProperty<int32>(this, &MercyScore, TEXT("MercyScore"))));
}

bool AUTCTFRoundGame::AvoidPlayerStart(AUTPlayerStart* P)
{
	return P && (bAsymmetricVictoryConditions && P->bIgnoreInASymCTF);
}

void AUTCTFRoundGame::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	if (GoalScore == 0)
	{
		GoalScore = 5;
	}

	// key options are ?Respawnwait=xx?RoundLives=xx?CTFMode=xx?Dash=xx?Asymm=xx?PerPlayerLives=xx
	RoundLives = FMath::Max(1, UGameplayStatics::GetIntOption(Options, TEXT("RoundLives"), RoundLives));

	FString InOpt = UGameplayStatics::ParseOption(Options, TEXT("Dash"));
	bUseDash = EvalBoolOptions(InOpt, bUseDash);

	InOpt = UGameplayStatics::ParseOption(Options, TEXT("Asymm"));
	bAsymmetricVictoryConditions = EvalBoolOptions(InOpt, bAsymmetricVictoryConditions);

	InOpt = UGameplayStatics::ParseOption(Options, TEXT("OwnFlag"));
	bCarryOwnFlag = EvalBoolOptions(InOpt, bCarryOwnFlag);
	// FIXMESTEVE - if carry own flag, need option whether enemy flag needs to be home to score, and need base effect if not

	InOpt = UGameplayStatics::ParseOption(Options, TEXT("FlagReturn"));
	bNoFlagReturn = EvalBoolOptions(InOpt, bNoFlagReturn);

	InOpt = UGameplayStatics::ParseOption(Options, TEXT("PerPlayerLives"));
	bPerPlayerLives = EvalBoolOptions(InOpt, bPerPlayerLives);

	ExtraHealth = FMath::Max(0, UGameplayStatics::GetIntOption(Options, TEXT("XHealth"), ExtraHealth));
	FlagPickupDelay = FMath::Max(1, UGameplayStatics::GetIntOption(Options, TEXT("FlagDelay"), FlagPickupDelay));
}

void AUTCTFRoundGame::SetPlayerDefaults(APawn* PlayerPawn)
{
	AUTCharacter* UTC = Cast<AUTCharacter>(PlayerPawn);
	if (UTC != NULL)
	{
		UTC->HealthMax = UTC->GetClass()->GetDefaultObject<AUTCharacter>()->HealthMax + ExtraHealth;
		UTC->SuperHealthMax = UTC->GetClass()->GetDefaultObject<AUTCharacter>()->SuperHealthMax + ExtraHealth;
	}
	Super::SetPlayerDefaults(PlayerPawn);
}

void AUTCTFRoundGame::DiscardInventory(APawn* Other, AController* Killer)
{
	AUTCharacter* UTC = Cast<AUTCharacter>(Other);
	if (UTC != NULL)
	{
		FVector StartLocation = UTC->GetActorLocation() + UTC->GetActorRotation().Vector() * UTC->GetSimpleCollisionCylinderExtent().X;
		if (UTC->RedSkullPickupClass)
		{
			for (int32 i = 0; i < UTC->RedSkullCount; i++)
			{
				FVector TossVelocity = UTC->GetVelocity() + UTC->GetActorRotation().RotateVector(FVector(FMath::FRandRange(0.0f, 200.0f), FMath::FRandRange(-400.0f, 400.0f), FMath::FRandRange(0.0f, 200.0f)) + FVector(300.0f, 0.0f, 150.0f));
				TossSkull(UTC->RedSkullPickupClass, StartLocation, TossVelocity, UTC);
			}
		}
		if (UTC->BlueSkullPickupClass)
		{
			for (int32 i = 0; i < UTC->BlueSkullCount; i++)
			{
				FVector TossVelocity = UTC->GetVelocity() + UTC->GetActorRotation().RotateVector(FVector(FMath::FRandRange(0.0f, 200.0f), FMath::FRandRange(-400.0f, 400.0f), FMath::FRandRange(0.0f, 200.0f)) + FVector(300.0f, 0.0f, 150.0f));
				TossSkull(UTC->BlueSkullPickupClass, StartLocation, TossVelocity, UTC);
			}
		}

		UTC->RedSkullCount = 0;
		UTC->BlueSkullCount = 0;
	}

	Super::DiscardInventory(Other, Killer);
}

void AUTCTFRoundGame::TossSkull(TSubclassOf<AUTSkullPickup> SkullPickupClass, const FVector& StartLocation, const FVector& TossVelocity, AUTCharacter* FormerInstigator)
{
	// pull back spawn location if it is embedded in world geometry
	FVector AdjustedStartLoc = StartLocation;
	UCapsuleComponent* TestCapsule = SkullPickupClass.GetDefaultObject()->Collision;
	if (TestCapsule != NULL)
	{
		FCollisionQueryParams QueryParams(FName(TEXT("DropPlacement")), false);
		FHitResult Hit;
		if (GetWorld()->SweepSingleByChannel(Hit, StartLocation - FVector(TossVelocity.X, TossVelocity.Y, 0.0f) * 0.25f, StartLocation, FQuat::Identity, TestCapsule->GetCollisionObjectType(), TestCapsule->GetCollisionShape(), QueryParams, TestCapsule->GetCollisionResponseToChannels()) &&
			!Hit.bStartPenetrating)
		{
			AdjustedStartLoc = Hit.Location;
		}
	}

	FActorSpawnParameters Params;
	Params.Instigator = FormerInstigator;
	AUTDroppedPickup* Pickup = GetWorld()->SpawnActor<AUTDroppedPickup>(SkullPickupClass, AdjustedStartLoc, TossVelocity.Rotation(), Params);
	if (Pickup != NULL)
	{
		Pickup->Movement->Velocity = TossVelocity;
	}
}

bool AUTCTFRoundGame::CheckScore_Implementation(AUTPlayerState* Scorer)
{
	CheckReachedGoalScore(Scorer->Team);
	return true;
}

bool AUTCTFRoundGame::CheckReachedGoalScore(AUTTeamInfo* ScoringTeam)
{
	if (ScoringTeam && CTFGameState)
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
			}
			else if ((Team != BestTeam) && (Team->Score == BestTeam->Score))
			{
				bHaveTie = true;
			}
		}
		if (!bHaveTie && (BestTeam->Score >= GoalScore))
		{
			// game can end after even number of rounds, or if losing team cannot catch up
			if ((CTFGameState->CTFRound % 2 == 0) || (Teams.Num() <2) || !Teams[0] || !Teams[1] || (FMath::Abs(Teams[0]->Score - Teams[1]->Score) > FlagCapScore))
			{
				EndGame(nullptr, FName(TEXT("scorelimit")));
				return true;
			}
		}
	}
	return false;
}

void AUTCTFRoundGame::HandleFlagCapture(AUTPlayerState* Holder)
{
	CheckScore(Holder);
	if (UTGameState && UTGameState->IsMatchInProgress())
	{
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

void AUTCTFRoundGame::HandleMatchHasStarted()
{
	if (!bFirstRoundInitialized)
	{
		InitRound();
		CTFGameState->CTFRound = 1;
		CTFGameState->bAsymmetricVictoryConditions = bAsymmetricVictoryConditions;
		bFirstRoundInitialized = true;
	}
	Super::HandleMatchHasStarted();
}

void AUTCTFRoundGame::HandleExitingIntermission()
{
	InitRound();
	Super::HandleExitingIntermission();
}

void AUTCTFRoundGame::AnnounceMatchStart()
{
	BroadcastVictoryConditions();
	Super::AnnounceMatchStart();
}

void AUTCTFRoundGame::InitFlags()
{
	for (AUTCTFFlagBase* Base : CTFGameState->FlagBases)
	{
		if (Base != NULL && Base->MyFlag)
		{
			AUTCarriedObject* Flag = Base->MyFlag;
			Flag->AutoReturnTime = 10.f; // fixmesteve make config
			Flag->bGradualAutoReturn = true;
			Flag->bDisplayHolderTrail = true;
			if (bAsymmetricVictoryConditions)
			{
				if (bRedToCap == (Flag->GetTeamNum() == 0))
				{
					Flag->bEnemyCanPickup = !bCarryOwnFlag;
					Flag->bFriendlyCanPickup = bCarryOwnFlag;
					Flag->bTeamPickupSendsHome = !Flag->bFriendlyCanPickup && !bNoFlagReturn;
					Flag->bEnemyPickupSendsHome = !Flag->bEnemyCanPickup && !bNoFlagReturn;
				}
				else
				{
					Flag->bEnemyCanPickup = false;
					Flag->bFriendlyCanPickup = false;
					Flag->bTeamPickupSendsHome = false;
					Flag->bEnemyPickupSendsHome = false;
				}
			}
			else
			{
				Flag->bEnemyCanPickup = !bCarryOwnFlag;
				Flag->bFriendlyCanPickup = bCarryOwnFlag;
				Flag->bTeamPickupSendsHome = !Flag->bFriendlyCanPickup && !bNoFlagReturn;
				Flag->bEnemyPickupSendsHome = !Flag->bEnemyCanPickup && !bNoFlagReturn;
			}
			// check for flag carrier already here waiting
			TArray<AActor*> Overlapping;
			Base->GetOverlappingActors(Overlapping, APawn::StaticClass());
			for (AActor* A : Overlapping)
			{
				Base->OnOverlapBegin(A, Cast<UPrimitiveComponent>(A->GetRootComponent()), 0, false, FHitResult(Base, Base->Capsule, A->GetActorLocation(), (A->GetActorLocation() - Base->GetActorLocation()).GetSafeNormal()));
			}
		}
	}
}

void AUTCTFRoundGame::BroadcastVictoryConditions()
{
	if (bAsymmetricVictoryConditions)
	{
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
			if (PC)
			{
				if (bRedToCap == (PC->GetTeamNum() == 0))
				{
					PC->ClientReceiveLocalizedMessage(UUTCTFRoleMessage::StaticClass(), bRedToCap ? 1 : 2);
				}
				else
				{
					PC->ClientReceiveLocalizedMessage(UUTCTFRoleMessage::StaticClass(), bRedToCap ? 3 : 4);
				}
			}
		}
	}
	else if (RoundLives > 0)
	{
		BroadcastLocalized(this, UUTCTFRoleMessage::StaticClass(), 5, NULL, NULL, NULL);
	}
}

void AUTCTFRoundGame::FlagCountDown()
{
	RemainingPickupDelay--;
	if (RemainingPickupDelay > 0)
	{
		if (GetMatchState() == MatchState::InProgress)
		{
			int32 CountdownMessage = bAsymmetricVictoryConditions ? (bRedToCap ? 17 : 18) : 19;
			BroadcastLocalized(this, UUTCTFGameMessage::StaticClass(), CountdownMessage, NULL, NULL, NULL);
			BroadcastLocalized(this, UUTCountDownMessage::StaticClass(), RemainingPickupDelay, NULL, NULL, NULL);
		}

		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTCTFRoundGame::FlagCountDown, 1.f*GetActorTimeDilation(), false);
	}
	else
	{
		InitFlags();
	}
}

void AUTCTFRoundGame::InitRound()
{
	bFirstBloodOccurred = false;
	bNeedFiveKillsMessage = true;
	if (CTFGameState)
	{
		CTFGameState->CTFRound++;
		if (!bPerPlayerLives)
		{
			CTFGameState->RedLivesRemaining = RoundLives;
			CTFGameState->BlueLivesRemaining = RoundLives;
		}
		if (CTFGameState->FlagBases.Num() > 1)
		{
			CTFGameState->RedLivesRemaining += CTFGameState->FlagBases[0] ? CTFGameState->FlagBases[0]->RoundLivesAdjustment : 0;
			CTFGameState->BlueLivesRemaining += CTFGameState->FlagBases[1] ? CTFGameState->FlagBases[0]->RoundLivesAdjustment : 0;
		}
	}

	bRedToCap = !bRedToCap;
	if (FlagPickupDelay > 0)
	{
		for (AUTCTFFlagBase* Base : CTFGameState->FlagBases)
		{
			if (Base != NULL && Base->MyFlag)
			{
				AUTCarriedObject* Flag = Base->MyFlag;
				Flag->bEnemyCanPickup = false;
				Flag->bFriendlyCanPickup = false;
				Flag->bTeamPickupSendsHome = false;
				Flag->bEnemyPickupSendsHome = false;
			}
		}
		RemainingPickupDelay = FlagPickupDelay;
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTCTFRoundGame::FlagCountDown, 1.f*GetActorTimeDilation(), false);
	}
	else
	{
		InitFlags();
	}

	if (bPerPlayerLives)
	{
		for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
			PS->bHasLifeLimit = false;
			if (PS && (PS->bIsInactive || !PS->Team))
			{
				PS->RemainingLives = 0;
				PS->SetOutOfLives(true);
			}
			else if (PS)
			{
				PS->RemainingLives = (!bAsymmetricVictoryConditions || (bRedToCap == (PS->Team->TeamIndex == 0))) ? RoundLives : 0;
				PS->bHasLifeLimit = (PS->RemainingLives > 0);
				PS->SetOutOfLives(false);
			}
		}
	}
	CTFGameState->TeamRespawnWaitTime[0] = 0.f;
	CTFGameState->TeamRespawnWaitTime[1] = 0.f;
	if (bAsymmetricVictoryConditions)
	{
		CTFGameState->TeamRespawnWaitTime[bRedToCap ? 1 : 0] = UnlimitedRespawnWaitTime;
	}
	CTFGameState->SetTimeLimit(TimeLimit);
}

bool AUTCTFRoundGame::ChangeTeam(AController* Player, uint8 NewTeam, bool bBroadcast)
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(Player->PlayerState);
	AUTTeamInfo* OldTeam = PS->Team;
	bool bResult = Super::ChangeTeam(Player, NewTeam, bBroadcast);
	if (bResult && (GetMatchState() == MatchState::InProgress))
	{
		if (PS)
		{
			PS->RemainingLives = 0;
			PS->SetOutOfLives(true);
		}
		if (OldTeam && UTGameState)
		{
			// verify that OldTeam still has players
			for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
			{
				AUTPlayerState* OtherPS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
				if (OtherPS && (OtherPS->Team == OldTeam) && !OtherPS->bOutOfLives && !OtherPS->bIsInactive)
				{
					return bResult;
				}
			}
			ScoreOutOfLives((OldTeam->TeamIndex == 0) ? 1 : 0);
		}
	}
	return bResult;
}

void AUTCTFRoundGame::RestartPlayer(AController* aPlayer)
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(aPlayer->PlayerState);
	if (bPerPlayerLives && PS && PS->Team)
	{
		if (PS->bOutOfLives)
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(aPlayer);
			if (PC != NULL)
			{
				PC->ChangeState(NAME_Spectating);
				PC->ClientGotoState(NAME_Spectating);

				AUTPlayerState* PS = Cast<AUTPlayerState>(PC->PlayerState);
				if (PS != NULL && PS->Team != NULL)
				{
					for (AController* Member : PS->Team->GetTeamMembers())
					{
						if (Member->GetPawn() != NULL)
						{
							PC->ServerViewPlayerState(Member->PlayerState);
							break;
						}
					}
				}
			}
			return;
		}
		if (!bAsymmetricVictoryConditions || (bRedToCap == (PS->Team->TeamIndex == 0)))
		{
			PS->RemainingLives--;
			if (PS->RemainingLives < 0)
			{
				// this player is out of lives
				PS->SetOutOfLives(true);
				for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
				{
					AUTPlayerState* OtherPS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
					if (OtherPS && (OtherPS->Team == PS->Team) && !OtherPS->bOutOfLives && !OtherPS->bIsInactive)
					{
						// found a live teammate, so round isn't over - notify about termination though
						BroadcastLocalized(NULL, UUTShowdownRewardMessage::StaticClass(), 3, PS);
						return;
					}
				}
				BroadcastLocalized(NULL, UUTShowdownRewardMessage::StaticClass(), 4);
				ScoreOutOfLives((PS->Team->TeamIndex == 0) ? 1 : 0);
				return;
			}
		}
	}
	Super::RestartPlayer(aPlayer);

	if (aPlayer->GetPawn() && !bPerPlayerLives && (RoundLives > 0) && PS && PS->Team && CTFGameState && CTFGameState->IsMatchInProgress())
	{
		if ((PS->Team->TeamIndex == 0) && (!bAsymmetricVictoryConditions || bRedToCap))
		{
			CTFGameState->RedLivesRemaining--;
			if (CTFGameState->RedLivesRemaining <= 0)
			{
				CTFGameState->RedLivesRemaining = 0;
				ScoreOutOfLives(1);
				return;
			}
			else if (bNeedFiveKillsMessage && (CTFGameState->RedLivesRemaining == 5))
			{
				bNeedFiveKillsMessage = false;
				BroadcastLocalized(NULL, UUTShowdownGameMessage::StaticClass(), 7);
			}
		}
		else if ((PS->Team->TeamIndex == 1) && (!bAsymmetricVictoryConditions || !bRedToCap))
		{
			CTFGameState->BlueLivesRemaining--;
			if (CTFGameState->BlueLivesRemaining <= 0)
			{
				CTFGameState->BlueLivesRemaining = 0;
				ScoreOutOfLives(0);
				return;
			}
			else if (bNeedFiveKillsMessage && (CTFGameState->BlueLivesRemaining == 5))
			{
				bNeedFiveKillsMessage = false;
				BroadcastLocalized(NULL, UUTShowdownGameMessage::StaticClass(), 7);
			}
		}
	}
}

void AUTCTFRoundGame::ScoreOutOfLives(int32 WinningTeamIndex)
{
	FindAndMarkHighScorer();
	AUTTeamInfo* WinningTeam = (Teams.Num() > WinningTeamIndex) ? Teams[WinningTeamIndex] : NULL;
	if (WinningTeam)
	{
		WinningTeam->Score++;
		if (CTFGameState)
		{
			FCTFScoringPlay NewScoringPlay;
			NewScoringPlay.Team = WinningTeam;
			NewScoringPlay.bDefenseWon = true;
			NewScoringPlay.TeamScores[0] = CTFGameState->Teams[0] ? CTFGameState->Teams[0]->Score : 0;
			NewScoringPlay.TeamScores[1] = CTFGameState->Teams[1] ? CTFGameState->Teams[1]->Score : 0;
			CTFGameState->AddScoringPlay(NewScoringPlay);
		}

		WinningTeam->ForceNetUpdate();
		LastTeamToScore = WinningTeam;
		BroadcastLocalized(NULL, UUTShowdownGameMessage::StaticClass(), 3 + WinningTeam->TeamIndex);
		if (!CheckReachedGoalScore(LastTeamToScore) && (MercyScore > 0))
		{
			int32 Spread = LastTeamToScore->Score;
			for (AUTTeamInfo* OtherTeam : Teams)
			{
				if (OtherTeam != LastTeamToScore)
				{
					Spread = FMath::Min<int32>(Spread, LastTeamToScore->Score - OtherTeam->Score);
				}
			}
			if (Spread >= MercyScore)
			{
				EndGame(NULL, FName(TEXT("MercyScore")));
			}
		}
		if (UTGameState->IsMatchInProgress())
		{
			SetMatchState(MatchState::MatchIntermission);
		}
	}
}

void AUTCTFRoundGame::CheckGameTime()
{
	if (IsMatchInProgress() && !HasMatchEnded() && TimeLimit > 0 && UTGameState->RemainingTime <= 0)
	{
		// Round is over, defense wins.
		ScoreOutOfLives(bRedToCap ? 1 : 0);
	}
}




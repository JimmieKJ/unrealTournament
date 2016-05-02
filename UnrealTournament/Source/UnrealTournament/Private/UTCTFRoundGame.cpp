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
#include "UTArmor.h"
#include "UTTimedPowerup.h"
#include "UTPlayerState.h"
#include "UTFlagRunHUD.h"
#include "UTGhostFlag.h"
#include "UTCTFRoundGameState.h"
#include "UTAsymCTFSquadAI.h"

AUTCTFRoundGame::AUTCTFRoundGame(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	TimeLimit = 5;
	IntermissionDuration = 30.f;
	RoundLives = 5;
	bPerPlayerLives = true;
	bNeedFiveKillsMessage = true;
	FlagCapScore = 1;
	UnlimitedRespawnWaitTime = 2.f;
	bForceRespawn = true;
	bOneFlagGameMode = true;
	bCarryOwnFlag = true;
	bNoFlagReturn = true;
	bFirstRoundInitialized = false;
	ExtraHealth = 0;
	FlagPickupDelay = 10;
	HUDClass = AUTFlagRunHUD::StaticClass();
	GameStateClass = AUTCTFRoundGameState::StaticClass();
	SquadType = AUTAsymCTFSquadAI::StaticClass();
	NumRounds = 6;
	bGiveSpawnInventoryBonus = true;

	bAttackerLivesLimited = false;
	bDefenderLivesLimited = true;

	OffenseKillsNeededForPowerUp = 10;
	DefenseKillsNeededForPowerUp = 10;

	InitialBoostCount = 0;
	bNoLivesEndRound = true;
	MaxTimeScoreBonus = 150;

	// remove translocator - fixmesteve make this an option
	TranslocatorObject = nullptr;

	ShieldBeltObject = FStringAssetReference(TEXT("/Game/RestrictedAssets/Pickups/Armor/Armor_ShieldBelt.Armor_ShieldBelt_C"));
	ThighPadObject = FStringAssetReference(TEXT("/Game/RestrictedAssets/Pickups/Armor/Armor_ThighPads.Armor_ThighPads_C"));
	ArmorVestObject = FStringAssetReference(TEXT("/Game/RestrictedAssets/Pickups/Armor/Armor_Chest.Armor_Chest_C"));
	ActivatedPowerupPlaceholderObject = FStringAssetReference(TEXT("/Game/RestrictedAssets/Pickups/Powerups/BP_ActivatedPowerup_UDamage.BP_ActivatedPowerup_UDamage_C"));
	RepulsorObject = FStringAssetReference(TEXT("/Game/RestrictedAssets/Pickups/Powerups/BP_Repulsor.BP_Repulsor_C"));

	RollingAttackerRespawnDelay = 5.f;
	LastAttackerSpawnTime = 0.f;
	RollingSpawnStartTime = 0.f;
	bRollingAttackerSpawns = true;
	ForceRespawnTime = 0.1f;
	LimitedRespawnWaitTime = 6.f;

	MainScoreboardDisplayTime = 7.5f;
	EndScoreboardDelay = 6.f;
}

int32 AUTCTFRoundGame::GetFlagCapScore()
{
	int32 BonusTime = UTGameState->GetRemainingTime();
	if (BonusTime > GoldBonusTime)
	{
		return GoldScore;
	}
	if (BonusTime > SilverBonusTime)
	{
		return SilverScore;
	}
	return BronzeScore;
}

void AUTCTFRoundGame::CreateGameURLOptions(TArray<TSharedPtr<TAttributePropertyBase>>& MenuProps)
{
	MenuProps.Add(MakeShareable(new TAttributeProperty<int32>(this, &BotFillCount, TEXT("BotFill"))));
	MenuProps.Add(MakeShareable(new TAttributePropertyBool(this, &bBalanceTeams, TEXT("BalanceTeams"))));
	MenuProps.Add(MakeShareable(new TAttributeProperty<int32>(this, &OffenseKillsNeededForPowerUp, TEXT("OffKillsForPowerup"))));
	MenuProps.Add(MakeShareable(new TAttributeProperty<int32>(this, &OffenseKillsNeededForPowerUp, TEXT("DefKillsForPowerup"))));
}

bool AUTCTFRoundGame::AvoidPlayerStart(AUTPlayerStart* P)
{
	return P && (bAsymmetricVictoryConditions && P->bIgnoreInASymCTF);
}

void AUTCTFRoundGame::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (!ShieldBeltObject.IsNull())
	{
		ShieldBeltClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *ShieldBeltObject.ToStringReference().ToString(), NULL, LOAD_NoWarn));
	}
	if (!ThighPadObject.IsNull())
	{
		ThighPadClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *ThighPadObject.ToStringReference().ToString(), NULL, LOAD_NoWarn));
	}
	if (!ArmorVestObject.IsNull())
	{
		ArmorVestClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *ArmorVestObject.ToStringReference().ToString(), NULL, LOAD_NoWarn));
	}
	if (!ActivatedPowerupPlaceholderObject.IsNull())
	{
		ActivatedPowerupPlaceholderClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *ActivatedPowerupPlaceholderObject.ToStringReference().ToString(), NULL, LOAD_NoWarn));
	}
	if (!RepulsorObject.IsNull())
	{
		RepulsorClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *RepulsorObject.ToStringReference().ToString(), NULL, LOAD_NoWarn));
	}

	// key options are ?RoundLives=xx?Dash=xx?Asymm=xx?PerPlayerLives=xx?OffKillsForPowerup=xx?DefKillsForPowerup=xx
	RoundLives = FMath::Max(1, UGameplayStatics::GetIntOption(Options, TEXT("RoundLives"), RoundLives));

	FString InOpt = UGameplayStatics::ParseOption(Options, TEXT("OwnFlag"));
	bCarryOwnFlag = EvalBoolOptions(InOpt, bCarryOwnFlag);

	InOpt = UGameplayStatics::ParseOption(Options, TEXT("FlagReturn"));
	bNoFlagReturn = EvalBoolOptions(InOpt, bNoFlagReturn);

	InOpt = UGameplayStatics::ParseOption(Options, TEXT("PerPlayerLives"));
	bPerPlayerLives = EvalBoolOptions(InOpt, bPerPlayerLives);

	ExtraHealth = FMath::Max(0, UGameplayStatics::GetIntOption(Options, TEXT("XHealth"), ExtraHealth));
	FlagPickupDelay = FMath::Max(1, UGameplayStatics::GetIntOption(Options, TEXT("FlagDelay"), FlagPickupDelay));

	OffenseKillsNeededForPowerUp = FMath::Max(0, UGameplayStatics::GetIntOption(Options, TEXT("OffKillsForPowerup"), OffenseKillsNeededForPowerUp));
	DefenseKillsNeededForPowerUp = FMath::Max(0, UGameplayStatics::GetIntOption(Options, TEXT("DefKillsForPowerup"), DefenseKillsNeededForPowerUp));
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

void AUTCTFRoundGame::GiveDefaultInventory(APawn* PlayerPawn)
{
	Super::GiveDefaultInventory(PlayerPawn);
	AUTCharacter* UTCharacter = Cast<AUTCharacter>(PlayerPawn);
	if (UTCharacter != NULL && bGiveSpawnInventoryBonus)
	{
		AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(UTCharacter->PlayerState);
		bool bOnLastLife = (UTPlayerState && (UTPlayerState->RemainingLives == 0) && UTPlayerState->bHasLifeLimit);
		TSubclassOf<AUTInventory> StartingArmor = (bOnLastLife && UTPlayerState->Team && IsTeamOnOffense(UTPlayerState->Team->TeamIndex)) ? ShieldBeltClass : ThighPadClass;
		UTCharacter->AddInventory(GetWorld()->SpawnActor<AUTInventory>(StartingArmor, FVector(0.0f), FRotator(0.f, 0.f, 0.f)), true);
		
		AUTPlayerController* UTPC = Cast<AUTPlayerController>(UTPlayerState->GetOwner());
		if (bOnLastLife)
		{
			UTCharacter->bShouldWearLeaderHat = true;
			UTCharacter->LeaderHatStatusChanged();
		}
	}
}

void AUTCTFRoundGame::BroadcastScoreUpdate(APlayerState* ScoringPlayer, AUTTeamInfo* ScoringTeam, int32 OldScore)
{
	int32 BonusType = 100 + BronzeScore;
	if (ScoringTeam->RoundBonus > GoldBonusTime)
	{
		BonusType = 300 + GoldScore;
	}
	else if (ScoringTeam->RoundBonus > SilverBonusTime)
	{
		BonusType = 200 + SilverScore;
	}

	BroadcastLocalized(this, UUTCTFRewardMessage::StaticClass(), BonusType, ScoringPlayer, NULL, ScoringTeam);
	BroadcastLocalized(this, UUTCTFMajorMessage::StaticClass(), 2, ScoringPlayer, NULL, ScoringTeam);
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

	HandlePowerupUnlocks(Other, Killer);

	Super::DiscardInventory(Other, Killer);
}

void AUTCTFRoundGame::HandleMatchIntermission()
{
	// view defender base, with last team to score around it
	int32 TeamToWatch = IntermissionTeamToView(nullptr);
	PlacePlayersAroundFlagBase(TeamToWatch, bRedToCap ? 1 : 0);

	// Tell the controllers to look at defender base
	for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
		if (PC != NULL)
		{
			PC->ClientHalftime();
			PC->SetViewTarget(CTFGameState->FlagBases[bRedToCap ? 1 : 0]);
		}
	}

	// Freeze all of the pawns
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		if (*It && !Cast<ASpectatorPawn>((*It).Get()))
		{
			(*It)->TurnOff();
		}
	}

	CTFGameState->bIsAtIntermission = true;
	CTFGameState->OnIntermissionChanged();

	CTFGameState->bStopGameClock = true;
	Cast<AUTCTFRoundGameState>(CTFGameState)->IntermissionTime  = IntermissionDuration;
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
	CheckForWinner(Scorer->Team);
	return true;
}

int32 AUTCTFRoundGame::PickCheatWinTeam()
{
	bool bPickRedTeam = bAsymmetricVictoryConditions ? bRedToCap : (FMath::FRand() < 0.5f);
	return bPickRedTeam ? 0 : 1;
}

void AUTCTFRoundGame::PlayEndOfMatchMessage()
{
	if (UTGameState && UTGameState->WinningTeam)
	{
		int32 IsFlawlessVictory = (UTGameState->WinningTeam->Score > 3) ? 1 : 0;
		for (int32 i = 0; i < Teams.Num(); i++)
		{
			if ((Teams[i] != UTGameState->WinningTeam) && (Teams[i]->Score > 0))
			{
				IsFlawlessVictory = 0;
				break;
			}
		}
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* Controller = *Iterator;
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
				if (Team->SecondaryScore != BestTeam->SecondaryScore)
				{
					BestTeam = (Team->SecondaryScore > BestTeam->SecondaryScore) ? Team : BestTeam;
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

	// Check if a team has an insurmountable lead
	// current implementation assumes 6 rounds and 2 teams
	if (CTFGameState && (CTFGameState->CTFRound >= NumRounds - 2) && Teams[0] && Teams[1])
	{
		bSecondaryWin = false;
		if ((CTFGameState->CTFRound == NumRounds - 2) && (FMath::Abs(Teams[0]->Score - Teams[1]->Score) > DefenseScore + GoldScore))
		{
			AUTTeamInfo* BestTeam = (Teams[0]->Score > Teams[1]->Score) ? Teams[0] : Teams[1];
			EndTeamGame(BestTeam, FName(TEXT("scorelimit")));
			return true;
		}
		if (CTFGameState->CTFRound == NumRounds - 1)
		{
			if (bRedToCap)
			{
				// next round is blue cap
				if (Teams[0]->Score > Teams[1]->Score + GoldScore)
				{
					EndTeamGame(Teams[0], FName(TEXT("scorelimit")));
					return true;
				}
				if (Teams[1]->Score > Teams[0]->Score + DefenseScore)
				{
					EndTeamGame(Teams[1], FName(TEXT("scorelimit")));
					return true;
				}
			}
			else
			{
				// next round is red cap
				if (Teams[0]->Score > Teams[1]->Score + DefenseScore)
				{
					EndTeamGame(Teams[0], FName(TEXT("scorelimit")));
					return true;
				}
				if (Teams[1]->Score > Teams[0]->Score + GoldScore)
				{
					EndTeamGame(Teams[1], FName(TEXT("scorelimit")));
					return true;
				}
			}
		}
	}
	return false;
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
		FString MatchStats = FString::Printf(TEXT("%i"), GetWorld()->GetGameState()->ElapsedTime);

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
	PlacePlayersAroundFlagBase(Winner->TeamIndex, bRedToCap ? 1 : 0);
	AUTCTFFlagBase* WinningBase = CTFGameState->FlagBases[bRedToCap ? 1 : 0];
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
				Controller->GameHasEnded(BaseToView, (Controller->UTPlayerState->Team && (Controller->UTPlayerState->Team->TeamIndex == Winner->TeamIndex)));
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
	EndMatch();
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
				Holder->Team->SecondaryScore += Holder->Team->RoundBonus;
			}
		}
	}

	Super::ScoreObject_Implementation(GameObject, HolderPawn, Holder, Reason);
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
		CTFGameState->NumRounds = NumRounds;
		CTFGameState->bOneFlagGameMode = bOneFlagGameMode;
		CTFGameState->bDefenderLivesLimited = bDefenderLivesLimited;
		CTFGameState->bAttackerLivesLimited = bAttackerLivesLimited;
		CTFGameState->bOverrideToggle = true;
		CTFGameState->HalftimeScoreDelay = 6.f;
		bFirstRoundInitialized = true;
		AUTCTFRoundGameState* RCTFGameState = Cast<AUTCTFRoundGameState>(CTFGameState);
		if (RCTFGameState)
		{
			RCTFGameState->GoldBonusThreshold = GoldBonusTime;
			RCTFGameState->SilverBonusThreshold = SilverBonusTime;
		}
		if (Super::UTIsHandlingReplays() && GetGameInstance() != nullptr)
		{
			GetGameInstance()->StartRecordingReplay(GetWorld()->GetMapName(), GetWorld()->GetMapName());
		}
	}
	Super::HandleMatchHasStarted();
}

void AUTCTFRoundGame::HandleExitingIntermission()
{
	CTFGameState->bStopGameClock = false;
	InitRound();
	Super::HandleExitingIntermission();
}

void AUTCTFRoundGame::InitFlags()
{
	for (AUTCTFFlagBase* Base : CTFGameState->FlagBases)
	{
		if (Base != NULL && Base->MyFlag)
		{
			AUTCarriedObject* Flag = Base->MyFlag;
			Flag->AutoReturnTime = 9.f; // fixmesteve make config
			Flag->bGradualAutoReturn = true;
			Flag->bDisplayHolderTrail = true;
			Flag->bShouldPingFlag = true;
			Flag->ClearGhostFlag();
			Flag->bSendHomeOnScore = false;
			if (bAsymmetricVictoryConditions)
			{
				if (IsTeamOnOffense(Flag->GetTeamNum()))
				{
					Flag->SetActorHiddenInGame(false);
					Flag->ClearGhostFlag();
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
			Flag->GetOverlappingActors(Overlapping, AUTCharacter::StaticClass());
			for (AActor* A : Overlapping)
			{
				AUTCharacter* Character = Cast<AUTCharacter>(A);
				if (Character != NULL)
				{
					if (!GetWorld()->LineTraceTestByChannel(Character->GetActorLocation(), Flag->GetActorLocation(), ECC_Pawn, FCollisionQueryParams(), WorldResponseParams))
					{
						Flag->TryPickup(Character);
					}
				}
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
				if (IsTeamOnOffense(PC->GetTeamNum()))
				{
					PC->ClientReceiveLocalizedMessage(UUTCTFRoleMessage::StaticClass(), 10 * PC->GetTeamNum() + (bDefenderLivesLimited ? 1 : 2));
				}
				else
				{
					PC->ClientReceiveLocalizedMessage(UUTCTFRoleMessage::StaticClass(), bDefenderLivesLimited ? 3 : 4);
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
	AUTCTFRoundGameState* RCTFGameState = Cast<AUTCTFRoundGameState>(CTFGameState);
	if (RCTFGameState)
	{
		RCTFGameState->RemainingPickupDelay--;
		if (RCTFGameState->RemainingPickupDelay > 0)
		{
			FTimerHandle TempHandle;
			GetWorldTimerManager().SetTimer(TempHandle, this, &AUTCTFRoundGame::FlagCountDown, 1.f*GetActorTimeDilation(), false);
		}
		else
		{
			BroadcastLocalized(this, UUTCTFMajorMessage::StaticClass(), 21, NULL, NULL, NULL);
			InitFlags();
		}
	}
}

void AUTCTFRoundGame::InitRound()
{
	bFirstBloodOccurred = false;
	bLastManOccurred = false;
	bNeedFiveKillsMessage = true;
	AUTCTFRoundGameState* RCTFGameState = Cast<AUTCTFRoundGameState>(CTFGameState);
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

		if (RCTFGameState)
		{
			RCTFGameState->OffenseKills = 0;
			RCTFGameState->DefenseKills = 0;
			RCTFGameState->OffenseKillsNeededForPowerup = OffenseKillsNeededForPowerUp;
			RCTFGameState->DefenseKillsNeededForPowerup = DefenseKillsNeededForPowerUp;
			RCTFGameState->bIsOffenseAbleToGainPowerup = true;
			RCTFGameState->bIsDefenseAbleToGainPowerup = true;
		}
	}

	bRedToCap = !bRedToCap;
	CTFGameState->bRedToCap = bRedToCap;
	BroadcastVictoryConditions();

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
				if (bAsymmetricVictoryConditions && IsTeamOnOffense(Flag->GetTeamNum()))
				{
					Flag->SetActorHiddenInGame(true);
					Flag->PutGhostFlagAt(Flag->GetActorLocation());
				}
			}
		}
		RCTFGameState->RemainingPickupDelay = FlagPickupDelay;
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
			PS->RespawnWaitTime = IsPlayerOnLifeLimitedTeam(PS) ? LimitedRespawnWaitTime : UnlimitedRespawnWaitTime;
			PS->SetRemainingBoosts(InitialBoostCount);
			PS->bSpecialTeamPlayer = false;
			PS->bSpecialPlayer = false;
			if (PS && (PS->bIsInactive || !PS->Team || PS->bOnlySpectator))
			{
				PS->RemainingLives = 0;
				PS->SetOutOfLives(true);
			}
			else if (PS)
			{
				PS->RemainingLives = (!bAsymmetricVictoryConditions || (IsPlayerOnLifeLimitedTeam(PS))) ? RoundLives : 0;
				PS->bHasLifeLimit = (PS->RemainingLives > 0);
				PS->SetOutOfLives(false);
			}
		}
	}
	CTFGameState->SetTimeLimit(TimeLimit);

	// re-initialize all AI squads, in case objectives have changed sides
	for (AUTTeamInfo* Team : Teams)
	{
		Team->ReinitSquads();
	}
}

bool AUTCTFRoundGame::ChangeTeam(AController* Player, uint8 NewTeamIndex, bool bBroadcast)
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(Player->PlayerState);
	AUTTeamInfo* OldTeam = PS->Team;
	bool bResult = Super::ChangeTeam(Player, NewTeamIndex, bBroadcast);
	if (bResult && (GetMatchState() == MatchState::InProgress))
	{
		if (PS)
		{
			PS->RemainingLives = 0;
			PS->SetOutOfLives(true);
		}
		if (UTGameState)
		{
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
				return bResult;
			}
			if (!bNewTeamHasPlayers && NewTeam)
			{
				ScoreAlternateWin((NewTeam->TeamIndex == 0) ? 1 : 0);
				return bResult;
			}
		}
	}
	return bResult;
}

void AUTCTFRoundGame::RestartPlayer(AController* aPlayer)
{
	if (GetMatchState() == MatchState::MatchIntermission)
	{
		// placing players during intermission
		if (bPlacingPlayersAtIntermission)
		{
			Super::RestartPlayer(aPlayer);
		}
		return;
	}
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
			if (PS->RemainingLives >= 0)
			{
				PS->RemainingLives--;
				if (PS->RemainingLives < 0)
				{
					if (bNoLivesEndRound)
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
						ScoreAlternateWin((PS->Team->TeamIndex == 0) ? 1 : 0);
					}
					return;
				}
				AUTPlayerController* PC = Cast<AUTPlayerController>(aPlayer);
				if (PS->RemainingLives == 0)
				{
					if (PC)
					{
						PC->ClientReceiveLocalizedMessage(UUTShowdownRewardMessage::StaticClass(), 5, PS, NULL, NULL);
					}
					PS->RespawnWaitTime = 2.f;
					PS->OnRespawnWaitReceived();
				}
				else if (PC)
				{
					PC->ClientReceiveLocalizedMessage(UUTShowdownRewardMessage::StaticClass(), 30+PS->RemainingLives, PS, NULL, NULL);
				}
			}
			else
			{
				return;
			}
		}
	}
	if (PS->Team && IsTeamOnOffense(PS->Team->TeamIndex))
	{
		LastAttackerSpawnTime = GetWorld()->GetTimeSeconds();
	}
	Super::RestartPlayer(aPlayer);

	if (aPlayer->GetPawn() && !bPerPlayerLives && (RoundLives > 0) && PS && PS->Team && CTFGameState && CTFGameState->IsMatchInProgress())
	{
		if ((PS->Team->TeamIndex == 0) && IsPlayerOnLifeLimitedTeam(PS))
		{
			CTFGameState->RedLivesRemaining--;
			if (CTFGameState->RedLivesRemaining <= 0)
			{
				CTFGameState->RedLivesRemaining = 0;
				ScoreAlternateWin(1);
				return;
			}
			else if (bNeedFiveKillsMessage && (CTFGameState->RedLivesRemaining == 5))
			{
				bNeedFiveKillsMessage = false;
				BroadcastLocalized(NULL, UUTShowdownGameMessage::StaticClass(), 7);
			}
		}
		else if ((PS->Team->TeamIndex == 1) && IsPlayerOnLifeLimitedTeam(PS))
		{
			CTFGameState->BlueLivesRemaining--;
			if (CTFGameState->BlueLivesRemaining <= 0)
			{
				CTFGameState->BlueLivesRemaining = 0;
				ScoreAlternateWin(0);
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

void AUTCTFRoundGame::ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
	Super::ScoreKill_Implementation(Killer, Other, KilledPawn, DamageType);

	AUTPlayerState* OtherPS = Other ? Cast<AUTPlayerState>(Other->PlayerState) : nullptr;
	if (OtherPS && OtherPS->Team && IsTeamOnOffense(OtherPS->Team->TeamIndex) && bRollingAttackerSpawns)
	{
		if (LastAttackerSpawnTime < 1.f)
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
		OtherPS->OnRespawnWaitReceived();
	}
	if (!bLastManOccurred && OtherPS && IsPlayerOnLifeLimitedTeam(OtherPS) && (OtherPS->RemainingLives > 0))
	{
		// check if just transitioned to last man
		bLastManOccurred = true;
		for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
			if (PS && (PS != OtherPS) && (OtherPS->Team == PS->Team) && !PS->bOutOfLives && !PS->bIsInactive)
			{
				bLastManOccurred = false;
				break;
			}
		}
		if (bLastManOccurred)
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
	}
}

void AUTCTFRoundGame::AdjustLeaderHatFor(AUTCharacter* UTChar)
{
// no leader hat for high score, only for last life
}

void AUTCTFRoundGame::ScoreAlternateWin(int32 WinningTeamIndex, uint8 Reason)
{
	FindAndMarkHighScorer();
	AUTTeamInfo* WinningTeam = (Teams.Num() > WinningTeamIndex) ? Teams[WinningTeamIndex] : NULL;
	if (WinningTeam)
	{
		WinningTeam->Score += IsTeamOnOffense(WinningTeamIndex) ? GetFlagCapScore() : DefenseScore;
		if (CTFGameState)
		{
			for (int32 i = 0; i < Teams.Num(); i++)
			{
				if (Teams[i])
				{
					Teams[i]->RoundBonus = 0;
				}
			}
			WinningTeam->RoundBonus = FMath::Min(MaxTimeScoreBonus, CTFGameState->GetRemainingTime());
			WinningTeam->SecondaryScore += WinningTeam->RoundBonus;

			FCTFScoringPlay NewScoringPlay;
			NewScoringPlay.Team = WinningTeam;
			NewScoringPlay.bDefenseWon = !IsTeamOnOffense(WinningTeamIndex);
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
		BroadcastLocalized(NULL, UUTShowdownGameMessage::StaticClass(), 3 + WinningTeam->TeamIndex);
		CheckForWinner(LastTeamToScore);
		if (UTGameState->IsMatchInProgress())
		{
			SetMatchState(MatchState::MatchIntermission);
		}
	}
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
		int32 RemainingTime = UTGameState ? UTGameState->GetRemainingTime() : 100;
		if (RemainingTime <= 0)
		{
			// Round is over, defense wins.
			ScoreAlternateWin(bRedToCap ? 1 : 0, 1);
		}
		else
		{
			if (RCTFGameState)
			{
				uint8 OldBonusLevel = RCTFGameState->BonusLevel;
				RCTFGameState->BonusLevel = (RemainingTime > GoldBonusTime) ? 3 : 2;
				if (RemainingTime <= SilverBonusTime)
				{
					RCTFGameState->BonusLevel = 1;
				}
				if (OldBonusLevel != RCTFGameState->BonusLevel)
				{
					RCTFGameState->OnBonusLevelChanged();
					RCTFGameState->ForceNetUpdate();
				}
			}
		}
	}
	else
	{
		Super::CheckGameTime();
	}
}

bool AUTCTFRoundGame::IsTeamOnOffense(int32 TeamNumber) const
{
	const bool bIsOnRedTeam = (TeamNumber == 0);
	return (bRedToCap == bIsOnRedTeam);
}

bool AUTCTFRoundGame::IsTeamOnDefense(int32 TeamNumber) const
{
	return !IsTeamOnOffense(TeamNumber);
}

bool AUTCTFRoundGame::IsPlayerOnLifeLimitedTeam(AUTPlayerState* PlayerState) const
{
	if (!PlayerState || !PlayerState->Team)
	{
		return false;
	}

	return IsTeamOnOffense(PlayerState->Team->TeamIndex) ? bAttackerLivesLimited : bDefenderLivesLimited;
}

void AUTCTFRoundGame::HandlePowerupUnlocks(APawn* Other, AController* Killer)
{
	AUTPlayerState* KillerPS = Killer ? Cast<AUTPlayerState>(Killer->PlayerState) : nullptr;
	AUTPlayerState* VictimPS = Other ? Cast<AUTPlayerState>(Other->PlayerState) : nullptr;

	UpdatePowerupUnlockProgress(VictimPS, KillerPS);

	const int RedTeamIndex = 0;
	const int BlueTeamIndex = 1;
	
	AUTCTFRoundGameState* RCTFGameState = Cast<AUTCTFRoundGameState>(CTFGameState);
	if (RCTFGameState)
	{
		if ((RCTFGameState->OffenseKills >= OffenseKillsNeededForPowerUp) && RCTFGameState->bIsOffenseAbleToGainPowerup)
		{
			RCTFGameState->OffenseKills = 0;

			GrantPowerupToTeam(IsTeamOnOffense(RedTeamIndex) ? RedTeamIndex : BlueTeamIndex, KillerPS);
			RCTFGameState->bIsOffenseAbleToGainPowerup = false;
		}

		if ((RCTFGameState->DefenseKills >= DefenseKillsNeededForPowerUp) && RCTFGameState->bIsDefenseAbleToGainPowerup)
		{
			RCTFGameState->DefenseKills = 0;

			GrantPowerupToTeam(IsTeamOnDefense(RedTeamIndex) ? RedTeamIndex : BlueTeamIndex, KillerPS);
			RCTFGameState->bIsDefenseAbleToGainPowerup = false;
		}
	}
}

void AUTCTFRoundGame::UpdatePowerupUnlockProgress(AUTPlayerState* VictimPS, AUTPlayerState* KillerPS)
{
	AUTCTFRoundGameState* RCTFGameState = Cast<AUTCTFRoundGameState>(CTFGameState);

	if (RCTFGameState && VictimPS && VictimPS->Team && KillerPS && KillerPS->Team)
	{
		//No credit for suicides
		if (VictimPS->Team->TeamIndex != KillerPS->Team->TeamIndex)
		{
			if (IsTeamOnDefense(VictimPS->Team->TeamIndex))
			{
				++(RCTFGameState->OffenseKills);
			}
			else
			{
				++(RCTFGameState->DefenseKills);
			}
		}
	}
}

void AUTCTFRoundGame::GrantPowerupToTeam(int TeamIndex, AUTPlayerState* PlayerToHighlight)
{
	for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (PS && PS->Team)
		{
			if (PS->Team->TeamIndex == TeamIndex)
			{
				PS->SetRemainingBoosts(1);
			}
			AUTPlayerController* PC = Cast<AUTPlayerController>(PS->GetOwner());
			if (PC)
			{
				if (PS->Team->TeamIndex == TeamIndex)
				{
					PC->ClientReceiveLocalizedMessage(UUTCTFRewardMessage::StaticClass(), 7, PlayerToHighlight);
				}
				else
				{
					PC->ClientReceiveLocalizedMessage(UUTCTFRoleMessage::StaticClass(), 7, PlayerToHighlight);
				}
			}
		}
	}
}
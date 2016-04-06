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
#include "UTArmor.h"
#include "UTTimedPowerup.h"
#include "UTPlayerState.h"
#include "UTFlagRunHUD.h"
#include "UTGhostFlag.h"
#include "UTCTFRoundGameState.h"

AUTCTFRoundGame::AUTCTFRoundGame(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	TimeLimit = 5;
	DisplayName = NSLOCTEXT("UTGameMode", "CTFR", "Flag Run");
	RoundLives = 5;
	bPerPlayerLives = true;
	bNeedFiveKillsMessage = true;
	FlagCapScore = 2;
	UnlimitedRespawnWaitTime = 2.f;
	bForceRespawn = true;
	bUseDash = false;
	bAsymmetricVictoryConditions = true;
	bCarryOwnFlag = true;
	bNoFlagReturn = true;
	bFirstRoundInitialized = false;
	ExtraHealth = 0;
	FlagPickupDelay = 10;
	RemainingPickupDelay = 0;
	RollingAttackerRespawnDelay = 8.f;
	HUDClass = AUTFlagRunHUD::StaticClass();
	GameStateClass = AUTCTFRoundGameState::StaticClass();
	NumRounds = 6;

	// remove translocator - fixmesteve make this an option
	TranslocatorObject = nullptr;

	ShieldBeltObject = FStringAssetReference(TEXT("/Game/RestrictedAssets/Pickups/Armor/Armor_ShieldBelt.Armor_ShieldBelt_C"));
	ThighPadObject = FStringAssetReference(TEXT("/Game/RestrictedAssets/Pickups/Armor/Armor_ThighPads.Armor_ThighPads_C"));
	UDamageObject = FStringAssetReference(TEXT("/Game/RestrictedAssets/Pickups/Powerups/BP_UDamage_RCTF.BP_UDamage_RCTF_C"));
	ArmorVestObject = FStringAssetReference(TEXT("/Game/RestrictedAssets/Pickups/Armor/Armor_Chest.Armor_Chest_C"));
	ActivatedPowerupPlaceholderObject = FStringAssetReference(TEXT("/Game/RestrictedAssets/Pickups/Powerups/BP_ActivatedPowerup_UDamage.BP_ActivatedPowerup_UDamage_C"));
}
//589,0 45,39 HUDAtlas01

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

	if (!ShieldBeltObject.IsNull())
	{
		ShieldBeltClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *ShieldBeltObject.ToStringReference().ToString(), NULL, LOAD_NoWarn));
	}
	if (!ThighPadObject.IsNull())
	{
		ThighPadClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *ThighPadObject.ToStringReference().ToString(), NULL, LOAD_NoWarn));
	}
	if (!UDamageObject.IsNull())
	{
		UDamageClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *UDamageObject.ToStringReference().ToString(), NULL, LOAD_NoWarn));
	}
	if (!ArmorVestObject.IsNull())
	{
		ArmorVestClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *ArmorVestObject.ToStringReference().ToString(), NULL, LOAD_NoWarn));
	}
	if (!ActivatedPowerupPlaceholderObject.IsNull())
	{
		ActivatedPowerupPlaceholderClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *ActivatedPowerupPlaceholderObject.ToStringReference().ToString(), NULL, LOAD_NoWarn));
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

void AUTCTFRoundGame::GiveDefaultInventory(APawn* PlayerPawn)
{
	Super::GiveDefaultInventory(PlayerPawn);
	AUTCharacter* UTCharacter = Cast<AUTCharacter>(PlayerPawn);
	if (UTCharacter != NULL)
	{
		AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(UTCharacter->PlayerState);
		bool bOnLastLife = (UTPlayerState && (UTPlayerState->RemainingLives == 0) && UTPlayerState->bHasLifeLimit);
		TSubclassOf<AUTInventory> StartingArmor = bOnLastLife  ? ShieldBeltClass : ThighPadClass;
		UTCharacter->AddInventory(GetWorld()->SpawnActor<AUTInventory>(StartingArmor, FVector(0.0f), FRotator(0.f, 0.f, 0.f)), true);
		
		AUTPlayerController* UTPC = Cast<AUTPlayerController>(UTPlayerState->GetOwner());
		if (UTPC)
		{
			UTPC->TimeToHoldPowerUpButtonToActivate = 0.75f;
			UTCharacter->AddInventory(GetWorld()->SpawnActor<AUTInventory>(ActivatedPowerupPlaceholderClass, FVector(0.0f), FRotator(0.0f, 0.0f, 0.0f)), true);
		}
	}
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
			}
			else if ((Team != BestTeam) && (Team->Score == BestTeam->Score))
			{
				bHaveTie = true;
				if (Team->SecondaryScore > BestTeam->SecondaryScore)
				{
					BestTeam = Team;
					bHaveTie = false;
				}
			}
		}
		if (!bHaveTie)
		{
			EndGame(nullptr, FName(TEXT("scorelimit")));
			return true;
		}
	}
	return false;
}

void AUTCTFRoundGame::HandleFlagCapture(AUTPlayerState* Holder)
{
	if (UTGameState && Holder->Team)
	{
		Holder->Team->SecondaryScore += UTGameState->RemainingTime;
	}
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
		CTFGameState->bOverrideToggle = true;
		bFirstRoundInitialized = true;
	}
	Super::HandleMatchHasStarted();
}

void AUTCTFRoundGame::HandleExitingIntermission()
{
	CTFGameState->bStopGameClock = false;
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
			Flag->AutoReturnTime = 6.f; // fixmesteve make config
			Flag->bGradualAutoReturn = true;
			Flag->bDisplayHolderTrail = true;
			Flag->bShouldPingFlag = true;
			UE_LOG(UT, Warning, TEXT("InitFlags"));
			Flag->ClearGhostFlag();
			if (bAsymmetricVictoryConditions)
			{
				Flag->bSendHomeOnScore = !bNoFlagReturn;
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
	CTFGameState->bRedToCap = bRedToCap;
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
			PS->RespawnWaitTime = 0.f;
			PS->RemainingBoosts = 1;
			PS->BoostClass = UDamageClass;
			if (bAsymmetricVictoryConditions && !IsPlayerOnLifeLimitedTeam(PS))
			{
				PS->BoostClass = ArmorVestClass;
			}
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
				if (bAsymmetricVictoryConditions && !PS->bHasLifeLimit)
				{
					PS->RespawnWaitTime = UnlimitedRespawnWaitTime;
				}
			}
		}
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
		if (!bAsymmetricVictoryConditions || (IsPlayerOnLifeLimitedTeam(PS)))
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
		else if (bAsymmetricVictoryConditions)
		{
			PS->RespawnWaitTime += 1.f;
		}
	}
	Super::RestartPlayer(aPlayer);

	if (aPlayer->GetPawn() && !bPerPlayerLives && (RoundLives > 0) && PS && PS->Team && CTFGameState && CTFGameState->IsMatchInProgress())
	{
		if ((PS->Team->TeamIndex == 0) && (!bAsymmetricVictoryConditions || IsPlayerOnLifeLimitedTeam(PS)))
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
		else if ((PS->Team->TeamIndex == 1) && (!bAsymmetricVictoryConditions || IsPlayerOnLifeLimitedTeam(PS)))
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
			NewScoringPlay.RemainingTime = CTFGameState->GetClockTime();
			CTFGameState->AddScoringPlay(NewScoringPlay);
		}

		WinningTeam->ForceNetUpdate();
		LastTeamToScore = WinningTeam;
		BroadcastLocalized(NULL, UUTShowdownGameMessage::StaticClass(), 3 + WinningTeam->TeamIndex);
		if (!CheckForWinner(LastTeamToScore) && (MercyScore > 0))
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
	if (CTFGameState->IsMatchIntermission())
	{
		AUTCTFRoundGameState* RCTFGameState = Cast<AUTCTFRoundGameState>(CTFGameState);
		if (RCTFGameState && (RCTFGameState->IntermissionTime <= 0))
		{
			SetMatchState(MatchState::MatchExitingIntermission);
		}
	}
	else if ((GetMatchState() == MatchState::InProgress) && TimeLimit > 0)
	{
		if (UTGameState && UTGameState->RemainingTime <= 0)
		{
			// Round is over, defense wins.
			ScoreOutOfLives(bRedToCap ? 1 : 0);
		}
		else
		{
			// increase defender respawn time by one second every minute
			if (UTGameState->RemainingTime % 60 == 0)
			{
				for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
				{
					AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
					if (PS && !PS->bOnlySpectator && !PS->bOutOfLives && !PS->bIsInactive && bAsymmetricVictoryConditions && !IsPlayerOnLifeLimitedTeam(PS))
					{
						PS->RespawnWaitTime += 1.f;
					}
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

	return IsTeamOnOffense(PlayerState->Team->TeamIndex);
}
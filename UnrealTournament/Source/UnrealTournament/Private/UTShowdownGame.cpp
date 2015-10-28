// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTShowdownGame.h"
#include "UTTimerMessage.h"
#include "UTShowdownGameMessage.h"
#include "UTHUD_Showdown.h"
#include "UTShowdownGameState.h"
#include "UTTimedPowerup.h"
#include "Slate/SlateGameResources.h"
#include "Slate/SUWindowsStyle.h"
#include "SNumericEntryBox.h"
#include "UTShowdownSquadAI.h"

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
	XPMultiplier = 15.0f;
	bHasRespawnChoices = false; // unique system
	HUDClass = AUTHUD_Showdown::StaticClass();
	GameStateClass = AUTShowdownGameState::StaticClass();

	PowerupBreakerPickupClass.AssetLongPathname = TEXT("/Game/RestrictedAssets/Pickups/Powerups/SuperchargeBase.SuperchargeBase_C");
	PowerupBreakerItemClass.AssetLongPathname = TEXT("/Game/RestrictedAssets/Pickups/Powerups/BP_Supercharge.BP_Supercharge_C");

	SuperweaponReplacementPickupClass.AssetLongPathname = TEXT("/Game/RestrictedAssets/Pickups/Powerups/PowerupBase.PowerupBase_C");
	SuperweaponReplacementItemClass.AssetLongPathname = TEXT("/Game/RestrictedAssets/Pickups/Powerups/BP_Invis.BP_Invis_C");

	bPowerupBreaker = true;
}

void AUTShowdownGame::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	bXRayBreaker = EvalBoolOptions(ParseOption(Options, TEXT("XRayBreaker")), bXRayBreaker);
	bPowerupBreaker = EvalBoolOptions(ParseOption(Options, TEXT("PowerupBreaker")), bPowerupBreaker);
	bBroadcastPlayerHealth = EvalBoolOptions(ParseOption(Options, TEXT("BroadcastPlayerHealth")), bBroadcastPlayerHealth);

	// this game mode requires a goal score
	if (GoalScore <= 0)
	{
		GoalScore = 5;
	}
}

void AUTShowdownGame::InitGameState()
{
	Super::InitGameState();

	Cast<AUTShowdownGameState>(GameState)->bBroadcastPlayerHealth = bBroadcastPlayerHealth;

	if (bPowerupBreaker)
	{
		TSubclassOf<AUTTimedPowerup> PowerupClass = Cast<UClass>(PowerupBreakerItemClass.TryLoad());
		if (PowerupClass != NULL)
		{
			PowerupClass.GetDefaultObject()->AddOverlayMaterials(UTGameState);
		}
	}
}

void AUTShowdownGame::StartNewRound()
{
	RoundElapsedTime = 0;
	LastRoundWinner = NULL;
	// reset everything
	for (FActorIterator It(GetWorld()); It; ++It)
	{
		// pickups spawn at start, don't respawn
		AUTPickup* Pickup = Cast<AUTPickup>(*It);
		if (Pickup != NULL && Pickup != BreakerPickup)
		{
			Pickup->bDelayedSpawn = false;
			Pickup->RespawnTime = 0.0f;
		}
		if (Cast<AUTCharacter>(*It) != NULL)
		{
			It->Destroy();
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

bool AUTShowdownGame::CheckRelevance_Implementation(AActor* Other)
{
	if (BreakerPickup != NULL && Cast<AUTTimedPowerup>(Other) != NULL && Other->GetClass() == BreakerPickup->GetInventoryType())
	{
		// don't override breaker powerup duration
		return true;
	}
	else
	{
		// @TODO FIXMESTEVE - don't check for weapon stay - once have deployable base class, remove all deployables from duel
		AUTPickupWeapon* PickupWeapon = Cast<AUTPickupWeapon>(Other);
		if (PickupWeapon != NULL && PickupWeapon->WeaponType != NULL && !PickupWeapon->WeaponType.GetDefaultObject()->bWeaponStay)
		{
			TSubclassOf<AUTPickupInventory> ReplacementPickupClass = SuperweaponReplacementPickupClass.TryLoadClass<AUTPickupInventory>();
			TSubclassOf<AUTInventory> ReplacementItemClass = SuperweaponReplacementItemClass.TryLoadClass<AUTInventory>();
			if (ReplacementPickupClass != NULL && ReplacementItemClass != NULL)
			{
				FActorSpawnParameters Params;
				Params.bNoCollisionFail = true;
				AUTPickupInventory* Pickup = GetWorld()->SpawnActor<AUTPickupInventory>(ReplacementPickupClass, PickupWeapon->GetActorLocation(), PickupWeapon->GetActorRotation(), Params);
				if (Pickup != NULL)
				{
					Pickup->SetInventoryType(ReplacementItemClass);
				}
			}
			return false;
		}
		else
		{
			return Super::CheckRelevance_Implementation(Other);
		}
	}
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

void AUTShowdownGame::ScoreExpiredRoundTime()
{
	// end round; player with highest health
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
	if (bTied || RoundWinner == NULL)
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
}

void AUTShowdownGame::CheckGameTime()
{
	static FName NAME_StartIntermission(TEXT("StartIntermission"));
	if (IsMatchInProgress() && !HasMatchEnded() && TimeLimit > 0 && UTGameState->RemainingTime <= 0 && !IsTimerActiveUFunc(this, NAME_StartIntermission))
	{
		ScoreExpiredRoundTime();
		SetTimerUFunc(this, NAME_StartIntermission, 2.0f, false);
	}
}

void AUTShowdownGame::StartIntermission()
{
	ClearTimerUFunc(this, FName(TEXT("StartIntermission")));
	if (!HasMatchEnded())
	{
		// if there's not enough time for a new round to work, then award victory now
		bool bTied = false;
		AUTPlayerState* Winner = NULL;
		Winner = IsThereAWinner(bTied);
		if (Winner != NULL && !bTied && ((Winner->Team == NULL) ? (Winner->Score >= GoalScore) : (Winner->Team->Score >= GoalScore)))
		{
			EndGame(Winner, TEXT("scorelimit"));
		}
		else
		{
			SetMatchState(MatchState::MatchIntermission);
			GameState->ForceNetUpdate();
		}
	}
}

void AUTShowdownGame::RestartPlayer(AController* aPlayer)
{
	if (bAllowPlayerRespawns)
	{
		Super::RestartPlayer(aPlayer);
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

float AUTShowdownGame::RatePlayerStart(APlayerStart* P, AController* Player)
{
	AUTShowdownGameState* GS = Cast<AUTShowdownGameState>(GameState);
	if (Player != NULL && GS->SpawnSelector == Player->PlayerState && !GS->IsAllowedSpawnPoint(Cast<AUTPlayerState>(Player->PlayerState), P))
	{
		return -1000.0f;
	}
	else
	{
		return Super::RatePlayerStart(P, Player);
	}
}

void AUTShowdownGame::HandleMatchIntermission()
{
	// respawn breaker pickup
	if (bPowerupBreaker)
	{
		if (BreakerPickup != NULL)
		{
			BreakerPickup->Destroy();
		}
		TSubclassOf<AUTPickupInventory> PickupClass = Cast<UClass>(PowerupBreakerPickupClass.TryLoad());
		TSubclassOf<AUTTimedPowerup> PowerupClass = Cast<UClass>(PowerupBreakerItemClass.TryLoad());
		if (PickupClass != NULL && PowerupClass != NULL)
		{
			// TODO: use game objective points or something
			AUTRecastNavMesh* NavData = GetUTNavData(GetWorld());
			if (NavData != NULL)
			{
				AActor* StartSpot = FindPlayerStart(NULL);
				FVector Extent = NavData->GetPOIExtent(StartSpot);
				FRandomDestEval NodeEval;
				float Weight = 0.0f;
				TArray<FRouteCacheItem> Route;
				FVector SpawnLoc = StartSpot->GetActorLocation();
				if (NavData->FindBestPath(NULL, FNavAgentProperties(Extent.X, Extent.Z * 2.0f), NodeEval, StartSpot->GetActorLocation(), Weight, false, Route) && Route.Num() > 0)
				{
					SpawnLoc = Route.Last().GetLocation(NULL);
					// try to pick a better poly for spawning (we'd like to stay away from walls)
					for (int32 i = Route.Num() - 1; i >= 0; i--)
					{
						if (NavData->GetPolySurfaceArea2D(Route[i].TargetPoly) > 10000.0f || NavData->GetPolyWalls(Route[i].TargetPoly).Num() == 0)
						{
							SpawnLoc = Route[i].GetLocation(NULL);
							break;
						}
					}
				}
				FActorSpawnParameters Params;
				Params.bNoCollisionFail = true;
				BreakerPickup = GetWorld()->SpawnActor<AUTPickupInventory>(PickupClass, SpawnLoc + Extent.Z, FRotator(0.0f, 360.0f * FMath::FRand(), 0.0f), Params);
				if (BreakerPickup != NULL)
				{
					BreakerPickup->SetInventoryType(PowerupClass);
					BreakerPickup->bDelayedSpawn = true;
					BreakerPickup->RespawnTime = 70.0f;
				}
			}
		}
	}

	AUTShowdownGameState* GS = Cast<AUTShowdownGameState>(GameState);

	GS->bActivateXRayVision = false;
	GS->IntermissionStageTime = 3;
	GS->bStartedSpawnSelection = false;
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
	TArray<AUTPlayerState*> UnsortedPicks;
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		AController* C = It->Get();
		if (C != NULL)
		{
			APawn* P = C->GetPawn();
			if (P != NULL)
			{
				APlayerState* SavedPlayerState = P->PlayerState; // keep the PlayerState reference for end of round HUD stuff
				P->TurnOff();
				C->UnPossess();
				P->PlayerState = SavedPlayerState;
				// we want the character around for the HUD displays of status but we don't need to actually see it and this prevents potential camera clipping
				P->GetRootComponent()->SetHiddenInGame(true, true);
			}
			AUTPlayerState* PS = Cast<AUTPlayerState>(C->PlayerState);
			if (PS != NULL && !PS->bOnlySpectator && PS->Team != NULL)
			{
				PS->RespawnChoiceA = NULL;
				PS->RespawnChoiceB = NULL;
				UnsortedPicks.Add(PS);
				// make sure players that were spectating while dead go back to dead state
				AUTPlayerController* PC = Cast<AUTPlayerController>(C);
				if (PC != NULL && PC->IsInState(NAME_Spectating))
				{
					PC->ChangeState(NAME_Inactive);
					PC->ClientGotoState(NAME_Inactive);
				}
			}
		}
	}
	// sort players by score (highest first)
	UnsortedPicks.Sort([&](const AUTPlayerState& A, const AUTPlayerState& B){ return A.Score > B.Score; });
	// sort team pick order by: winner of previous round last, others sorted by lowest score to highest score
	TArray<AUTTeamInfo*> SortedTeams = Teams;
	SortedTeams.Sort([&](const AUTTeamInfo& A, const AUTTeamInfo& B)
	{
		if (&A == LastRoundWinner)
		{
			return false;
		}
		else if (&B == LastRoundWinner)
		{
			return true;
		}
		else
		{
			return A.Score < B.Score;
		}
	});

	while (UnsortedPicks.Num() > 0)
	{
		bool bFoundAny = false;
		for (int32 i = 0; i < SortedTeams.Num(); i++)
		{
			for (AUTPlayerState* Pick : UnsortedPicks)
			{
				if (Pick->Team == SortedTeams[i])
				{
					RemainingPicks.Add(Pick);
					UnsortedPicks.Remove(Pick);
					bFoundAny = true;
					break;
				}
			}
		}
		if (!bFoundAny)
		{
			// sanity check so we don't infinitely recurse
			UE_LOG(UT, Warning, TEXT("Showdown spawn selection sorting error with %s on team %s"), *UnsortedPicks[0]->PlayerName, *UnsortedPicks[0]->Team->TeamName.ToString());
			RemainingPicks.Add(UnsortedPicks[0]);
			UnsortedPicks.RemoveAt(0);
		}
	}
	for (int32 i = 0; i < RemainingPicks.Num(); i++)
	{
		RemainingPicks[i]->SelectionOrder = uint8(i);
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
	AUTShowdownGameState* GS = Cast<AUTShowdownGameState>(GameState);
	if (GetMatchState() == MatchState::MatchIntermission)
	{
		// process bot spawn selection
		if (GS->SpawnSelector != NULL && (GS->IntermissionStageTime == 1 || FMath::FRand() < 0.3f))
		{
			AUTBot* B = Cast<AUTBot>(GS->SpawnSelector->GetOwner());
			if (B != NULL)
			{
				TArray<APlayerStart*> Choices;
				for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
				{
					if (GS->IsAllowedSpawnPoint(GS->SpawnSelector, *It))
					{
						Choices.Add(*It);
					}
				}
				GS->SpawnSelector->RespawnChoiceA = B->PickSpawnPoint(Choices);
			}
		}

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
			if (!GS->bStartedSpawnSelection)
			{
				GS->bStartedSpawnSelection = true;
				GS->IntermissionStageTime = 3;
			}
			else
			{
				if (GS->SpawnSelector != NULL)
				{
					if (GS->SpawnSelector->RespawnChoiceA == NULL)
					{
						GS->SpawnSelector->RespawnChoiceA = Cast<APlayerStart>(FindPlayerStart(Cast<AController>(GS->SpawnSelector->GetOwner())));
						GS->SpawnSelector->ForceNetUpdate();
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
		}
		if (GS->bFinalIntermissionDelay)
		{
			BroadcastLocalized(NULL, UUTTimerMessage::StaticClass(), int32(GS->IntermissionStageTime) - 1);
		}
	}
	else
	{
		RoundElapsedTime++;

		if (!GS->bActivateXRayVision && bXRayBreaker && RoundElapsedTime >= 70)
		{
			GS->bActivateXRayVision = true;
			GS->OnRep_XRayVision();
		}

		Super::DefaultTimer();
	}
}

void AUTShowdownGame::CreateGameURLOptions(TArray<TSharedPtr<TAttributePropertyBase>>& MenuProps)
{
	MenuProps.Add(MakeShareable(new TAttributeProperty<int32>(this, &TimeLimit, TEXT("TimeLimit"))));
	MenuProps.Add(MakeShareable(new TAttributeProperty<int32>(this, &GoalScore, TEXT("GoalScore"))));
	MenuProps.Add(MakeShareable(new TAttributePropertyBool(this, &bPowerupBreaker, TEXT("PowerupBreaker"))));
	MenuProps.Add(MakeShareable(new TAttributePropertyBool(this, &bBroadcastPlayerHealth, TEXT("BroadcastPlayerHealth"))));
	MenuProps.Add(MakeShareable(new TAttributePropertyBool(this, &bXRayBreaker, TEXT("XRayBreaker"))));
}

#if !UE_SERVER
void AUTShowdownGame::CreateConfigWidgets(TSharedPtr<class SVerticalBox> MenuSpace, bool bCreateReadOnly, TArray< TSharedPtr<TAttributePropertyBase> >& ConfigProps)
{
	CreateGameURLOptions(ConfigProps);

	TSharedPtr< TAttributeProperty<int32> > TimeLimitAttr = StaticCastSharedPtr<TAttributeProperty<int32>>(FindGameURLOption(ConfigProps, TEXT("TimeLimit")));
	TSharedPtr< TAttributeProperty<int32> > GoalScoreAttr = StaticCastSharedPtr<TAttributeProperty<int32>>(FindGameURLOption(ConfigProps, TEXT("GoalScore")));
	TSharedPtr< TAttributePropertyBool > PowerupBreakerAttr = StaticCastSharedPtr<TAttributePropertyBool>(FindGameURLOption(ConfigProps, TEXT("PowerupBreaker")));
	TSharedPtr< TAttributePropertyBool > BroadcastHealthAttr = StaticCastSharedPtr<TAttributePropertyBool>(FindGameURLOption(ConfigProps, TEXT("BroadcastPlayerHealth")));
	TSharedPtr< TAttributePropertyBool > XRayBreakerAttr = StaticCastSharedPtr<TAttributePropertyBool>(FindGameURLOption(ConfigProps, TEXT("XRayBreaker")));

	MenuSpace->AddSlot()
	.Padding(0.0f, 0.0f, 0.0f, 5.0f)
	.AutoHeight()
	.VAlign(VAlign_Top)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(350)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text(NSLOCTEXT("UTGameMode", "GoalScore", "Goal Score"))
			]
		]
		+ SHorizontalBox::Slot()
		.Padding(20.0f, 0.0f, 0.0f, 0.0f)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(300)
			[
				bCreateReadOnly ?
				StaticCastSharedRef<SWidget>(
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				.Text(GoalScoreAttr.ToSharedRef(), &TAttributeProperty<int32>::GetAsText)
				) :
				StaticCastSharedRef<SWidget>(
				SNew(SNumericEntryBox<int32>)
				.Value(GoalScoreAttr.ToSharedRef(), &TAttributeProperty<int32>::GetOptional)
				.OnValueChanged(GoalScoreAttr.ToSharedRef(), &TAttributeProperty<int32>::Set)
				.AllowSpin(true)
				.Delta(1)
				.MinValue(0)
				.MaxValue(999)
				.MinSliderValue(0)
				.MaxSliderValue(99)
				.EditableTextBoxStyle(SUWindowsStyle::Get(), "UT.Common.NumEditbox.White")
				)
			]
		]
	];
	MenuSpace->AddSlot()
	.Padding(0.0f, 0.0f, 0.0f, 5.0f)
	.AutoHeight()
	.VAlign(VAlign_Top)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(350)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text(NSLOCTEXT("UTGameMode", "RoundTimeLimit", "Round Time Limit"))
			]
		]
		+ SHorizontalBox::Slot()
		.Padding(20.0f, 0.0f, 0.0f, 0.0f)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(300)
			[
				bCreateReadOnly ?
				StaticCastSharedRef<SWidget>(
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				.Text(TimeLimitAttr.ToSharedRef(), &TAttributeProperty<int32>::GetAsText)
				) :
				StaticCastSharedRef<SWidget>(
				SNew(SNumericEntryBox<int32>)
				.Value(TimeLimitAttr.ToSharedRef(), &TAttributeProperty<int32>::GetOptional)
				.OnValueChanged(TimeLimitAttr.ToSharedRef(), &TAttributeProperty<int32>::Set)
				.AllowSpin(true)
				.Delta(1)
				.MinValue(0)
				.MaxValue(999)
				.MinSliderValue(0)
				.MaxSliderValue(60)
				.EditableTextBoxStyle(SUWindowsStyle::Get(), "UT.Common.NumEditbox.White")
				)
			]
		]
	];
	MenuSpace->AddSlot()
	.Padding(0.0f, 0.0f, 0.0f, 5.0f)
	.AutoHeight()
	.VAlign(VAlign_Top)
	[
		SNew(STextBlock)
		.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
		.Text(NSLOCTEXT("UTGameMode", "EXPERIMENTAL", "--- Experimental Options ---"))
	];
	MenuSpace->AddSlot()
	.AutoHeight()
	.VAlign(VAlign_Top)
	.Padding(0.0f, 0.0f, 0.0f, 5.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(350)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text(NSLOCTEXT("UTGameMode", "PowerupBreaker", "Spawn Overcharge Powerup after 70s"))
			]
		]
		+ SHorizontalBox::Slot()
		.Padding(20.0f, 0.0f, 0.0f, 10.0f)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(300)
			[
				bCreateReadOnly ?
				StaticCastSharedRef<SWidget>(
				SNew(SCheckBox)
				.IsChecked(PowerupBreakerAttr.ToSharedRef(), &TAttributePropertyBool::GetAsCheckBox)
				.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
				.ForegroundColor(FLinearColor::White)
				.Type(ESlateCheckBoxType::CheckBox)
				) :
				StaticCastSharedRef<SWidget>(
				SNew(SCheckBox)
				.IsChecked(PowerupBreakerAttr.ToSharedRef(), &TAttributePropertyBool::GetAsCheckBox)
				.OnCheckStateChanged(PowerupBreakerAttr.ToSharedRef(), &TAttributePropertyBool::SetFromCheckBox)
				.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
				.ForegroundColor(FLinearColor::White)
				.Type(ESlateCheckBoxType::CheckBox)
				)
			]
		]
	];
	MenuSpace->AddSlot()
	.AutoHeight()
	.VAlign(VAlign_Top)
	.Padding(0.0f, 0.0f, 0.0f, 5.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(350)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text(NSLOCTEXT("UTGameMode", "BroadcastPlayerHealth", "Show All Players' Status"))
			]
		]
		+ SHorizontalBox::Slot()
		.Padding(20.0f, 0.0f, 0.0f, 10.0f)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(300)
			[
				bCreateReadOnly ?
				StaticCastSharedRef<SWidget>(
				SNew(SCheckBox)
				.IsChecked(BroadcastHealthAttr.ToSharedRef(), &TAttributePropertyBool::GetAsCheckBox)
				.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
				.ForegroundColor(FLinearColor::White)
				.Type(ESlateCheckBoxType::CheckBox)
				) :
				StaticCastSharedRef<SWidget>(
				SNew(SCheckBox)
				.IsChecked(BroadcastHealthAttr.ToSharedRef(), &TAttributePropertyBool::GetAsCheckBox)
				.OnCheckStateChanged(BroadcastHealthAttr.ToSharedRef(), &TAttributePropertyBool::SetFromCheckBox)
				.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
				.ForegroundColor(FLinearColor::White)
				.Type(ESlateCheckBoxType::CheckBox)
				)
			]
		]
	];
	MenuSpace->AddSlot()
	.AutoHeight()
	.VAlign(VAlign_Top)
	.Padding(0.0f, 0.0f, 0.0f, 5.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(350)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text(NSLOCTEXT("UTGameMode", "XRayBreaker", "Get XRay Vision at 70s"))
			]
		]
		+ SHorizontalBox::Slot()
		.Padding(20.0f, 0.0f, 0.0f, 10.0f)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(300)
			[
				bCreateReadOnly ?
				StaticCastSharedRef<SWidget>(
				SNew(SCheckBox)
				.IsChecked(XRayBreakerAttr.ToSharedRef(), &TAttributePropertyBool::GetAsCheckBox)
				.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
				.ForegroundColor(FLinearColor::White)
				.Type(ESlateCheckBoxType::CheckBox)
				) :
				StaticCastSharedRef<SWidget>(
				SNew(SCheckBox)
				.IsChecked(XRayBreakerAttr.ToSharedRef(), &TAttributePropertyBool::GetAsCheckBox)
				.OnCheckStateChanged(XRayBreakerAttr.ToSharedRef(), &TAttributePropertyBool::SetFromCheckBox)
				.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
				.ForegroundColor(FLinearColor::White)
				.Type(ESlateCheckBoxType::CheckBox)
				)
			]
		]
	];
}
#endif
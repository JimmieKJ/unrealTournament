// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTFlagRunPvEGame.h"
#include "UTFlagRunGameState.h"
#include "UTFlagRunPvEHUD.h"
#include "UTPickupHealth.h"
#include "UTFlagRunPvESquadAI.h"
#include "UTPvEGameMessage.h"
#include "UTFlagRunPvEGameState.h"

AUTFlagRunPvEGame::AUTFlagRunPvEGame(const FObjectInitializer& OI)
	: Super(OI)
{
	bBalanceTeams = false;
	BotFillCount = 5;
	QuickPlayersToStart = 5;
	NumRounds = 1;
	GoalScore = 1;
	UnlimitedRespawnWaitTime = 10.0f;
	RollingAttackerRespawnDelay = 10.0f;
	bRollingAttackerSpawns = true;
	RoundLives = 3;
	BaseKillsForExtraLife = 25;
	TimeLimit = 10;
	EliteCostLimit = 5;
	bUseLevelTiming = false;
	HUDClass = AUTFlagRunPvEHUD::StaticClass();
	SquadType = AUTFlagRunPvESquadAI::StaticClass();
	GameStateClass = AUTFlagRunPvEGameState::StaticClass();
	DisplayName = NSLOCTEXT("UTGameMode", "FRPVE", "Flag Invasion");

	EditableMonsterTypes.Add(FStringClassReference(TEXT("/Game/RestrictedAssets/Monsters/BronzeTaye.BronzeTaye_C")));
	EditableMonsterTypes.Add(FStringClassReference(TEXT("/Game/RestrictedAssets/Monsters/BronzeTayeBio.BronzeTayeBio_C")));
	EditableMonsterTypes.Add(FStringClassReference(TEXT("/Game/RestrictedAssets/Monsters/SilverSkaarj.SilverSkaarj_C")));
	EditableMonsterTypes.Add(FStringClassReference(TEXT("/Game/RestrictedAssets/Monsters/SilverSkaarjBoots.SilverSkaarjBoots_C")));
	EditableMonsterTypes.Add(FStringClassReference(TEXT("/Game/RestrictedAssets/Monsters/Ghost.Ghost_C")));
	EditableMonsterTypes.Add(FStringClassReference(TEXT("/Game/RestrictedAssets/Monsters/ShieldBot.ShieldBot_C")));

	BoostPowerupTypes.Add(FStringClassReference(TEXT("/Game/RestrictedAssets/Pickups/Powerups/BP_Recall.BP_Recall_C")));
	BoostPowerupTypes.Add(FStringClassReference(TEXT("/Game/RestrictedAssets/Pickups/Powerups/Boost_RocketSalvo.Boost_RocketSalvo_C")));
	BoostPowerupTypes.Add(FStringClassReference(TEXT("/Game/RestrictedAssets/Pickups/Powerups/Boost_ShieldBubble.Boost_ShieldBubble_C")));

	VialReplacement = FStringClassReference(TEXT("/Game/RestrictedAssets/Pickups/Energy_Small.Energy_Small_C"));
}

void AUTFlagRunPvEGame::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	for (const FStringClassReference& Item : EditableMonsterTypes)
	{
		TSubclassOf<AUTMonster> NewType = Item.TryLoadClass<AUTMonster>();
		if (NewType != nullptr)
		{
			if (NewType.GetDefaultObject()->Cost <= 0)
			{
				MonsterTypesPeon.Add(NewType);
			}
			else
			{
				MonsterTypesElite.Add(NewType);
			}
		}
	}

	Super::InitGame(MapName, Options, ErrorMessage);
}

bool AUTFlagRunPvEGame::CheckRelevance_Implementation(AActor* Other)
{
	AUTPickupHealth* Health = Cast<AUTPickupHealth>(Other);
	if (Health != nullptr && Health->bSuperHeal && Health->HealAmount <= 5)
	{
		TSubclassOf<AUTPickup> NewItemType = VialReplacement.TryLoadClass<AUTPickup>();
		if (NewItemType != nullptr)
		{
			GetWorld()->SpawnActor<AUTPickup>(NewItemType, Health->GetActorTransform());
			return false;
		}
		else
		{
			return Super::CheckRelevance_Implementation(Other);
		}
	}
	else
	{
		return Super::CheckRelevance_Implementation(Other);
	}
}

void AUTFlagRunPvEGame::StartMatch()
{
	Super::StartMatch();

	AUTFlagRunPvEGameState* GS = Cast<AUTFlagRunPvEGameState>(GameState);
	if (BoostPowerupTypes.Num() > 0)
	{
		bAllowBoosts = true;
		GS->bAllowBoosts = true;
		GS->BoostRechargeTime = 180.0f;
		GS->OffenseKillsNeededForPowerup = 1000000; // i.e. never
		GS->DefenseKillsNeededForPowerup = 1000000;
		TArray<TSubclassOf<AUTInventory>> BoostList;
		for (const FStringClassReference& Item : BoostPowerupTypes)
		{
			TSubclassOf<AUTInventory> InvClass = Item.TryLoadClass<AUTInventory>();
			if (InvClass != nullptr)
			{
				BoostList.Add(InvClass);
			}
		}
		GS->SetSelectablePowerups(TArray<TSubclassOf<AUTInventory>>(), BoostList);
	}
	GS->KillsUntilExtraLife = BaseKillsForExtraLife;
}

bool AUTFlagRunPvEGame::ChangeTeam(AController* Player, uint8 NewTeam, bool bBroadcast)
{
	// all humans and player-bots go on team 1, monsters on team 0
	NewTeam = (Cast<APlayerController>(Player) != nullptr || Cast<AUTBotPlayer>(Player) != nullptr) ? 1 : 0;
	bBroadcast = false;
	return Super::ChangeTeam(Player, NewTeam, bBroadcast);
}

bool AUTFlagRunPvEGame::CheckForWinner(AUTTeamInfo* ScoringTeam)
{
	EndTeamGame(ScoringTeam, FName(TEXT("scorelimit")));
	return true;
}

AUTBotPlayer* AUTFlagRunPvEGame::AddBot(uint8 TeamNum)
{
	// player-bots are always difficulty 6 (GameDifficulty used for monsters)
	TeamNum = 1;
	TGuardValue<float> DifficultySwap(GameDifficulty, 6.0f);
	return Super::AddBot(TeamNum);
}

void AUTFlagRunPvEGame::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	AddPeonMonsters(8);

	SetTimerUFunc(this, FName(TEXT("EscalateMonsters")), 45.0f, true);
}

void AUTFlagRunPvEGame::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();

	ClearTimerUFunc(this, FName(TEXT("EscalateMonsters")));
}

void AUTFlagRunPvEGame::GiveDefaultInventory(APawn* PlayerPawn)
{
	AUTMonster* M = Cast<AUTMonster>(PlayerPawn);
	if (M != nullptr)
	{
		M->AddDefaultInventory(TArray<TSubclassOf<AUTInventory>>());
		// make monster's default weapon have unlimited ammo and don't drop it
		for (TInventoryIterator<AUTWeapon> It(M); It; ++It)
		{
			It->Ammo = It->MaxAmmo = 1000000;
			It->DroppedPickupClass = nullptr;
			break;
		}
	}
	else
	{
		Super::GiveDefaultInventory(PlayerPawn);
	}
}

void AUTFlagRunPvEGame::SpawnMonster(TSubclassOf<AUTMonster> MonsterClass)
{
	TSubclassOf<AUTMonsterAI> AIType = *MonsterClass.GetDefaultObject()->AIControllerClass;
	if (AIType == nullptr)
	{
		AIType = AUTMonsterAI::StaticClass();
	}
	AUTMonsterAI* NewAI = GetWorld()->SpawnActor<AUTMonsterAI>(AIType);
	NewAI->PawnClass = MonsterClass;
	NewAI->PlayerState->SetPlayerName(MonsterClass.GetDefaultObject()->DisplayName.ToString());
	NewAI->InitializeSkill(GameDifficulty);
	AUTPlayerState* PS = Cast<AUTPlayerState>(NewAI->PlayerState);
	if (PS != NULL)
	{
		PS->bReadyToPlay = true;
		PS->RemainingLives = MonsterClass.GetDefaultObject()->NumRespawns;
		HandleRollingAttackerRespawn(PS);
		PS->OnRespawnWaitReceived();
	}
	ChangeTeam(NewAI, 0, false);
}

void AUTFlagRunPvEGame::AddEliteMonsters(int32 MaxNum)
{
	TArray<TSubclassOf<AUTMonster>> ValidTypes;
	for (int32 i = 0; i < MaxNum; i++)
	{
		const int32 PointsAvailable = FMath::Min<int32>(EliteCostLimit, ElitePointsRemaining);
		ValidTypes.Reset(MonsterTypesElite.Num());
		for (const TSubclassOf<AUTMonster>& NextType : MonsterTypesElite)
		{
			if (NextType != nullptr && NextType.GetDefaultObject()->Cost <= PointsAvailable)
			{
				ValidTypes.Add(NextType);
			}
		}
		if (ValidTypes.Num() == 0)
		{
			break;
		}
		else
		{
			TSubclassOf<AUTMonster> Choice = ValidTypes[FMath::RandHelper(ValidTypes.Num())];
			SpawnMonster(Choice);
			ElitePointsRemaining -= Choice.GetDefaultObject()->Cost;
		}
	}
}

void AUTFlagRunPvEGame::AddPeonMonsters(int32 Num)
{
	for (int32 i = 0; i < Num; i++)
	{
		if (MonsterTypesPeon.Num() == 0)
		{
			break;
		}
		else
		{
			TSubclassOf<AUTMonster> Choice = MonsterTypesPeon[FMath::RandHelper(MonsterTypesPeon.Num())];
			SpawnMonster(Choice);
		}
	}
}

void AUTFlagRunPvEGame::EscalateMonsters()
{
	EliteCostLimit += 2 + int32(GameDifficulty) / 3;
	ElitePointsRemaining += EliteCostLimit * 1.5;
	AddPeonMonsters(1);
	AddEliteMonsters(ElitePointsRemaining / GetClass()->GetDefaultObject<AUTFlagRunPvEGame>()->EliteCostLimit);
	BroadcastLocalized(nullptr, UUTPvEGameMessage::StaticClass(), 0);
}

bool AUTFlagRunPvEGame::ModifyDamage_Implementation(int32& Damage, FVector& Momentum, APawn* Injured, AController* InstigatedBy, const FHitResult& HitInfo, AActor* DamageCauser, TSubclassOf<UDamageType> DamageType)
{
	// reduce monster damage to players if on low difficulty
	if (GameDifficulty < 5.0f && (Cast<APlayerController>(Injured->Controller) != nullptr || Cast<AUTBotPlayer>(Injured->Controller) != nullptr) && Cast<AUTMonsterAI>(InstigatedBy) != nullptr)
	{
		Damage *= 0.4f + GameDifficulty * 0.12f;
	}
	Super::ModifyDamage_Implementation(Damage, Momentum, Injured, InstigatedBy, HitInfo, DamageCauser, DamageType);
	if (Damage > 0 && InstigatedBy != nullptr && !UTGameState->OnSameTeam(InstigatedBy, Injured))
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(InstigatedBy->PlayerState);
		if (PS != nullptr)
		{
			PS->BoostRechargePct += float(Damage) * 0.00005f;
		}
	}
	return true;
}

void AUTFlagRunPvEGame::ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
	Super::ScoreKill_Implementation(Killer, Other, KilledPawn, DamageType);

	if ((Cast<APlayerController>(Killer) != nullptr || Cast<AUTBotPlayer>(Killer) != nullptr) && Cast<AUTMonster>(KilledPawn) != nullptr)
	{
		AUTFlagRunPvEGameState* GS = GetWorld()->GetGameState<AUTFlagRunPvEGameState>();
		GS->KillsUntilExtraLife--;
		if (GS->KillsUntilExtraLife <= 0)
		{
			for (int32 i = 0; i < GameState->PlayerArray.Num(); i++)
			{
				AUTPlayerState* PS = Cast<AUTPlayerState>(GameState->PlayerArray[i]);
				if (PS != nullptr && PS->Team == Teams[1])
				{
					PS->RemainingLives++;
					if (PS->bOutOfLives)
					{
						PS->SetOutOfLives(false);
						PS->ForceNetUpdate();
						RestartPlayer(Cast<AController>(PS->GetOwner()));
					}
				}
			}
			GS->ExtraLivesGained++;
			GS->KillsUntilExtraLife = BaseKillsForExtraLife * (GS->ExtraLivesGained + 1);
			BroadcastLocalized(nullptr, UUTPvEGameMessage::StaticClass(), 1);
		}
		GS->ForceNetUpdate();
	}

	AUTMonsterAI* MobAI = Cast<AUTMonsterAI>(Other);
	if (MobAI != nullptr)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(MobAI->PlayerState);
		if (PS != nullptr && PS->RemainingLives > 0)
		{
			PS->RemainingLives--;
			if (PS->RemainingLives == 0)
			{
				PS->bOutOfLives = true;
				PS->RespawnTime = 100000.0;
				MobAI->SetLifeSpan(3.0f); // for replication of death messages and the like
			}
		}
	}
}

void AUTFlagRunPvEGame::FindAndMarkHighScorer()
{
	int32 BestScore = 0;

	for (int32 PlayerIdx = 0; PlayerIdx < Teams[1]->GetTeamMembers().Num(); PlayerIdx++)
	{
		if (Teams[1]->GetTeamMembers()[PlayerIdx] != nullptr)
		{
			AUTPlayerState *PS = Cast<AUTPlayerState>(Teams[1]->GetTeamMembers()[PlayerIdx]->PlayerState);
			if (PS != nullptr)
			{
				PS->Score = PS->RoundKillAssists + PS->RoundKills;
				if (BestScore == 0 || PS->Score > BestScore)
				{
					BestScore = PS->Score;
				}
			}
		}
	}

	for (int32 PlayerIdx = 0; PlayerIdx < Teams[1]->GetTeamMembers().Num(); PlayerIdx++)
	{
		if (Teams[1]->GetTeamMembers()[PlayerIdx] != nullptr)
		{
			AUTPlayerState *PS = Cast<AUTPlayerState>(Teams[1]->GetTeamMembers()[PlayerIdx]->PlayerState);
			if (PS != nullptr)
			{
				bool bOldHighScorer = PS->bHasHighScore;
				PS->bHasHighScore = (BestScore == PS->Score) && (BestScore > 0);
				if ((bOldHighScorer != PS->bHasHighScore) && (GetNetMode() != NM_DedicatedServer))
				{
					PS->OnRep_HasHighScore();
				}
			}
		}
	}
}

void AUTFlagRunPvEGame::HandleRollingAttackerRespawn(AUTPlayerState* OtherPS)
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
}
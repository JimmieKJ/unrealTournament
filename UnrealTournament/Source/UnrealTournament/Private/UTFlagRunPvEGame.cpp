// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTFlagRunPvEGame.h"
#include "UTFlagRunGameState.h"
#include "UTFlagRunPvEHUD.h"
#include "UTPickupHealth.h"

AUTFlagRunPvEGame::AUTFlagRunPvEGame(const FObjectInitializer& OI)
	: Super(OI)
{
	bBalanceTeams = false;
	BotFillCount = 5;
	QuickPlayersToStart = 5;
	NumRounds = 1;
	GoalScore = 1;
	UnlimitedRespawnWaitTime = 8.0f;
	RollingAttackerRespawnDelay = 8.0f;
	bRollingAttackerSpawns = false;
	RoundLives = 9;
	TimeLimit = 10;
	MonsterCostLimit = 5;
	bUseLevelTiming = false;
	HUDClass = AUTFlagRunPvEHUD::StaticClass();
	DisplayName = NSLOCTEXT("UTGameMode", "FRPVE", "Flag Invasion");

	EditableMonsterTypes.Add(FStringClassReference(TEXT("/Game/RestrictedAssets/Monsters/BronzeTaye.BronzeTaye_C")));
	EditableMonsterTypes.Add(FStringClassReference(TEXT("/Game/RestrictedAssets/Monsters/BronzeTayeBio.BronzeTayeBio_C")));
	EditableMonsterTypes.Add(FStringClassReference(TEXT("/Game/RestrictedAssets/Monsters/SilverSkaarj.SilverSkaarj_C")));
	EditableMonsterTypes.Add(FStringClassReference(TEXT("/Game/RestrictedAssets/Monsters/SilverSkaarjBoots.SilverSkaarjBoots_C")));
	EditableMonsterTypes.Add(FStringClassReference(TEXT("/Game/RestrictedAssets/Monsters/Ghost.Ghost_C")));

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
			MonsterTypes.Add(NewType);
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

	if (BoostPowerupTypes.Num() > 0)
	{
		AUTFlagRunGameState* GS = Cast<AUTFlagRunGameState>(GameState);
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
}

bool AUTFlagRunPvEGame::ChangeTeam(AController* Player, uint8 NewTeam, bool bBroadcast)
{
	// all humans and player-bots go on team 1, monsters on team 0
	NewTeam = (Cast<APlayerController>(Player) != nullptr || Cast<AUTBotPlayer>(Player) != nullptr) ? 1 : 0;
	bBroadcast = false;
	return Super::ChangeTeam(Player, NewTeam, bBroadcast);
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

	MonsterPointsRemaining = MonsterCostLimit * 5;
	AddMonsters(5);

	SetTimerUFunc(this, FName(TEXT("EscalateMonsters")), 30.0f, true);
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

void AUTFlagRunPvEGame::AddMonsters(int32 MaxNum)
{
	TArray<TSubclassOf<AUTMonster>> ValidTypes;
	for (int32 i = 0; i < MaxNum; i++)
	{
		const int32 PointsAvailable = FMath::Min<int32>(MonsterCostLimit, MonsterPointsRemaining);
		ValidTypes.Reset(MonsterTypes.Num());
		for (const TSubclassOf<AUTMonster>& NextType : MonsterTypes)
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
			TSubclassOf<AUTMonsterAI> AIType = *Choice.GetDefaultObject()->AIControllerClass;
			if (AIType == nullptr)
			{
				AIType = AUTMonsterAI::StaticClass();
			}
			AUTMonsterAI* NewAI = GetWorld()->SpawnActor<AUTMonsterAI>(AIType);
			NewAI->PawnClass = Choice;
			NewAI->PlayerState->SetPlayerName(Choice.GetDefaultObject()->DisplayName.ToString());
			NewAI->InitializeSkill(GameDifficulty);
			AUTPlayerState* PS = Cast<AUTPlayerState>(NewAI->PlayerState);
			if (PS != NULL)
			{
				PS->bReadyToPlay = true;
				PS->RespawnWaitTime = UnlimitedRespawnWaitTime;
			}
			ChangeTeam(NewAI, 0, false);
			RestartPlayer(NewAI);
			MonsterPointsRemaining -= Choice.GetDefaultObject()->Cost;
		}
	}
}

void AUTFlagRunPvEGame::EscalateMonsters()
{
	MonsterCostLimit += 1 + int32(GameDifficulty) / 3;
	MonsterPointsRemaining += MonsterCostLimit;
	AddMonsters(MonsterPointsRemaining / GetClass()->GetDefaultObject<AUTFlagRunPvEGame>()->MonsterCostLimit);
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
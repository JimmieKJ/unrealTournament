// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamShowdownGame.h"
#include "UTTimerMessage.h"
#include "UTTeamShowdownGameMessage.h"
#include "UTHUD_Showdown.h"
#include "UTShowdownGameState.h"
#include "UTShowdownSquadAI.h"
#include "UTPickupAmmo.h"
#include "UTDroppedPickup.h"

AUTTeamShowdownGame::AUTTeamShowdownGame(const FObjectInitializer& OI)
	: Super(OI)
{
	TimeLimit = 2.0f; // per round
	GoalScore = 5;
	DisplayName = NSLOCTEXT("UTGameMode", "TeamShowdown", "Team Showdown");
}

void AUTTeamShowdownGame::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	int32 SavedBotFillCount = BotFillCount;

	Super::InitGame(MapName, Options, ErrorMessage);

	// skip Duel overrides we don't want
	if (HasOption(Options, TEXT("Bots")))
	{
		BotFillCount = GetIntOption(Options, TEXT("Bots"), SavedBotFillCount) + 1;
	}
	else
	{
		BotFillCount = GetIntOption(Options, TEXT("BotFill"), SavedBotFillCount);
	}
	GameSession->MaxPlayers = GetIntOption(Options, TEXT("MaxPlayers"), GameSession->GetClass()->GetDefaultObject<AGameSession>()->MaxPlayers);
}

bool AUTTeamShowdownGame::CheckRelevance_Implementation(AActor* Other)
{
	// weapons and ammo pickups are worth double ammo
	AUTWeapon* W = Cast<AUTWeapon>(Other);
	if (W != NULL)
	{
		W->Ammo = FMath::Min<int32>(W->MaxAmmo, W->Ammo * 2);
	}
	else
	{
		AUTPickupAmmo* AmmoPickup = Cast<AUTPickupAmmo>(Other);
		if (AmmoPickup != NULL)
		{
			AmmoPickup->Ammo.Amount *= 2;
		}
		else
		{
			// make dropped weapons last for the whole round
			AUTDroppedPickup* Drop = Cast<AUTDroppedPickup>(Other);
			if (Drop != NULL && Drop->GetInventoryType() != NULL && Drop->GetInventoryType()->IsChildOf(AUTWeapon::StaticClass()))
			{
				Drop->SetLifeSpan(0.0f);
			}
		}
	}
	return Super::CheckRelevance_Implementation(Other);
}

void AUTTeamShowdownGame::DiscardInventory(APawn* Other, AController* Killer)
{
	AUTCharacter* UTC = Cast<AUTCharacter>(Other);
	if (UTC != NULL)
	{
		// discard all weapons instead of just the one
		FRotationMatrix RotMat(Other->GetActorRotation());
		FVector ThrowDirs[] = { RotMat.GetUnitAxis(EAxis::Y), -RotMat.GetUnitAxis(EAxis::Y), -RotMat.GetUnitAxis(EAxis::X) };

		int32 i = 0;
		for (TInventoryIterator<AUTWeapon> It(UTC); It; ++It)
		{
			if (*It != UTC->GetWeapon())
			{
				FVector FinalDir;
				if (i < ARRAY_COUNT(ThrowDirs))
				{
					FinalDir = ThrowDirs[i];
				}
				else
				{
					FinalDir = FMath::VRand();
					FinalDir.Z = 0.0f;
					FinalDir.Normalize();
				}
				It->DropFrom(UTC->GetActorLocation(), FinalDir * 1000.0f + FVector(0.0f, 0.0f, 250.0f));
			}
		}
	}
	Super::DiscardInventory(Other, Killer);
}

void AUTTeamShowdownGame::ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
	if (GetMatchState() != MatchState::MatchIntermission && (TimeLimit <= 0 || UTGameState->RemainingTime > 0))
	{
		if (Other != NULL)
		{
			AUTPlayerState* OtherPS = Cast<AUTPlayerState>(Other->PlayerState);
			if (OtherPS != NULL && OtherPS->Team != NULL)
			{
				AUTPlayerState* KillerPS = (Killer != NULL && Killer != Other) ? Cast<AUTPlayerState>(Killer->PlayerState) : NULL;
				AUTTeamInfo* KillerTeam = (KillerPS != NULL && KillerPS->Team != OtherPS->Team) ? KillerPS->Team : Teams[1 - FMath::Min<int32>(1, OtherPS->Team->TeamIndex)];

				bool bAnyOtherAlive = false;
				for (AController* C : OtherPS->Team->GetTeamMembers())
				{
					if (C != Other && C->GetPawn() != NULL && !C->GetPawn()->bTearOff)
					{
						bAnyOtherAlive = true;
						break;
					}
				}
				if (!bAnyOtherAlive)
				{
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
		}
		AUTTeamGameMode::ScoreKill_Implementation(Killer, Other, KilledPawn, DamageType);
	}
}

void AUTTeamShowdownGame::ScoreExpiredRoundTime()
{
	TArray<int32> LivingPlayersPerTeam;
	TArray<int32> TotalHealthPerTeam;

	// end round; team with most players alive wins
	// if equal, most total health wins
	// if that's equal too, both teams get a point
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		AUTCharacter* UTC = Cast<AUTCharacter>(It->Get());
		if (UTC != NULL && !UTC->IsDead())
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(UTC->PlayerState);
			if (PS != NULL && PS->Team != NULL)
			{
				LivingPlayersPerTeam.SetNumZeroed(FMath::Max<int32>(LivingPlayersPerTeam.Num(), PS->Team->TeamIndex + 1));
				TotalHealthPerTeam.SetNumZeroed(FMath::Max<int32>(TotalHealthPerTeam.Num(), PS->Team->TeamIndex + 1));
				LivingPlayersPerTeam[PS->Team->TeamIndex]++;
				TotalHealthPerTeam[PS->Team->TeamIndex] += UTC->Health;
			}
		}
	}
	
	int32 BestNumIndex = INDEX_NONE;
	int32 BestNum = 0;
	int32 BestHealthIndex = INDEX_NONE;
	int32 BestHealth = 0;
	for (int32 i = 0; i < LivingPlayersPerTeam.Num(); i++)
	{
		if (LivingPlayersPerTeam[i] > BestNum)
		{
			BestNumIndex = i;
			BestNum = LivingPlayersPerTeam[i];
		}
		else if (LivingPlayersPerTeam[i] == BestNum)
		{
			BestNumIndex = INDEX_NONE;
		}
		if (TotalHealthPerTeam[i] > BestHealth)
		{
			BestHealthIndex = i;
			BestHealth = TotalHealthPerTeam[i];
		}
		else if (TotalHealthPerTeam[i] == BestHealth)
		{
			BestHealthIndex = INDEX_NONE;
		}
	}

	LastRoundWinner = NULL;
	if (BestNumIndex != INDEX_NONE)
	{
		LastRoundWinner = Teams[BestNumIndex];
	}
	else if (BestHealthIndex != INDEX_NONE)
	{
		LastRoundWinner = Teams[BestHealthIndex];
	}
	if (LastRoundWinner == NULL)
	{
		for (AUTTeamInfo* Team : Teams)
		{
			Team->Score += 1.0f;
			Team->ForceNetUpdate();
		}
		BroadcastLocalized(NULL, UUTTeamShowdownGameMessage::StaticClass(), 2);
	}
	else
	{
		LastRoundWinner->Score += 1.0f;
		LastRoundWinner->ForceNetUpdate();
		BroadcastLocalized(NULL, UUTTeamShowdownGameMessage::StaticClass(), (BestNumIndex != INDEX_NONE) ? 0 : 1, NULL, NULL, LastRoundWinner);
	}
}
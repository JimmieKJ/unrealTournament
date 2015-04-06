// Copyright 1998 - 2015 Epic Games, Inc.All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamGameMode.h"
#include "UTCTFGameMode.h"
#include "UTCTFScoring.h"

AUTCTFScoring::AUTCTFScoring(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void AUTCTFScoring::BeginPlay()
{
	Super::BeginPlay();

	FTimerHandle TempHandle;
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTCTFScoring::FlagHeldTimer, 1.0f, true);
}

void AUTCTFScoring::FlagHeldTimer()
{
	for (int32 FlagTeam = 0; FlagTeam < CTFGameState->FlagBases.Num(); FlagTeam++)
	{
		AUTCTFFlag* Flag = CTFGameState->FlagBases[FlagTeam] ? CTFGameState->FlagBases[FlagTeam]->MyFlag : NULL;
		if (Flag && Flag->Holder)
		{
			// Look to see if the holder's flag is out (thus our holder is at minimum preventing the other team from scoring)
			int32 HolderTeam = Flag->Holder->GetTeamNum();
			if (CTFGameState->FlagBases.IsValidIndex(HolderTeam) && CTFGameState->FlagBases[HolderTeam] && CTFGameState->FlagBases[FlagTeam]->GetCarriedObjectHolder() != NULL)
			{
				// score holder for keeping flag out, preventing capture.
				Flag->Holder->Score += FlagHolderPointsPerSecond;
			}
		}
	}
}

void AUTCTFScoring::ScoreObject(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason, float TimeLimit)
{
	if (!CTFGameState)
	{
		return;
	}
	float DistanceFromHome = (GameObject->GetActorLocation() - CTFGameState->FlagBases[GameObject->GetTeamNum()]->GetActorLocation()).SizeSquared();
	float DistanceFromScore = (GameObject->GetActorLocation() - CTFGameState->FlagBases[1 - GameObject->GetTeamNum()]->GetActorLocation()).SizeSquared();

	UE_LOG(UT, Verbose, TEXT("========================================="));
	UE_LOG(UT, Verbose, TEXT("Flag Score by %s - Reason: %s"), *Holder->PlayerName, *Reason.ToString());

	if (Reason == FName("SentHome"))
	{
		for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
		{
			AController* C = *Iterator;
			AUTPlayerState* PS = Cast<AUTPlayerState>(C->PlayerState);
			if (PS != NULL && PS->GetTeamNum() == Holder->GetTeamNum())
			{
				if (C->GetPawn() != NULL && (GameObject->GetActorLocation() - C->GetPawn()->GetActorLocation()).SizeSquared() <= FlagCombatBonusDistance)
				{
					if (PS == Holder)
					{
						uint32 Points = FlagReturnPoints;
						if (DistanceFromHome > DistanceFromScore)
						{
							Points += FlagReturnEnemyZoneBonus;
						}

						if (DistanceFromScore <= FlagDenialDistance)
						{
							Points += FlagReturnDenialBonus;
						}
						UE_LOG(UT, Verbose, TEXT("    Player %s received %i"), *PS->PlayerName, Points);
						PS->FlagReturns++;
						PS->AdjustScore(Points);
					}
					else
					{
						UE_LOG(UT, Verbose, TEXT("    Player %s received %i"), *PS->PlayerName, ProximityReturnBonus);
						PS->AdjustScore(ProximityReturnBonus);
						//PS->Assists++; // TODO: some other stat? people expect an assist implies a scoring play
					}
				}
			}
		}
	}
	else if (Reason == FName("FlagCapture"))
	{
		FCTFScoringPlay NewScoringPlay;
		NewScoringPlay.Team = Holder->Team;
		NewScoringPlay.ScoredBy = FSafePlayerName(Holder);
		// TODO: need to handle no timelimit
		if (CTFGameState->bPlayingAdvantage)
		{
			NewScoringPlay.ElapsedTime = TimeLimit + 60 - CTFGameState->RemainingTime;
		}
		else
		{
			NewScoringPlay.ElapsedTime = TimeLimit - CTFGameState->RemainingTime;
		}
		if (CTFGameState->IsMatchInOvertime())
		{
			NewScoringPlay.Period = 2;
		}
		else if (CTFGameState->bSecondHalf)
		{
			NewScoringPlay.Period = 1;
		}

		Holder->FlagCaptures++;

		// NOTE: It's possible that the first player to pickup this flag might have left so first might be NULL.  Not
		// sure if we should then assign the points to the next in line so I'm ditching the points for now.
		for (int i = 0; i<GameObject->AssistTracking.Num(); i++)
		{
			AUTPlayerState* Who = GameObject->AssistTracking[i].Holder;
			if (Who != NULL)
			{
				float HeldTime = GameObject->GetHeldTime(Who);
				int32 Points = i == 0 ? FlagFirstPickupPoints : 0;
				if (HeldTime > 0 && GameObject->TotalHeldTime > 0)
				{
					float Perc = HeldTime / GameObject->TotalHeldTime;
					Points = Points + int(float(FlagTotalScorePool * Perc));
				}
				UE_LOG(UT, Verbose, TEXT("    Assist Points for %s = %i"), *Who->PlayerName, Points)
					Who->AdjustScore(Points);
				if (Who != Holder)
				{
					NewScoringPlay.Assists.AddUnique(FSafePlayerName(Who));
				}
			}
		}

		for (AController* Rescuer : GameObject->HolderRescuers)
		{
			if (Rescuer != NULL && Rescuer->PlayerState != Holder && Cast<AUTPlayerState>(Rescuer->PlayerState) != NULL && CTFGameState->OnSameTeam(Rescuer, Holder))
			{
				NewScoringPlay.Assists.AddUnique(FSafePlayerName((AUTPlayerState*)Rescuer->PlayerState));
			}
		}

		// Give out bonus points to all teammates near the flag.
		for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
		{
			AController* C = *Iterator;
			AUTPlayerState* PS = Cast<AUTPlayerState>(C->PlayerState);
			if (PS != NULL && PS->GetTeamNum() == Holder->GetTeamNum())
			{
				if (C->GetPawn() != NULL && PS != Holder && (GameObject->GetActorLocation() - C->GetPawn()->GetActorLocation()).SizeSquared() <= FlagCombatBonusDistance)
				{
					UE_LOG(UT, Verbose, TEXT("    Prox Bonus for %s = %i"), *PS->PlayerName, ProximityCapBonus);
					PS->AdjustScore(ProximityCapBonus);
				}
			}
		}

		// increment assist counter for players who got them
		for (const FSafePlayerName& Assist : NewScoringPlay.Assists)
		{
			if (Assist.PlayerState != NULL)
			{
				Assist.PlayerState->Assists++;
			}
		}

		CTFGameState->AddScoringPlay(NewScoringPlay);
	}
}

void AUTCTFScoring::ScorePickup(AUTPickup* Pickup, AUTPlayerState* PickedUpBy, AUTPlayerState* LastPickedUpBy)
{
	if (PickedUpBy != NULL && Pickup != NULL)
	{
		int Points = 0;
		switch (Pickup->PickupType)
		{
		case PC_Minor:
			Points = MinorPickupScore;
			break;
		case PC_Major:
			Points = MajorPickupScore;
			break;
		case PC_Super:
			Points = SuperPickupScore;
			break;
		default:
			Points = 0;
			break;
		}

		if (PickedUpBy == LastPickedUpBy)
		{
			Points = uint32(float(Points) * ControlFreakMultiplier);
		}

		PickedUpBy->AdjustScore(Points);

		UE_LOG(UT, Verbose, TEXT("========================================="));
		UE_LOG(UT, Verbose, TEXT("ScorePickup: %s %s %i"), *PickedUpBy->PlayerName, *GetNameSafe(Pickup), Points);
		UE_LOG(UT, Verbose, TEXT("========================================="));
	}
}

void AUTCTFScoring::ScoreDamage(int32 DamageAmount, AController* Victim, AController* Attacker)
{
	// No Damage for environmental damage
	if (Attacker == NULL) return;

	AUTPlayerState* AttackerPS = Cast<AUTPlayerState>(Attacker->PlayerState);

	if (AttackerPS != NULL)
	{
		int AdjustedDamageAmount = FMath::Clamp<int>(DamageAmount, 0, 100);
		if (Attacker != Victim)
		{
			if (Attacker->GetPawn() != NULL && IsCloseToFlagCarrier(Attacker->GetPawn(), FlagCombatBonusDistance, 255))
			{
				AdjustedDamageAmount = uint32(float(AdjustedDamageAmount) * FlagCarrierCombatMultiplier);
			}
			AttackerPS->AdjustScore(AdjustedDamageAmount);
			UE_LOG(UT, Verbose, TEXT("========================================="));
			UE_LOG(UT, Verbose, TEXT("DamageScore: %s %i"), *AttackerPS->PlayerName, AdjustedDamageAmount);
			UE_LOG(UT, Verbose, TEXT("========================================="));
		}
	}
}

bool AUTCTFScoring::IsCloseToFlagCarrier(AActor* Who, float CheckDistanceSquared, uint8 TeamNum)
{
	if (CTFGameState != NULL)
	{
		// not enough of these to worry about the minor inefficiency in the specific team case; better to keep code simple
		for (AUTCTFFlagBase* Base : CTFGameState->FlagBases)
		{
			if (Base != NULL && (TeamNum == 255 || Base->GetTeamNum() == TeamNum) &&
				Base->MyFlag->ObjectState == CarriedObjectState::Held && (Base->MyFlag->GetActorLocation() - Who->GetActorLocation()).SizeSquared() <= CheckDistanceSquared)
			{
				return true;
			}
		}
	}
	return false;
}

void AUTCTFScoring::ScoreKill(AController* Killer, AController* Other, TSubclassOf<UDamageType> DamageType)
{
	if ((Killer != NULL && Killer != Other))
	{
		AUTPlayerState* AttackerPS = Cast<AUTPlayerState>(Killer->PlayerState);
		if (AttackerPS != NULL)
		{
			uint32 Points = BaseKillScore;
			bool bGaveCombatBonus = false;
			if (Killer->GetPawn() != NULL && IsCloseToFlagCarrier(Killer->GetPawn(), FlagCombatBonusDistance, 255))
			{
				Points += CombatBonusKillBonus;
				bGaveCombatBonus = true;
			}
			// tracking of assists for flag carrier rescues
			if (CTFGameState != NULL && !CTFGameState->OnSameTeam(Killer, Other) && Cast<IUTTeamInterface>(Killer) != NULL)
			{
				uint8 KillerTeam = Cast<IUTTeamInterface>(Killer)->GetTeamNum();
				if (KillerTeam != 255)
				{
					bool bFCRescue = false;
					for (int32 i = 0; i < CTFGameState->FlagBases.Num(); i++)
					{
						if (CTFGameState->FlagBases[i] != NULL && CTFGameState->FlagBases[i]->MyFlag != NULL && CTFGameState->FlagBases[i]->MyFlag->HoldingPawn != NULL &&
							CTFGameState->FlagBases[i]->MyFlag->HoldingPawn != Killer->GetPawn() && CTFGameState->FlagBases[i]->MyFlag->HoldingPawn->GetTeamNum() == KillerTeam)
						{
							bFCRescue = CTFGameState->FlagBases[i]->MyFlag->HoldingPawn->LastHitBy == Other || (Killer->GetPawn() != NULL && IsCloseToFlagCarrier(Killer->GetPawn(), FlagCombatBonusDistance, i)) || (Other->GetPawn() != NULL && IsCloseToFlagCarrier(Other->GetPawn(), FlagCombatBonusDistance, i));
							if (bFCRescue)
							{
								CTFGameState->FlagBases[i]->MyFlag->HolderRescuers.AddUnique(Killer);
							}
						}
					}
					if (bFCRescue && !bGaveCombatBonus)
					{
						Points += CombatBonusKillBonus;
						bGaveCombatBonus = true;
					}
				}
			}

			AttackerPS->AdjustScore(Points);
		}
	}
}
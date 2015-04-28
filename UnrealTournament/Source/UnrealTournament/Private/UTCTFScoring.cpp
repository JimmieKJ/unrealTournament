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
			if (CTFGameState->FlagBases.IsValidIndex(HolderTeam) && CTFGameState->FlagBases[HolderTeam] && CTFGameState->FlagBases[HolderTeam]->GetCarriedObjectHolder() != NULL)
			{
				// score holder for keeping flag out, preventing capture.
				Flag->Holder->Score += FlagHolderPointsPerSecond;
			}
		}
	}
}

float AUTCTFScoring::GetTotalHeldTime(AUTCarriedObject* GameObject)
{
	if (!GameObject)
	{
		return 0.f;
	}
	float TotalHeldTime = 0.f;
	for (int i = 0; i < GameObject->AssistTracking.Num(); i++)
	{
		AUTPlayerState* FlagRunner = GameObject->AssistTracking[i].Holder;
		if (FlagRunner != NULL)
		{
			TotalHeldTime += GameObject->GetHeldTime(FlagRunner);;
		}
	}
	return TotalHeldTime;
}

void AUTCTFScoring::ScoreObject(AUTCarriedObject* GameObject, AUTCharacter* ScoringPawn, AUTPlayerState* ScorerPS, FName Reason, float TimeLimit)
{
	if (!CTFGameState)
	{
		return;
	}
	if (Reason == FName("SentHome"))
	{
		ScorerPS->FlagReturns++; 
		int32 Points = FlagReturnPoints + FMath::Min<int32>(MaxFlagHeldBonus, FlagReturnHeldBonus * GetTotalHeldTime(GameObject) / 2);
		ScorerPS->AdjustScore(Points);
		ScorerPS->LastFlagReturnTime = GetWorld()->GetTimeSeconds();
		//UE_LOG(UT, Warning, TEXT("Flag Return %s score %d"), *ScorerPS->PlayerName, Points);
	}
	else if (Reason == FName("FlagCapture"))
	{
		FCTFScoringPlay NewScoringPlay;
		NewScoringPlay.Team = ScorerPS->Team;
		NewScoringPlay.ScoredBy = FSafePlayerName(ScorerPS);
		NewScoringPlay.TeamScores[0] = CTFGameState->Teams[0] ? CTFGameState->Teams[0]->Score : 0;
		NewScoringPlay.TeamScores[1] = CTFGameState->Teams[1] ? CTFGameState->Teams[1]->Score : 1;
		NewScoringPlay.TeamScores[ScorerPS->Team->TeamIndex] += 1;

		// TODO: need to handle no timelimit
		NewScoringPlay.RemainingTime = CTFGameState->bPlayingAdvantage ? 0.f : CTFGameState->RemainingTime;
		if (CTFGameState->IsMatchInOvertime())
		{
			NewScoringPlay.Period = 2;
		}
		else if (CTFGameState->bSecondHalf)
		{
			NewScoringPlay.Period = 1;
		}

		ScorerPS->FlagCaptures++;
		NewScoringPlay.ScoredByCaps = ScorerPS->FlagCaptures;
		int32 FlagPickupPoints = FlagFirstPickupPoints;
		float TotalHeldTime = GetTotalHeldTime(GameObject);
		for (int i = 0; i<GameObject->AssistTracking.Num(); i++)
		{
			AUTPlayerState* FlagRunner = GameObject->AssistTracking[i].Holder;
			if (FlagRunner != NULL)
			{
				float HeldTime = GameObject->GetHeldTime(FlagRunner);
				int32 Points = FlagPickupPoints;
				FlagPickupPoints = 0;
				if (HeldTime > 0.f && TotalHeldTime > 0.f)
				{
					Points = Points + int(float(FlagRunScorePool * HeldTime / TotalHeldTime));
				}
				if (FlagRunner != ScorerPS)
				{
					FCTFAssist NewAssist;
					NewAssist.AssistName = FSafePlayerName(FlagRunner);
					NewAssist.bCarryAssist = true;
					NewScoringPlay.Assists.AddUnique(NewAssist);
				}
				else
				{
					Points += FlagCapPoints;
				}
				FlagRunner->AdjustScore(Points);
				//UE_LOG(UT, Warning, TEXT("Flag assist (held) %s score %d"), *ScorerPS->PlayerName, Points);
			}
		}

		for (AController* Rescuer : GameObject->HolderRescuers)
		{
			if (Rescuer != NULL && Rescuer->PlayerState != ScorerPS && Cast<AUTPlayerState>(Rescuer->PlayerState) != NULL && CTFGameState->OnSameTeam(Rescuer, ScorerPS))
			{
				bool bFoundAssist = false;
				for (int32 j = 0; j < NewScoringPlay.Assists.Num(); j++)
				{
					if (NewScoringPlay.Assists[j].AssistName == FSafePlayerName((AUTPlayerState*)Rescuer->PlayerState))
					{
						bFoundAssist = true;
						NewScoringPlay.Assists[j].bDefendAssist = true;
						break;
					}
				}
				if (!bFoundAssist)
				{
					FCTFAssist NewAssist;
					NewAssist.AssistName = FSafePlayerName((AUTPlayerState*)Rescuer->PlayerState);
					NewAssist.bDefendAssist = true;
					NewScoringPlay.Assists.AddUnique(NewAssist);
				}
			}
		}

		// flag return enabling score gets bonus and assist
		for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
		{
			AController* C = *Iterator;
			AUTPlayerState* PS =  C ? Cast<AUTPlayerState>(C->PlayerState) : NULL;
			if (PS && (PS != ScorerPS) && CTFGameState->OnSameTeam(PS, ScorerPS))
			{
				if (GetWorld()->GetTimeSeconds() - PS->LastFlagReturnTime < RecentActionTimeThreshold)
				{
					PS->AdjustScore(FlagReturnAssist);
					//UE_LOG(UT, Warning, TEXT("Flag assist (return) %s score 100"), *PS->PlayerName);
					bool bFoundAssist = false;
					for (int32 j = 0; j < NewScoringPlay.Assists.Num(); j++)
					{
						if (NewScoringPlay.Assists[j].AssistName == FSafePlayerName(PS))
						{
							bFoundAssist = true;
							NewScoringPlay.Assists[j].bReturnAssist = true;
							break;
						}
					}
					if (!bFoundAssist)
					{
						FCTFAssist NewAssist;
						NewAssist.AssistName = FSafePlayerName(PS);
						NewAssist.bReturnAssist = true;
						NewScoringPlay.Assists.AddUnique(NewAssist);
					}
				}
				else
				{
					// everybody on team gets some bonus for cap
					PS->AdjustScore(TeamCapBonus);
					//UE_LOG(UT, Warning, TEXT("Flag assist (general) %s score 20"), *PS->PlayerName);
				}
			}
		}

		// increment assist counter for players who got them
		for (const FCTFAssist& Assist : NewScoringPlay.Assists)
		{
			if (Assist.AssistName.PlayerState != NULL)
			{
				Assist.AssistName.PlayerState->Assists++;
				Assist.AssistName.PlayerState->bNeedsAssistAnnouncement = true;
			}
		}
		CTFGameState->AddScoringPlay(NewScoringPlay);
	}
}

/** Save partial credit for flag carrier kills. @TODO FIXMESTEVE - only award score when flag carrier dies */
void AUTCTFScoring::ScoreDamage(int32 DamageAmount, AController* Victim, AController* Attacker)
{
	AUTPlayerState* VictimPS = Victim ? Cast<AUTPlayerState>(Victim->PlayerState) : NULL;
	if (!VictimPS || !VictimPS->CarriedObject || (DamageAmount <= 0))
	{
		return;
	}
	AUTPlayerState* AttackerPS = Attacker ? Cast<AUTPlayerState>(Attacker->PlayerState) : NULL;

	if (AttackerPS && (AttackerPS != VictimPS))
	{
		int32 DamagePoints = FMath::Clamp<int32>(AttackerPS->FCDamageAccum + DamageAmount, 0, 100) / 20;
		AttackerPS->FCDamageAccum = AttackerPS->FCDamageAccum + DamageAmount - 20 * DamagePoints;
		AttackerPS->AdjustScore(DamagePoints);
		AttackerPS->LastShotFCTime = GetWorld()->GetTimeSeconds();
		AttackerPS->LastShotFC = VictimPS;
	}
}

bool AUTCTFScoring::WasThreateningFlagCarrier(AUTPlayerState *VictimPS, APawn* KilledPawn, AUTPlayerState *KillerPS)
{
	AUTPlayerState* FlagHolder = CTFGameState ? CTFGameState->GetFlagHolder(VictimPS->GetTeamNum()) : NULL;
	if (FlagHolder && FlagHolder->CarriedObject && (KillerPS != FlagHolder))
	{
		if ((VictimPS->LastShotFC == FlagHolder) && (GetWorld()->GetTimeSeconds() - VictimPS->LastShotFCTime < RecentActionTimeThreshold))
		{
			// this player recently shot the flag holder, so definitely threat
			return true;
		}
		AController* VictimController = Cast<AController>(VictimPS->GetOwner());
		AController* FCController = Cast<AController>(FlagHolder->GetOwner());
		APawn* FlagCarrier = FCController ? FCController->GetPawn() : NULL;
		if (KilledPawn && FlagCarrier && ((VictimController->GetControlRotation().Vector() | (FlagCarrier->GetActorLocation() - KilledPawn->GetActorLocation()).GetSafeNormal()) > 0.8f))
		{
			// threat if was looking in direction of FC and has line of sight to FC
			static FName NAME_LineOfSight = FName(TEXT("LineOfSight"));
			FCollisionQueryParams CollisionParams(NAME_LineOfSight, true, KilledPawn);
			CollisionParams.AddIgnoredActor(FlagCarrier);
			FVector ViewPoint = KilledPawn->GetActorLocation() + FVector(0.f, 0.f, KilledPawn->BaseEyeHeight);
			FVector TargetLoc = FlagCarrier->GetActorLocation() + FVector(0.f, 0.f, FlagCarrier->BaseEyeHeight);
			return !GetWorld()->LineTraceTest(ViewPoint, TargetLoc, ECC_Visibility, CollisionParams);
		}
	}
	return false;
}

void AUTCTFScoring::ScoreKill(AController* Killer, AController* Victim, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
	AUTPlayerState* VictimPS = Victim ? Cast<AUTPlayerState>(Victim->PlayerState) : NULL;
	AUTPlayerState* KillerPS = Killer ? Cast<AUTPlayerState>(Killer->PlayerState) : NULL;
	if (VictimPS && KillerPS && (VictimPS != KillerPS) && CTFGameState && !CTFGameState->OnSameTeam(Killer, Victim))
	{
		uint32 Points = BaseKillScore;
		if (VictimPS->CarriedObject)
		{
			Points += FlagCarrierKillBonus;
			KillerPS->LastKilledFCTime = GetWorld()->GetTimeSeconds();
			// bonus based on flag hold time
			Points += FMath::Min<int32>(FlagKillHeldBonus*GetTotalHeldTime(VictimPS->CarriedObject) / 2, MaxFlagHeldBonus);
		}
		else if (WasThreateningFlagCarrier(VictimPS, KilledPawn, KillerPS))
		{
			Points += FlagCombatKillBonus;
			CTFGameState->FlagBases[KillerPS->GetTeamNum()]->MyFlag->HolderRescuers.AddUnique(Killer);
		}
		KillerPS->AdjustScore(Points);
	}
	if (VictimPS)
	{
		VictimPS->LastShotFCTime = 0.f;
		VictimPS->LastShotFC = NULL;
	}
}
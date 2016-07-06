// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGameVolume.h"
#include "UTDmgType_Suicide.h"
#include "UTGameState.h"
#include "UTWeaponLocker.h"
#include "UTPlayerState.h"

AUTGameVolume::AUTGameVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VolumeName = FText::GetEmpty();
	TeamIndex = 255;
	bShowOnMinimap = true;
	bIsNoRallyZone = false;
	bIsTeamSafeVolume = false;
}

void AUTGameVolume::ActorEnteredVolume(class AActor* Other)
{
	if (Other && (bIsTeamSafeVolume || bIsNoRallyZone) && (Role == ROLE_Authority))
	{
		AUTCharacter* P = Cast<AUTCharacter>(Other);
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (P != nullptr && GS != nullptr && P->PlayerState != nullptr)
		{
			if (bIsTeamSafeVolume)
			{
				// friendlies are invulnerable, enemies must die
				if (!GS->OnSameTeam(this, P))
				{
					P->TakeDamage(1000.f, FDamageEvent(UUTDmgType_Suicide::StaticClass()), nullptr, this);
				}
				else
				{
					P->bDamageHurtsHealth = false;
				}
			}
			else if (bIsNoRallyZone && P->GetCarriedObject() && (GetWorld()->GetTimeSeconds() - FMath::Max(GS->LastEnemyEnteringBaseTime, GS->LastEnteringEnemyBaseTime) > 15.f))
			{
				if ((GetWorld()->GetTimeSeconds() - GS->LastEnteringEnemyBaseTime > 15.f) && Cast<AUTPlayerState>(P->PlayerState))
				{
					((AUTPlayerState *)(P->PlayerState))->AnnounceStatus(StatusMessage::ImGoingIn);
					GS->LastEnteringEnemyBaseTime = GetWorld()->GetTimeSeconds();
				}
				if (GetWorld()->GetTimeSeconds() - GS->LastEnemyEnteringBaseTime > 15.f)
				{
					for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
					{
						AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
						if (PC && !GS->OnSameTeam(P, PC) && PC->GetPawn() && PC->UTPlayerState && PC->LineOfSightTo(P))
						{
							PC->UTPlayerState->AnnounceStatus(StatusMessage::BaseUnderAttack);
							GS->LastEnemyEnteringBaseTime = GetWorld()->GetTimeSeconds();
							break;
						}
					}
				}
			}
		}
	}

	if (!VolumeName.IsEmpty())
	{
		AUTCharacter* P = Cast<AUTCharacter>(Other);
		if (P != nullptr)
		{
			//P->LastKnownLocation = this;
			AUTPlayerState* PS = Cast<AUTPlayerState>(P->PlayerState);
			if (PS != nullptr)
			{
				PS->LastKnownLocation = this;
			}
		}
	}
}

void AUTGameVolume::ActorLeavingVolume(class AActor* Other)
{
	if (Other && bIsTeamSafeVolume)
	{
		AUTCharacter* UTCharacter = Cast<AUTCharacter>(Other);
		if (UTCharacter)
		{
			UTCharacter->bDamageHurtsHealth = true;
			if ((Role == ROLE_Authority) && UTCharacter->GetController() && TeamLocker)
			{
				TeamLocker->ProcessTouch(UTCharacter);
			}
		}
	}
}

void AUTGameVolume::SetTeamForSideSwap_Implementation(uint8 NewTeamNum)
{
	TeamIndex = NewTeamNum;
}






// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTKMGameMode.h"
#include "UTFirstBloodMessage.h"
#include "UTGameMessage.h"
#include "UTHUD_KM.h"
#include "UTCharacter.h"
#include "UTPickupCrown.h"

AUTKMGameMode::AUTKMGameMode(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	HUDClass = AUTHUD_KM::StaticClass();
	DisplayName = NSLOCTEXT("UTGameMode", "KM", "King Maker");
}

void AUTKMGameMode::ScoreKill(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
	if (Other == NULL) return;	

	// Save the victim's player state for later
	AUTPlayerState* OtherPlayerState = Cast<AUTPlayerState>(Other->PlayerState);

	UE_LOG(UT,Log,TEXT("ScoreKill: %s (%i) %s"), *OtherPlayerState->PlayerName, OtherPlayerState->bSpecialPlayer, Killer && Killer->PlayerState ? *Killer->PlayerState->PlayerName : TEXT("NONE"));

	// Check to see if there was an actual killer...
	if (Killer != NULL && Killer != Other)
	{
		// We only care if someone kills the kings.
		AUTPlayerState* KillerPlayerState = Cast<AUTPlayerState>(Killer->PlayerState);

		// Special case if this is the first blood.  If it is, then the killer becomes the new king
		if (!bFirstBloodOccurred)
		{
			UE_LOG(UT,Log,TEXT("FB"));
			BecomeKing(KillerPlayerState);
			BroadcastLocalized(this, UUTFirstBloodMessage::StaticClass(), 0, KillerPlayerState, NULL, NULL);
			bFirstBloodOccurred = true;
			return;
		}
	
		if (OtherPlayerState && OtherPlayerState->bSpecialPlayer)
		{
			KingHasBeenKilled(OtherPlayerState, KilledPawn, KillerPlayerState);

			// We killed a king.  So we score points.
			KillerPlayerState->AdjustScore(+1);
			KillerPlayerState->IncrementKills(DamageType, true);
			FindAndMarkHighScorer();
			CheckScore(KillerPlayerState);
		}
		else
		{
			// We have killed someone so we get some of their health.  The king gets more
			AUTCharacter* UTCharacter = Cast<AUTCharacter>(Killer->GetPawn());
			if (UTCharacter && UTCharacter->Health < 100)
			{
				UTCharacter->Health = FMath::Min<int32>(UTCharacter->Health + (KillerPlayerState->bSpecialPlayer ? 25 : 10), 100);
			}
			KillerPlayerState->IncrementKills(DamageType, true);

			// If we are the king, we get credit for our kills...

			if (KillerPlayerState->bSpecialPlayer)
			{
				KillerPlayerState->AdjustScore(+1);
			}
		}

	}
	else if (OtherPlayerState->bSpecialPlayer)	// Special players handle suicide and environmental differently...
	{
		KingHasBeenKilled(OtherPlayerState, KilledPawn, NULL);
	}

}

void AUTKMGameMode::BecomeKing(AUTPlayerState* KingPlayerState)
{
	if (KingPlayerState)
	{
		//  Announce the new king...
		BroadcastLocalized(this, UUTGameMessage::StaticClass(), 11, KingPlayerState, NULL, NULL);
		KingPlayerState->bSpecialPlayer = true;
		KingPlayerState->SetOverrideHatClass(CrownClassName);
	}
}

void AUTKMGameMode::KingHasBeenKilled(AUTPlayerState* KingPlayerState, APawn* KingPawn, AUTPlayerState* KillerPlayerState)
{
	KingPlayerState->bSpecialPlayer = false;
	KingPlayerState->SetOverrideHatClass(TEXT(""));

	FActorSpawnParameters Params;
	Params.bNoCollisionFail = true;
	AUTPickupCrown* CrownPickup = GetWorld()->SpawnActor<AUTPickupCrown>(AUTPickupCrown::StaticClass(), KingPawn->GetActorLocation() + FVector(0,0,200), FRotator(0,0,0), Params);
	if (CrownPickup)
	{
		CrownPickup->Movement->Velocity = FVector(0,0,350);
	}
}

bool AUTKMGameMode::ValidateHat(AUTPlayerState* HatOwner, const FString& HatClass)
{
	return (HatClass != CrownClassName || HatOwner->bSpecialPlayer);
}
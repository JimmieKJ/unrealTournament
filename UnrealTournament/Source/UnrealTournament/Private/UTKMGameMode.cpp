// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTKMGameMode.h"
#include "UTFirstBloodMessage.h"
#include "UTGameMessage.h"
#include "UTHUD_KM.h"
#include "UTCharacter.h"
#include "UTPickupCrown.h"
#include "UTPickupCoin.h"
#include "UTPickupWeapon.h"

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
		CurrentKing = KingPlayerState;
		KingPlayerState->AdjustCurrency(100);
		//  Announce the new king...
		BroadcastLocalized(this, UUTGameMessage::StaticClass(), 11, KingPlayerState, NULL, NULL);
		KingPlayerState->bSpecialPlayer = true;
		KingPlayerState->SetOverrideHatClass(CrownClassName);
	}
}

void AUTKMGameMode::KingHasBeenKilled(AUTPlayerState* KingPlayerState, APawn* KingPawn, AUTPlayerState* KillerPlayerState)
{
	CurrentKing = NULL;
	KingPlayerState->bSpecialPlayer = false;
	KingPlayerState->SetOverrideHatClass(TEXT(""));

	SpawnCoin(KingPawn->GetActorLocation(),200);

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

bool AUTKMGameMode::ModifyDamage_Implementation(int32& Damage, FVector& Momentum, APawn* Injured, AController* InstigatedBy, const FHitResult& HitInfo, AActor* DamageCauser, TSubclassOf<UDamageType> DamageType)
{
	if (CurrentKing && Injured && InstigatedBy)
	{
		AUTPlayerState* InjuredPlayerState = Cast<AUTPlayerState>(Injured->PlayerState);
		AUTPlayerState* InstigatorPlayerState = Cast<AUTPlayerState>(InstigatedBy->PlayerState);
		if (InjuredPlayerState && InstigatorPlayerState)
		{
			if (!InjuredPlayerState->bSpecialPlayer && !InstigatorPlayerState->bSpecialPlayer)	
			{
				int32 Value = int32( float(FMath::Clamp<int32>(Damage, 0, 100)) * 0.2);
				SpawnCoin(Injured->GetActorLocation(), Value);
				Damage = 0;
				Momentum *= 1.25;
			}
		}
	}

	return Super::ModifyDamage_Implementation(Damage, Momentum, Injured, InstigatedBy, HitInfo, DamageCauser, DamageType);
}

void AUTKMGameMode::SpawnCoin(FVector Location, float Value)
{
	FActorSpawnParameters Params;
	Params.bNoCollisionFail = true;

	AUTPickupCoin* Coin = GetWorld()->SpawnActor<AUTPickupCoin>(AUTPickupCoin::StaticClass(), Location + FVector(0,0,200), FRotator(0,0,0), Params);
	if (Coin)
	{
		Coin->SetValue(Value);

		float RandX = FMath::FRandRange(-400,400);
		float RandY = FMath::FRandRange(-400,400);
		float RandZ = FMath::FRandRange(275,400);
		Coin->Movement->Velocity = FVector(RandX, RandY, RandZ);

		//Coin->Movement->Velocity = FVector(0,0,350);
	}
}

bool AUTKMGameMode::PlayerCanAltRestart_Implementation( APlayerController* Player )
{
	AUTPlayerController* PlayerController = Cast<AUTPlayerController>(Player);
	if (PlayerController)
	{
		PlayerController->ClientOpenLoadout(false);
		return false;
	}
	return PlayerCanRestart(Player);
}

void AUTKMGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	for (TActorIterator<AUTPickupWeapon> It(GetWorld()); It; ++It)
	{
		It->Destroy();

		// Spawn coin here..
	}

}
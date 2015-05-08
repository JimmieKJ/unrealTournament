// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTKMGameMode.generated.h"

UCLASS(Config=Game)
class UNREALTOURNAMENT_API AUTKMGameMode : public AUTGameMode
{
	GENERATED_UCLASS_BODY()

	void ScoreKill(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType);

	UPROPERTY(Config)
	FString CrownClassName;

	virtual bool ValidateHat(AUTPlayerState* HatOwner, const FString& HatClass);

	void BecomeKing(AUTPlayerState* KingPlayerState);
	void KingHasBeenKilled(AUTPlayerState* KingPlayerState, APawn* KingPawn, AUTPlayerState* KillerPlayerState);

	virtual bool ModifyDamage_Implementation(int32& Damage, FVector& Momentum, APawn* Injured, AController* InstigatedBy, const FHitResult& HitInfo, AActor* DamageCauser, TSubclassOf<UDamageType> DamageType);

	bool PlayerCanAltRestart_Implementation( APlayerController* Player );
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
protected:
	AUTPlayerState* CurrentKing;

	void SpawnCoin(FVector Location, float Value);

};




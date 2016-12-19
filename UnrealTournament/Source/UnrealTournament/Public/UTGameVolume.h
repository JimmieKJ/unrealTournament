// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTATypes.h"
#include "UTResetInterface.h"
#include "UTGameVolume.generated.h"

/**
* Type of physics volume that has UT specific gameplay effects/data.
*/

UCLASS(BlueprintType)
class UNREALTOURNAMENT_API AUTGameVolume : public APhysicsVolume, public IUTTeamInterface, public IUTResetInterface
{
	GENERATED_UCLASS_BODY()

	/** Displayed volume name, @TODO FIXMESTEVE should be localized. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
		FText VolumeName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
		bool bShowOnMinimap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
		FVector2D MinimapOffset;

	UPROPERTY(BlueprintReadWrite, Category = "Gameplay")
		TArray<class AUTWeaponLocker*> TeamLockers;

	UPROPERTY(BlueprintReadWrite, Category = "Gameplay")
		TArray<class AUTRallyPoint*> RallyPoints;

	/** If team safe volume, associated team members are invulnerable and everyone else is killed entering this volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	bool bIsTeamSafeVolume;

	/** Can't rally to flag carrier in this zone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
		bool bIsNoRallyZone;

	/** Character entering this volume immediately triggers teleporter in this volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
		bool bIsTeleportZone;

	/** Character entering this volume immediately triggers teleporter in this volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
		bool bIsWarningZone;

	/** Alarm sound played if this is bNoRallyZone and enemy flag carrier enters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
		USoundBase* AlarmSound;

	/** Sound played if player is getting health from this volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
		USoundBase* HealthSound;

	UPROPERTY()
		class AUTTeleporter* AssociatedTeleporter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
		uint8 TeamIndex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
		FName VoiceLinesSet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
		bool bReportDefenseStatus;

	/** Set when volume is entered for the first time. */
	UPROPERTY(BlueprintReadWrite, Category = "Gameplay")
		bool bHasBeenEntered;

	UPROPERTY(BlueprintReadWrite, Category = "Gameplay")
		bool bHasFCEntry;


	/** minimum interval between non-FC enemy in base warnings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameState)
		float MinEnemyInBaseInterval;

	/** Used to identify unique routes/entries to enemy base.  Default -1, inner base 0, entries each have own value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
		int32 RouteID;

	virtual void ActorEnteredVolume(class AActor* Other) override;
	virtual void ActorLeavingVolume(class AActor* Other) override;

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "GetTeamNum"))
		uint8 ScriptGetTeamNum();

	/** return team number the object is on, or 255 for no team */
	virtual uint8 GetTeamNum() const { return TeamIndex; };

	virtual void SetTeamForSideSwap_Implementation(uint8 NewTeamNum) override;
	virtual void Reset_Implementation() override;
	virtual void PostInitializeComponents() override;

	FTimerHandle HealthTimerHandle;

	virtual void HealthTimer();
};



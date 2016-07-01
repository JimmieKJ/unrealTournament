// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTGameVolume.generated.h"

/**
* Type of physics volume that has UT specific gameplay effects/data.
*/

UCLASS(BlueprintType)
class UNREALTOURNAMENT_API AUTGameVolume : public APhysicsVolume, public IUTTeamInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
		FText VolumeName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
		bool bShowOnMinimap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
		FVector2D MinimapOffset;

	UPROPERTY(BlueprintReadWrite, Category = "Gameplay")
		class AUTWeaponLocker* TeamLocker;

	/** If team safe volume, associated team members are invulnerable and everyone else is killed entering this volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	bool bIsTeamSafeVolume;

	/** Can't rally to flag carrier in this zone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
		bool bIsNoRallyZone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
		uint8 TeamIndex;

	virtual void ActorEnteredVolume(class AActor* Other) override;
	virtual void ActorLeavingVolume(class AActor* Other) override;

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "GetTeamNum"))
		uint8 ScriptGetTeamNum();

	/** return team number the object is on, or 255 for no team */
	virtual uint8 GetTeamNum() const { return TeamIndex; };

	virtual void SetTeamForSideSwap_Implementation(uint8 NewTeamNum) override;
};



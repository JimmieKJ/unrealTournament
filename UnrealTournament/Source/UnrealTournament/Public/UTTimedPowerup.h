// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTTimedPowerup.generated.h"

UCLASS(Blueprintable, Abstract)
class UNREALTOURNAMENT_API AUTTimedPowerup : public AUTInventory
{
	GENERATED_UCLASS_BODY()

	/** time remaining (in defaults, initial lifetime on pickup)
	 * note: only ticks while held (not dropped on the ground)
	 */
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, Category = Powerup)
	float TimeRemaining;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Powerup)
	bool bTimerPaused;

	/** How long this powerup lasts if it is the triggered version. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Powerup)
	float TriggeredTime;

	/** Rate powerup time remaining ticks down when powerup has been dropped */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Powerup)
	float DroppedTickRate;

	/** sound played on an interval for the last few seconds to indicate time is running out */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Effects)
	USoundBase* PowerupFadingSound;

	/** sound played on the current owner when the duration runs out */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Effects)
	USoundBase* PowerupOverSound;

	/** overlay effect added to the player's weapon while this powerup is in effect */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Effects)
	FOverlayEffect OverlayEffect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Effects)
	FOverlayEffect OverlayEffect1P;
	/** DEPRECATED, use OverlayEffect */
	UPROPERTY(VisibleDefaultsOnly, Category = Effects)
	UMaterialInterface* OverlayMaterial;
	/** DEPRECATED, use OverlayEffect1P */
	UPROPERTY(VisibleDefaultsOnly, Category = Effects)
	UMaterialInterface* OverlayMaterial1P;

	UPROPERTY(BlueprintReadOnly, Category = Powerup)
	class AUTDroppedPickup* MyPickup;

	UPROPERTY(BlueprintReadOnly, Category = Powerup)
	float StatCountTime;

	virtual void UpdateStatsCounter(float Amount);

	/**The stat for how long this was held for*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Powerup)
	FName StatsNameTime;

	virtual void AddOverlayMaterials_Implementation(AUTGameState* GS) const override
	{
		if (OverlayEffect.IsValid())
		{
			GS->AddOverlayEffect(OverlayEffect, OverlayEffect1P);
		}
		else
		{
			GS->AddOverlayEffect(FOverlayEffect(OverlayMaterial), FOverlayEffect(OverlayMaterial1P));
		}
	}

	UFUNCTION(Reliable, Client)
	void ClientSetTimeRemaining(float InTimeRemaining);

	FTimerHandle PlayFadingSoundHandle;

	UFUNCTION()
	virtual void PlayFadingSound();

	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void TimeExpired();

	virtual void Tick(float DeltaTime);

	virtual void Destroyed() override;
	virtual void InitAsTriggeredBoost(class AUTCharacter* TriggeringCharacter) override;

	virtual bool StackPickup_Implementation(AUTInventory* ContainedInv);

	virtual void GivenTo(AUTCharacter* NewOwner, bool bAutoActivate) override;
	virtual void Removed() override;
	virtual void InitializeDroppedPickup(AUTDroppedPickup* Pickup) override;

	virtual void DrawInventoryHUD_Implementation(UUTHUDWidget* Widget, FVector2D Pos, FVector2D Size) override;

	virtual float DetourWeight_Implementation(APawn* Asker, AActor* Pickup, float PathDistance) const
	{
		return BotDesireability(Asker, Asker->Controller, Pickup, PathDistance);
	}

public:
	/** Returns the HUD text to display for this item */
	virtual FText GetHUDText() const override
	{ 
		return FText::AsNumber(int32(TimeRemaining)); 
	}

	/** Returns the value of this TimedPowerup. Used by the HUD for some display purposes */
	virtual int32 GetHUDValue() const
	{
		return int32(TimeRemaining);
	}

	virtual bool HUDShouldRender_Implementation(UUTHUDWidget* TargetWidget) override;
};

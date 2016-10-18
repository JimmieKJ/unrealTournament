// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTJumpBoots.generated.h"

UCLASS(abstract)
class UNREALTOURNAMENT_API AUTJumpBoots : public AUTInventory
{
	GENERATED_UCLASS_BODY()

	/** number of super jumps allowed before the boots run out */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = JumpBoots)
	int32 NumJumps;

	/** Added to multijump Z speed while equipped */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = JumpBoots)
	float SuperJumpZ;

	/** sets multijump velocity threshold (how close to apex player must be to trigger) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = JumpBoots)
	float MaxMultiJumpZSpeed;

	/** Can an active flag carrier use these jump boots? */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = JumpBoots)
	bool bIsDisabledOnFlagCarrier;

	/** Air control during multijump while equipped */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = JumpBoots)
	float MultiJumpAirControl;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	USoundBase* SuperJumpSound;

	/** effect played on the character when the boots are activated */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	TSubclassOf<class AUTReplicatedEmitter> SuperJumpEffect;

	/** apply/remove the effect from UTOwner */
	virtual void AdjustOwner(bool bRemoveBonus);

	virtual void GivenTo(AUTCharacter* NewOwner, bool bAutoActivate) override;
	virtual void ClientGivenTo_Internal(bool bAutoActivate) override;
	virtual void Removed() override;
	virtual void ClientRemoved_Implementation() override;
	virtual void OwnerEvent_Implementation(FName EventName) override;
	virtual bool StackPickup_Implementation(AUTInventory* ContainedInv) override;
	virtual void DropFrom(const FVector& StartLocation, const FVector& TossVelocity) override;

	virtual float BotDesireability_Implementation(APawn* Asker, AController* RequestOwner, AActor* Pickup, float PathDistance) const;
	virtual float DetourWeight_Implementation(APawn* Asker, AActor* Pickup, float PathDistance) const;

public:
	/** Returns the HUD text to display for this item */
	virtual FText GetHUDText() const override
	{
		return FText::AsNumber(NumJumps);
	}

	virtual bool HUDShouldRender_Implementation(UUTHUDWidget* TargetWidget) override;

};
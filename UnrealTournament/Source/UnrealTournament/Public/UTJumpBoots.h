// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTJumpBoots.generated.h"

UCLASS()
class AUTJumpBoots : public AUTInventory
{
	GENERATED_UCLASS_BODY()

	/** number of super jumps allowed before the boots run out */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = JumpBoots)
	int32 NumJumps;
	/** Added to multijump Z speed while equipped */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = JumpBoots)
	float SuperJumpZ;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	USoundBase* SuperJumpSound;
	/** effect played on the character when the boots are activated */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	TSubclassOf<class AUTReplicatedEmitter> SuperJumpEffect;

	/** apply/remove the effect from UTOwner */
	virtual void AdjustOwner(bool bRemoveBonus);

	virtual void GivenTo(AUTCharacter* NewOwner, bool bAutoActivate) OVERRIDE;
	virtual void ClientGivenTo_Internal(bool bAutoActivate) OVERRIDE;
	virtual void Removed() OVERRIDE;
	virtual void ClientRemoved_Implementation() OVERRIDE;
	virtual void OwnerEvent_Implementation(FName EventName) OVERRIDE;
	virtual bool StackPickup_Implementation(AUTInventory* ContainedInv) OVERRIDE;
};
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFFlag.generated.h"

UCLASS()
class AUTCTFFlag : public AUTCarriedObject
{
	GENERATED_UCLASS_BODY()

	// The mesh for hte flag
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = GameObject)
	TSubobjectPtr<USkeletalMeshComponent> Mesh;

	virtual bool CanBePickedUpBy(AUTCharacter* Character);
	virtual void Destroyed() override;

	virtual void DetachFrom(USkeletalMeshComponent* AttachToMesh);
	virtual void OnObjectStateChanged();
	virtual void OnHolderChanged();

	virtual void SendHome() override;
	virtual void SendHomeWithNotify() override;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void DefaultTimer();

	virtual void Drop(AController* Killer) override;

	/** World time when flag was last dropped. */
	UPROPERTY()
		float FlagDropTime;

	/** Broadcast delayed flag drop announcement. */
	virtual void DelayedDropMessage();
};
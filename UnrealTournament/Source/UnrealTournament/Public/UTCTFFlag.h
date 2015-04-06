// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFFlag.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTCTFFlag : public AUTCarriedObject
{
	GENERATED_UCLASS_BODY()

	// The mesh for the flag
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = GameObject)
	USkeletalMeshComponent* Mesh;

	USkeletalMeshComponent* GetMesh() const
	{
		return Mesh;
	}

	virtual bool CanBePickedUpBy(AUTCharacter* Character);

	virtual void DetachFrom(USkeletalMeshComponent* AttachToMesh);
	virtual void OnObjectStateChanged();

	virtual void SendHome() override;

	FTimerHandle SendHomeWithNotifyHandle;
	virtual void SendHomeWithNotify() override;

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void Drop(AController* Killer) override;

	/** World time when flag was last dropped. */
	UPROPERTY()
		float FlagDropTime;

	/** Broadcast delayed flag drop announcement. */
	virtual void DelayedDropMessage();
};
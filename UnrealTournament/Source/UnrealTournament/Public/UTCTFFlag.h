// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFFlag.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTCTFFlag : public AUTCarriedObject
{
	GENERATED_UCLASS_BODY()

	// Flag mesh scaling when not held
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameObject)
	float FlagWorldScale;

	// Flag mesh scaling when held
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameObject)
		float FlagHeldScale;

	/** How much to blend in cloth when flag is home. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Flag)
		float ClothBlendHome;

	/** How much to blend in cloth when flag is held. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Flag)
		float ClothBlendHeld;

	// The mesh for the flag
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Flag)
	USkeletalMeshComponent* Mesh;

	USkeletalMeshComponent* GetMesh() const
	{
		return Mesh;
	}

	virtual bool CanBePickedUpBy(AUTCharacter* Character);

	virtual void DetachFrom(USkeletalMeshComponent* AttachToMesh) override;
	virtual void AttachTo(USkeletalMeshComponent* AttachToMesh) override;
	virtual void OnObjectStateChanged();

	FTimerHandle SendHomeWithNotifyHandle;
	virtual void SendHomeWithNotify() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Drop(AController* Killer) override;
	virtual void MoveToHome() override;


	/** World time when flag was last dropped. */
	UPROPERTY()
		float FlagDropTime;

	/** Broadcast delayed flag drop announcement. */
	virtual void DelayedDropMessage();

	virtual void PostInitializeComponents() override;
};
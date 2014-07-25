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
	virtual void Score(FName Reason);
	virtual void Destroyed() override;

	virtual void DetachFrom(USkeletalMeshComponent* AttachToMesh);
	virtual void OnObjectStateChanged();

	virtual void AutoReturn();

	virtual void OnConstruction(const FTransform& Transform) override;
};
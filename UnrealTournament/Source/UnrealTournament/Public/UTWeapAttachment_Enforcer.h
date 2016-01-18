// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTWeaponAttachment.h"
#include "UTWeapAttachment_Enforcer.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTWeapAttachment_Enforcer : public AUTWeaponAttachment
{
	GENERATED_UCLASS_BODY()

public:

	/** third person left hand mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	USkeletalMeshComponent* LeftMesh;
	UPROPERTY()
	USkeletalMeshComponent* LeftOverlayMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName LeftAttachSocket;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FVector LeftAttachOffset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	int32 BurstSize;

	UPROPERTY()
	uint32 AlternateCount;

	virtual void BeginPlay() override;
	virtual void AttachToOwnerNative() override;
	virtual void PlayFiringEffects() override;
	virtual void StopFiringEffects(bool bIgnoreCurrentMode) override;
	virtual void UpdateOverlays() override;
	virtual void SetSkin(UMaterialInterface* NewSkin) override;
};

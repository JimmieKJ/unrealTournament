// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTWeaponAttachment.h"
#include "UTWeapAttachment_LinkGun.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTWeapAttachment_LinkGun : public AUTWeaponAttachment
{
	GENERATED_BODY()
public:
	AUTWeapAttachment_LinkGun(const FObjectInitializer& OI)
	: Super(OI)
	{
		PrimaryActorTick.bCanEverTick = true;
		PrimaryActorTick.bStartWithTickEnabled = true;
	}
protected:
	/** simulated pulse target, set in FiringExtraUpdated() */
	UPROPERTY(BlueprintReadOnly)
	AActor* PulseTarget;

	float LastBeamPulseTime;
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	UParticleSystem* PulseSuccessEffect;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LinkGun)
	UParticleSystem* PulseFailEffect;

	virtual void FiringExtraUpdated() override;
	virtual void Tick(float DeltaTime) override;
};

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTWeaponAttachment.h"
#include "UTWeapAttachment_LinkGun.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTWeapAttachment_LinkGun : public AUTWeaponAttachment
{
	GENERATED_BODY()

	AUTWeapAttachment_LinkGun(const FObjectInitializer& OI)
	: Super(OI)
	{
		PrimaryActorTick.bCanEverTick = true;
		PrimaryActorTick.bStartWithTickEnabled = true;
	}

	float LastBeamPulseTime;

	virtual void PlayFiringEffects() override;
	virtual void FiringExtraUpdated() override;
	virtual void Tick(float DeltaTime) override;
};

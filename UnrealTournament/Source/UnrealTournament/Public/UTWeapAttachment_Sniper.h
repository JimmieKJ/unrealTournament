// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponAttachment.h"
#include "Particles/ParticleSystemComponent.h"
#include "UTWeapAttachment_Sniper.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API AUTWeapAttachment_Sniper : public AUTWeaponAttachment
{
	GENERATED_UCLASS_BODY()

	AUTWeapAttachment_Sniper(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}
};

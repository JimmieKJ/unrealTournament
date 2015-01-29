// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTWeaponAttachment.h"
#include "UTWeapAttachment_LinkGun.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTWeapAttachment_LinkGun : public AUTWeaponAttachment
{
	GENERATED_BODY()

	virtual void PlayFiringEffects() override;
};

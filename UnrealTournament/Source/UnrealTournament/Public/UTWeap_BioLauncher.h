// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTWeapon.h"
#include "UTWeap_RocketLauncher.h"
#include "UTWeap_BioLauncher.generated.h"

UCLASS(abstract)
class UNREALTOURNAMENT_API AUTWeap_BioLauncher : public AUTWeap_RocketLauncher
{
	GENERATED_BODY()

public:
	AUTWeap_BioLauncher();
};
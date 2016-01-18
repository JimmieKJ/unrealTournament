// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTNeutralFlag.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTNeutralFlag : public AUTCTFFlag
{
	GENERATED_UCLASS_BODY()

	virtual bool CanBePickedUpBy(AUTCharacter* Character);
};
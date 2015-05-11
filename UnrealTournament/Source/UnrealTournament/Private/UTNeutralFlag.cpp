// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCarriedObject.h"
#include "UTCTFFlag.h"
#include "UTNeutralFlag.h"

AUTNeutralFlag::AUTNeutralFlag(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

bool AUTNeutralFlag::CanBePickedUpBy(AUTCharacter* Character)
{

	return Super::CanBePickedUpBy(Character);
}

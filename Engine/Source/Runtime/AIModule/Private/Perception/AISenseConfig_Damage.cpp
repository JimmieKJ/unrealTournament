// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISense_Damage.h"

UAISenseConfig_Damage::UAISenseConfig_Damage(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) 
{
	DebugColor = FColor::Red;
}

TSubclassOf<UAISense> UAISenseConfig_Damage::GetSenseImplementation() const
{
	return Implementation;
}

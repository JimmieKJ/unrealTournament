// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "GameFramework/Volume.h"
#include "TriggerVolume.generated.h"

UCLASS()
class ATriggerVolume : public AVolume
{
	GENERATED_UCLASS_BODY()

	// Begin UObject interface.
#if WITH_EDITOR
	virtual void LoadedFromAnotherClass(const FName& OldClassName) override;
#endif // WITH_EDITOR	
	// End UObject interface.
};




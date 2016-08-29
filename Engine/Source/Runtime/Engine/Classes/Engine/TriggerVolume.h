// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "GameFramework/Volume.h"
#include "TriggerVolume.generated.h"

UCLASS()
class ENGINE_API ATriggerVolume : public AVolume
{
	GENERATED_UCLASS_BODY()

	//~ Begin UObject Interface.
#if WITH_EDITOR
	virtual void LoadedFromAnotherClass(const FName& OldClassName) override;
#endif // WITH_EDITOR	
	//~ End UObject Interface.
};




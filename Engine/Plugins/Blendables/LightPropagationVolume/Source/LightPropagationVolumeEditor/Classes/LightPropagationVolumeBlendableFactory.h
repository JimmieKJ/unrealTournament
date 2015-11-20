// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreUObject.h"
#include "Factories/Factory.h"
#include "LightPropagationVolumeBlendableFactory.generated.h"

UCLASS(hidecategories=Object)
class ULightPropagationVolumeBlendableFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// End of UFactory interface
};

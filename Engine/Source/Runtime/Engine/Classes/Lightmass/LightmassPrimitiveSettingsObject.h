// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/** 
 *	Primitive settings for Lightmass
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"

#include "LightmassPrimitiveSettingsObject.generated.h"

UCLASS(hidecategories=Object, editinlinenew, MinimalAPI,collapsecategories)
class ULightmassPrimitiveSettingsObject : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=Lightmass)
	struct FLightmassPrimitiveSettings LightmassSettings;

};


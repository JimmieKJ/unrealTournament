// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetUserData.generated.h"


/**
 * Object that can be subclassed to store custom data on Unreal asset objects.
 */
UCLASS(DefaultToInstanced)
class ENGINE_API UAssetUserData
	: public UObject
{
	GENERATED_UCLASS_BODY()

	/** used for debugging UAssetUserData data in editor */
	virtual void Draw(class FPrimitiveDrawInterface* PDI, const class FSceneView* View) const {}
};

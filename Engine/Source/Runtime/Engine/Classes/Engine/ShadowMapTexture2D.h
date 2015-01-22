// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Texture2D.h"
#include "ShadowMapTexture2D.generated.h"

UCLASS(MinimalAPI)
class UShadowMapTexture2D : public UTexture2D
{
	GENERATED_UCLASS_BODY()
	
	/** Bit-field with shadowmap flags. */
	UPROPERTY()
	TEnumAsByte<enum EShadowMapFlags> ShadowmapFlags;
};

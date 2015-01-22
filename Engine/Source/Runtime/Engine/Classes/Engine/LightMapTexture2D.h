// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * A 2D texture containing lightmap coefficients.
 */

#pragma once
#include "Engine/Texture2D.h"
#include "LightMapTexture2D.generated.h"

/**
 * Bit-field flags that affects storage (e.g. packing, streaming) and other info about a lightmap.
 */
enum ELightMapFlags
{
	LMF_None			= 0,			// No flags
	LMF_Streamed		= 0x00000001,	// Lightmap should be placed in a streaming texture
	LMF_LQLightmap	= 0x00000002,		// Whether this is a low quality lightmap or not
};

UCLASS(MinimalAPI)
class ULightMapTexture2D : public UTexture2D
{
	GENERATED_UCLASS_BODY()

	// Begin UObject interface.
	virtual void Serialize( FArchive& Ar ) override;
	virtual FString GetDesc() override;
	// End UObject interface. 

	bool IsLowQualityLightmap() const
	{
		return (LightmapFlags & LMF_LQLightmap) ? true : false;
	}

	/** Bit-field with lightmap flags. */
	ELightMapFlags LightmapFlags;
};

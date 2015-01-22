// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 *
 * This thumbnail renderer displays a given particle system
 */

#pragma once
#include "ParticleSystemThumbnailRenderer.generated.h"

UCLASS(config=Editor)
class UParticleSystemThumbnailRenderer : public UTextureThumbnailRenderer
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	class UTexture2D* NoImage;

	UPROPERTY()
	class UTexture2D* OutOfDate;


	// Begin UThumbnailRenderer Object
	virtual void GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const override;
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas) override;
	// End UThumbnailRenderer Object

	// UObject implementation
	UNREALED_API virtual void BeginDestroy() override;

private:
	class FParticleSystemThumbnailScene* ThumbnailScene;
};




// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LandscapeLayerInfoObject.generated.h"

UCLASS(MinimalAPI)
class ULandscapeLayerInfoObject : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(VisibleAnywhere, Category=LandscapeLayerInfoObject, AssetRegistrySearchable)
	FName LayerName;

	UPROPERTY(EditAnywhere, Category=LandscapeLayerInfoObject)
	class UPhysicalMaterial* PhysMaterial;

	UPROPERTY(EditAnywhere, Category=LandscapeLayerInfoObject)
	float Hardness;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category=LandscapeLayerInfoObject)
	uint32 bNoWeightBlend:1;
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	// Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
	// End UObject Interface
#endif
};




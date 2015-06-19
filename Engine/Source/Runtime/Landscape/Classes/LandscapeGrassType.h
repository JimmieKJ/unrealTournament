// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LandscapeGrassType.generated.h"

USTRUCT()
struct FGrassVariety
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=Grass)
	class UStaticMesh* GrassMesh;

	/* Instances per 10 square meters. */
	UPROPERTY(EditAnywhere, Category=Grass)
	float GrassDensity;

	/* If true, use a jittered grid sequence for placement, otherwise use a halton sequence. */
	UPROPERTY(EditAnywhere, Category=Grass)
	bool bUseGrid;

	UPROPERTY(EditAnywhere, Category=Grass, meta = (EditCondition = "bUseGrid", UIMin = 0, ClampMin = 0, UIMax = 1, ClampMax = 1))
	float PlacementJitter;

	/* The distance where instances will begin to fade out if using a PerInstanceFadeAmount material node. 0 disables. */
	UPROPERTY(EditAnywhere, Category=Grass)
	int32 StartCullDistance;

	/**
	 * The distance where instances will have completely faded out when using a PerInstanceFadeAmount material node. 0 disables. 
	 * When the entire cluster is beyond this distance, the cluster is completely culled and not rendered at all.
	 */
	UPROPERTY(EditAnywhere, Category = Grass)
	int32 EndCullDistance;

	/** 
	 * Specifies the smallest LOD that will be used for this component.  
	 * If -1 (default), the MinLOD of the static mesh asset will be used instead.
	 */
	UPROPERTY(EditAnywhere, Category = Grass)
	int32 MinLOD;

	UPROPERTY(EditAnywhere, Category = Grass)
	bool RandomRotation;

	UPROPERTY(EditAnywhere, Category = Grass)
	bool AlignToSurface;

	FGrassVariety()
	{
		GrassMesh = nullptr;
		GrassDensity = 400;
		StartCullDistance = 10000.0f;
		EndCullDistance = 10000.0f;
		PlacementJitter = 1.0f;
		MinLOD = -1;
		RandomRotation = true;
		AlignToSurface = true;
		bUseGrid = true;
	}
};

UCLASS(MinimalAPI)
class ULandscapeGrassType : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Grass)
	TArray<FGrassVariety> GrassVarieties;

	UPROPERTY()
	class UStaticMesh* GrassMesh_DEPRECATED;
	UPROPERTY()
	float GrassDensity_DEPRECATED;
	UPROPERTY()
	float PlacementJitter_DEPRECATED;
	UPROPERTY()
	int32 StartCullDistance_DEPRECATED;
	UPROPERTY()
	int32 EndCullDistance_DEPRECATED;
	UPROPERTY()
	bool RandomRotation_DEPRECATED;
	UPROPERTY()
	bool AlignToSurface_DEPRECATED;

	// Begin UObject Interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// End UObject Interface
};




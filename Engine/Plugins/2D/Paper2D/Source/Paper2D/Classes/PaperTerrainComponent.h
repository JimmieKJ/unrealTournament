// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SpriteDrawCall.h"
#include "PaperTerrainComponent.generated.h"

struct FPaperTerrainMaterialPair
{
	TArray<FSpriteDrawCallRecord> Records;
	UMaterialInterface* Material;
};

/**
 * The terrain visualization component for an associated spline component.
 * This takes a 2D terrain material and instances sprite geometry along the spline path.
 */

UCLASS(BlueprintType, Experimental)
class PAPER2D_API UPaperTerrainComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

public:
	/** The terrain material to apply to this component (set of rules for which sprites are used on different surfaces or the interior) */
	UPROPERTY(Category=Sprite, EditAnywhere, BlueprintReadOnly)
	class UPaperTerrainMaterial* TerrainMaterial;

	UPROPERTY(Category = Sprite, EditAnywhere, BlueprintReadOnly)
	bool bClosedSpline;

	UPROPERTY()
	class UPaperTerrainSplineComponent* AssociatedSpline;

	/** Random seed used for choosing which spline meshes to use. */
	UPROPERTY(Category=Sprite, EditAnywhere)
	int32 RandomSeed;

protected:
	/** The color of the terrain (passed to the sprite material as a vertex color) */
	UPROPERTY(Category=Sprite, BlueprintReadOnly, Interp)
	FLinearColor TerrainColor;

	/** Number of steps per spline segment to place in the reparameterization table */
	UPROPERTY(Category=Sprite, EditAnywhere, meta=(ClampMin=4, UIMin=4), AdvancedDisplay)
	int32 ReparamStepsPerSegment;

	/** Collision domain (no collision, 2D (experimental), or 3D) */
	UPROPERTY(Category=Collision, EditAnywhere)
	TEnumAsByte<ESpriteCollisionMode::Type> SpriteCollisionDomain;

	/** The extrusion thickness of collision geometry when using a 3D collision domain */
	UPROPERTY(Category=Collision, EditAnywhere)
	float CollisionThickness;

public:
	// UObject interface
	virtual const UObject* AdditionalStatObject() const override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// End of UObject interface

	// UActorComponent interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	// End of UActorComponent interface

	// UPrimitiveComponent interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual UBodySetup* GetBodySetup() override;
	// End of UPrimitiveComponent interface

	// Set color of the terrain
	UFUNCTION(BlueprintCallable, Category="Sprite")
	void SetTerrainColor(FLinearColor NewColor);

protected:
	void SpawnSegment(const class UPaperSprite* NewSprite, float Position, float HorizontalScale, float NominalWidth, const struct FPaperTerrainMaterialRule* Rule);
	void SpawnFromPoly(const class UPaperSprite* NewSprite, FSpriteDrawCallRecord& FillDrawCall, const FVector2D& TextureSize, const TArray<FVector2D>& SplinePolyVertices2D);

	void OnSplineEdited();

	/** Description of collision */
	UPROPERTY(Transient, DuplicateTransient)
	class UBodySetup* CachedBodySetup;

	TArray<FPaperTerrainMaterialPair> GeneratedSpriteGeometry;
	
	FTransform GetTransformAtDistance(float InDistance) const;
};


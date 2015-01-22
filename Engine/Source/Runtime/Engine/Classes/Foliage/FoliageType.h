// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FoliageType.generated.h"

UENUM()
enum FoliageVertexColorMask
{
	FOLIAGEVERTEXCOLORMASK_Disabled,
	FOLIAGEVERTEXCOLORMASK_Red,
	FOLIAGEVERTEXCOLORMASK_Green,
	FOLIAGEVERTEXCOLORMASK_Blue,
	FOLIAGEVERTEXCOLORMASK_Alpha,
};


UCLASS(hidecategories=Object, editinlinenew, MinimalAPI)
class UFoliageType : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=Painting)
	float Density;

	UPROPERTY(EditAnywhere, Category=Painting)
	float Radius;

	UPROPERTY(EditAnywhere, Category=Painting)
	float ScaleMinX;

	UPROPERTY(EditAnywhere, Category=Painting)
	float ScaleMinY;

	UPROPERTY(EditAnywhere, Category=Painting)
	float ScaleMinZ;

	UPROPERTY(EditAnywhere, Category=Painting)
	float ScaleMaxX;

	UPROPERTY(EditAnywhere, Category=Painting)
	float ScaleMaxY;

	UPROPERTY(EditAnywhere, Category=Painting)
	float ScaleMaxZ;

	UPROPERTY(EditAnywhere, Category=Painting)
	uint32 LockScaleX:1;

	UPROPERTY(EditAnywhere, Category=Painting)
	uint32 LockScaleY:1;

	UPROPERTY(EditAnywhere, Category=Painting)
	uint32 LockScaleZ:1;

	UPROPERTY(EditAnywhere, Category=Painting)
	float AlignMaxAngle;

	UPROPERTY(EditAnywhere, Category=Painting)
	float RandomPitchAngle;

	UPROPERTY(EditAnywhere, Category=Painting)
	float GroundSlope;

	UPROPERTY(EditAnywhere, Category=Painting)
	float HeightMin;

	UPROPERTY(EditAnywhere, Category=Painting)
	float HeightMax;

	UPROPERTY(EditAnywhere, Category=Painting)
	FName LandscapeLayer;

	UPROPERTY(EditAnywhere, Category=Painting)
	uint32 AlignToNormal:1;

	UPROPERTY(EditAnywhere, Category=Painting)
	uint32 RandomYaw:1;

	UPROPERTY(EditAnywhere, Category=Painting)
	uint32 UniformScale:1;

	UPROPERTY(EditAnywhere, Category=Painting)
	float ZOffsetMin;

	UPROPERTY(EditAnywhere, Category=Painting)
	float ZOffsetMax;

	UPROPERTY(EditAnywhere, Category=Painting)
	uint32 CollisionWithWorld:1;

	UPROPERTY(EditAnywhere, Category=Painting)
	FVector CollisionScale;

	UPROPERTY()
	FBoxSphereBounds MeshBounds;

	// X, Y is origin position and Z is radius...
	UPROPERTY()
	FVector LowBoundOriginRadius;

	UPROPERTY(EditAnywhere, Category=Painting)
	TEnumAsByte<enum FoliageVertexColorMask> VertexColorMask;

	UPROPERTY(EditAnywhere, Category=Painting)
	float VertexColorMaskThreshold;

	UPROPERTY(EditAnywhere, Category=Painting)
	uint32 VertexColorMaskInvert:1;

	UPROPERTY()
	float ReapplyDensityAmount;

	UPROPERTY()
	uint32 ReapplyDensity:1;

	UPROPERTY()
	uint32 ReapplyRadius:1;

	UPROPERTY()
	uint32 ReapplyAlignToNormal:1;

	UPROPERTY()
	uint32 ReapplyRandomYaw:1;

	UPROPERTY()
	uint32 ReapplyScaleX:1;

	UPROPERTY()
	uint32 ReapplyScaleY:1;

	UPROPERTY()
	uint32 ReapplyScaleZ:1;

	UPROPERTY()
	uint32 ReapplyRandomPitchAngle:1;

	UPROPERTY()
	uint32 ReapplyGroundSlope:1;

	UPROPERTY()
	uint32 ReapplyHeight:1;

	UPROPERTY()
	uint32 ReapplyLandscapeLayer:1;

	UPROPERTY()
	uint32 ReapplyZOffset:1;

	UPROPERTY()
	uint32 ReapplyCollisionWithWorld:1;

	UPROPERTY()
	uint32 ReapplyVertexColorMask:1;

	/* The distance where instances will begin to fade out if using a PerInstanceFadeAmount material node. 0 disables. */
	UPROPERTY(EditAnywhere, Category=Culling)
	int32 StartCullDistance;

	/**
	 * The distance where instances will have completely faded out when using a PerInstanceFadeAmount material node. 0 disables. 
	 * When the entire cluster is beyond this distance, the cluster is completely culled and not rendered at all.
	 */
	UPROPERTY(EditAnywhere, Category = Culling)
	int32 EndCullDistance;

	UPROPERTY()
	int32 DisplayOrder;

	UPROPERTY()
	uint32 IsSelected:1;

	UPROPERTY()
	uint32 ShowNothing:1;

	UPROPERTY()
	uint32 ShowPaintSettings:1;

	UPROPERTY()
	uint32 ShowInstanceSettings:1;

	/** Controls whether the foliage should cast a shadow or not. **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting)
	uint32 CastShadow:1;

	/** Controls whether the foliage should inject light into the Light Propagation Volume.  This flag is only used if CastShadow is true. **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting)
	uint32 bAffectDynamicIndirectLighting:1;

	/** Controls whether the primitive should affect dynamic distance field lighting methods.  This flag is only used if CastShadow is true. **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting, AdvancedDisplay, meta=(EditCondition="CastShadow"))
	uint32 bAffectDistanceFieldLighting:1;

	/** Controls whether the foliage should cast shadows in the case of non precomputed shadowing.  This flag is only used if CastShadow is true. **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting)
	uint32 bCastDynamicShadow:1;

	/** Whether the foliage should cast a static shadow from shadow casting lights.  This flag is only used if CastShadow is true. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting)
	uint32 bCastStaticShadow:1;

	/** Whether this foliage should cast dynamic shadows as if it were a two sided material. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting)
	uint32 bCastShadowAsTwoSided:1;

	/** Whether the foliage receives decals. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Rendering)
	uint32 bReceivesDecals : 1;
	
	/** Whether to override the lightmap resolution defined in the static mesh. */
	UPROPERTY(BlueprintReadOnly, Category=Lighting)
	uint32 bOverrideLightMapRes:1;

	/** Overrides the lightmap resolution defined in the static mesh. A value of 0 disables static lighting/shadowing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting, meta=(DisplayName="Light Map Resolution", EditCondition="bOverrideLightMapRes"))
	int32 OverriddenLightMapRes;

	/** Custom collision for foliage */
	UPROPERTY(EditAnywhere, Category=Collision, meta=(HideObjectType=true))
	struct FBodyInstance BodyInstance;

	/* Gets the mesh associated with this FoliageType */
	virtual UStaticMesh* GetStaticMesh() PURE_VIRTUAL(UFoliageType::GetStaticMesh, return nullptr; );

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};



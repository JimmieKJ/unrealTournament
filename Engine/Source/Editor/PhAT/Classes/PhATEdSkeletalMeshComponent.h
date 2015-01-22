// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "PhysicsPublic.h"
#include "PhATEdSkeletalMeshComponent.generated.h"


UCLASS()
class UPhATEdSkeletalMeshComponent : public USkeletalMeshComponent
{
	GENERATED_UCLASS_BODY()

	/** Data and methods shared across multiple classes */
	class FPhATSharedData* SharedData;

	// Draw colors

	FColor BoneUnselectedColor;
	FColor BoneSelectedColor;
	FColor ElemSelectedColor;
	FColor ElemSelectedBodyColor;
	FColor NoCollisionColor;
	FColor FixedColor;
	FColor ConstraintBone1Color;
	FColor ConstraintBone2Color;
	FColor HierarchyDrawColor;
	FColor AnimSkelDrawColor;
	float COMRenderSize;
	float InfluenceLineLength;
	FColor InfluenceLineColor;

	// Materials

	UPROPERTY(transient)
	UMaterialInterface* ElemSelectedMaterial;
	UPROPERTY(transient)
	UMaterialInterface* BoneSelectedMaterial;
	UPROPERTY(transient)
	UMaterialInterface* BoneUnselectedMaterial;
	UPROPERTY(transient)
	UMaterialInterface* BoneMaterialHit;
	UPROPERTY(transient)
	UMaterialInterface* BoneNoCollisionMaterial;

	/** Mesh-space matrices showing state of just animation (ie before physics) - useful for debugging! */
	TArray<FTransform> AnimationSpaceBases;


	/** UPrimitiveComponent interface */
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	
	/** Renders non-hitproxy elements for the viewport, this function is called in the Game Thread */
	virtual void Render(const FSceneView* View, class FPrimitiveDrawInterface* PDI);
	
	/** Renders hitproxy elements for the viewport, this function is called in the Game Thread */
	virtual void RenderHitTest(const FSceneView* View,class FPrimitiveDrawInterface* PDI);

	/** Handles most of the rendering logic for this component */
	void RenderAssetTools(const FSceneView* View, class FPrimitiveDrawInterface* PDI, bool bHitTest);

	/** Draw the mesh skeleton using lines. bAnimSkel means draw the animation skeleton */
	void DrawHierarchy(FPrimitiveDrawInterface* PDI, bool bAnimSkel);

	/** Draws a constraint */
	void DrawConstraint(int32 ConstraintIndex, const FSceneView* View, FPrimitiveDrawInterface* PDI, bool bDrawAsPoint);

	/** Draws visual indication of which polys are effected by the currently selected body */
	void DrawCurrentInfluences(FPrimitiveDrawInterface* PDI);

	/** Accessors/helper methods */
	FTransform GetPrimitiveTransform(FTransform& BoneTM, int32 BodyIndex, EKCollisionPrimitiveType PrimType, int32 PrimIndex, float Scale);
	FColor GetPrimitiveColor(int32 BodyIndex, EKCollisionPrimitiveType PrimitiveType, int32 PrimitiveIndex);
	UMaterialInterface* GetPrimitiveMaterial(int32 BodyIndex, EKCollisionPrimitiveType PrimitiveType, int32 PrimitiveIndex, bool bHitTest);

	/** Returns the physics asset for this PhATEd component - note: This hides the implementation in the USkinnedMeshComponent base class */
	class UPhysicsAsset* GetPhysicsAsset() const;
};

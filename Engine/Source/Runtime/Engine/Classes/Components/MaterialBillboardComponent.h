// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "MaterialBillboardComponent.generated.h"


USTRUCT()
struct FMaterialSpriteElement
{
	GENERATED_USTRUCT_BODY()

	/** The material that the sprite is rendered with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialSpriteElement)
	class UMaterialInterface* Material;
	
	/** A curve that maps distance on the X axis to the sprite opacity on the Y axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialSpriteElement)
	UCurveFloat* DistanceToOpacityCurve;
	
	/** Whether the size is defined in screen-space or world-space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialSpriteElement)
	uint32 bSizeIsInScreenSpace:1;

	/** The base width of the sprite, multiplied with the DistanceToSizeCurve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialSpriteElement)
	float BaseSizeX;

	/** The base height of the sprite, multiplied with the DistanceToSizeCurve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialSpriteElement)
	float BaseSizeY;

	/** A curve that maps distance on the X axis to the sprite size on the Y axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialSpriteElement)
	UCurveFloat* DistanceToSizeCurve;

	FMaterialSpriteElement()
		: Material(NULL)
		, DistanceToOpacityCurve(NULL)
		, bSizeIsInScreenSpace(false)
		, BaseSizeX(32)
		, BaseSizeY(32)
		, DistanceToSizeCurve(NULL)
	{
	}

	friend FArchive& operator<<(FArchive& Ar,FMaterialSpriteElement& LODElement);
};

/** 
 * A 2d material that will be rendered always facing the camera.
 */
UCLASS(ClassGroup=Rendering, collapsecategories, hidecategories=(Object,Activation,"Components|Activation",Physics,Collision,Lighting,Mesh,PhysicsVolume), editinlinenew, meta=(BlueprintSpawnableComponent))
class ENGINE_API UMaterialBillboardComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Sprite)
	TArray<struct FMaterialSpriteElement> Elements;

	/** Adds an element to the sprite. */
	UFUNCTION(BlueprintCallable,Category="Rendering|Components|MaterialSprite")
	virtual void AddElement(
		class UMaterialInterface* Material,
		class UCurveFloat* DistanceToOpacityCurve,
		bool bSizeIsInScreenSpace,
		float BaseSizeX,
		float BaseSizeY,
		class UCurveFloat* DistanceToSizeCurve
		);

	// Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual UMaterialInterface* GetMaterial(int32 Index) const override;
	virtual void SetMaterial(int32 ElementIndex, class UMaterialInterface* Material) override;
	// End UPrimitiveComponent Interface
};

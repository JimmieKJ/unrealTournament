// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "PointLightComponent.h"
#include "SpotLightComponent.generated.h"

/**
 * A spot light component emits a directional cone shaped light (Eg a Torch).
 */
UCLASS(Blueprintable, ClassGroup=Lights, hidecategories=Object, editinlinenew, meta=(BlueprintSpawnableComponent))
class ENGINE_API USpotLightComponent : public UPointLightComponent
{
	GENERATED_UCLASS_BODY()

	/** Degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, meta=(UIMin = "1.0", UIMax = "80.0"))
	float InnerConeAngle;

	/** Degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, meta=(UIMin = "1.0", UIMax = "80.0"))
	float OuterConeAngle;

	/** Degrees. */
	UPROPERTY(/*EditAnywhere, BlueprintReadOnly, Category=LightShaft, meta=(UIMin = "1.0", UIMax = "180.0")*/)
	float LightShaftConeAngle;

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetInnerConeAngle(float NewInnerConeAngle);

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetOuterConeAngle(float NewOuterConeAngle);

	// Disable for now
	//UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	//void SetLightShaftConeAngle(float NewLightShaftConeAngle);

	// ULightComponent interface.
	virtual FSphere GetBoundingSphere() const override;
	virtual bool AffectsBounds(const FBoxSphereBounds& InBounds) const override;
	virtual ELightComponentType GetLightType() const override;
	virtual FLightSceneProxy* CreateSceneProxy() const override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};




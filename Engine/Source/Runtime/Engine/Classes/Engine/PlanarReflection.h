// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 * APlanarReflection
 */

#pragma once
#include "SceneCapture.h"
#include "PlanarReflection.generated.h"

UCLASS(hidecategories=(Collision, Material, Attachment, Actor), MinimalAPI)
class APlanarReflection : public ASceneCapture
{
	GENERATED_UCLASS_BODY()

private_subobject:
	/** Planar reflection component. */
	UPROPERTY(Category = SceneCapture, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UPlanarReflectionComponent* PlanarReflectionComponent;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	UBillboardComponent* SpriteComponent;
#endif

public:

	UPROPERTY(EditAnywhere, Category = PlanarReflection)
	bool bShowPreviewPlane;

	//~ Begin AActor Interface
	ENGINE_API virtual void PostActorCreated() override;

#if WITH_EDITOR
	virtual void EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End AActor Interface.

	UFUNCTION(BlueprintCallable, Category="Rendering")
	void OnInterpToggle(bool bEnable);

	/** Returns subobject **/
	ENGINE_API class UPlanarReflectionComponent* GetPlanarReflectionComponent() const
	{
		return PlanarReflectionComponent;
	}

#if WITH_EDITORONLY_DATA
	/** Returns SpriteComponent subobject **/
	ENGINE_API UBillboardComponent* GetSpriteComponent() const { return SpriteComponent; }
#endif
};




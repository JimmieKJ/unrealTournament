// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Camera/CameraTypes.h"
#include "Components/SceneComponent.h"
#include "CameraComponent.generated.h"

/**
  * Represents a camera viewpoint and settings, such as projection type, field of view, and post-process overrides.
  * The default behavior for an actor used as the camera view target is to look for an attached camera component and use its location, rotation, and settings.
  */
UCLASS(HideCategories=(Mobility, Rendering, LOD), ClassGroup=Camera, meta=(BlueprintSpawnableComponent), MinimalAPI)
class UCameraComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	/** The horizontal field of view (in degrees) in perspective mode (ignored in Orthographic mode) */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category=CameraSettings, meta=(UIMin = "5.0", UIMax = "170", ClampMin = "0.001", ClampMax = "360.0"))
	float FieldOfView;

	/** The desired width (in world units) of the orthographic view (ignored in Perspective mode) */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category=CameraSettings)
	float OrthoWidth;

	// Aspect Ratio (Width/Height)
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category=CameraSettings, meta=(ClampMin = "0.1", ClampMax = "100.0", EditCondition="bConstrainAspectRatio"))
	float AspectRatio;

	// If bConstrainAspectRatio is true, black bars will be added if the destination view has a different aspect ratio than this camera requested.
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category=CameraSettings)
	uint32 bConstrainAspectRatio:1;

	/**
	 * If this camera component is placed on a pawn, should it use the view/control rotation of the pawn where possible?
	 * @see APawn::GetViewRotation()
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CameraSettings)
	uint32 bUsePawnControlRotation:1;

	// The type of camera
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category=CameraSettings)
	TEnumAsByte<ECameraProjectionMode::Type> ProjectionMode;

	/** Indicates if PostProcessSettings should be used when using this Camera to view through. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category=CameraSettings, meta=(UIMin = "0.0", UIMax = "1.0"))
	float PostProcessBlendWeight;

	/** Post process settings to use for this camera. Don't forget to check the properties you want to override */
	UPROPERTY(Interp, BlueprintReadWrite, Category=CameraSettings, meta=(ShowOnlyInnerProperties))
	struct FPostProcessSettings PostProcessSettings;

	// UActorComponent interface
	ENGINE_API virtual void OnRegister() override;
	ENGINE_API virtual void OnUnregister() override;
	ENGINE_API virtual void PostLoad() override;
#if WITH_EDITOR
	ENGINE_API virtual void OnComponentDestroyed() override;
	ENGINE_API virtual void CheckForErrors() override;
	// End of UActorComponent interface

	// UObject interface
	ENGINE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// End of UObject interface

	/**
	 * Returns camera's Point of View.
	 * Called by Camera class. Subclass and postprocess to add any effects.
	 */
	UFUNCTION(BlueprintCallable, Category=Camera)
	ENGINE_API virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView);

protected:
#if WITH_EDITORONLY_DATA
	// The frustum component used to show visually where the camera field of view is
	UPROPERTY(transient)
	class UDrawFrustumComponent* DrawFrustum;

	UPROPERTY(transient)
	class UStaticMesh* CameraMesh;

	// The camera mesh to show visually where the camera is placed
	UPROPERTY(transient)
	class UStaticMeshComponent* ProxyMeshComponent;
#endif
public:
#if WITH_EDITORONLY_DATA
	// Refreshes the visual components to match the component state
	ENGINE_API virtual void RefreshVisualRepresentation();


	ENGINE_API void OverrideFrustumColor(FColor OverrideColor);
	ENGINE_API void RestoreFrustumColor();
#endif

public:
	/** DEPRECATED: use bUsePawnControlRotation instead */
	UPROPERTY()
	uint32 bUseControllerViewRotation_DEPRECATED:1;

	/**
	 * DEPRECATED variable: use "bUsePawnControlRotation" instead. Existing code using this value may not behave correctly.
	 *
	 * The correct way to be backwards-compatible for existing saved content using CameraComponents is:
	 *  - add "_DEPRECATED" to any constructor initialization of bUseControllerViewRotation (which becomes bUseControllerViewRotation_DEPRECATED).
	 *	- add initialization of the new "bUsePawnControlRotation" to your desired default value.
	 *	- if there was no previous initialization of bUseControllerViewRotation in code, then you MUST add initialization of bUsePawnControlRotation
	 *    to "true" if you wish to maintain the same behavior as before (since the default has changed).
	 *
	 * This is not a UPROPERTY, with good reason: we don't want to serialize in old values (bUseControllerViewRotation_DEPRECATED handles that).
	 */
	DEPRECATED(4.5, "This variable is deprecated; please see the upgrade notes. It only exists to allow compilation of old projects, and code should stop using it in favor of the new bUsePawnControlRotation.")
	bool bUseControllerViewRotation;
};

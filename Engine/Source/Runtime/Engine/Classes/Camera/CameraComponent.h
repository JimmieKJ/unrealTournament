// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Camera/CameraTypes.h"
#include "Components/SceneComponent.h"
#include "CameraComponent.generated.h"

/**
  * Represents a camera viewpoint and settings, such as projection type, field of view, and post-process overrides.
  * The default behavior for an actor used as the camera view target is to look for an attached camera component and use its location, rotation, and settings.
  */
UCLASS(HideCategories=(Mobility, Rendering, LOD), Blueprintable, ClassGroup=Camera, meta=(BlueprintSpawnableComponent))
class ENGINE_API UCameraComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	/** The horizontal field of view (in degrees) in perspective mode (ignored in Orthographic mode) */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = CameraSettings, meta = (UIMin = "5.0", UIMax = "170", ClampMin = "0.001", ClampMax = "360.0", Units = deg))
	float FieldOfView;
	UFUNCTION(BlueprintCallable, Category = Camera)
	void SetFieldOfView(float InFieldOfView) { FieldOfView = InFieldOfView; }

	/** The desired width (in world units) of the orthographic view (ignored in Perspective mode) */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = CameraSettings)
	float OrthoWidth;
	UFUNCTION(BlueprintCallable, Category = Camera)
	void SetOrthoWidth(float InOrthoWidth) { OrthoWidth = InOrthoWidth; }

	/** The near plane distance of the orthographic view (in world units) */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = CameraSettings)
	float OrthoNearClipPlane;
	UFUNCTION(BlueprintCallable, Category = Camera)
	void SetOrthoNearClipPlane(float InOrthoNearClipPlane) { OrthoNearClipPlane = InOrthoNearClipPlane; }

	/** The far plane distance of the orthographic view (in world units) */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = CameraSettings)
	float OrthoFarClipPlane;
	UFUNCTION(BlueprintCallable, Category = Camera)
	void SetOrthoFarClipPlane(float InOrthoFarClipPlane) { OrthoFarClipPlane = InOrthoFarClipPlane; }

	// Aspect Ratio (Width/Height)
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = CameraSettings, meta = (ClampMin = "0.1", ClampMax = "100.0", EditCondition = "bConstrainAspectRatio"))
	float AspectRatio;
	UFUNCTION(BlueprintCallable, Category = Camera)
	void SetAspectRatio(float InAspectRatio) { AspectRatio = InAspectRatio; }

	// If bConstrainAspectRatio is true, black bars will be added if the destination view has a different aspect ratio than this camera requested.
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = CameraSettings)
	uint32 bConstrainAspectRatio : 1;
	UFUNCTION(BlueprintCallable, Category = Camera)
	void SetConstraintAspectRatio(bool bInConstrainAspectRatio) { bConstrainAspectRatio = bInConstrainAspectRatio; }

	// If true, account for the field of view angle when computing which level of detail to use for meshes.
	UPROPERTY(Interp, EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = CameraSettings)
	uint32 bUseFieldOfViewForLOD : 1;
	UFUNCTION(BlueprintCallable, Category = Camera)
	void SetUseFieldOfViewForLOD(bool bInUseFieldOfViewForLOD) { bUseFieldOfViewForLOD = bInUseFieldOfViewForLOD; }

	/** True if the camera's orientation and position should be locked to the HMD */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CameraSettings)
	uint32 bLockToHmd : 1;

	/**
	 * If this camera component is placed on a pawn, should it use the view/control rotation of the pawn where possible?
	 * @see APawn::GetViewRotation()
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CameraSettings)
	uint32 bUsePawnControlRotation : 1;

protected:
	/** True to enable the additive view offset, for adjusting the view without moving the component. */
	uint32 bUseAdditiveOffset : 1;

public:
	// The type of camera
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = CameraSettings)
	TEnumAsByte<ECameraProjectionMode::Type> ProjectionMode;
	UFUNCTION(BlueprintCallable, Category = Camera)
	void SetProjectionMode(ECameraProjectionMode::Type InProjectionMode) { ProjectionMode = InProjectionMode; }

	/** Indicates if PostProcessSettings should be used when using this Camera to view through. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = PostProcess, meta = (UIMin = "0.0", UIMax = "1.0"))
	float PostProcessBlendWeight;
	UFUNCTION(BlueprintCallable, Category = Camera)
	void SetPostProcessBlendWeight(float InPostProcessBlendWeight) { PostProcessBlendWeight = InPostProcessBlendWeight; }

	/** Post process settings to use for this camera. Don't forget to check the properties you want to override */
	UPROPERTY(Interp, BlueprintReadWrite, Category = PostProcess)
	struct FPostProcessSettings PostProcessSettings;

	// UActorComponent interface
	virtual void OnRegister() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void CheckForErrors() override;
#endif
	// End of UActorComponent interface

	// UObject interface
#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
#endif
	virtual void Serialize(FArchive& Ar) override;
	// End of UObject interface

	/**
	 * Returns camera's Point of View.
	 * Called by Camera class. Subclass and postprocess to add any effects.
	 */
	UFUNCTION(BlueprintCallable, Category = Camera)
	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView);

	/** Adds an Blendable (implements IBlendableInterface) to the array of Blendables (if it doesn't exist) and update the weight */
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void AddOrUpdateBlendable(TScriptInterface<IBlendableInterface> InBlendableObject, float InWeight = 1.0f) { PostProcessSettings.AddBlendable(InBlendableObject, InWeight); }

#if WITH_EDITORONLY_DATA
	virtual void SetCameraMesh(UStaticMesh* Mesh);
#endif

protected:
#if WITH_EDITORONLY_DATA
	// The frustum component used to show visually where the camera field of view is
	class UDrawFrustumComponent* DrawFrustum;

	UPROPERTY(transient)
	class UStaticMesh* CameraMesh;

	// The camera mesh to show visually where the camera is placed
	class UStaticMeshComponent* ProxyMeshComponent;
	
	virtual void ResetProxyMeshTransform();
#endif

	/** An optional extra transform to adjust the final view without moving the component, in the camera's local space */
	FTransform AdditiveOffset;

	/** An optional extra FOV offset to adjust the final view without modifying the component */
	float AdditiveFOVOffset;

	/** Optional extra PostProcessing blends stored for this camera. They are not applied automatically in GetCameraView. */
	TArray<FPostProcessSettings> ExtraPostProcessBlends;
	TArray<float> ExtraPostProcessBlendWeights;

public:

	/** Applies the given additive offset, preserving any existing offset */
	void AddAdditiveOffset(FTransform const& Transform, float FOV);
	/** Removes any additive offset. */
	void ClearAdditiveOffset();

	/** Stores a given PP and weight to be later applied when the final PP is computed. Used for things like in-editor camera animation preview. */
	void AddExtraPostProcessBlend(FPostProcessSettings const& PPSettings, float PPBlendWeight);
	/** Removes any extra PP blends. */
	void ClearExtraPostProcessBlends();
	/** Returns any extra PP blends that were stored. */
	void GetExtraPostProcessBlends(TArray<FPostProcessSettings>& OutSettings, TArray<float>& OutWeights) const;

	/** 
	 * Can be called from external code to notify that this camera was cut to, so it can update 
	 * things like interpolation if necessary.
	 */
	virtual void NotifyCameraCut();

public:
#if WITH_EDITORONLY_DATA
	// Refreshes the visual components to match the component state
	virtual void RefreshVisualRepresentation();

	void OverrideFrustumColor(FColor OverrideColor);
	void RestoreFrustumColor();
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

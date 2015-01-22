// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Camera/CameraTypes.h"
#include "ContentWidget.h"
#include "Viewport.generated.h"

class FPreviewScene;

/**
* Stores the transformation data for the viewport camera
*/
struct UMG_API FUMGViewportCameraTransform
{
public:
	FUMGViewportCameraTransform();

	/** Sets the transform's location */
	void SetLocation(const FVector& Position);

	/** Sets the transform's rotation */
	void SetRotation(const FRotator& Rotation)
	{
		ViewRotation = Rotation;
	}

	/** Sets the location to look at during orbit */
	void SetLookAt(const FVector& InLookAt)
	{
		LookAt = InLookAt;
	}

	/** Set the ortho zoom amount */
	void SetOrthoZoom(float InOrthoZoom)
	{
		OrthoZoom = InOrthoZoom;
	}


	/** @return The transform's location */
	FORCEINLINE const FVector& GetLocation() const { return ViewLocation; }

	/** @return The transform's rotation */
	FORCEINLINE const FRotator& GetRotation() const { return ViewRotation; }

	/** @return The look at point for orbiting */
	FORCEINLINE const FVector& GetLookAt() const { return LookAt; }

	/** @return The ortho zoom amount */
	FORCEINLINE float GetOrthoZoom() const { return OrthoZoom; }

	/**
	* Animates from the current location to the desired location
	*
	* @param InDesiredLocation	The location to transition to
	* @param bInstant			If the desired location should be set instantly rather than transitioned to over time
	*/
	void TransitionToLocation(const FVector& InDesiredLocation, bool bInstant);

	/**
	* Updates any current location transitions
	*
	* @return true if there is currently a transition
	*/
	bool UpdateTransition();

	/**
	* Computes a matrix to use for viewport location and rotation when orbiting
	*/
	FMatrix ComputeOrbitMatrix() const;
private:
	/** Curve for animating between locations */
	TSharedPtr<struct FCurveSequence> TransitionCurve;
	/** Current viewport Position. */
	FVector	ViewLocation;
	/** Current Viewport orientation; valid only for perspective projections. */
	FRotator ViewRotation;
	/** Desired viewport location when animating between two locations */
	FVector	DesiredLocation;
	/** When orbiting, the point we are looking at */
	FVector LookAt;
	/** Viewport start location when animating to another location */
	FVector StartLocation;
	/** Ortho zoom amount */
	float OrthoZoom;
};


class UMG_API FUMGViewportClient : public FCommonViewportClient, public FViewElementDrawer, public FGCObject
{
public:
	FUMGViewportClient(FPreviewScene* InPreviewScene = nullptr);
	virtual ~FUMGViewportClient();

	virtual void AddReferencedObjects(FReferenceCollector & Collector) override;

	virtual void Draw(FViewport* InViewport, FCanvas* Canvas) override;

	virtual void Tick(float InDeltaTime);

	using FViewElementDrawer::Draw;

	virtual UWorld* GetWorld() const override;

	/**
	* @return The scene being rendered in this viewport
	*/
	virtual FSceneInterface* GetScene() const;

	virtual FSceneView* CalcSceneView(FSceneViewFamily* ViewFamily);

	bool IsOrtho() const;

	bool IsPerspective() const;

	bool IsAspectRatioConstrained() const;

	void SetBackgroundColor(FLinearColor InBackgroundColor);
	FLinearColor GetBackgroundColor() const;

	float GetNearClipPlane() const;

	void OverrideNearClipPlane(float InNearPlane);

	float GetFarClipPlaneOverride() const;

	void OverrideFarClipPlane(const float InFarPlane);

	ECameraProjectionMode::Type GetProjectionType() const;

	/** Sets the location of the viewport's camera */
	void SetViewLocation(const FVector& NewLocation)
	{
		ViewTransform.SetLocation(NewLocation);
	}

	/** Sets the location of the viewport's camera */
	void SetViewRotation(const FRotator& NewRotation)
	{
		ViewTransform.SetRotation(NewRotation);
	}

	/**
	* Sets the look at location of the viewports camera for orbit *
	*
	* @param LookAt The new look at location
	* @param bRecalulateView	If true, will recalculate view location and rotation to look at the new point immediatley
	*/
	void SetLookAtLocation(const FVector& LookAt, bool bRecalculateView = false)
	{
		ViewTransform.SetLookAt(LookAt);

		if ( bRecalculateView )
		{
			FMatrix OrbitMatrix = ViewTransform.ComputeOrbitMatrix();
			OrbitMatrix = OrbitMatrix.InverseFast();

			ViewTransform.SetRotation(OrbitMatrix.Rotator());
			ViewTransform.SetLocation(OrbitMatrix.GetOrigin());
		}
	}

	/** Sets ortho zoom amount */
	void SetOrthoZoom(float InOrthoZoom)
	{
		// A zero ortho zoom is not supported and causes NaN/div0 errors
		check(InOrthoZoom != 0);
		ViewTransform.SetOrthoZoom(InOrthoZoom);
	}

	/** @return the current viewport camera location */
	const FVector& GetViewLocation() const
	{
		return ViewTransform.GetLocation();
	}

	/** @return the current viewport camera rotation */
	const FRotator& GetViewRotation() const
	{
		return ViewTransform.GetRotation();
	}

	/** @return the current look at location */
	const FVector& GetLookAtLocation() const
	{
		return ViewTransform.GetLookAt();
	}

	/** @return the current ortho zoom amount */
	float GetOrthoZoom() const
	{
		return ViewTransform.GetOrthoZoom();
	}

	/** @return The number of units per pixel displayed in this viewport */
	float GetOrthoUnitsPerPixel(const FViewport* Viewport) const;

	void SetEngineShowFlags(FEngineShowFlags InEngineShowFlags)
	{
		EngineShowFlags = InEngineShowFlags;
	}

protected:

	/** The scene used for the viewport. Owned externally */
	FPreviewScene* PreviewScene;

	FMinimalViewInfo ViewInfo;

	FLinearColor BackgroundColor;

	/** Viewport camera transform data */
	FUMGViewportCameraTransform ViewTransform;

	FViewport* Viewport;

	/** near plane adjustable for each editor view, if < 0 GNearClippingPlane should be used. */
	float NearPlane;

	/** If > 0, overrides the view's far clipping plane with a plane at the specified distance. */
	float FarPlane;

	/** Viewport's current horizontal field of view (can be modified by locked cameras etc.) */
	float ViewFOV;
	/** Viewport's stored horizontal field of view (saved in ini files). */
	float FOVAngle;

	float AspectRatio;

	/** The viewport's scene view state. */
	FSceneViewStateReference ViewState;

	/** A set of flags that determines visibility for various scene elements. */
	FEngineShowFlags		EngineShowFlags;
};

/**
 * 
 */
UCLASS(Experimental, ClassGroup=UserInterface)
class UMG_API UViewport : public UContentWidget
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, Category=Appearance)
	FLinearColor BackgroundColor;

	UFUNCTION(BlueprintCallable, Category="Viewport")
	UWorld* GetViewportWorld() const;

	UFUNCTION(BlueprintCallable, Category="Camera")
	FVector GetViewLocation() const;

	UFUNCTION(BlueprintCallable, Category="Camera")
	void SetViewLocation(FVector Location);

	UFUNCTION(BlueprintCallable, Category="Camera")
	FRotator GetViewRotation() const;

	UFUNCTION(BlueprintCallable, Category="Camera")
	void SetViewRotation(FRotator Rotation);

	UFUNCTION(BlueprintCallable, Category="Viewport")
	AActor* Spawn(TSubclassOf<AActor> ActorClass);

	// UWidget interface
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FSlateBrush* GetEditorIcon() override;
	virtual const FText GetPaletteCategory() override;
#endif

protected:

	// UPanelWidget
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End UPanelWidget

protected:
	TSharedPtr<class SAutoRefreshViewport> ViewportWidget;

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

	/** Show flags for the engine for this viewport */
	FEngineShowFlags ShowFlags;
};

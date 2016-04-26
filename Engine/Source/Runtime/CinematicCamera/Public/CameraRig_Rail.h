// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CameraRig_Rail.generated.h"

class USceneComponent;
class USplineComponent;
class USplineMeshComponent;

/** 
 * 
 */
UCLASS(Blueprintable)
class CINEMATICCAMERA_API ACameraRig_Rail : public AActor
{
	GENERATED_BODY()
	
public:
	// ctor
	ACameraRig_Rail(const FObjectInitializer& ObjectInitialier);
	
	virtual void Tick(float DeltaTime) override;
	virtual bool ShouldTickIfViewportsOnly() const override;

	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Rail Controls")
	float CurrentPositionOnRail;

#if WITH_EDITOR
	virtual class USceneComponent* GetDefaultAttachComponent() const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
private:
	/** Makes sure all components are arranged properly. Call when something changes that might affect components. */
	void UpdateRailComponents();
	void UpdatePreviewMeshes();
	USplineMeshComponent* CreateSplinePreviewSegment();

 	/** Root component to give the whole actor a transform. */
	UPROPERTY(EditDefaultsOnly, Category = "Rail Components")
 	USceneComponent* TransformComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Rail Components")
	USplineComponent* RailSplineComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Rail Components")
	USceneComponent* RailCameraMount;

	UPROPERTY()
	USplineMeshComponent* PreviewMesh_Rail;

	UPROPERTY()
	TArray<USplineMeshComponent*> PreviewRailMeshSegments;

	UPROPERTY()
	UStaticMesh* PreviewRailStaticMesh;

	UPROPERTY()
	UStaticMeshComponent* PreviewMesh_Mount;
};

// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

//#include "Engine/Scene.h"

#include "CameraRig_Crane.generated.h"

class USceneComponent;

/** 
 * 
 */
UCLASS(Blueprintable)
class CINEMATICCAMERA_API ACameraRig_Crane : public AActor
{
	GENERATED_BODY()
	
public:

	// ctor
	ACameraRig_Crane(const FObjectInitializer& ObjectInitialier);

	virtual void Tick(float DeltaTime) override;
	virtual bool ShouldTickIfViewportsOnly() const override;

	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Crane Controls", meta = (UIMin = "-360", UIMax = "360", Units = deg))
	float CranePitch;
	
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Crane Controls", meta = (UIMin = "-360", UIMax = "360", Units = deg))
	float CraneYaw;
	
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Crane Controls", meta = (ClampMin=0, Units = cm))
	float CraneArmLength;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual class USceneComponent* GetDefaultAttachComponent() const override;
#endif
	
private:

	void UpdatePreviewMeshes();
	void UpdateCraneComponents();

	/** Root component to give the whole actor a transform. */
	UPROPERTY(EditDefaultsOnly, Category = "Crane Components")
	USceneComponent* TransformComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Crane Components")
	USceneComponent* CraneYawControl;

	UPROPERTY(EditDefaultsOnly, Category = "Crane Components")
	USceneComponent* CranePitchControl;

	UPROPERTY(EditDefaultsOnly, Category = "Crane Components")
	USceneComponent* CraneCameraMount;

	UPROPERTY()
	UStaticMeshComponent* PreviewMesh_CraneArm;

	UPROPERTY()
	UStaticMeshComponent* PreviewMesh_CraneBase;

	UPROPERTY()
	UStaticMeshComponent* PreviewMesh_CraneMount;

	UPROPERTY()
	UStaticMeshComponent* PreviewMesh_CraneCounterWeight;
};

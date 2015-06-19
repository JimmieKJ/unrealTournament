// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Components/SceneCaptureComponent.h"
#include "SceneCaptureComponentCube.generated.h"
/**
 *	Used to capture a 'snapshot' of the scene from a 6 planes and feed it to a render target.
 */
UCLASS(hidecategories = (Collision, Object, Physics, SceneComponent), ClassGroup=Rendering, MinimalAPI, editinlinenew, meta = (BlueprintSpawnableComponent))
class USceneCaptureComponentCube : public USceneCaptureComponent
{
	GENERATED_UCLASS_BODY()

	/** Temporary render target that can be used by the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SceneCapture)
	class UTextureRenderTargetCube* TextureTarget;

public:
	// Begin UActorComponent Interface
	virtual void OnRegister() override;
	virtual void SendRenderTransform_Concurrent() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	// End UActorComponent Interface

	// Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	// End UObject Interface

	/** Render the scene to the texture */
	UFUNCTION(BlueprintCallable,Category = "Rendering|SceneCapture")
	void UpdateContent();

	ENGINE_API static void UpdateDeferredCaptures(FSceneInterface* Scene);
};
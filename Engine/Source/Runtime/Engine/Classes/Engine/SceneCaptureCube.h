// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * 
 *
 */

#pragma once
#include "SceneCapture.h"
#include "SceneCaptureCube.generated.h"

UCLASS(hidecategories = (Collision, Material, Attachment, Actor), MinimalAPI)
class ASceneCaptureCube : public ASceneCapture
{
	GENERATED_UCLASS_BODY()

private_subobject:
	/** Scene capture component. */
	DEPRECATED_FORGAME(4.6, "CaptureComponentCube should not be accessed directly, please use GetCaptureComponentCube() function instead. CaptureComponentCube will soon be private and your code will not compile.")
	UPROPERTY(Category = DecalActor, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	class USceneCaptureComponentCube* CaptureComponentCube;

	/** To allow drawing the camera frustum in the editor. */
	DEPRECATED_FORGAME(4.6, "DrawFrustum should not be accessed directly, please use GetDrawFrustum() function instead. DrawFrustum will soon be private and your code will not compile.")
	UPROPERTY()
	class UDrawFrustumComponent* DrawFrustum;

public:

	// Begin AActor interface
	virtual void PostActorCreated() override;
#if WITH_EDITOR
	ENGINE_API virtual void PostEditMove(bool bFinished) override;
#endif
	// End AActor interface.

	/** Used to synchronize the DrawFrustumComponent with the SceneCaptureComponentCube settings. */
	void UpdateDrawFrustum();

	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void OnInterpToggle(bool bEnable);

	/** Returns CaptureComponentCube subobject **/
	ENGINE_API class USceneCaptureComponentCube* GetCaptureComponentCube() const;
	/** Returns DrawFrustum subobject **/
	ENGINE_API class UDrawFrustumComponent* GetDrawFrustum() const;
};




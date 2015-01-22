// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * 
 *
 */

#pragma once
#include "SceneCapture.h"
#include "SceneCapture2D.generated.h"

UCLASS(hidecategories=(Collision, Material, Attachment, Actor), MinimalAPI)
class ASceneCapture2D : public ASceneCapture
{
	GENERATED_UCLASS_BODY()

private_subobject:
	/** Scene capture component. */
	DEPRECATED_FORGAME(4.6, "CaptureComponent2D should not be accessed directly, please use GetCaptureComponent2D() function instead. CaptureComponent2D will soon be private and your code will not compile.")
	UPROPERTY(Category = DecalActor, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	class USceneCaptureComponent2D* CaptureComponent2D;
	/** To allow drawing the camera frustum in the editor. */
	DEPRECATED_FORGAME(4.6, "DrawFrustum should not be accessed directly, please use GetDrawFrustum() function instead. DrawFrustum will soon be private and your code will not compile.")
	UPROPERTY()
	class UDrawFrustumComponent* DrawFrustum;

public:

	// Begin AActor interface
	ENGINE_API virtual void PostActorCreated() override;
	// End AActor interface.

	/** Used to synchronize the DrawFrustumComponent with the SceneCaptureComponent2D settings. */
	void UpdateDrawFrustum();

	UFUNCTION(BlueprintCallable, Category="Rendering")
	void OnInterpToggle(bool bEnable);

	/** Returns CaptureComponent2D subobject **/
	ENGINE_API class USceneCaptureComponent2D* GetCaptureComponent2D() const;
	/** Returns DrawFrustum subobject **/
	ENGINE_API class UDrawFrustumComponent* GetDrawFrustum() const;
};




// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ReflectionCapture.h"
#include "SphereReflectionCapture.generated.h"

class UDrawSphereComponent;

/** 
 *	Actor used to capture the scene for reflection in a sphere shape.
 *	@see https://docs.unrealengine.com/latest/INT/Resources/ContentExamples/Reflections/1_4
 */
UCLASS(hidecategories = (Collision, Attachment, Actor), MinimalAPI)
class ASphereReflectionCapture : public AReflectionCapture
{
	GENERATED_UCLASS_BODY()

private_subobject:
	/** Sphere component used to visualize the capture radius */
	DEPRECATED_FORGAME(4.6, "DrawCaptureRadius should not be accessed directly, please use GetDrawCaptureRadius() function instead. DrawCaptureRadius will soon be private and your code will not compile.")
	UPROPERTY()
	UDrawSphereComponent* DrawCaptureRadius;

public:

#if WITH_EDITOR
	// Begin AActor interface.
	virtual void EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown) override;
	// End AActor interface.
#endif

	/** Returns DrawCaptureRadius subobject **/
	ENGINE_API UDrawSphereComponent* GetDrawCaptureRadius() const;
};




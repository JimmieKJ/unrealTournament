// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysicsPublic.h"

class FPhAT;
class FPhATSharedData;
class SPhATPreviewViewport;


/*-----------------------------------------------------------------------------
   FPhATViewportClient
-----------------------------------------------------------------------------*/

class FPhATEdPreviewViewportClient : public FEditorViewportClient
{
public:
	/** Constructor */
	FPhATEdPreviewViewportClient(TWeakPtr<FPhAT> InPhAT, TSharedPtr<FPhATSharedData> Data, const TSharedRef<SPhATPreviewViewport>& InPhATPreviewViewport);
	~FPhATEdPreviewViewportClient();

	/** FEditorViewportClient interface */
	virtual void DrawCanvas( FViewport& InViewport, FSceneView& View, FCanvas& Canvas ) override;
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool InputKey(FViewport* Viewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed = 1.0f, bool bGamepad = false) override;
	virtual bool InputAxis(FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples = 1, bool bGamepad = false) override;
	virtual void ProcessClick(class FSceneView& View, class HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;
	virtual bool InputWidgetDelta( FViewport* Viewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale ) override;
	virtual void TrackingStarted( const struct FInputEventState& InInputState, bool bIsDragging, bool bNudge ) override;
	virtual void TrackingStopped() override;
	virtual FWidget::EWidgetMode GetWidgetMode() const override;
	virtual FVector GetWidgetLocation() const override;
	virtual FMatrix GetWidgetCoordSystem() const override;
	virtual ECoordSystem GetWidgetCoordSystemSpace() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FSceneInterface* GetScene() const override;
	virtual FLinearColor GetBackgroundColor() const override;

private:
	/** Methods for building the various context menus */
	void OpenBodyMenu();
	void OpenConstraintMenu();
	
	/** Methods for interacting with the asset while the simulation is running */
	void StartManipulating(EAxisList::Type Axis, const FViewportClick& ViewportClick, const FMatrix& WorldToCamera);
	void EndManipulating();

	/** Simulation mouse forces */
	void SimMousePress(FViewport* Viewport, bool bConstrainRotation, FKey Key);
	void SimMouseMove(float DeltaX, float DeltaY);
	void SimMouseRelease();
	void SimMouseWheelUp();
	void SimMouseWheelDown();

	/** Changes the orientation of a constraint */
	void CycleSelectedConstraintOrientation();

	/** Scales a collision body */
	void ModifyPrimitiveSize(int32 BodyIndex, EKCollisionPrimitiveType PrimType, int32 PrimIndex, FVector DeltaSize);

	/** Called when no scene proxy is hit, deselects everything */
	void HitNothing();

	void CycleTransformMode();
private:
	/** Pointer back to the PhysicsAsset editor tool that owns us */
	TWeakPtr<FPhAT> PhATPtr;

	/** Data and methods shared across multiple classes */
	TSharedPtr<FPhATSharedData> SharedData;

	/** Misc consts */
	const float	MinPrimSize;
	const float PhAT_TranslateSpeed;
	const float PhAT_RotateSpeed;
	const float PhAT_LightRotSpeed;
	const float	SimGrabCheckDistance;
	const float	SimHoldDistanceChangeDelta;
	const float	SimMinHoldDistance;
	const float SimGrabMoveSpeed;

	/** Font used for drawing debug text to the viewport */
	UFont* PhATFont;

	/** Simulation mouse forces */
	float SimGrabPush;
	float SimGrabMinPush;
	FVector SimGrabLocation;
	FVector SimGrabX;
	FVector SimGrabY;
	FVector SimGrabZ;

	/** Members used for interacting with the asset while the simulation is running */
	FTransform StartManRelConTM;
	FTransform StartManParentConTM;
	FTransform StartManChildConTM;

	/** Misc members used for input handling */
	bool bAllowedToMoveCamera;

	float DragX;
	float DragY;
};

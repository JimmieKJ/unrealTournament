// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsConstraintComponent.generated.h"

/**
 *	This is effectively a joint that allows you to connect 2 rigid bodies together. You can create different types of joints using the various parameters of this component.
 */
UCLASS(ClassGroup=Physics, MinimalAPI, meta=(BlueprintSpawnableComponent), HideCategories=(Activation,"Components|Activation", Physics, Mobility), ShowCategories=("Physics|Components|PhysicsConstraint"))
class UPhysicsConstraintComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	/** Pointer to first Actor to constrain.  */
	UPROPERTY(EditInstanceOnly, Category=Constraint)
	AActor* ConstraintActor1;

	/** 
	 *	Name of first component property to constrain. If Actor1 is NULL, will look within Owner.
	 *	If this is NULL, will use RootComponent of Actor1
	 */
	UPROPERTY(EditAnywhere, Category=Constraint)
	FConstrainComponentPropName ComponentName1;


	/** Pointer to second Actor to constrain. */
	UPROPERTY(EditInstanceOnly, Category=Constraint)
	AActor* ConstraintActor2;

	/** 
	 *	Name of second component property to constrain. If Actor2 is NULL, will look within Owner. 
	 *	If this is NULL, will use RootComponent of Actor2
	 */
	UPROPERTY(EditAnywhere, Category=Constraint)
	FConstrainComponentPropName ComponentName2;


	/** Allows direct setting of first component to constraint. */
	TWeakObjectPtr<UPrimitiveComponent> OverrideComponent1;

	/** Allows direct setting of second component to constraint. */
	TWeakObjectPtr<UPrimitiveComponent> OverrideComponent2;


	UPROPERTY(instanced)
	class UPhysicsConstraintTemplate* ConstraintSetup_DEPRECATED;

	/** Notification when constraint is broken. */
	UPROPERTY(BlueprintAssignable)
	FConstraintBrokenSignature OnConstraintBroken;

public:
	/** All constraint settings */
	UPROPERTY(EditAnywhere, Category=ConstraintComponent, meta=(ShowOnlyInnerProperties))
	FConstraintInstance			ConstraintInstance;

	//Begin UObject Interface
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//End UObject interface

	//Begin ActorComponent interface
#if WITH_EDITOR
	virtual void CheckForErrors() override;
	virtual void OnRegister() override;
#endif // WITH_EDITOR
	virtual void OnUnregister() override;
	virtual void InitializeComponent() override;
	//End ActorComponent interface

	//~ Begin SceneComponent Interface
#if WITH_EDITOR
	virtual void PostEditComponentMove(bool bFinished) override;
#endif // WITH_EDITOR
	//~ End SceneComponent Interface

	/** Get the body frame. Works without constraint being created */
	ENGINE_API FTransform GetBodyTransform(EConstraintFrame::Type Frame) const;
	
	/** Get body bounding box. Works without constraint being created */
	ENGINE_API FBox GetBodyBox(EConstraintFrame::Type Frame) const;

	/** Initialize the frames and creates constraint */
	void InitComponentConstraint();

	/** Break the constraint */
	void TermComponentConstraint();

	/** Directly specify component to connect. Will update frames based on current position. */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void SetConstrainedComponents(UPrimitiveComponent* Component1, FName BoneName1, UPrimitiveComponent* Component2, FName BoneName2);

	/** Break this constraint */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void BreakConstraint();

	/** Enables/Disables linear position drive 
	 *	
	 *	@param bEnableDriveX	Indicates whether the drive for the X-Axis should be enabled
	 *	@param bEnableDriveY	Indicates whether the drive for the Y-Axis should be enabled
	 *	@param bEnableDriveZ	Indicates whether the drive for the Z-Axis should be enabled
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void SetLinearPositionDrive(bool bEnableDriveX, bool bEnableDriveY, bool bEnableDriveZ);

	/** Enables/Disables linear position drive 
	 *	
	 *	@param bEnableDriveX	Indicates whether the drive for the X-Axis should be enabled
	 *	@param bEnableDriveY	Indicates whether the drive for the Y-Axis should be enabled
	 *	@param bEnableDriveZ	Indicates whether the drive for the Z-Axis should be enabled
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void SetLinearVelocityDrive(bool bEnableDriveX, bool bEnableDriveY, bool bEnableDriveZ);

	/** Enables/Disables angular orientation drive 
	 *	
	 *	@param bEnableSwingDrive	Indicates whether the drive for the swing axis should be enabled
	 *	@param bEnableTwistDrive	Indicates whether the drive for the twist axis should be enabled
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void SetAngularOrientationDrive(bool bEnableSwingDrive, bool bEnableTwistDrive);

	/** Enables/Disables angular velocity drive 
	 *	
	 *	@param bEnableSwingDrive	Indicates whether the drive for the swing axis should be enabled
	 *	@param bEnableTwistDrive	Indicates whether the drive for the twit axis should be enabled
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void SetAngularVelocityDrive(bool bEnableSwingDrive, bool bEnableTwistDrive);

	/** Sets the target position for the linear drive. 
	 *	@param InPosTarget		Target position
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void	SetLinearPositionTarget(const FVector& InPosTarget);
	
	/** Sets the target velocity for the linear drive. 
	 *	@param InVelTarget		Target velocity
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void	SetLinearVelocityTarget(const FVector& InVelTarget);

	/** Sets the drive params for the linear drive. 
	 *	@param InSpring		Spring force for the drive
	 *	@param InDamping	Damping of the drive
	 *	@param InForceLimit	Max force applied by the drive
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void	SetLinearDriveParams(float InSpring, float InDamping, float InForceLimit);

	/** Sets the target orientation for the angular drive. 
	 *	@param InPosTarget		Target orientation
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void	SetAngularOrientationTarget(const FRotator& InPosTarget);

	
	/** Sets the target velocity for the angular drive. 
	 *	@param InVelTarget		Target velocity
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void	SetAngularVelocityTarget(const FVector& InVelTarget);

	/** Sets the drive params for the angular drive. 
	 *	@param InSpring		Spring force for the drive
	 *	@param InDamping	Damping of the drive
	 *	@param InForceLimit	Max force applied by the drive
	 */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsConstraint")
	ENGINE_API void	SetAngularDriveParams(float InSpring, float InDamping, float InForceLimit);


	/** Sets the LinearX Motion Type
	*	@param ConstraintType	New Constraint Type
	*	@param LimitSize		Size of limit
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	ENGINE_API void	SetLinearXLimit(ELinearConstraintMotion ConstraintType, float LimitSize);

	/** Sets the LinearY Motion Type
	*	@param ConstraintType	New Constraint Type
	*	@param LimitSize		Size of limit
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	ENGINE_API void	SetLinearYLimit(ELinearConstraintMotion ConstraintType, float LimitSize);

	/** Sets the LinearZ Motion Type
	*	@param ConstraintType	New Constraint Type
	*	@param LimitSize		Size of limit
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	ENGINE_API void	SetLinearZLimit(ELinearConstraintMotion ConstraintType, float LimitSize);

	/** Sets the Angular Swing1 Motion Type
	*	@param ConstraintType	New Constraint Type
	*	@param Swing1LimitAngle	Size of limit in degrees
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	ENGINE_API void	SetAngularSwing1Limit(EAngularConstraintMotion MotionType, float Swing1LimitAngle);

	/** Sets the Angular Swing2 Motion Type
	*	@param ConstraintType	New Constraint Type
	*	@param Swing2LimitAngle	Size of limit in degrees
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	ENGINE_API void	SetAngularSwing2Limit(EAngularConstraintMotion MotionType, float Swing2LimitAngle);

	/** Sets the Angular Twist Motion Type
	*	@param ConstraintType	New Constraint Type
	*	@param TwistLimitAngle	Size of limit in degrees
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	ENGINE_API void	SetAngularTwistLimit(EAngularConstraintMotion ConstraintType, float TwistLimitAngle);


	/** Gets the current Angular Twist of the constraint */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	ENGINE_API float GetCurrentTwist() const;

	/** Gets the current Swing1 of the constraint */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	ENGINE_API float GetCurrentSwing1() const;

	/** Gets the current Swing2 of the constraint */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	ENGINE_API float GetCurrentSwing2() const;

	/** 
	 *	Update the reference frames held inside the constraint that indicate the joint location in the reference frame 
	 *	of the two connected bodies. You should call this whenever the constraint or either Component moves, or if you change
	 *	the connected Components. This function does nothing though once the joint has been initialized.
	 */
	ENGINE_API void UpdateConstraintFrames();

	// Pass in reference frame in. If the constraint is currently active, this will set its active local pose. Otherwise the change will take affect in InitConstraint. 
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	ENGINE_API void SetConstraintReferenceFrame(EConstraintFrame::Type Frame, const FTransform& RefFrame);
	
	// Pass in reference position in (maintains reference orientation). If the constraint is currently active, this will set its active local pose. Otherwise the change will take affect in InitConstraint.
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	ENGINE_API void SetConstraintReferencePosition(EConstraintFrame::Type Frame, const FVector& RefPosition);
	
	// Pass in reference orientation in (maintains reference position). If the constraint is currently active, this will set its active local pose. Otherwise the change will take affect in InitConstraint.
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	ENGINE_API void SetConstraintReferenceOrientation(EConstraintFrame::Type Frame, const FVector& PriAxis, const FVector& SecAxis);

	// If true, the collision between the two rigid bodies of the constraint will be disabled.
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	ENGINE_API void SetDisableCollision(bool bDisableCollision);
	
	// Retrieve the constraint force most recently applied to maintain this constraint. Returns 0 forces if the constraint is not initialized or broken.
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsConstraint")
	ENGINE_API void GetConstraintForce(FVector& OutLinearForce, FVector& OutAngularForce);

#if WITH_EDITOR
	ENGINE_API void UpdateSpriteTexture();
#endif

protected:

	friend class FConstraintComponentVisualizer;

	/** Get the body instance that we want to constrain to */
	ENGINE_API FBodyInstance* GetBodyInstance(EConstraintFrame::Type Frame) const;

	/** Internal util to get body transform from actor/component name/bone name information */
	FTransform GetBodyTransformInternal(EConstraintFrame::Type Frame, FName InBoneName) const;
	/** Internal util to get body box from actor/component name/bone name information */
	FBox GetBodyBoxInternal(EConstraintFrame::Type Frame, FName InBoneName) const;
	/** Internal util to get component from actor/component name */
	UPrimitiveComponent* GetComponentInternal(EConstraintFrame::Type Frame) const;

	/** Routes the FConstraint callback to the dynamic delegate */
	void OnConstraintBrokenHandler(FConstraintInstance* BrokenConstraint);

	/** Returns the scale of the constraint as it will be passed into the ConstraintInstance*/
	float GetConstraintScale() const;

private:
	/** Wrapper that calls our constraint broken delegate */
	void OnConstraintBrokenWrapper(int32 ConstraintIndex);
};


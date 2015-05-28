// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "PhysicsHandleComponent.generated.h"

namespace physx
{
	class PxD6Joint;
	class PxRigidDynamic;
}

/**
 *	Utility object for moving physics objects around.
 */
UCLASS(collapsecategories, ClassGroup=Physics, hidecategories=Object, MinimalAPI, meta=(BlueprintSpawnableComponent))
class UPhysicsHandleComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

	/** Component we are currently holding */
	UPROPERTY()
	class UPrimitiveComponent* GrabbedComponent;

	/** Name of bone, if we are grabbing a skeletal component */
	FName GrabbedBoneName;

	/** Physics scene index of the body we are grabbing. */
	int32 SceneIndex;

	/** Are we currently constraining the rotation of the grabbed object. */
	uint32 bRotationConstrained:1;

	/** Linear damping of the handle spring. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=PhysicsHandle)
	float LinearDamping;

	/** Linear stiffness of the handle spring */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=PhysicsHandle)
	float LinearStiffness;

	/** Angular stiffness of the handle spring */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=PhysicsHandle)
	float AngularDamping;

	/** Angular stiffness of the handle spring */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=PhysicsHandle)
	float AngularStiffness;

	/** Target transform */
	FTransform TargetTransform;
	/** Current transform */
	FTransform CurrentTransform;

	/** How quickly we interpolate the physics target transform */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=PhysicsHandle)
	float InterpolationSpeed;


protected:
	/** Pointer to PhysX joint used by the handle*/
	physx::PxD6Joint* HandleData;
	/** Pointer to kinematic actor jointed to grabbed object */
	physx::PxRigidDynamic* KinActorData;

	// Begin UActorComponent interface.
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	// End UActorComponent interface.

public:

	/** Grab the specified component */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsHandle")
	ENGINE_API void GrabComponent(class UPrimitiveComponent* Component, FName InBoneName, FVector GrabLocation, bool bConstrainRotation);

	/** Release the currently held component */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsHandle")
	ENGINE_API void ReleaseComponent();

	/** Set the target location */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsHandle")
	ENGINE_API void SetTargetLocation(FVector NewLocation);

	/** Set the target rotation */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsHandle")
	ENGINE_API void SetTargetRotation(FRotator NewRotation);

	/** Set target location and rotation */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsHandle")
	ENGINE_API void SetTargetLocationAndRotation(FVector NewLocation, FRotator NewRotation);

	/** Get the current location and rotation */
	UFUNCTION(BlueprintCallable, Category="Physics|Components|PhysicsHandle")
	ENGINE_API void GetTargetLocationAndRotation(FVector& TargetLocation, FRotator& TargetRotation) const;

	/** Set linear damping */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsHandle")
	ENGINE_API void SetLinearDamping(float NewLinearDamping);

	/** Set linear stiffness */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsHandle")
	ENGINE_API void SetLinearStiffness(float NewLinearStiffness);

	/** Set angular damping */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsHandle")
	ENGINE_API void SetAngularDamping(float NewAngularDamping);

	/** Set angular stiffness */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsHandle")
	ENGINE_API void SetAngularStiffness(float NewAngularStiffness);

	/** Set interpolation speed */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|PhysicsHandle")
	ENGINE_API void SetInterpolationSpeed(float NewInterpolationSpeed);

protected:
	/** Move the kinematic handle to the specified */
	void UpdateHandleTransform(const FTransform& NewTransform);

	/** Update the underlying constraint drive settings from the params in this component */
	void UpdateDriveSettings();
};




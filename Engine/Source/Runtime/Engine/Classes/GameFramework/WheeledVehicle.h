// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GameFramework/Pawn.h"
#include "WheeledVehicle.generated.h"

class FDebugDisplayInfo;

/**
 * WheeledVehicle is the base wheeled vehicle pawn actor.
 * By default it uses UWheeledVehicleMovementComponent4W for its simulation, but this can be overridden by inheriting from the class and modifying its constructor like so:
 * Super(ObjectInitializer.SetDefautSubobjectClass<UMyMovement>(VehicleMovementComponentName))
 * Where UMyMovement is the new movement type that inherits from UWheeledVehicleMovementComponent
 * 
 * @see https://docs.unrealengine.com/latest/INT/Engine/Physics/Vehicles/VehicleUserGuide/
 * @see UWheeledVehicleMovementComponent4W
 */

UCLASS(abstract, config=Game, BlueprintType)
class ENGINE_API AWheeledVehicle : public APawn
{
	GENERATED_UCLASS_BODY()

private_subobject:
	/**  The main skeletal mesh associated with this Vehicle */
	DEPRECATED_FORGAME(4.6, "Mesh should not be accessed directly, please use GetMesh() function instead. Mesh will soon be private and your code will not compile.")
	UPROPERTY(Category = Vehicle, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class USkeletalMeshComponent* Mesh;

	/** vehicle simulation component */
	DEPRECATED_FORGAME(4.6, "VehicleMovement should not be accessed directly, please use GetVehicleMovement() function instead. VehicleMovement will soon be private and your code will not compile.")
	UPROPERTY(Category = Vehicle, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UWheeledVehicleMovementComponent* VehicleMovement;
public:

	/** Name of the MeshComponent. Use this name if you want to prevent creation of the component (with ObjectInitializer.DoNotCreateDefaultSubobject). */
	static FName VehicleMeshComponentName;

	/** Name of the VehicleMovement. Use this name if you want to use a different class (with ObjectInitializer.SetDefaultSubobjectClass). */
	static FName VehicleMovementComponentName;

	/** Util to get the wheeled vehicle movement component */
	class UWheeledVehicleMovementComponent* GetVehicleMovementComponent() const;

	// Begin AActor interface
	virtual void DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;
	// End Actor interface

	/** Returns Mesh subobject **/
	class USkeletalMeshComponent* GetMesh() const;
	/** Returns VehicleMovement subobject **/
	class UWheeledVehicleMovementComponent* GetVehicleMovement() const;
};

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.



#pragma once
#include "RigidBodyBase.h"
#include "PhysicsThruster.generated.h"

/** 
 *	Attach one of these on an object using physics simulation and it will apply a force down the negative-X direction
 *	ie. point X in the direction you want the thrust in.
 */
UCLASS(hideCategories=(Input,Collision,Replication), showCategories=("Input|MouseInput", "Input|TouchInput"), ComponentWrapperClass)
class APhysicsThruster : public ARigidBodyBase
{
	GENERATED_UCLASS_BODY()

private_subobject:
	/** Thruster component */
	DEPRECATED_FORGAME(4.6, "ThrusterComponent should not be accessed directly, please use GetThrusterComponent() function instead. ThrusterComponent will soon be private and your code will not compile.")
	UPROPERTY(Category = Physics, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Activation,Components|Activation", AllowPrivateAccess = "true"))
	class UPhysicsThrusterComponent* ThrusterComponent;

#if WITH_EDITORONLY_DATA

	DEPRECATED_FORGAME(4.6, "ArrowComponent should not be accessed directly, please use GetArrowComponent() function instead. ArrowComponent will soon be private and your code will not compile.")
	UPROPERTY()
	class UArrowComponent* ArrowComponent;

	DEPRECATED_FORGAME(4.6, "SpriteComponent should not be accessed directly, please use GetSpriteComponent() function instead. SpriteComponent will soon be private and your code will not compile.")
	UPROPERTY()
	class UBillboardComponent* SpriteComponent;
#endif

public:
	/** Returns ThrusterComponent subobject **/
	ENGINE_API class UPhysicsThrusterComponent* GetThrusterComponent() const;
#if WITH_EDITORONLY_DATA
	/** Returns ArrowComponent subobject **/
	ENGINE_API class UArrowComponent* GetArrowComponent() const;
	/** Returns SpriteComponent subobject **/
	ENGINE_API class UBillboardComponent* GetSpriteComponent() const;
#endif
};




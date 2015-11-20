// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "PhATSimOptions.generated.h"

UCLASS(hidecategories=Object, config=EditorPerProjectUserSettings)
class UNREALED_API UPhATSimOptions : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Lets you manually control the physics/animation */
	UPROPERTY(EditAnywhere, transient, Category=Anim)
	float PhysicsBlend;

	/** Lets you manually control the physics/animation */
	UPROPERTY(EditAnywhere, transient, Category = Anim)
	bool bUpdateJointsFromAnimation;

	/** Time between poking ragdoll and starting to blend back. */
	UPROPERTY(EditAnywhere, config, Category=Anim)
	float PokePauseTime;

	/** Time taken to blend from physics to animation. */
	UPROPERTY(EditAnywhere, config, Category=Anim)
	float PokeBlendTime;
	
	/** The gap between the floor & physics object (requires restart of PhAT) */
	UPROPERTY(VisibleAnywhere, config, Category=Simulation)
	float FloorGap;
	/** Scale factor for the gravity used in the simulation */
	UPROPERTY(EditAnywhere, config, Category=Simulation)
	float GravScale;

	/** Max FPS for simulation in PhAT. This is helpful for targeting the same FPS as your game. -1 means disabled*/
	UPROPERTY(EditAnywhere, config, Category = Simulation)
	int32 MaxFPS;

	/** Dilate time by scale*/
	UPROPERTY(EditAnywhere, config, Category = Simulation, meta = (UIMin = 0) )
	float TimeDilation;

	/** Linear damping of mouse spring forces */
	UPROPERTY(EditAnywhere, config, Category=MouseSpring)
	float HandleLinearDamping;

	/** Linear stiffness of mouse spring forces */
	UPROPERTY(EditAnywhere, config, Category=MouseSpring)
	float HandleLinearStiffness;

	/** Angular damping of mouse spring forces */
	UPROPERTY(EditAnywhere, config, Category=MouseSpring)
	float HandleAngularDamping;

	/** Angular stiffness of mouse spring forces */
	UPROPERTY(EditAnywhere, config, Category=MouseSpring)
	float HandleAngularStiffness;

	/** How quickly we interpolate the physics target transform for mouse spring forces */
	UPROPERTY(EditAnywhere, config, Category=MouseSpring)
	float InterpolationSpeed;

	/** Strength of the impulse used when poking with left mouse button */
	UPROPERTY(EditAnywhere, config, Category=Poking)
	float PokeStrength;

	/** Whether to draw constraints as points */
	UPROPERTY(EditAnywhere, config, Category=Advanced)
	uint32 bShowConstraintsAsPoints:1;

	/** Whether to draw bone names in the viewport */
	UPROPERTY(EditAnywhere, config, Category=Advanced)
	uint32 bShowNamesInHierarchy:1;

	/** Controls how large constraints are drawn in PhAT */
	UPROPERTY(EditAnywhere, config, Category=Drawing)
	float ConstraintDrawSize;

};




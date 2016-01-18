// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BodySetupEnums.generated.h"

UENUM()
enum ECollisionTraceFlag
{
	/** Keep simple/complex separate for each test. This is the default. */
	CTF_UseDefault UMETA(DisplayName="Default"),
	/** Use simple collision for all collision tests. */
	CTF_UseSimpleAsComplex UMETA(DisplayName="Use Simple Collision As Complex"),
	/** Use complex collision (per poly) for all collision tests. */
	CTF_UseComplexAsSimple UMETA(DisplayName="Use Complex Collision As Simple"),
	CTF_MAX,
};

UENUM()
enum EPhysicsType
{
	/** Follow owner. */
	PhysType_Default UMETA(DisplayName="Default"),	
	/** Do not follow owner, but make kinematic. */
	PhysType_Kinematic	UMETA(DisplayName="Kinematic"),		
	/** Do not follow owner, but simulate. */
	PhysType_Simulated	UMETA(DisplayName="Simulated")	
};

UENUM()
namespace EBodyCollisionResponse
{
	enum Type
	{
		BodyCollision_Enabled UMETA(DisplayName="Enabled"), 
		BodyCollision_Disabled UMETA(DisplayName="Disabled")//, 
		//BodyCollision_Custom UMETA(DisplayName="Custom")
	};
}
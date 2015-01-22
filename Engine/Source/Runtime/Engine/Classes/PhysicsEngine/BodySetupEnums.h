// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BodySetupEnums.generated.h"

UENUM()
enum ECollisionTraceFlag
{
	// default, we keep simple/complex separate for each test
	CTF_UseDefault UMETA(DisplayName="Default"),
	// use simple collision for complex collision test
	CTF_UseSimpleAsComplex UMETA(DisplayName="Use Simple Collision As Complex"),
	// use complex collision (per poly) for simple collision test
	CTF_UseComplexAsSimple UMETA(DisplayName="Use Complex Collision As Simple"),
	CTF_MAX,
};

UENUM()
enum EPhysicsType
{
	// follow owner option
	PhysType_Default UMETA(DisplayName="Default"),	
	// Do not follow owner, but make kinematic
	PhysType_Kinematic	UMETA(DisplayName="Kinematic"),		
	// Do not follow owner, but simulate
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
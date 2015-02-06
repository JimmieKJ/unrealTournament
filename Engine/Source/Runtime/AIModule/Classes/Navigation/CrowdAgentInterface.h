// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AITypes.h"
#include "CrowdAgentInterface.generated.h"

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UCrowdAgentInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class ICrowdAgentInterface
{
	GENERATED_IINTERFACE_BODY()

	/** @return current location of crowd agent */
	virtual FVector GetCrowdAgentLocation() const { return FAISystem::InvalidLocation; }

	/** @return current velocity of crowd agent */
	virtual FVector GetCrowdAgentVelocity() const { return FVector::ZeroVector; }

	/** fills information about agent's collision cylinder */
	virtual void GetCrowdAgentCollisions(float& CylinderRadius, float& CylinderHalfHeight) const {}

	/** @return max speed of crowd agent */
	virtual float GetCrowdAgentMaxSpeed() const { return 0.0f; }
};

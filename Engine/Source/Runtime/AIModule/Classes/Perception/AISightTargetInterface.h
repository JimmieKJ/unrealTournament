// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AISightTargetInterface.generated.h"

UINTERFACE()
class AIMODULE_API UAISightTargetInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class AIMODULE_API IAISightTargetInterface
{
	GENERATED_IINTERFACE_BODY()

	/**	Implementation should check whether from given ObserverLocation
	 *	implementer can be seen. If so OutSeenLocation should contain
	 *	first visible location
	 *  Return sight strength for how well the target is seen.
	 */
	virtual bool CanBeSeenFrom(const FVector& ObserverLocation, FVector& OutSeenLocation, int32& NumberOfLoSChecksPerformed, float& OutSightStrength, const AActor* IgnoreActor = NULL) const
	{ 
		NumberOfLoSChecksPerformed = 0;
		OutSightStrength = 0;
		return false; 
	}

	DEPRECATED(4.8, "This function is deprecated. Please use the other CanBeSeenFrom version.")
	virtual bool CanBeSeenFrom(const FVector& ObserverLocation, FVector& OutSeenLocation, int32& NumberOfLoSChecksPerformed, const AActor* IgnoreActor = NULL) const
	{
		float SightStrength = 1.f;
		return CanBeSeenFrom(ObserverLocation, OutSeenLocation, NumberOfLoSChecksPerformed, SightStrength, IgnoreActor);
	}
};


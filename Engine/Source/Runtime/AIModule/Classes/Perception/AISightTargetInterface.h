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
	 */
	virtual bool CanBeSeenFrom(const FVector& ObserverLocation, FVector& OutSeenLocation, int32& NumberOfLoSChecksPerformed, const AActor* IgnoreActor = NULL) const
	{ 
		NumberOfLoSChecksPerformed = 0;
		return false; 
	}
};


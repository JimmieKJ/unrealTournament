// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "ParticleModuleEventSendToGame.generated.h"

UCLASS(abstract, editinlinenew, hidecategories=Object)
class UParticleModuleEventSendToGame : public UObject
{
	GENERATED_UCLASS_BODY()


	/** This is our function to allow subclasses to "do the event action" **/
	virtual void DoEvent( const FVector& InCollideDirection, const FVector& InHitLocation, const FVector& InHitNormal, const FName& InBoneName ) {}
};


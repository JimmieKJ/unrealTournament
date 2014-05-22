// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTGameplayStatics.generated.h"

UENUM()
enum ESoundReplicationType
{
	SRT_All, // replicate to all in audible range
	SRT_AllButOwner, // replicate to all but the owner of SourceActor
	SRT_IfSourceNotReplicated, // only replicate to clients on which SourceActor does not exist
	SRT_None, // no replication; local only
	SRT_MAX
};

UCLASS(CustomConstructor)
class UUTGameplayStatics : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UUTGameplayStatics(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}

	/** plays a sound with optional replication parameters
	* additionally will check that clients will actually be able to hear the sound (don't replicate if out of sound's audible range)
	* if called on client, always local only
	*/
	UFUNCTION(BlueprintCallable, Category = Sound)
	static void UTPlaySound(UWorld* TheWorld, USoundBase* TheSound, AActor* SourceActor = NULL, ESoundReplicationType RepType = SRT_All, bool bStopWhenOwnerDestroyed = false, const FVector& SoundLoc = FVector::ZeroVector);
};

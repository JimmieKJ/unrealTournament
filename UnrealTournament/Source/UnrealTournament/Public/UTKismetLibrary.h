// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UTKismetLibrary.generated.h"

UCLASS()
class UUTKismetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/** Plays a sound cue on an actor, sound wave may change depending on team affiliation compared to the listener */
	UFUNCTION(BlueprintCallable, Category = "UT", meta = (DefaultToSelf = "SoundTarget"))
	static class UAudioComponent* PlaySoundTeamAdjusted(USoundCue* SoundToPlay, AActor* SoundInstigator, AActor* SoundTarget, bool Attached);

	UFUNCTION(BlueprintCallable, Category = "UT", meta = (DefaultToSelf = "SoundTarget"))
	static void AssignTeamAdjustmentValue(UAudioComponent* AudioComponent, AActor* SoundInstigator);
};
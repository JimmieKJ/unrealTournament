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
	* @param AmpedListener - amplify volume of the sound for this player; used for e.g. making hit sounds louder for player that caused the hit
	*/
	UFUNCTION(BlueprintCallable, Category = Sound, meta = (HidePin = "TheWorld", DefaultToSelf = "SourceActor", AutoCreateRefTerm = "SoundLoc"))
	static void UTPlaySound(UWorld* TheWorld, USoundBase* TheSound, AActor* SourceActor = NULL, ESoundReplicationType RepType = SRT_All, bool bStopWhenOwnerDestroyed = false, const FVector& SoundLoc = FVector::ZeroVector, class AUTPlayerController* AmpedListener = NULL);

	/** Hurt locally authoritative actors within the radius. Uses the Weapon trace channel.
	 * Also allows passing in momentum (instead of using value hardcoded in damage type - allows for gameplay code to scale, e.g. for a charging weapon)
	* @param BaseDamage - The base damage to apply, i.e. the damage at the origin.
	* @param MinimumDamage - Minimum damage (at max radius)
	* @param BaseMomentumMag - The base momentum (impulse) to apply, scaled the same way damage is and oriented from Origin to the surface of the hit object
	* @param Origin - Epicenter of the damage area.
	* @param DamageInnerRadius - Radius of the full damage area, from Origin
	* @param DamageOuterRadius - Radius of the minimum damage area, from Origin
	* @param DamageFalloff - Falloff exponent of damage from DamageInnerRadius to DamageOuterRadius
	* @param DamageTypeClass - Class that describes the damage that was done.
	* @param IgnoreActors - Actors to never hit; these Actors also don't block the trace that makes sure targets aren't behind a wall, etc. DamageCauser is automatically in this list.
	* @param DamageCauser - Actor that actually caused the damage (e.g. the grenade that exploded)
	* @param InstigatedByController - Controller that was responsible for causing this damage (e.g. player who threw the grenade)
	* @return true if damage was applied to at least one actor.
	*/
	UFUNCTION(BlueprintCallable, Category = "Game|Damage", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", AutoCreateRefTerm = "IgnoreActors"))
	static bool UTHurtRadius(UObject* WorldContextObject, float BaseDamage, float MinimumDamage, float BaseMomentumMag, const FVector& Origin, float DamageInnerRadius, float DamageOuterRadius, float DamageFalloff, TSubclassOf<class UDamageType> DamageTypeClass, const TArray<AActor*>& IgnoreActors, AActor* DamageCauser, AController* InstigatedByController);
};

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
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
class UNREALTOURNAMENT_API UUTGameplayStatics : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UUTGameplayStatics(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	/** plays a sound with optional replication parameters
	* additionally will check that clients will actually be able to hear the sound (don't replicate if out of sound's audible range)
	* if called on client, always local only
	* @param AmpedListener - amplify volume of the sound for this player; used for e.g. making hit sounds louder for player that caused the hit
	* @param Instigator - Pawn that caused the sound to be played (if any) - if SourceActor is a Pawn it defaults to that
	* @param bNotifyAI - whether AI can hear this sound (subject to sound radius and bot skill)
	*/
	UFUNCTION(BlueprintCallable, Category = Sound, meta = (HidePin = "TheWorld", DefaultToSelf = "SourceActor", AutoCreateRefTerm = "SoundLoc"))
	static void UTPlaySound(UWorld* TheWorld, USoundBase* TheSound, AActor* SourceActor = NULL, ESoundReplicationType RepType = SRT_All, bool bStopWhenOwnerDestroyed = false, const FVector& SoundLoc = FVector::ZeroVector, class AUTPlayerController* AmpedListener = NULL, APawn* Instigator = NULL, bool bNotifyAI = true);

	/** retrieves gravity; if no location is specified, level default gravity is returned */
	UFUNCTION(BlueprintCallable, Category = World, meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", AutoCreateRefTerm = "TestLoc"))
	static float GetGravityZ(UObject* WorldContextObject, const FVector& TestLoc = FVector::ZeroVector);

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
	* @param FFInstigatedBy (optional) - Controller that gets credit for damage to enemies on the same team as InstigatedByController (including damaging itself)
	*									this is used to grant two way kill credit for mechanics where the opposition is partially responsible for the damage (e.g. blowing up an enemy's projectile in flight)
	* @param FFDamageType (optional) - when FFInstigatedBy is assigned for damage credit, optionally also use this damage type instead of the default (primarily for death message clarity)
	* @param CollisionFreeRadius (optional) - allow damage to be dealt inside this radius even if visibility checks would fail
	* @return true if damage was applied to at least one actor.
	*/
	UFUNCTION(BlueprintCallable, Category = "Game|Damage", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", AutoCreateRefTerm = "IgnoreActors"))
	static bool UTHurtRadius( UObject* WorldContextObject, float BaseDamage, float MinimumDamage, float BaseMomentumMag, const FVector& Origin, float DamageInnerRadius, float DamageOuterRadius, float DamageFalloff,
								TSubclassOf<class UDamageType> DamageTypeClass, const TArray<AActor*>& IgnoreActors, AActor* DamageCauser, AController* InstigatedByController, AController* FFInstigatedBy = NULL, TSubclassOf<UDamageType> FFDamageType = NULL, float CollisionFreeRadius = 0 );

	/** select visible controlled enemy Pawn for which direction from StartLoc is closest to FireDir and within aiming cone/distance constraints
	 * commonly used for autoaim, homing locks, etc
	 * @param AskingC - Controller that is looking for a target; may not be NULL
	 * @param StartLoc - start location of fire (instant hit trace start, projectile spawn loc, etc)
	 * @param FireDir - fire direction
	 * @param MinAim - minimum dot product of directions that can be returned (maximum 0)
	 * @param MaxRange - maximum range to search
	 * @param TargetClass - optional subclass of Pawn to look for; default is all pawns
	 * @param BestAim - if specified, filled with actual dot product of returned target
	 * @param BestDist - if specified, filled with actual distance to returned target
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Game|Targeting")
	static APawn* PickBestAimTarget(AController* AskingC, FVector StartLoc, FVector FireDir, float MinAim, float MaxRange, TSubclassOf<APawn> TargetClass = NULL
#if CPP // hack: UHT doesn't support this (or any 'optional out' type construct)
	, float* BestAim = NULL, float* BestDist = NULL
#endif
	);

	/** returns PlayerController at the specified player index */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Player", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	APlayerController* GetLocalPlayerController(UObject* WorldContextObject, int32 PlayerIndex = 0);

	/** saves the config properties to the .ini file
	 * if you pass a Class, saves the values in its default object
	 * if you pass anything else, the instance's values are saved as its class's new defaults
	 */
	UFUNCTION(BlueprintCallable, Category = "Config")
	static void SaveConfig(UObject* Obj);


	/** Not replicated. Plays a sound cue on an actor, sound wave may change depending on team affiliation compared to the listener */
	UFUNCTION(BlueprintCosmetic, BlueprintCallable, Category = "UT", meta = (DefaultToSelf = "SoundTarget"))
	static class UAudioComponent* PlaySoundTeamAdjusted(USoundCue* SoundToPlay, AActor* SoundInstigator, AActor* SoundTarget, bool Attached);

	UFUNCTION(BlueprintCosmetic, BlueprintCallable, Category = "UT")
	static void AssignTeamAdjustmentValue(UAudioComponent* AudioComponent, AActor* SoundInstigator);

	UFUNCTION(BlueprintCallable, Category = "UT", meta = (WorldContext = "WorldContextObject"))
	static bool HasTokenBeenPickedUpBefore(UObject* WorldContextObject, FName TokenUniqueID);

	/** Token pick up noted in temporary storage, not committed to profile storage until TokenCommit called */
	UFUNCTION(BlueprintCallable, Category = "UT", meta = (WorldContext = "WorldContextObject"))
	static void TokenPickedUp(UObject* WorldContextObject, FName TokenUniqueID);

	/** Remove a token pick up from temporary storage so it won't get committed to profile storage */
	UFUNCTION(BlueprintCallable, Category = "UT", meta = (WorldContext = "WorldContextObject"))
	static void TokenRevoke(UObject* WorldContextObject, FName TokenUniqueID);

	/** Save tokens picked up this level to profile */
	UFUNCTION(BlueprintCallable, Category = "UT", meta = (WorldContext = "WorldContextObject"))
	static void TokensCommit(UObject* WorldContextObject);

	/** Reset tokens picked up this level so they don't get saved to profile */
	UFUNCTION(BlueprintCallable, Category = "UT", meta = (WorldContext = "WorldContextObject"))
	static void TokensReset(UObject* WorldContextObject);
};

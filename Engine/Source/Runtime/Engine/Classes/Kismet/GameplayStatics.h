// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/LatentActionManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Sound/DialogueTypes.h"
#include "GameplayStatics.generated.h"

//
// Forward declarations.
//
class USaveGame;
struct FDialogueContext;
class UParticleSystemComponent;
class UParticleSystem;

UENUM()
namespace ESuggestProjVelocityTraceOption
{
	enum Type
	{
		DoNotTrace,
		TraceFullPath,
		OnlyTraceWhileAsceding,
	};
}

UCLASS()
class ENGINE_API UGameplayStatics : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	// --- Create Object
	UFUNCTION(BlueprintCallable, Category = "Spawning", meta = (BlueprintInternalUseOnly = "true"))
	static UObject* SpawnObject(TSubclassOf<UObject> ObjectClass, UObject* Outer);

	static bool CanSpawnObjectOfClass(TSubclassOf<UObject> ObjectClass, bool bAllowAbstract = false);

	// --- Spawning functions ------------------------------

	/** Spawns an instance of a blueprint, but does not automatically run its construction script.  */
	UFUNCTION(BlueprintCallable, Category="Spawning", meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction = "true", BlueprintInternalUseOnly = "true", DeprecatedFunction, DeprecationMessage="Use BeginSpawningActorFromClass"))
	static class AActor* BeginSpawningActorFromBlueprint(UObject* WorldContextObject, const class UBlueprint* Blueprint, const FTransform& SpawnTransform, bool bNoCollisionFail);

	DEPRECATED(4.9, "This function is deprecated. Please use BeginDeferredActorSpawnFromClass instead.")
	UFUNCTION(BlueprintCallable, Category="Spawning", meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction = "true", BlueprintInternalUseOnly = "true"))
	static class AActor* BeginSpawningActorFromClass(UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform, bool bNoCollisionFail = false, AActor* Owner = nullptr);

	/** Spawns an instance of an actor class, but does not automatically run its construction script.  */
	UFUNCTION(BlueprintCallable, Category = "Spawning", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true", BlueprintInternalUseOnly = "true"))
	static class AActor* BeginDeferredActorSpawnFromClass(UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform, ESpawnActorCollisionHandlingMethod CollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::Undefined, AActor* Owner = nullptr);

	/** 'Finish' spawning an actor.  This will run the construction script. */
	UFUNCTION(BlueprintCallable, Category="Spawning", meta=(UnsafeDuringActorConstruction = "true", BlueprintInternalUseOnly = "true"))
	static class AActor* FinishSpawningActor(class AActor* Actor, const FTransform& SpawnTransform);

	// --- Actor functions ------------------------------

	/** Find the average location (centroid) of an array of Actors */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation")
	static FVector GetActorArrayAverageLocation(const TArray<AActor*>& Actors);

	/** Bind the bounds of an array of Actors */
	UFUNCTION(BlueprintCallable, Category="Collision")
	static void GetActorArrayBounds(const TArray<AActor*>& Actors, bool bOnlyCollidingComponents, FVector& Center, FVector& BoxExtent);

	/** 
	 *	Find all Actors in the world of the specified class. 
	 *	This is a slow operation, use with caution e.g. do not use every frame.
	 *	@param	ActorClass	Class of Actor to find. Must be specified or result array will be empty.
	 *	@param	OutActors	Output array of Actors of the specified class.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities",  meta=(WorldContext="WorldContextObject", DeterminesOutputType="ActorClass", DynamicOutputParam="OutActors"))
	static void GetAllActorsOfClass(UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, TArray<AActor*>& OutActors);

	/** 
	 *	Find all Actors in the world with the specified interface.
	 *	This is a slow operation, use with caution e.g. do not use every frame.
	 *	@param	Interface	Interface to find. Must be specified or result array will be empty.
	 *	@param	OutActors	Output array of Actors of the specified interface.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities",  meta=(WorldContext="WorldContextObject", DeterminesOutputType="Interface", DynamicOutputParam="OutActors"))
	static void GetAllActorsWithInterface(UObject* WorldContextObject, TSubclassOf<UInterface> Interface, TArray<AActor*>& OutActors);

	/**
	*	Find all Actors in the world with the specified tag.
	*	This is a slow operation, use with caution e.g. do not use every frame.
	*	@param	Tag			Tag to find. Must be specified or result array will be empty.
	*	@param	OutActors	Output array of Actors of the specified tag.
	*/
	UFUNCTION(BlueprintCallable, Category="Utilities",  meta=(WorldContext="WorldContextObject"))
	static void GetAllActorsWithTag(UObject* WorldContextObject, FName Tag, TArray<AActor*>& OutActors);

	// --- Player functions ------------------------------

	/** Returns the game instance object  */
	UFUNCTION(BlueprintPure, Category="Game", meta=(WorldContext="WorldContextObject"))
	static class UGameInstance* GetGameInstance(UObject* WorldContextObject);

	/** Returns the player controller at the specified player index */
	UFUNCTION(BlueprintPure, Category="Game", meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
	static class APlayerController* GetPlayerController(UObject* WorldContextObject, int32 PlayerIndex);

	/** Returns the player pawn at the specified player index */
	UFUNCTION(BlueprintPure, Category="Game", meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
	static class APawn* GetPlayerPawn(UObject* WorldContextObject, int32 PlayerIndex);

	/** Returns the player character (NULL if the player pawn doesn't exist OR is not a character) at the specified player index */
	UFUNCTION(BlueprintPure, Category="Game", meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
	static class ACharacter* GetPlayerCharacter(UObject* WorldContextObject, int32 PlayerIndex);

	/** Returns the player's camera manager for the specified player index */
	UFUNCTION(BlueprintPure, Category="Game", meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
	static class APlayerCameraManager* GetPlayerCameraManager(UObject* WorldContextObject, int32 PlayerIndex);

	/** Create a new player for this game.  
	 *  @param ControllerId		The ID of the controller that the should control the newly created player.  A value of -1 specifies to use the next available ID
	 *  @param bSpawnPawn		Whether a pawn should be spawned immediately. If false a pawn will not be created until transition to the next map.
	 */
	UFUNCTION(BlueprintCallable, Category="Game", meta=(WorldContext="WorldContextObject", AdvancedDisplay="2", UnsafeDuringActorConstruction="true"))
	static class APlayerController* CreatePlayer(UObject* WorldContextObject, int32 ControllerId = -1, bool bSpawnPawn = true);

	/** Removes a player from this game.  
	 *  @param Player			The player controller of the player to be removed
	 *  @param bDestroyPawn		Whether the controlled pawn should be deleted as well
	 */
	UFUNCTION(BlueprintCallable, Category="Game", meta=(UnsafeDuringActorConstruction="true"))
	static void RemovePlayer(APlayerController* Player, bool bDestroyPawn);

	/** 
	* Gets what controller ID a Player is using
	* @param Player		The player controller of the player to get the ID of
	* @return			The ID of the passed in player. -1 if there is no controller for the passed in player
	*/
	UFUNCTION(BlueprintCallable, Category="Game", meta=(UnsafeDuringActorConstruction="true"))
	static int32 GetPlayerControllerID(APlayerController* Player);

	/** 
	 * Sets what controller ID a Player should be using
	 * @param Player			The player controller of the player to change the controller ID of
	 * @param ControllerId		The controller ID to assign to this player
	 */
	UFUNCTION(BlueprintCallable, Category="Game", meta=(UnsafeDuringActorConstruction="true"))
	static void SetPlayerControllerID(APlayerController* Player, int32 ControllerId);

	// --- Level Streaming functions ------------------------
	
	/** Stream the level with the LevelName ; Calling again before it finishes has no effect */
	UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", Latent = "", LatentInfo = "LatentInfo"), Category="Game")
	static void LoadStreamLevel(UObject* WorldContextObject, FName LevelName, bool bMakeVisibleAfterLoad, bool bShouldBlockOnLoad, FLatentActionInfo LatentInfo);

	/** Unload a streamed in level */
	UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", Latent = "", LatentInfo = "LatentInfo"), Category="Game")
	static void UnloadStreamLevel(UObject* WorldContextObject, FName LevelName, FLatentActionInfo LatentInfo);
	
	/** Returns level streaming object with specified level package name */
	UFUNCTION(BlueprintPure, meta=(WorldContext="WorldContextObject"), Category="Game")
	static class ULevelStreaming* GetStreamingLevel(UObject* WorldContextObject, FName PackageName);

	/** Flushes level streaming in blocking fashion and returns when all sub-levels are loaded / visible / hidden */
	UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject"), Category = "Game")
	static void FlushLevelStreaming(UObject* WorldContextObject);
		
	/** Cancels all currently queued streaming packages */
	UFUNCTION(BlueprintCallable, Category = "Game")
	static void CancelAsyncLoading();


	/**
	 * Travel to another level
	 *
	 * @param	LevelName			the level to open
	 * @param	bAbsolute			if true options are reset, if false options are carried over from current level
	 * @param	Options				a string of options to use for the travel URL
	 */
	UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", AdvancedDisplay = "2"), Category="Game")
	static void OpenLevel(UObject* WorldContextObject, FName LevelName, bool bAbsolute = true, FString Options = FString(TEXT("")));

	/**
	* Get the name of the currently-open level.
	*
	* @param bRemovePrefixString	remove any streaming- or editor- added prefixes from the level name.
	*/
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "1"), Category = "Game")
	static FString GetCurrentLevelName(UObject* WorldContextObject, bool bRemovePrefixString = true);

	// --- Global functions ------------------------------

	/** Returns the current GameMode or NULL if the GameMode can't be retrieved */
	UFUNCTION(BlueprintPure, Category="Game", meta=(WorldContext="WorldContextObject") )
	static class AGameMode* GetGameMode(UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category="Game", meta=(WorldContext="WorldContextObject") )
	static class AGameState* GetGameState(UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "GetClass", DeterminesOutputType = "Object"), Category="Utilities")
	static class UClass *GetObjectClass(const UObject *Object);

	/**
	 * Gets the current global time dilation.
	 * @return Current time dilation.
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|Time", meta=(WorldContext="WorldContextObject") )
	static float GetGlobalTimeDilation(UObject* WorldContextObject);

	/**
	 * Sets the global time dilation.
	 * @param	TimeDilation	value to set the global time dilation to
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Time", meta=(WorldContext="WorldContextObject") )
	static void SetGlobalTimeDilation(UObject* WorldContextObject, float TimeDilation);

	/**
	 * Sets the game's paused state
	 * @param	bPaused		Whether the game should be paused or not
	 * @return	Whether the game was successfully paused/unpaused
	 */
	UFUNCTION(BlueprintCallable, Category="Game", meta=(WorldContext="WorldContextObject") )
	static bool SetGamePaused(UObject* WorldContextObject, bool bPaused);

	/**
	 * Returns the game's paused state
	 * @return	Whether the game is currently paused or not
	 */
	UFUNCTION(BlueprintPure, Category="Game", meta=(WorldContext="WorldContextObject") )
	static bool IsGamePaused(UObject* WorldContextObject);


	/** Hurt locally authoritative actors within the radius. Will only hit components that block the Visibility channel.
	 * @param BaseDamage - The base damage to apply, i.e. the damage at the origin.
	 * @param Origin - Epicenter of the damage area.
	 * @param DamageRadius - Radius of the damage area, from Origin
	 * @param DamageTypeClass - Class that describes the damage that was done.
	 * @param DamageCauser - Actor that actually caused the damage (e.g. the grenade that exploded).  This actor will not be damaged and it will not block damage.
	 * @param InstigatedByController - Controller that was responsible for causing this damage (e.g. player who threw the grenade)
	 * @param bFullDamage - if true, damage not scaled based on distance from Origin
	 * @param DamagePreventionChannel - Damage will not be applied to victim if there is something between the origin and the victim which blocks traces on this channel
	 * @return true if damage was applied to at least one actor.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Game|Damage", meta=(WorldContext="WorldContextObject", AutoCreateRefTerm="IgnoreActors"))
	static bool ApplyRadialDamage(UObject* WorldContextObject, float BaseDamage, const FVector& Origin, float DamageRadius, TSubclassOf<class UDamageType> DamageTypeClass, const TArray<AActor*>& IgnoreActors, AActor* DamageCauser = NULL, AController* InstigatedByController = NULL, bool bDoFullDamage = false, ECollisionChannel DamagePreventionChannel = ECC_Visibility);
	
	/** Hurt locally authoritative actors within the radius. Will only hit components that block the Visibility channel.
	 * @param BaseDamage - The base damage to apply, i.e. the damage at the origin.
	 * @param Origin - Epicenter of the damage area.
	 * @param DamageInnerRadius - Radius of the full damage area, from Origin
	 * @param DamageOuterRadius - Radius of the minimum damage area, from Origin
	 * @param DamageFalloff - Falloff exponent of damage from DamageInnerRadius to DamageOuterRadius
	 * @param DamageTypeClass - Class that describes the damage that was done.
	 * @param DamageCauser - Actor that actually caused the damage (e.g. the grenade that exploded)
	 * @param InstigatedByController - Controller that was responsible for causing this damage (e.g. player who threw the grenade)
	 * @param bFullDamage - if true, damage not scaled based on distance from Origin
	 * @param DamagePreventionChannel - Damage will not be applied to victim if there is something between the origin and the victim which blocks traces on this channel
	 * @return true if damage was applied to at least one actor.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Game|Damage", meta=(WorldContext="WorldContextObject", AutoCreateRefTerm="IgnoreActors"))
	static bool ApplyRadialDamageWithFalloff(UObject* WorldContextObject, float BaseDamage, float MinimumDamage, const FVector& Origin, float DamageInnerRadius, float DamageOuterRadius, float DamageFalloff, TSubclassOf<class UDamageType> DamageTypeClass, const TArray<AActor*>& IgnoreActors, AActor* DamageCauser = NULL, AController* InstigatedByController = NULL, ECollisionChannel DamagePreventionChannel = ECC_Visibility);
	

	/** Hurts the specified actor with the specified impact.
	 * @param DamagedActor - Actor that will be damaged.
	 * @param BaseDamage - The base damage to apply.
	 * @param HitFromDirection - Direction the hit came FROM
	 * @param HitInfo - Collision or trace result that describes the hit
	 * @param EventInstigator - Controller that was responsible for causing this damage (e.g. player who shot the weapon)
	 * @param DamageCauser - Actor that actually caused the damage (e.g. the grenade that exploded)
	 * @param DamageTypeClass - Class that describes the damage that was done.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Game|Damage")
	static void ApplyPointDamage(AActor* DamagedActor, float BaseDamage, const FVector& HitFromDirection, const FHitResult& HitInfo, AController* EventInstigator, AActor* DamageCauser, TSubclassOf<class UDamageType> DamageTypeClass);

	/** Hurts the specified actor with generic damage.
	 * @param DamagedActor - Actor that will be damaged.
	 * @param BaseDamage - The base damage to apply.
	 * @param EventInstigator - Controller that was responsible for causing this damage (e.g. player who shot the weapon)
	 * @param DamageCauser - Actor that actually caused the damage (e.g. the grenade that exploded)
	 * @param DamageTypeClass - Class that describes the damage that was done.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Game|Damage")
	static void ApplyDamage(AActor* DamagedActor, float BaseDamage, AController* EventInstigator, AActor* DamageCauser, TSubclassOf<class UDamageType> DamageTypeClass);

	// --- Camera functions ------------------------------

	/** Plays an in-world camera shake that affects all nearby local players, with distance-based attenuation. Does not replicate.
	 * @param WorldContextObject - Object that we can obtain a world context from
	 * @param Shake - Camera shake asset to use
	 * @param Epicenter - location to place the effect in world space
	 * @param InnerRadius - Cameras inside this radius are ignored
	 * @param OuterRadius - Cameras outside of InnerRadius and inside this are effected
	 * @param Falloff - Affects falloff of effect as it nears OuterRadius
	 * @param bOrientShakeTowardsEpicenter - Changes the rotation of shake to point towards epicenter instead of forward
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Feedback", meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static void PlayWorldCameraShake(UObject* WorldContextObject, TSubclassOf<class UCameraShake> Shake, FVector Epicenter, float InnerRadius, float OuterRadius, float Falloff = 1.f, bool bOrientShakeTowardsEpicenter = false);

	// --- Particle functions ------------------------------

	/** Plays the specified effect at the given location and rotation, fire and forget. The system will go away when the effect is complete. Does not replicate.
	 * @param WorldContextObject - Object that we can obtain a world context from
	 * @param EmitterTemplate - particle system to create
	 * @param Location - location to place the effect in world space
	 * @param Rotation - rotation to place the effect in world space	
	 * @param bAutoDestroy - Whether the component will automatically be destroyed when the particle system completes playing or whether it can be reactivated
	 */
	UFUNCTION(BlueprintCallable, Category="Effects|Components|ParticleSystem", meta=(Keywords = "particle system", WorldContext="WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static UParticleSystemComponent* SpawnEmitterAtLocation(UObject* WorldContextObject, class UParticleSystem* EmitterTemplate, FVector Location, FRotator Rotation = FRotator::ZeroRotator, bool bAutoDestroy = true);

	/** Plays the specified effect at the given location and rotation, fire and forget. The system will go away when the effect is complete. Does not replicate.
	 * @param World - The World to spawn in
	 * @param EmitterTemplate - particle system to create
	 * @param SpawnTransform - transform with which to place the effect in world space
	 * @param bAutoDestroy - Whether the component will automatically be destroyed when the particle system completes playing or whether it can be reactivated
	 */
	static UParticleSystemComponent* SpawnEmitterAtLocation(UWorld* World, class UParticleSystem* EmitterTemplate, const FTransform& SpawnTransform, bool bAutoDestroy = true);

	/** Plays the specified effect attached to and following the specified component. The system will go away when the effect is complete. Does not replicate.
	* @param EmitterTemplate - particle system to create
	 * @param AttachComponent - Component to attach to.
	 * @param AttachPointName - Optional named point within the AttachComponent to spawn the emitter at
	 * @param Location - Depending on the value of Location Type this is either a relative offset from the attach component/point or an absolute world position that will be translated to a relative offset
	 * @param Rotation - Depending on the value of LocationType this is either a relative offset from the attach component/point or an absolute world rotation that will be translated to a realative offset
	 * @param LocationType - Specifies whether Location is a relative offset or an absolute world position
	 * @param bAutoDestroy - Whether the component will automatically be destroyed when the particle system completes playing or whether it can be reactivated
	 */
	UFUNCTION(BlueprintCallable, Category="Effects|Components|ParticleSystem", meta=(Keywords = "particle system", UnsafeDuringActorConstruction = "true"))
	static UParticleSystemComponent* SpawnEmitterAttached(class UParticleSystem* EmitterTemplate, class USceneComponent* AttachToComponent, FName AttachPointName = NAME_None, FVector Location = FVector(ForceInit), FRotator Rotation = FRotator::ZeroRotator, EAttachLocation::Type LocationType = EAttachLocation::KeepRelativeOffset, bool bAutoDestroy = true);

	// --- Sound functions ------------------------------
	
	/**
	 * Determines if any audio listeners are within range of the specified location
	 * @param Location		The location to potentially play a sound at
	 * @param MaximumRange	The maximum distance away from Location that a listener can be
	 * @note This will always return false if there is no audio device, or the audio device is disabled.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", meta = (WorldContext = "WorldContextObject"))
	static bool AreAnyListenersWithinRange(UObject* WorldContextObject, FVector Location, float MaximumRange);
	

	/**
	* Sets a global pitch modulation scalar that will apply to all non-UI sounds
	*
	* * Fire and Forget.
	* * Not Replicated.
	* @param PitchModulation - A pitch modulation value to globally set.
	* @param TimeSec - A time value to linearly interpolate the global modulation pitch over from it's current value.
	*/
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Audio", meta = (WorldContext = "WorldContextObject"))
	static void SetGlobalPitchModulation(UObject* WorldContextObject, float PitchModulation, float TimeSec);

	/**
	* Sets the global listener focus parameters which will scale focus behavior of sounds based on their focus azimuth settings in their attenuation settings. 
	*
	* * Fire and Forget.
	* * Not Replicated.
	* @param FocusAzimuthScale - An angle scale value used to scale the azimuth angle that defines where sounds are in-focus.
	* @param NonFocusAzimuthScale- An angle scale value used to scale the azimuth angle that defines where sounds are out-of-focus.
	* @param FocusDistanceScale - A distance scale value to use for sounds which are in-focus. Values < 1.0 will reduce perceived distance to sounds, values > 1.0 will increase perceived distance to in-focus sounds.
	* @param NonFocusDistanceScale - A distance scale value to use for sounds which are out-of-focus. Values < 1.0 will reduce perceived distance to sounds, values > 1.0 will increase perceived distance to in-focus sounds.
	* @param FocusVolumeScale- A volume attenuation value to use for sounds which are in-focus.
	* @param NonFocusVolumeScale- A volume attenuation value to use for sounds which are out-of-focus.
	* @param FocusPriorityScale - A priority scale value (> 0.0) to use for sounds which are in-focus. Values < 1.0 will reduce the priority of in-focus sounds, values > 1.0 will increase the priority of in-focus sounds.
	* @param NonFocusPriorityScale - A priority scale value (> 0.0) to use for sounds which are out-of-focus. Values < 1.0 will reduce the priority of sounds out-of-focus sounds, values > 1.0 will increase the priority of out-of-focus sounds.
	*/
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Audio", meta = (WorldContext = "WorldContextObject"))
	static void SetGlobalListenerFocusParameters(UObject* WorldContextObject, float FocusAzimuthScale = 1.0f, float NonFocusAzimuthScale = 1.0f, float FocusDistanceScale = 1.0f, float NonFocusDistanceScale = 1.0f, float FocusVolumeScale = 1.0f, float NonFocusVolumeScale = 1.0f, float FocusPriorityScale = 1.0f, float NonFocusPriorityScale = 1.0f);

	/**
	 * Plays a sound directly with no attenuation, perfect for UI sounds.
	 *
	 * * Fire and Forget.
	 * * Not Replicated.
	 * @param Sound - Sound to play.
	 * @param VolumeMultiplier - Multiplied with the volume to make the sound louder or softer.
	 * @param PitchMultiplier - Multiplies the pitch.
	 * @param ConcurrencySettings - Override concurrency settings package to play sound with
	 * @param StartTime - How far in to the sound to begin playback at
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Audio", meta=( WorldContext="WorldContextObject", AdvancedDisplay = "2", UnsafeDuringActorConstruction = "true" ))
	static void PlaySound2D(UObject* WorldContextObject, class USoundBase* Sound, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, class USoundConcurrency* ConcurrencySettings = nullptr);

	/**
	 * Spawns a sound with no attenuation, perfect for UI sounds.
	 *
	 * * Not Replicated.
	 * @param Sound - Sound to play.
	 * @param VolumeMultiplier - Multiplied with the volume to make the sound louder or softer.
	 * @param PitchMultiplier - Multiplies the pitch.
	 * @param StartTime - How far in to the sound to begin playback at
	 * @param ConcurrencySettings - Override concurrency settings package to play sound with
	 * @return An audio component to manipulate the spawned sound
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Audio", meta=( WorldContext="WorldContextObject", AdvancedDisplay = "2", UnsafeDuringActorConstruction = "true", Keywords = "play" ))
	static UAudioComponent* SpawnSound2D(UObject* WorldContextObject, class USoundBase* Sound, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, class USoundConcurrency* ConcurrencySettings = nullptr);

	/**
	 * Creates a sound with no attenuation, perfect for UI sounds. This does NOT play the sound
	 *
	 * ● Not Replicated.
	 * @param Sound - Sound to create.
	 * @param VolumeMultiplier - Multiplied with the volume to make the sound louder or softer.
	 * @param PitchMultiplier - Multiplies the pitch.
	 * @param StartTime - How far in to the sound to begin playback at
	 * @return An audio component to manipulate the spawned sound
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Audio", meta=( WorldContext="WorldContextObject", AdvancedDisplay = "2", UnsafeDuringActorConstruction = "true", Keywords = "play" ))
		static UAudioComponent* CreateSound2D(UObject* WorldContextObject, class USoundBase* Sound, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, class USoundConcurrency* ConcurrencySettings = nullptr);

	/**
	 * Plays a sound at the given location. This is a fire and forget sound and does not travel with any actor. Replication is also not handled at this point.
	 * @param Sound - sound to play
	 * @param Location - World position to play sound at
	 * @param Rotation - World rotation to play sound at
	 * @param VolumeMultiplier - Volume multiplier 
	 * @param PitchMultiplier - PitchMultiplier
	 * @param StartTime - How far in to the sound to begin playback at
	 * @param AttenuationSettings - Override attenuation settings package to play sound with
	 * @param ConcurrencySettings - Override concurrency settings package to play sound with
	 */
	UFUNCTION(BlueprintCallable, Category="Audio", meta=(WorldContext="WorldContextObject", AdvancedDisplay = "3", UnsafeDuringActorConstruction = "true"))
	static void PlaySoundAtLocation(UObject* WorldContextObject, class USoundBase* Sound, FVector Location, FRotator Rotation, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, class USoundAttenuation* AttenuationSettings = nullptr, class USoundConcurrency* ConcurrencySettings = nullptr);

	static void PlaySoundAtLocation(UObject* WorldContextObject, class USoundBase* Sound, FVector Location, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, class USoundAttenuation* AttenuationSettings = nullptr, class USoundConcurrency* ConcurrencySettings = nullptr)
	{
		PlaySoundAtLocation(WorldContextObject, Sound, Location, FRotator::ZeroRotator, VolumeMultiplier, PitchMultiplier, StartTime, AttenuationSettings, ConcurrencySettings);
	}

	/**
	 * Spawns a sound at the given location. This does not travel with any actor. Replication is also not handled at this point.
	 * @param Sound - sound to play
	 * @param Location - World position to play sound at
	 * @param Rotation - World rotation to play sound at
	 * @param VolumeMultiplier - Volume multiplier 
	 * @param PitchMultiplier - PitchMultiplier
	 * @param StartTime - How far in to the sound to begin playback at
	 * @param AttenuationSettings - Override attenuation settings package to play sound with
	 * @param ConcurrencySettings - Override concurrency settings package to play sound with
	 * @return An audio component to manipulate the spawned sound
	 */
	UFUNCTION(BlueprintCallable, Category="Audio", meta=(WorldContext="WorldContextObject", AdvancedDisplay = "3", UnsafeDuringActorConstruction = "true", Keywords = "play"))
	static UAudioComponent* SpawnSoundAtLocation(UObject* WorldContextObject, class USoundBase* Sound, FVector Location, FRotator Rotation = FRotator::ZeroRotator, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, class USoundAttenuation* AttenuationSettings = nullptr, class USoundConcurrency* ConcurrencySettings = nullptr);

	/** Plays a sound attached to and following the specified component. This is a fire and forget sound. Replication is also not handled at this point.
	 * @param Sound - sound to play
	 * @param AttachComponent - Component to attach to.
	 * @param AttachPointName - Optional named point within the AttachComponent to play the sound at
	 * @param Location - Depending on the value of Location Type this is either a relative offset from the attach component/point or an absolute world position that will be translated to a relative offset
	 * @param Rotation - Depending on the value of Location Type this is either a relative offset from the attach component/point or an absolute world rotation that will be translated to a relative offset
	 * @param LocationType - Specifies whether Location is a relative offset or an absolute world position
	 * @param bStopWhenAttachedToDestroyed - Specifies whether the sound should stop playing when the owner of the attach to component is destroyed.
	 * @param VolumeMultiplier - Volume multiplier 
	 * @param PitchMultiplier - PitchMultiplier	 
	 * @param StartTime - How far in to the sound to begin playback at
	 * @param AttenuationSettings - Override attenuation settings package to play sound with
	 * @param ConcurrencySettings - Override concurrency settings package to play sound with
	 * @return An audio component to manipulate the spawned sound
	 */
	UFUNCTION(BlueprintCallable, Category="Audio", meta=(AdvancedDisplay = "2", UnsafeDuringActorConstruction = "true", Keywords = "play"))
	static class UAudioComponent* SpawnSoundAttached(class USoundBase* Sound, class USceneComponent* AttachToComponent, FName AttachPointName = NAME_None, FVector Location = FVector(ForceInit), FRotator Rotation = FRotator::ZeroRotator, EAttachLocation::Type LocationType = EAttachLocation::KeepRelativeOffset, bool bStopWhenAttachedToDestroyed = false, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, class USoundAttenuation* AttenuationSettings = nullptr, class USoundConcurrency* ConcurrencySettings = nullptr);

	DEPRECATED(4.9, "PlaySoundAttached has been renamed SpawnSoundAttached")
	static class UAudioComponent* PlaySoundAttached(class USoundBase* Sound, class USceneComponent* AttachToComponent, FName AttachPointName = NAME_None, FVector Location = FVector(ForceInit), EAttachLocation::Type LocationType = EAttachLocation::KeepRelativeOffset, bool bStopWhenAttachedToDestroyed = false, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, class USoundAttenuation* AttenuationSettings = nullptr)
	{
		return SpawnSoundAttached(Sound, AttachToComponent, AttachPointName, Location, FRotator::ZeroRotator, LocationType, bStopWhenAttachedToDestroyed, VolumeMultiplier, PitchMultiplier, StartTime, AttenuationSettings);
	}

	static class UAudioComponent* SpawnSoundAttached(class USoundBase* Sound, class USceneComponent* AttachToComponent, FName AttachPointName, FVector Location, EAttachLocation::Type LocationType = EAttachLocation::KeepRelativeOffset, bool bStopWhenAttachedToDestroyed = false, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, class USoundAttenuation* AttenuationSettings = nullptr, class USoundConcurrency* ConcurrencySettings = nullptr)
	{
		return SpawnSoundAttached(Sound, AttachToComponent, AttachPointName, Location, FRotator::ZeroRotator, LocationType, bStopWhenAttachedToDestroyed, VolumeMultiplier, PitchMultiplier, StartTime, AttenuationSettings, ConcurrencySettings);
	}

	/**
	 * Plays a dialogue directly with no attenuation, perfect for UI.
	 *
	 * * Fire and Forget.
	 * * Not Replicated.
	 * @param Dialogue - dialogue to play
	 * @param Context - context the dialogue is to play in
	 * @param VolumeMultiplier - Multiplied with the volume to make the sound louder or softer.
	 * @param PitchMultiplier - Multiplies the pitch.
	 * @param StartTime - How far in to the dialogue to begin playback at
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Audio", meta=( WorldContext="WorldContextObject", AdvancedDisplay = "3", UnsafeDuringActorConstruction = "true" ))
	static void PlayDialogue2D(UObject* WorldContextObject, class UDialogueWave* Dialogue, const struct FDialogueContext& Context, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f);

	/**
	 * Spawns a dialogue with no attenuation, perfect for UI.
	 *
	 * * Not Replicated.
	 * @param Dialogue - dialogue to play
	 * @param Context - context the dialogue is to play in
	 * @param VolumeMultiplier - Multiplied with the volume to make the sound louder or softer.
	 * @param PitchMultiplier - Multiplies the pitch.
	 * @param StartTime - How far in to the dialogue to begin playback at
	 * @return An audio component to manipulate the spawned sound
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Audio", meta=( WorldContext="WorldContextObject", AdvancedDisplay = "3", UnsafeDuringActorConstruction = "true", Keywords = "play" ))
	static UAudioComponent* SpawnDialogue2D(UObject* WorldContextObject, class UDialogueWave* Dialogue, const struct FDialogueContext& Context, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f);

	/** Plays a dialogue at the given location. This is a fire and forget sound and does not travel with any actor. Replication is also not handled at this point.
	 * @param Dialogue - dialogue to play
	 * @param Context - context the dialogue is to play in
	 * @param Location - World position to play dialogue at
	 * @param Rotation - World rotation to play dialogue at
	 * @param VolumeMultiplier - Volume multiplier 
	 * @param PitchMultiplier - Pitch multiplier
	 * @param StartTime - How far in to the dialogue to begin playback at
	 * @param AttenuationSettings - Override attenuation settings package to play sound with
	 */
	UFUNCTION(BlueprintCallable, Category="Audio", meta=(WorldContext="WorldContextObject", AdvancedDisplay = "4", UnsafeDuringActorConstruction = "true"))
	static void PlayDialogueAtLocation(UObject* WorldContextObject, class UDialogueWave* Dialogue, const struct FDialogueContext& Context, FVector Location, FRotator Rotation, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, class USoundAttenuation* AttenuationSettings = NULL);

	static void PlayDialogueAtLocation(UObject* WorldContextObject, class UDialogueWave* Dialogue, const struct FDialogueContext& Context, FVector Location, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, class USoundAttenuation* AttenuationSettings = NULL)
	{
		PlayDialogueAtLocation(WorldContextObject, Dialogue, Context, Location, FRotator::ZeroRotator, VolumeMultiplier, PitchMultiplier, StartTime, AttenuationSettings);
	}

	/** Plays a dialogue at the given location. This is a fire and forget sound and does not travel with any actor. Replication is also not handled at this point.
	 * @param Dialogue - dialogue to play
	 * @param Context - context the dialogue is to play in
	 * @param Location - World position to play dialogue at
	 * @param Rotation - World rotation to play dialogue at
	 * @param VolumeMultiplier - Volume multiplier 
	 * @param PitchMultiplier - PitchMultiplier
	 * @param StartTime - How far in to the dialogue to begin playback at
	 * @param AttenuationSettings - Override attenuation settings package to play sound with
	 * @return Audio Component to manipulate the playing dialogue with
	 */
	UFUNCTION(BlueprintCallable, Category="Audio", meta=(WorldContext="WorldContextObject", AdvancedDisplay = "4", UnsafeDuringActorConstruction = "true", Keywords = "play"))
	static UAudioComponent* SpawnDialogueAtLocation(UObject* WorldContextObject, class UDialogueWave* Dialogue, const struct FDialogueContext& Context, FVector Location, FRotator Rotation = FRotator::ZeroRotator, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, class USoundAttenuation* AttenuationSettings = NULL);

	/** Plays a dialogue attached to and following the specified component. This is a fire and forget sound. Replication is also not handled at this point.
	 * @param Dialogue - dialogue to play
	 * @param Context - context the dialogue is to play in
	 * @param AttachComponent - Component to attach to.
	 * @param AttachPointName - Optional named point within the AttachComponent to play the sound at
	 * @param Location - Depending on the value of Location Type this is either a relative offset from the attach component/point or an absolute world position that will be translated to a relative offset
	 * @param Rotation - Depending on the value of Location Type this is either a relative offset from the attach component/point or an absolute world rotation that will be translated to a relative offset
	 * @param LocationType - Specifies whether Location is a relative offset or an absolute world position
	 * @param bStopWhenAttachedToDestroyed - Specifies whether the sound should stop playing when the owner of the attach to component is destroyed.
	 * @param VolumeMultiplier - Volume multiplier 
	 * @param PitchMultiplier - PitchMultiplier	 
	 * @param StartTime - How far in to the dialogue to begin playback at
	 * @param AttenuationSettings - Override attenuation settings package to play sound with
	 * @return Audio Component to manipulate the playing dialogue with
	 */
	UFUNCTION(BlueprintCallable, Category="Audio", meta=(AdvancedDisplay = "2", UnsafeDuringActorConstruction = "true", Keywords = "play"))
	static class UAudioComponent* SpawnDialogueAttached(class UDialogueWave* Dialogue, const struct FDialogueContext& Context, class USceneComponent* AttachToComponent, FName AttachPointName = NAME_None, FVector Location = FVector(ForceInit), FRotator Rotation = FRotator::ZeroRotator, EAttachLocation::Type LocationType = EAttachLocation::KeepRelativeOffset, bool bStopWhenAttachedToDestroyed = false, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, class USoundAttenuation* AttenuationSettings = NULL);

	DEPRECATED(4.9, "PlayDialogueAttached has been renamed SpawnDialogueAttached")
	static class UAudioComponent* PlayDialogueAttached(class UDialogueWave* Dialogue, const struct FDialogueContext& Context, class USceneComponent* AttachToComponent, FName AttachPointName = NAME_None, FVector Location = FVector(ForceInit), EAttachLocation::Type LocationType = EAttachLocation::KeepRelativeOffset, bool bStopWhenAttachedToDestroyed = false, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, class USoundAttenuation* AttenuationSettings = NULL)
	{
		return SpawnDialogueAttached(Dialogue, Context, AttachToComponent, AttachPointName, Location, FRotator::ZeroRotator, LocationType, bStopWhenAttachedToDestroyed, VolumeMultiplier, PitchMultiplier, StartTime, AttenuationSettings);
	}

	static class UAudioComponent* SpawnDialogueAttached(class UDialogueWave* Dialogue, const struct FDialogueContext& Context, class USceneComponent* AttachToComponent, FName AttachPointName, FVector Location, EAttachLocation::Type LocationType = EAttachLocation::KeepRelativeOffset, bool bStopWhenAttachedToDestroyed = false, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f, float StartTime = 0.f, class USoundAttenuation* AttenuationSettings = NULL)
	{
		return SpawnDialogueAttached(Dialogue, Context, AttachToComponent, AttachPointName, Location, FRotator::ZeroRotator, LocationType, bStopWhenAttachedToDestroyed, VolumeMultiplier, PitchMultiplier, StartTime, AttenuationSettings);
	}

	/**
	 * Will set subtitles to be enabled or disabled.
	 * @param bEnabled will enable subtitle drawing if true, disable if false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|Subtitles")
	static void SetSubtitlesEnabled(bool bEnabled);

	/**
	 * Returns whether or not subtitles are currently enabled.
	 * @return true if subtitles are enabled.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|Subtitles")
	static bool AreSubtitlesEnabled();

	// --- Audio Functions ----------------------------
	/** Set the sound mix of the audio system for special EQing **/
	UFUNCTION(BlueprintCallable, Category="Audio", meta=(WorldContext = "WorldContextObject"))
	static void SetBaseSoundMix(UObject* WorldContextObject, class USoundMix* InSoundMix);

	/** Overrides the sound class adjuster in the given sound mix. If the sound class does not exist in the input sound mix, the sound class adjustment will be added to the sound mix.
	 * @param InSoundMixModifier The sound mix to modify.
	 * @param InSoundClass The sound class to override (or add) in the sound mix.
	 * @param Volume The volume scale to set the sound class adjuster to.
	 * @param Pitch The pitch scale to set the sound class adjuster to.
	 * @param FadeInTime The interpolation time to use to go from the current sound class adjuster values to the new values.
	 * @param bApplyToChildren Whether or not to apply this override to the sound class' children or to just the specified sound class.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", meta = (WorldContext = "WorldContextObject"))
	static void SetSoundMixClassOverride(UObject* WorldContextObject, class USoundMix* InSoundMixModifier, class USoundClass* InSoundClass, float Volume = 1.0f, float Pitch = 1.0f, float FadeInTime = 1.0f, bool bApplyToChildren = true);

	/** Clears the override of the sound class adjuster in the given sound mix. If the override did not exist in the sound mix, this will do nothing.
	 * @param InSoundMixModifier The sound mix to modify.
	 * @param InSoundClass The sound class to override (or add) in the sound mix.
	 * @param FadeOutTime The interpolation time to use to go from the current sound class adjuster override values to the non-override values.
	*/
	UFUNCTION(BlueprintCallable, Category = "Audio", meta = (WorldContext = "WorldContextObject"))
	static void ClearSoundMixClassOverride(UObject* WorldContextObject, class USoundMix* InSoundMixModifier, class USoundClass* InSoundClass, float FadeOutTime = 1.0f);

	/** Push a sound mix modifier onto the audio system **/
	UFUNCTION(BlueprintCallable, Category="Audio", meta=(WorldContext = "WorldContextObject"))
	static void PushSoundMixModifier(UObject* WorldContextObject, class USoundMix* InSoundMixModifier);

	/** Pop a sound mix modifier from the audio system **/
	UFUNCTION(BlueprintCallable, Category="Audio", meta=(WorldContext = "WorldContextObject"))
	static void PopSoundMixModifier(UObject* WorldContextObject, class USoundMix* InSoundMixModifier);

	/** Clear all sound mix modifiers from the audio system **/
	UFUNCTION(BlueprintCallable, Category="Audio", meta=(WorldContext = "WorldContextObject"))
	static void ClearSoundMixModifiers(UObject* WorldContextObject);

	/** Activates a Reverb Effect without the need for a volume
	 * @param ReverbEffect Reverb Effect to use
	 * @param TagName Tag to associate with Reverb Effect
	 * @param Priority Priority of the Reverb Effect
	 * @param Volume Volume level of Reverb Effect
	 * @param FadeTime Time before Reverb Effect is fully active
	 */
	UFUNCTION(BlueprintCallable, Category="Audio", meta=(WorldContext = "WorldContextObject", AdvancedDisplay = "2"))
	static void ActivateReverbEffect(UObject* WorldContextObject, class UReverbEffect* ReverbEffect, FName TagName, float Priority = 0.f, float Volume = 0.5f, float FadeTime = 2.f);

	/**
	 * Deactivates a Reverb Effect not applied by a volume
	 *
	 * @param TagName Tag associated with Reverb Effect to remove
	 */
	UFUNCTION(BlueprintCallable, Category="Audio", meta=(WorldContext = "WorldContextObject"))
	static void DeactivateReverbEffect(UObject* WorldContextObject, FName TagName);

	/** 
	 * Returns the highest priority reverb settings currently active from any source (volumes or manual setting).
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", meta = (WorldContext = "WorldContextObject"))
	static class UReverbEffect* GetCurrentReverbEffect(UObject* WorldContextObject);

	// --- Decal functions ------------------------------

	/** Spawns a decal at the given location and rotation, fire and forget. Does not replicate.
	 * @param DecalMaterial - decal's material
	 * @param DecalSize - size of decal
	 * @param Location - location to place the decal in world space
	 * @param Rotation - rotation to place the decal in world space	
	 * @param LifeSpan - destroy decal component after time runs out (0 = infinite)
	 */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Decal", meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static UDecalComponent* SpawnDecalAtLocation(UObject* WorldContextObject, class UMaterialInterface* DecalMaterial, FVector DecalSize, FVector Location, FRotator Rotation = FRotator(-90, 0, 0), float LifeSpan = 0);

	/** Spawns a decal attached to and following the specified component. Does not replicate.
	 * @param DecalMaterial - decal's material
	 * @param DecalSize - size of decal
	 * @param AttachComponent - Component to attach to.
	 * @param AttachPointName - Optional named point within the AttachComponent to spawn the emitter at
	 * @param Location - Depending on the value of Location Type this is either a relative offset from the attach component/point or an absolute world position that will be translated to a relative offset
	 * @param Rotation - Depending on the value of LocationType this is either a relative offset from the attach component/point or an absolute world rotation that will be translated to a realative offset
	 * @param LocationType - Specifies whether Location is a relative offset or an absolute world position
	 * @param LifeSpan - destroy decal component after time runs out (0 = infinite)
	 */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|Decal", meta=(UnsafeDuringActorConstruction = "true"))
	static UDecalComponent* SpawnDecalAttached(class UMaterialInterface* DecalMaterial, FVector DecalSize, class USceneComponent* AttachToComponent, FName AttachPointName = NAME_None, FVector Location = FVector(ForceInit), FRotator Rotation = FRotator::ZeroRotator, EAttachLocation::Type LocationType = EAttachLocation::KeepRelativeOffset, float LifeSpan = 0);

	/** Extracts data from a HitResult. 
	 * @param Hit			The source HitResult.
	 * @param bBlockingHit	True if there was a blocking hit, false otherwise.
	 * @param bInitialOverlap True if the hit started in an initial overlap. In this case some other values should be interpreted differently. Time will be 0, ImpactPoint will equal Location, and normals will be equal and indicate a depenetration vector.
	 * @param Time			'Time' of impact along trace direction ranging from [0.0 to 1.0) if there is a hit, indicating time between start and end. Equals 1.0 if there is no hit.
	 * @param Location		Location of the hit in world space. If this was a swept shape test, this is the location where we can place the shape in the world where it will not penetrate.
	 * @param Normal		Normal of the hit in world space, for the object that was swept (e.g. for a sphere trace this points towards the sphere's center). Equal to ImpactNormal for line tests.
	 * @param ImpactPoint	Location of the actual contact point of the trace shape with the surface of the hit object. Equal to Location in the case of an initial overlap.
	 * @param ImpactNormal	Normal of the hit in world space, for the object that was hit by the sweep.
	 * @param PhysMat		Physical material that was hit. Must set bReturnPhysicalMaterial to true in the query params for this to be returned.
	 * @param HitActor		Actor hit by the trace.
	 * @param HitComponent	PrimitiveComponent hit by the trace.
	 * @param HitBoneName	Name of the bone hit (valid only if we hit a skeletal mesh).
	 * @param HitItem		Primitive-specific data recording which item in the primitive was hit
	 * @param FaceIndex		If colliding with trimesh or landscape, index of face that was hit.
	 */
	UFUNCTION(BlueprintPure, Category = "Collision", meta=(NativeBreakFunc))
	static void BreakHitResult(const struct FHitResult& Hit, bool& bBlockingHit, bool& bInitialOverlap, float& Time, FVector& Location, FVector& ImpactPoint, FVector& Normal, FVector& ImpactNormal, class UPhysicalMaterial*& PhysMat, class AActor*& HitActor, class UPrimitiveComponent*& HitComponent, FName& HitBoneName, int32& HitItem, int32& FaceIndex, FVector& TraceStart, FVector& TraceEnd);

	/** 
	 *	Create a HitResult struct
	 * @param Hit			The source HitResult.
	 * @param bBlockingHit	True if there was a blocking hit, false otherwise.
	 * @param bInitialOverlap True if the hit started in an initial overlap. In this case some other values should be interpreted differently. Time will be 0, ImpactPoint will equal Location, and normals will be equal and indicate a depenetration vector.
	 * @param Time			'Time' of impact along trace direction ranging from [0.0 to 1.0) if there is a hit, indicating time between start and end. Equals 1.0 if there is no hit.
	 * @param Location		Location of the hit in world space. If this was a swept shape test, this is the location where we can place the shape in the world where it will not penetrate.
	 * @param Normal		Normal of the hit in world space, for the object that was swept (e.g. for a sphere trace this points towards the sphere's center). Equal to ImpactNormal for line tests.
	 * @param ImpactPoint	Location of the actual contact point of the trace shape with the surface of the hit object. Equal to Location in the case of an initial overlap.
	 * @param ImpactNormal	Normal of the hit in world space, for the object that was hit by the sweep.
	 * @param PhysMat		Physical material that was hit. Must set bReturnPhysicalMaterial to true in the query params for this to be returned.
	 * @param HitActor		Actor hit by the trace.
	 * @param HitComponent	PrimitiveComponent hit by the trace.
	 * @param HitBoneName	Name of the bone hit (valid only if we hit a skeletal mesh).
	 * @param HitItem		Primitive-specific data recording which item in the primitive was hit
	 * @param FaceIndex		If colliding with trimesh or landscape, index of face that was hit.
	 */
	UFUNCTION(BlueprintPure, Category = "Collision", meta = (NativeMakeFunc, Normal="0,0,1", ImpactNormal="0,0,1"))
	static FHitResult MakeHitResult(bool bBlockingHit, bool bInitialOverlap, float Time, FVector Location, FVector ImpactPoint, FVector Normal, FVector ImpactNormal, class UPhysicalMaterial* PhysMat, class AActor* HitActor, class UPrimitiveComponent* HitComponent, FName HitBoneName, int32 HitItem, int32 FaceIndex, FVector TraceStart, FVector TraceEnd);


	/** Returns the EPhysicalSurface type of the given Hit. 
	 * To edit surface type for your project, use ProjectSettings/Physics/PhysicalSurface section
	 */
	UFUNCTION(BlueprintPure, Category="Collision")
	static EPhysicalSurface GetSurfaceType(const struct FHitResult& Hit);

	/** 
	 *	Try and find the UV for a collision impact. Note this ONLY works if 'Support UV From Hit Results' is enabled in Physics Settings.
	 */
	UFUNCTION(BlueprintPure, Category = "Collision")
	static bool FindCollisionUV(const struct FHitResult& Hit, int32 UVChannel, FVector2D& UV);

	// --- Save Game functions ------------------------------

	/** 
	 *	Create a new, empty SaveGame object to set data on and then pass to SaveGameToSlot.
	 *	@param	SaveGameClass	Class of SaveGame to create
	 *	@return					New SaveGame object to write data to
	 */
	UFUNCTION(BlueprintCallable, Category="Game")
	static USaveGame* CreateSaveGameObject(TSubclassOf<USaveGame> SaveGameClass);

	/** 
	 *	Create a new, empty SaveGame object to set data on and then pass to SaveGameToSlot.
	 *	@param	SaveGameBlueprint	Blueprint of SaveGame to create
	 *	@return						New SaveGame object to write data to
	 */
	UFUNCTION(BlueprintCallable, Category="Game", meta=(DeprecatedFunction, DeprecationMessage="Use GameplayStatics.CreateSaveGameObject instead."))
	static USaveGame* CreateSaveGameObjectFromBlueprint(UBlueprint* SaveGameBlueprint);

	/** 
	 *	Save the contents of the SaveGameObject to a slot.
	 *	@param SaveGameObject	Object that contains data about the save game that we want to write out
	 *	@param SlotName			Name of save game slot to save to.
	 *  @param UserIndex		For some platforms, master user index to identify the user doing the saving.
	 *	@return					Whether we successfully saved this information
	 */
	UFUNCTION(BlueprintCallable, Category="Game")
	static bool SaveGameToSlot(USaveGame* SaveGameObject, const FString& SlotName, const int32 UserIndex);

	/**
	 *	See if a save game exists with the specified name.
	 *	@param SlotName			Name of save game slot.
	 *  @param UserIndex		For some platforms, master user index to identify the user doing the saving.
	 */
	UFUNCTION(BlueprintCallable, Category="Game")
	static bool DoesSaveGameExist(const FString& SlotName, const int32 UserIndex);

	/** 
	 *	Load the contents from a given slot.
	 *	@param SlotName			Name of the save game slot to load from.
	 *  @param UserIndex		For some platforms, master user index to identify the user doing the loading.
	 *	@return SaveGameObject	Object containing loaded game state (NULL if load fails)
	 */
	UFUNCTION(BlueprintCallable, Category="Game")
	static USaveGame* LoadGameFromSlot(const FString& SlotName, const int32 UserIndex);

	/**
	 * Delete a save game in a particular slot.
	 *	@param SlotName			Name of save game slot to delete.
	 *  @param UserIndex		For some platforms, master user index to identify the user doing the deletion.
	 *  @return True if a file was actually able to be deleted. use DoesSaveGameExist to distinguish between delete failures and failure due to file not existing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Game")
	static bool DeleteGameInSlot(const FString& SlotName, const int32 UserIndex);

	/** Returns the frame delta time in seconds, adjusted by time dilation. */
	UFUNCTION(BlueprintPure, Category = "Utilities|Time", meta = (WorldContext="WorldContextObject"))
	static float GetWorldDeltaSeconds(UObject* WorldContextObject);

	/** Returns time in seconds since world was brought up for play, does NOT stop when game pauses, NOT dilated/clamped */
	UFUNCTION(BlueprintPure, Category="Utilities|Time", meta=(WorldContext="WorldContextObject"))
	static float GetRealTimeSeconds(UObject* WorldContextObject);
	
	/** Returns time in seconds since world was brought up for play, IS stopped when game pauses, NOT dilated/clamped. */
	UFUNCTION(BlueprintPure, Category="Utilities|Time", meta=(WorldContext="WorldContextObject"))
	static float GetAudioTimeSeconds(UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category="Utilities|Time", meta=(WorldContext="WorldContextObject"))
	static void GetAccurateRealTime(UObject* WorldContextObject, int32& Seconds, float& PartialSeconds);

	/*~ DVRStreaming API */
	
	/**
	 * Toggle live DVR streaming.
	 * @param Enable			If true enable streaming, otherwise disable.
	 */
	UFUNCTION(BlueprintCallable, Category="Game", meta=(Enable="true"))
	static void EnableLiveStreaming(bool Enable);

	/**
	 * Returns the string name of the current platform, to perform different behavior based on platform. 
	 * (Platform names include Windows, Mac, IOS, Android, PS4, XboxOne, HTML5, Linux) */
	UFUNCTION(BlueprintCallable, Category="Game")
	static FString GetPlatformName();

	/**
	 * Calculates an launch velocity for a projectile to hit a specified point.
	 * @param TossVelocity		(output) Result launch velocity.
	 * @param StartLocation		Intended launch location
	 * @param EndLocation		Desired landing location
	 * @param LaunchSpeed		Desired launch speed
	 * @param OverrideGravityZ	Optional gravity override.  0 means "do not override".
	 * @param TraceOption		Controls whether or not to validate a clear path by tracing along the calculated arc
	 * @param CollisionRadius	Radius of the projectile (assumed spherical), used when tracing
	 * @param bFavorHighArc		If true and there are 2 valid solutions, will return the higher arc.  If false, will favor the lower arc.
	 * @param bDrawDebug		When true, a debug arc is drawn (red for an invalid arc, green for a valid arc)
	 * @return					Returns false if there is no valid solution or the valid solutions are blocked.  Returns true otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Components|ProjectileMovement", DisplayName="SuggestProjectileVelocity", meta=(WorldContext="WorldContextObject"))
	static bool BlueprintSuggestProjectileVelocity(UObject* WorldContextObject, FVector& TossVelocity, FVector StartLocation, FVector EndLocation, float LaunchSpeed, float OverrideGravityZ, ESuggestProjVelocityTraceOption::Type TraceOption, float CollisionRadius, bool bFavorHighArc, bool bDrawDebug);

	/** Native version, has more options than the Blueprint version. */
	static bool SuggestProjectileVelocity(UObject* WorldContextObject, FVector& TossVelocity, FVector StartLocation, FVector EndLocation, float TossSpeed, bool bHighArc = false, float CollisionRadius = 0.f, float OverrideGravityZ = 0, ESuggestProjVelocityTraceOption::Type TraceOption = ESuggestProjVelocityTraceOption::TraceFullPath, const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam, const TArray<AActor*>& ActorsToIgnore = TArray<AActor*>(), bool bDrawDebug = false);

	/**
	* Predict the arc of a virtual projectile affected by gravity with collision checks along the arc. Returns a list of positions of the simulated arc and the destination reached by the simulation.
	* Returns true if it hit something.
	*
	* @param OutPathPositions			Predicted projectile path. Ordered series of positions from StartPos to the end. Includes location at point of impact if it hit something.
	* @param OutHit						Predicted hit result, if the projectile will hit something
	* @param OutLastTraceDestination	Goal position of the final trace it did. Will not be in the path if there is a hit.
	* @param StartPos					First start trace location
	* @param LaunchVelocity				Velocity the "virtual projectile" is launched at
	* @param bTracePath					Trace along the entire path to look for blocking hits
	* @param ProjectileRadius			Radius of the virtual projectile to sweep against the environment
	* @param ObjectTypes				ObjecTypes to trace against
	* @param bTraceComplex				Use TraceComplex (trace against triangles not primitives)
	* @param ActorsToIgnore				Actors to exclude from the traces
	* @param DrawDebugType				Debug type (one-frame, duration, persistent)
	* @param DrawDebugTime				Duration of debug lines (only relevant for DrawDebugType::Duration)
	* @param SimFrequency				Determines size of each sub-step in the simulation (chopping up MaxSimTime)
	* @param MaxSimTime					Maximum simulation time for the virtual projectile.
	* @param OverrideGravityZ			Optional override of Gravity (default uses WorldGravityZ
	*/
	UFUNCTION(BlueprintCallable, Category = "Game", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "ActorsToIgnore", AdvancedDisplay = "DrawDebugTime, DrawDebugType, SimFrequency, MaxSimTime, OverrideGravityZ", TraceChannel = ECC_WorldDynamic, bTracePath = true))
	static bool PredictProjectilePath(UObject* WorldContextObject, FHitResult& OutHit, TArray<FVector>& OutPathPositions, FVector& OutLastTraceDestination, FVector StartPos, FVector LaunchVelocity, bool bTracePath, float ProjectileRadius, const TArray<TEnumAsByte<EObjectTypeQuery> >& ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, float DrawDebugTime, float SimFrequency = 30.f, float MaxSimTime = 2.f, float OverrideGravityZ = 0);

	/**
	* Returns the launch velocity needed for a projectile at rest at StartPos to land on EndPos.
	* Assumes a medium arc (e.g. 45 deg on level ground). Projectile velocity is variable and unconstrained.
	* Does no tracing.
	*
	* @param OutLaunchVelocity			Returns the launch velocity required to reach the EndPos
	* @param StartPos					Start position of the simulation
	* @param EndPos						Desired end location for the simulation
	* @param OverrideGravityZ			Optional override of WorldGravityZ
	* @param ArcParam					Change height of arc between 0.0-1.0 where 0.5 is the default medium arc
	*/
	UFUNCTION(BlueprintCallable, Category = "Game", DisplayName = "SuggestProjectileVelocity Custom Arc", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "OverrideGravityZ, ArcParam"))
	static bool SuggestProjectileVelocity_CustomArc(UObject* WorldContextObject, FVector& OutLaunchVelocity, FVector StartPos, FVector EndPos, float OverrideGravityZ = 0, float ArcParam = 0.5f);

	/** Returns world origin current location. */
	UFUNCTION(BlueprintPure, Category="Game", meta=(WorldContext="WorldContextObject") )
	static FIntVector GetWorldOriginLocation(UObject* WorldContextObject);
	
	/** Requests a new location for a world origin. */
	UFUNCTION(BlueprintCallable, Category="Game", meta=(WorldContext="WorldContextObject"))
	static void SetWorldOriginLocation(UObject* WorldContextObject, FIntVector NewLocation);

	/**
	* Counts how many grass foliage instances overlap a given sphere.
	*
	* @param	Mesh			The static mesh we are interested in counting.
	* @param	CenterPosition	The center position of the sphere.
	* @param	Radius			The radius of the sphere.
	*
	* @return Number of foliage instances with their mesh set to Mesh that overlap the sphere.
	*/
	UFUNCTION(BlueprintCallable, Category = "Foliage", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static int32 GrassOverlappingSphereCount(UObject* WorldContextObject, const UStaticMesh* StaticMesh, FVector CenterPosition, float Radius);

	/** 
	 * Transforms the given 2D screen space coordinate into a 3D world-space point and direction.
	 * @param Player			Deproject using this player's view.
	 * @param ScreenPosition	2D screen space to deproject.
	 * @param WorldPosition		(out) Corresponding 3D position in world space.
	 * @param WorldDirection	(out) World space direction vector away from the camera at the given 2d poiunt.
	 */
	UFUNCTION(BlueprintPure, Category = "Utilities", meta = (Keywords = "unproject"))
	static bool DeprojectScreenToWorld(APlayerController const* Player, const FVector2D& ScreenPosition, FVector& WorldPosition, FVector& WorldDirection);

	/** 
	 * Transforms the given 3D world-space point into a its 2D screen space coordinate. 
	 * @param Player			Project using this player's view.
	 * @param WorldPosition		World position to project.
	 * @param ScreenPosition	(out) Corresponding 2D position in screen space
	 */
	UFUNCTION(BlueprintPure, Category = "Utilities")
	static bool ProjectWorldToScreen(APlayerController const* Player, const FVector& WorldPosition, FVector2D& ScreenPosition);

	//~ Utility functions for interacting with Options strings

	//~=========================================================================
	//~ URL Parsing

	static bool GrabOption( FString& Options, FString& ResultString );

	/** 
	 * Break up a key=value pair into its key and value. 
	 * @param Pair			The string containing a pair to split apart.
	 * @param Key			(out) Key portion of Pair. If no = in string will be the same as Pair.
	 * @param Value			(out) Value portion of Pair. If no = in string will be empty.
	 */
	UFUNCTION(BlueprintPure, Category="Game Options")
	static void GetKeyValue( const FString& Pair, FString& Key, FString& Value );

	/** 
	 * Find an option in the options string and return it.
	 * @param Options		The string containing the options.
	 * @param Key			The key to find the value of in Options.
	 * @return				The value associated with Key if Key found in Options string.
	 */
	UFUNCTION(BlueprintPure, Category="Game Options")
	static FString ParseOption( FString Options, const FString& Key );

	/** 
	 * Returns whether a key exists in an options string.
	 * @param Options		The string containing the options.
	 * @param Key			The key to determine if it exists in Options.
	 * @return				Whether Key was found in Options.
	 */
	UFUNCTION(BlueprintPure, Category="Game Options")
	static bool HasOption( FString Options, const FString& InKey );

	/** 
	 * Find an option in the options string and return it as an integer.
	 * @param Options		The string containing the options.
	 * @param Key			The key to find the value of in Options.
	 * @return				The value associated with Key as an integer if Key found in Options string, otherwise DefaultValue.
	 */
	UFUNCTION(BlueprintPure, Category="Game Options")
	static int32 GetIntOption( const FString& Options, const FString& Key, int32 DefaultValue);

};


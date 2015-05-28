// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class UGameplayCueSet;

#include "GameplayTags.h"
#include "GameplayEffect.h"
#include "GameplayCueNotify_Actor.h"
#include "GameplayCueManager.generated.h"

/**
 *	
 *	Major TODO:
 *	
 *	-Remove LoadObjectLibraryFromPaths from base implementation. Just have project pass in ObjectLibrary, let it figure out how to make it.
 *		-On Bob's recommendation: in hopes of removing object libraries coupling with directory structure.
 *		-This becomes trickier with smaller projects/licensees. It would be nice to offer a path of least resistence for using gameplaycues (without having to implement a load object library function).
 *		-This gets even trickier though when dealing with having to reload that objecet library when assets are added/deleted/renamed. Ideally the GameplaycueManager handles all of this, but if we are getting
 *		an objectlibrary passed in, we don't know how to recreate it. Could add a delegate to invoke when we need the object library reloaded, but then ownership of that library feels pretty weird.
 *	
 *	-Async loading of all required GameplayCueNotifies
 *		-Currently async loaded on demand (first GameplayCueNotify is dropped)
 *		-Need way to enumerate all required GameplayCue tags (Interface? GameplayCueTags can come from GameplayEffects and GameplayAbilities, possibly more?)
 *		-Implemented UGameplayCueManager::BeginLoadingGameplayCueNotify
 *		
 *	-Better figure out how to handle instancing of GameplayCueNotifies
 *		-How to handle status type GameplayCueNotifies? (Code/blueprint will spawn stuff, needs to keep track and destroy them when GameplayCue is removed)
 *		-Instanced InstantiatedObjects is growing unbounded!
 *		 
 *	
 *	-Editor Workflow:
 *		-Make way to create new GameplayCueNotifies from UGameplayCueManager (details customization)
 *		-Jump to GameplayCueManager entry for GameplayCueTag (details customization)
 *		-Implement HandleAssetAdded and HandleAssetDeleted
 *			-Must make sure we update GameplayCueData/GameplayCueDataMap at appropriate times
 *				-On startup/begin PIE or try to do it as things change?
 *		
 *	-Overriding/forwarding: are we doing this right?
 *		-When can things override, when can they call parent implementations, etc
 *		-Do GameplayCueNotifies ever override GameplayCue Events or vice versa?
 *		
 *	-Take a pass on destruction
 *		-Make sure we are cleaning up GameplayCues when actors are destroyed
 *			(Register with Actor delegate or force game code to call EndGameplayCuesFor?)
 *	
 */

/** Type of payload to pass along with this cue */
UENUM()
enum class EGameplayCuePayloadType : uint8
{
	EffectContext,
	CueParameters,
	FromSpec,
};

/** Structure to keep track of pending gameplay cues that haven't been applied yet */
USTRUCT()
struct FGameplayCuePendingExecute
{
	GENERATED_USTRUCT_BODY()

	FGameplayCuePendingExecute()
	: PayloadType(EGameplayCuePayloadType::EffectContext)
	, OwningComponent(NULL)
	{
	}

	UPROPERTY()
	FGameplayTag GameplayCueTag;
	
	/** Prediction key that spawned this cue */
	UPROPERTY()
	FPredictionKey PredictionKey;

	/** What type of payload is attached to this cue */
	UPROPERTY()
	EGameplayCuePayloadType PayloadType;

	/** What component to send the cue on */
	UPROPERTY()
	UAbilitySystemComponent* OwningComponent;

	/** If this cue is from a spec, here's the copy of that spec */
	UPROPERTY()
	FGameplayEffectSpecForRPC FromSpec;

	/** Store the full cue parameters or just the effect context depending on type */
	UPROPERTY()
	FGameplayCueParameters CueParameters;
};

/**
 *	FScopedGameplayCueSendContext
 *	Add this around code that sends multiple gameplay cues to allow grouping them into a smalkler number of cues for more efficient networking
 */
struct GAMEPLAYABILITIES_API FScopedGameplayCueSendContext
{
	FScopedGameplayCueSendContext();
	~FScopedGameplayCueSendContext();
};

/**
 *	A self contained handler of a GameplayCue. These are similar to AnimNotifies in implementation.
 */

UCLASS()
class GAMEPLAYABILITIES_API UGameplayCueManager : public UDataAsset
{
	GENERATED_UCLASS_BODY()

	// -------------------------------------------------------------
	// Wrappers to handle replicating executed cues
	// -------------------------------------------------------------

	virtual void InvokeGameplayCueExecuted_FromSpec(UAbilitySystemComponent* OwningComponent, const FGameplayEffectSpecForRPC Spec, FPredictionKey PredictionKey);
	virtual void InvokeGameplayCueExecuted(UAbilitySystemComponent* OwningComponent, const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext);
	virtual void InvokeGameplayCueExecuted_WithParams(UAbilitySystemComponent* OwningComponent, const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayCueParameters GameplayCueParameters);

	/** Start or stop a gameplay cue send context. Used by FScopedGameplayCueSendContext above, when all contexts are removed the cues are flushed */
	void StartGameplayCueSendContext();
	void EndGameplayCueSendContext();

	/** Send out any pending cues */
	virtual void FlushPendingCues();

	/** Process a pending cue, return false if the cue should be rejected. */
	virtual bool ProcessPendingCueExecute(FGameplayCuePendingExecute& PendingCue);

	/** Returns true if two pending cues match, can be overridden in game */
	virtual bool DoesPendingCueExecuteMatch(FGameplayCuePendingExecute& PendingCue, FGameplayCuePendingExecute& ExistingCue);

	// -------------------------------------------------------------
	// Handling GameplayCues at runtime:
	// -------------------------------------------------------------

	virtual void HandleGameplayCues(AActor* TargetActor, const FGameplayTagContainer& GameplayCueTags, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters);

	virtual void HandleGameplayCue(AActor* TargetActor, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters);

	/** Force any instanced GameplayCueNotifies to stop */
	virtual void EndGameplayCuesFor(AActor* TargetActor);

	/** Returns the cached instance cue. Creates it if it doesn't exist */
	virtual AGameplayCueNotify_Actor* GetInstancedCueActor(AActor* TargetActor, UClass* CueClass);

	// -------------------------------------------------------------
	//  Loading GameplayCueNotifies from ObjectLibraries
	// -------------------------------------------------------------

	/** Loading soft refs to all GameplayCueNotifies */
	void LoadObjectLibraryFromPaths(const TArray<FString>& Paths);

	UPROPERTY(transient)
	UGameplayCueSet* GlobalCueSet;
	
	UPROPERTY(transient)
	UObjectLibrary* GameplayCueNotifyActorObjectLibrary;

	UPROPERTY(transient)
	UObjectLibrary* GameplayCueNotifyStaticObjectLibrary;

	// -------------------------------------------------------------
	// Preload GameplayCue tags that we think we will need:
	// -------------------------------------------------------------

	void	BeginLoadingGameplayCueNotify(FGameplayTag GameplayCueTag);

	int32	FinishLoadingGameplayCueNotifies();

	FStreamableManager	StreamableManager;

	// Fixme: we can combine the AActor* and the FGameplayTag into a single struct with a decent hash and avoid double map lookups
	TMap<TWeakObjectPtr<AActor>, TMap<TWeakObjectPtr<UClass>, TWeakObjectPtr<AGameplayCueNotify_Actor>>>		NotifyMapActor;

	void PrintGameplayCueNotifyMap();

#if WITH_EDITOR
	bool IsAssetInLoadedPaths(UObject *Object) const;

	/** Handles updating an object library when a new asset is created */
	void HandleAssetAdded(UObject *Object);

	/** Handles cleaning up an object library if it matches the passed in object */
	void HandleAssetDeleted(UObject *Object);

	bool RegisteredEditorCallbacks;

	bool bAccelerationMapOutdated;
#endif

protected:

#if WITH_EDITOR
	//This handles the case where GameplayCueNotifications have changed between sessions, which is possible in editor.
	void ReloadObjectLibrary(UWorld* World, const UWorld::InitializationValues IVS);
#endif

	void LoadObjectLibrary_Internal();

	void BuildCuesToAddToGlobalSet(const TArray<FAssetData>& AssetDataList, FName TagPropertyName, bool bAsyncLoadAfterAdd, TArray<struct FGameplayCueReferencePair>& OutCuesToAdd);

	TArray<FString>	LoadedPaths;

	/** List of gameplay cue executes that haven't been processed yet */
	UPROPERTY()
	TArray<FGameplayCuePendingExecute> PendingExecuteCues;

	/** Number of active gameplay cue send contexts, when it goes to 0 cues are flushed */
	UPROPERTY()
	int32 GameplayCueSendContextCount;
};
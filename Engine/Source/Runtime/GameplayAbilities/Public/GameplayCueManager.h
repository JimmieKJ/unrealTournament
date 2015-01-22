// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

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


USTRUCT()
struct FGameplayCueNotifyData
{
	GENERATED_USTRUCT_BODY()

	FGameplayCueNotifyData()
	: LoadedGameplayCueClass(nullptr)
	, ParentDataIdx( INDEX_NONE )
	{
	}

	UPROPERTY(VisibleDefaultsOnly, Category=GameplayCue, meta=(AllowedClasses="GameplayCueNotify"))
	FStringAssetReference	GameplayCueNotifyObj;

	UPROPERTY()
	UClass*					LoadedGameplayCueClass;

	FGameplayTag			GameplayCueTag;

	int32 ParentDataIdx;
};

/**
 *	A self contained handler of a GameplayCue. These are similiar to AnimNotifies in implementation.
 */

UCLASS()
class GAMEPLAYABILITIES_API UGameplayCueManager : public UDataAsset
{
	GENERATED_UCLASS_BODY()

	// -------------------------------------------------------------
	// Handling GameplayCues at runtime:
	// -------------------------------------------------------------

	virtual void HandleGameplayCues(AActor* TargetActor, const FGameplayTagContainer& GameplayCueTags, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters);

	virtual void HandleGameplayCue(AActor* TargetActor, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters);

	/** Force any instanced GameplayCueNotifies to stop */
	virtual void EndGameplayCuesFor(AActor* TargetActor);

	// -------------------------------------------------------------
	//  Loading GameplayCueNotifies from ObjectLibraries
	// -------------------------------------------------------------

	/** Loading soft refs to all GameplayCueNotifies */
	void LoadObjectLibraryFromPaths(const TArray<FString>& Paths, bool bFullyLoad);

	UPROPERTY(EditDefaultsOnly, Category = GameplayCue)
	TArray<FGameplayCueNotifyData>	GameplayCueData;

	/** Maps GameplayCue Tag to index into above GameplayCues array. */
	TMap<FGameplayTag, int32>	GameplayCueDataMap;
	
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

	// Fixme: we can combine the AActor* and the int32 into a single struct with a decent hash and avoid double map lookups
	TMap<TWeakObjectPtr<AActor>, TMap<int32, TWeakObjectPtr<AGameplayCueNotify_Actor>>>		NotifyMapActor;

	static FGameplayTag	BaseGameplayCueTag();

	void PrintGameplayCueNotifyMap();

#if WITH_EDITOR
	/** Handles updating an object library when a new asset is created */
	void HandleAssetAdded(UObject *Object);

	/** Handles cleaning up an object library if it matches the passed in object */
	void HandleAssetDeleted(UObject *Object);

	bool RegisteredEditorCallbacks;

	bool bAccelerationMapOutdated;
#endif

private:

	virtual void HandleGameplayCueNotify_Internal(AActor* TargetActor, int32 DataIdx, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters);

	#if WITH_EDITOR
		//This handles the case where GameplayCueNotifications have changed between sessions, which is possible in editor.
		void ReloadObjectLibrary(UWorld* World, const UWorld::InitializationValues IVS);
	#endif

	void LoadObjectLibrary_Internal();

	void AddGameplayCueData_Internal(FGameplayTag  GameplayCueTag, FStringAssetReference StringRef);

	void BuildAccelerationMap_Internal();

	TArray<FString>	LoadedPaths;

	bool bFullyLoad;
};
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTags.h"
#include "GameplayEffect.h"
#include "GameplayCueNotify_Actor.generated.h"


/**
 *	A self contained handler of a GameplayCue. These are similiar to AnimNotifies in implementation.
 *	Instanced GameplayCueNotify which runs arbitrary blueprint code. (TODO: This should be the NON-instanced version!)
 *	
 *	
 *	TODO/Fixme:
 *		-Unsure: Leave K2_HandleGameplayCue in as generic function?
 *		-OnExecute/Active/Remove are more clear, easy to use. Make it harder to share info between events.
 *	
 *	
 */

UCLASS(Blueprintable, meta = (ShowWorldContextPin), hidecategories = (Replication))
class GAMEPLAYABILITIES_API AGameplayCueNotify_Actor : public AActor
{
	GENERATED_UCLASS_BODY()

	/** Does this GameplayCueNotify handle this type of GameplayCueEvent? */
	virtual bool HandlesEvent(EGameplayCueEvent::Type EventType) const;

	UFUNCTION()
	virtual void OnOwnerDestroyed();

	virtual void BeginPlay() override;

	virtual void PostInitProperties() override;

	virtual void Serialize(FArchive& Ar) override;

	virtual void HandleGameplayCue(AActor* MyTarget, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	/** Generic Event Graph event that will get called for every event type */
	UFUNCTION(BlueprintImplementableEvent, Category = "GameplayCueNotify", DisplayName = "HandleGameplayCue")
	void K2_HandleGameplayCue(AActor* MyTarget, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters);

	UFUNCTION(BlueprintNativeEvent, Category = "GameplayCueNotify")
	bool OnExecute(AActor* MyTarget, FGameplayCueParameters Parameters);

	UFUNCTION(BlueprintNativeEvent, Category = "GameplayCueNotify")
	bool OnActive(AActor* MyTarget, FGameplayCueParameters Parameters);

	UFUNCTION(BlueprintNativeEvent, Category = "GameplayCueNotify")
	bool WhileActive(AActor* MyTarget, FGameplayCueParameters Parameters);

	UFUNCTION(BlueprintNativeEvent, Category = "GameplayCueNotify")
	bool OnRemove(AActor* MyTarget, FGameplayCueParameters Parameters);

	UPROPERTY(EditDefaultsOnly, Category=GameplayCue, meta=(Categories="GameplayCue"))
	FGameplayTag	GameplayCueTag;

	/** Mirrors GameplayCueTag in order to be asset registry searchable */
	UPROPERTY(AssetRegistrySearchable)
	FName GameplayCueName;

	/** We will auto destroy this GameplayCueActor when the OnRemove event fires (after OnRemove is called). */
	UPROPERTY(EditDefaultsOnly, Category = GameplayCue)
	bool	bAutoDestroyOnRemove;

	/** If bAutoDestroyOnRemove is true, the actor will stay alive for this many seconds before being auto destroyed. */
	UPROPERTY(EditAnywhere, Category = GameplayCue)
	float AutoDestroyDelay;

	/** Does this Cue override other cues, or is it called in addition to them? E.g., If this is Damage.Physical.Slash, we wont call Damage.Physical afer we run this cue. */
	UPROPERTY(EditDefaultsOnly, Category = GameplayCue)
	bool IsOverride;

	/**
	 *	Does this cue get a new instance for each instigator? For example if two instigators apply a GC to the same source, do we create two of these GameplayCue Notify actors or just one?
	 *	If the notify is simply playing FX or sounds on the source, it should not need unique instances. If this Notify is attaching a beam from the instigator to the target, it does need a unique instance per instigator.
	 */ 	 
	UPROPERTY(EditDefaultsOnly, Category = GameplayCue)
	bool bUniqueInstancePerInstigator;

	/**
	 *	Does this cue get a new instance for each source object? For example if two source objects apply a GC to the same source, do we create two of these GameplayCue Notify actors or just one?
	 *	If the notify is simply playing FX or sounds on the source, it should not need unique instances. If this Notify is attaching a beam from the source object to the target, it does need a unique instance per instigator.
	 */ 	 
	UPROPERTY(EditDefaultsOnly, Category = GameplayCue)
	bool bUniqueInstancePerSourceObject;

private:
	virtual void DeriveGameplayCueTagFromAssetName();
};

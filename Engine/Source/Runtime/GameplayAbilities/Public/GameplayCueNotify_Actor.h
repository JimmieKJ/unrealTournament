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

	UPROPERTY(EditDefaultsOnly, Category = GameplayCue)
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

private:
	virtual void DeriveGameplayCueTagFromAssetName();
};

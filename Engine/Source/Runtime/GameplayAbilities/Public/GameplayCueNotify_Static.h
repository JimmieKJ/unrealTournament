// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTags.h"
#include "GameplayEffect.h"
#include "GameplayCueNotify_Static.generated.h"


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
class GAMEPLAYABILITIES_API UGameplayCueNotify_Static : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Does this GameplayCueNotify handle this type of GameplayCueEvent? */
	virtual bool HandlesEvent(EGameplayCueEvent::Type EventType) const;

	virtual void OnOwnerDestroyed();

	virtual void PostInitProperties() override;

	virtual void Serialize(FArchive& Ar) override;

	virtual void HandleGameplayCue(AActor* MyTarget, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters);

	UWorld* GetWorld() const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	/** Generic Event Graph event that will get called for every event type */
	UFUNCTION(BlueprintImplementableEvent, Category = "GameplayCueNotify", DisplayName = "HandleGameplayCue")
	void K2_HandleGameplayCue(AActor* MyTarget, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "GameplayCueNotify")
	bool OnExecute(AActor* MyTarget, FGameplayCueParameters Parameters) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "GameplayCueNotify")
	bool OnActive(AActor* MyTarget, FGameplayCueParameters Parameters) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "GameplayCueNotify")
	bool WhileActive(AActor* MyTarget, FGameplayCueParameters Parameters) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "GameplayCueNotify")
	bool OnRemove(AActor* MyTarget, FGameplayCueParameters Parameters) const;

	UPROPERTY(EditDefaultsOnly, Category = GameplayCue, meta=(Categories="GameplayCue"))
	FGameplayTag	GameplayCueTag;

	/** Mirrors GameplayCueTag in order to be asset registry searchable */
	UPROPERTY(AssetRegistrySearchable)
	FName GameplayCueName;

	/** Does this Cue override other cues, or is it called in addition to them? E.g., If this is Damage.Physical.Slash, we wont call Damage.Physical afer we run this cue. */
	UPROPERTY(EditDefaultsOnly, Category = GameplayCue)
	bool IsOverride;

private:
	virtual void DeriveGameplayCueTagFromAssetName();
};

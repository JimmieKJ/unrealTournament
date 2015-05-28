// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayCueSet.generated.h"

USTRUCT()
struct FGameplayCueNotifyData
{
	GENERATED_USTRUCT_BODY()

	FGameplayCueNotifyData()
	: LoadedGameplayCueClass(nullptr)
	, ParentDataIdx( INDEX_NONE )
	{
	}

	UPROPERTY(EditAnywhere, Category=GameplayCue)
	FGameplayTag GameplayCueTag;

	UPROPERTY(EditAnywhere, Category=GameplayCue, meta=(AllowedClasses="GameplayCueNotify"))
	FStringAssetReference GameplayCueNotifyObj;

	UPROPERTY(transient)
	UClass* LoadedGameplayCueClass;

	int32 ParentDataIdx;
};

struct FGameplayCueReferencePair
{
	FGameplayTag GameplayCueTag;
	FStringAssetReference StringRef;

	FGameplayCueReferencePair(const FGameplayTag& InGameplayCueTag, const FStringAssetReference& InStringRef)
		: GameplayCueTag(InGameplayCueTag)
		, StringRef(InStringRef)
	{}
};

/**
 *	A set of gameplay cue actors to handle gameplay cue events
 */
UCLASS()
class GAMEPLAYABILITIES_API UGameplayCueSet : public UDataAsset
{
	GENERATED_UCLASS_BODY()

	/** Handles the cue event by spawning the cue actor. Returns true if the event was handled. */
	virtual bool HandleGameplayCue(AActor* TargetActor, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters);

	/** Adds a list of cues to the set */
	virtual void AddCues(const TArray<FGameplayCueReferencePair>& CuesToAdd);

	/** Removes all cues from the set matching any of the supplied tags */
	virtual void RemoveCuesByTags(const FGameplayTagContainer& TagsToRemove);

	/** Removes all cues from the set matching the supplied string refs */
	virtual void RemoveCuesByStringRefs(const TArray<FStringAssetReference>& CuesToRemove);

	/** Removes all cues from the set */
	virtual void Empty();

	virtual void PrintCues() const;

protected:
	virtual bool HandleGameplayCueNotify_Internal(AActor* TargetActor, int32 DataIdx, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters);
	virtual void BuildAccelerationMap_Internal();
	
	static FGameplayTag	BaseGameplayCueTag();

protected:
	UPROPERTY(EditAnywhere, Category=CueSet)
	TArray<FGameplayCueNotifyData> GameplayCueData;

	/** Maps GameplayCue Tag to index into above GameplayCues array. */
	TMap<FGameplayTag, int32> GameplayCueDataMap;
};
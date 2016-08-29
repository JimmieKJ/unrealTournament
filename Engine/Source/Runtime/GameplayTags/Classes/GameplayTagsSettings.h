// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "GameplayTagsSettings.generated.h"

/**
 *	Class for importing GameplayTags directly from a config file.
 *	FGameplayTagsEditorModule::StartupModule adds this class to the Project Settings menu to be edited.
 *	Editing this in Project Settings will output changes to Config/DEfaultEngine.ini.
 *	
 *	Primary advantages of this approach are:
 *	-Adding new tags doesn't require checking out external and editing file (CSV or xls) then reimporting.
 *	-New tags are mergeable since .ini are text and non exclusive checkout.
 *	
 *	To do:
 *	-Better support could be added for adding new tags. We could match existing tags and autocomplete subtags as
 *	the user types (e.g, autocomplete 'Damage.Physical' as the user is adding a 'Damage.Physical.Slash' tag).
 *	
 */
UCLASS(config=GameplayTags, defaultconfig, notplaceable)
class GAMEPLAYTAGS_API UGameplayTagsSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(config, EditAnywhere, Category=GameplayTags)
	TArray<FString>		GameplayTags;

	/** List of tags most frequently replicated */
	UPROPERTY(config, EditAnywhere, Category="Advanced Replication")
	TArray<FString>		CommonlyReplicatedTags;

	UPROPERTY(config, EditAnywhere, Category="Advanced Replication")
	int32 NetIndexFirstBitSegment;

	/** Sorts tags alphabetically */
	void SortTags();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

};

UCLASS(config=GameplayTags, notplaceable)
class GAMEPLAYTAGS_API UGameplayTagsDeveloperSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Allows new tags to be saved into their own INI file. This is make merging easier for non technical developers by setting up their own ini file. */
	UPROPERTY(config, EditAnywhere, Category=GameplayTags)
	FString DeveloperConfigName;
};

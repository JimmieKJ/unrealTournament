// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "GameplayTagAssetInterface.generated.h"

/** Interface for assets which contain gameplay tags */
UINTERFACE(Blueprintable, MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UGameplayTagAssetInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class GAMEPLAYTAGS_API IGameplayTagAssetInterface
{
	GENERATED_IINTERFACE_BODY()

	/**
	 * Get any owned gameplay tags on the asset
	 * 
	 * @param OutTags	[OUT] Set of tags on the asset
	 */
	 UFUNCTION(BlueprintCallable, Category = GameplayTags)
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const=0;

	/**
	 * Check if the asset has a gameplay tag that matches against the specified tag (expands to include parents of asset tags)
	 * 
	 * @param TagToCheck	Tag to check for a match
	 * 
	 * @return True if the asset has a gameplay tag that matches, false if not
	 */
	UFUNCTION(BlueprintCallable, Category=GameplayTags)
	virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const;

	/**
	 * Check if the asset has gameplay tags that matches against all of the specified tags (expands to include parents of asset tags)
	 * 
	 * @param TagContainer			Tag container to check for a match
	 * @param bCountEmptyAsMatch	If true, the parameter tag container will count as matching, even if it's empty
	 * 
	 * @return True if the asset has matches all of the gameplay tags
	 */
	UFUNCTION(BlueprintCallable, Category=GameplayTags)
	virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer, bool bCountEmptyAsMatch = true) const;

	/**
	 * Check if the asset has gameplay tags that matches against any of the specified tags (expands to include parents of asset tags)
	 * 
	 * @param TagContainer			Tag container to check for a match
	 * @param bCountEmptyAsMatch	If true, the parameter tag container will count as matching, even if it's empty
	 * 
	 * @return True if the asset has matches any of the gameplay tags
	 */
	UFUNCTION(BlueprintCallable, Category=GameplayTags)
	virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer, bool bCountEmptyAsMatch = true) const;
};


// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "GameplayTagAssetInterface.h"
#include "BlueprintGameplayTagLibrary.generated.h"

// Forward declarations
struct FGameplayTag;
struct FGameplayTagContainer;

UCLASS(MinimalAPI)
class UBlueprintGameplayTagLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/**
	 * Determine if the specified gameplay tags match, given the specified match types
	 * 
	 * @param TagOne			First tag to check
	 * @param TagTwo			Second tag to check
	 * @param TagOneMatchType	Matching type to use on the first tag
	 * @param TagTwoMatchType	Matching type to use on the second tag
	 * 
	 * @return True if the tags match, false if they do not
	 */
	UFUNCTION(BlueprintPure, Category="GameplayTags")
	static bool DoGameplayTagsMatch(const FGameplayTag& TagOne, const FGameplayTag& TagTwo, TEnumAsByte<EGameplayTagMatchType::Type> TagOneMatchType, TEnumAsByte<EGameplayTagMatchType::Type> TagTwoMatchType);

	/**
	 * Get the number of gameplay tags in the specified container
	 * 
	 * @param TagContainer	Tag container to get the number of tags from
	 * 
	 * @return The number of tags in the specified container
	 */
	UFUNCTION(BlueprintPure, Category="GameplayTags|Tag Container")
	static int32 GetNumGameplayTagsInContainer(const FGameplayTagContainer& TagContainer);

	/**
	 * Check if the specified tag container has the specified tag, using the specified tag matching types
	 *
	 * @param TagContainer				Container to check for the tag
	 * @param ContainerTagsMatchType	Matching options to use for tags inside the container
	 * @param Tag						Tag to check for in the container
	 * @param TagMatchType				Matching option to use for the tag
	 *
	 * @return True if the container has the specified tag, false if it does not
	 */
	UFUNCTION(BlueprintPure, Category="GameplayTags|Tag Container")
	static bool DoesContainerHaveTag(const FGameplayTagContainer& TagContainer, TEnumAsByte<EGameplayTagMatchType::Type> ContainerTagsMatchType, const FGameplayTag& Tag, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType);

	/**
	 * Check if the specified tag container matches ANY of the tags in the other container. Matching is performed by expanding the tag container out to include all of its parent tags as well.
	 * 
	 * @param TagContainer			Container to check if it matches any of the tags in the other container
	 * @param OtherContainer		Container to check against
	 * @param bCountEmptyAsMatch	If true, the other container will count as a match, even if it's empty
	 * 
	 * @return True if the container matches ANY of the tags in the other container
	 */
	UFUNCTION(BlueprintPure, Category="GameplayTags|Tag Container")
	static bool DoesContainerMatchAnyTagsInContainer(const FGameplayTagContainer& TagContainer, const FGameplayTagContainer& OtherContainer, bool bCountEmptyAsMatch);

	/**
	 * Check if the specified tag container matches ALL of the tags in the other container. Matching is performed by expanding the tag container out to include all of its parent tags as well.
	 * 
	 * @param TagContainer			Container to check if it matches all of the tags in the other container
	 * @param OtherContainer		Container to check against
	 * @param bCountEmptyAsMatch	If true, the other container will count as a match, even if it's empty
	 * 
	 * @return True if the container matches ALL of the tags in the other container
	 */
	UFUNCTION(BlueprintPure, Category="GameplayTags|Tag Container")
	static bool DoesContainerMatchAllTagsInContainer(const FGameplayTagContainer& TagContainer, const FGameplayTagContainer& OtherContainer, bool bCountEmptyAsMatch);

	/**
	 * Creates a literal FGameplayTag
	 *
	 * @param	Value	value to set the FGameplayTag to
	 *
	 * @return	The literal FGameplayTag
	 */
	UFUNCTION(BlueprintPure, Category = "GameplayTags")
	static FGameplayTag MakeLiteralGameplayTag(FGameplayTag Value);

	/**
	 * Check Gameplay tags in the interface has all of the specified tags in the tag container (expands to include parents of asset tags)
	 *
	 * @param TagContainerInterface		An Interface to a tag container
	 * @param OtherContainer			A Tag Container
	 *
	 * @return True if the tagcontainer in the interface has all the tags inside the container.
	 */
	UFUNCTION(BlueprintPure, meta = (BlueprintInternalUseOnly = "TRUE"))
	static bool HasAllMatchingGameplayTags(TScriptInterface<IGameplayTagAssetInterface> TagContainerInterface, const FGameplayTagContainer& OtherContainer, bool bCountEmptyAsMatch);

	/**
	* Check if the specified tag container has the specified tag, using the specified tag matching types
	*
	* @param TagContainerInterface		An Interface to a tag container
	* @param ContainerTagsMatchType		Matching options to use for tags inside the container
	* @param Tag						Tag to check for in the container
	* @param TagMatchType				Matching option to use for the tag
	*
	* @return True if the container has the specified tag, false if it does not
	*/
	UFUNCTION(BlueprintPure, meta = (BlueprintInternalUseOnly = "TRUE"))
	static bool DoesTagAssetInterfaceHaveTag(TScriptInterface<IGameplayTagAssetInterface> TagContainerInterface, TEnumAsByte<EGameplayTagMatchType::Type> ContainerTagsMatchType, const FGameplayTag& Tag, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType);

};

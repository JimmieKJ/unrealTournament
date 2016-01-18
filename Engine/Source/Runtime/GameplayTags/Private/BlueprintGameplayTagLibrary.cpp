// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsModulePrivatePCH.h"

UBlueprintGameplayTagLibrary::UBlueprintGameplayTagLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBlueprintGameplayTagLibrary::DoGameplayTagsMatch(const FGameplayTag& TagOne, const FGameplayTag& TagTwo, TEnumAsByte<EGameplayTagMatchType::Type> TagOneMatchType, TEnumAsByte<EGameplayTagMatchType::Type> TagTwoMatchType)
{
	return TagOne.Matches(TagOneMatchType, TagTwo, TagTwoMatchType);
}

int32 UBlueprintGameplayTagLibrary::GetNumGameplayTagsInContainer(const FGameplayTagContainer& TagContainer)
{
	return TagContainer.Num();
}

bool UBlueprintGameplayTagLibrary::DoesContainerHaveTag(const FGameplayTagContainer& TagContainer, TEnumAsByte<EGameplayTagMatchType::Type> ContainerTagsMatchType, const FGameplayTag& Tag, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType)
{
	return (TagContainer.HasTag(Tag, ContainerTagsMatchType, TagMatchType));
}

bool UBlueprintGameplayTagLibrary::DoesContainerMatchAnyTagsInContainer(const FGameplayTagContainer& TagContainer, const FGameplayTagContainer& OtherContainer, bool bCountEmptyAsMatch)
{
	return TagContainer.MatchesAny(OtherContainer, bCountEmptyAsMatch);
}

bool UBlueprintGameplayTagLibrary::DoesContainerMatchAllTagsInContainer(const FGameplayTagContainer& TagContainer, const FGameplayTagContainer& OtherContainer, bool bCountEmptyAsMatch)
{
	return TagContainer.MatchesAll(OtherContainer, bCountEmptyAsMatch);
}

bool UBlueprintGameplayTagLibrary::DoesContainerMatchTagQuery(const FGameplayTagContainer& TagContainer, const FGameplayTagQuery& TagQuery)
{
	return TagQuery.Matches(TagContainer);
}

FGameplayTag UBlueprintGameplayTagLibrary::MakeLiteralGameplayTag(FGameplayTag Value)
{
	return Value;
}

FGameplayTagQuery UBlueprintGameplayTagLibrary::MakeGameplayTagQuery(FGameplayTagQuery TagQuery)
{
	return TagQuery;
}

bool UBlueprintGameplayTagLibrary::HasAllMatchingGameplayTags(TScriptInterface<IGameplayTagAssetInterface> TagContainerInterface, const FGameplayTagContainer& OtherContainer, bool bCountEmptyAsMatch)
{
	if (TagContainerInterface.GetInterface() == NULL)
	{
		if (bCountEmptyAsMatch)
		{
			return (OtherContainer.Num() == 0);
		}
		return false;
	}

	return TagContainerInterface->HasAllMatchingGameplayTags(OtherContainer, bCountEmptyAsMatch);
}

bool UBlueprintGameplayTagLibrary::DoesTagAssetInterfaceHaveTag(TScriptInterface<IGameplayTagAssetInterface> TagContainerInterface, TEnumAsByte<EGameplayTagMatchType::Type> ContainerTagsMatchType, const FGameplayTag& Tag, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType)
{
	if (TagContainerInterface.GetInterface() == NULL)
	{
		return false;
	}

	FGameplayTagContainer OwnedTags;
	TagContainerInterface->GetOwnedGameplayTags(OwnedTags);
	return (OwnedTags.HasTag(Tag, ContainerTagsMatchType, TagMatchType));
}

bool UBlueprintGameplayTagLibrary::AppendGameplayTagContainers(const FGameplayTagContainer& InTagContainer, UPARAM(ref) FGameplayTagContainer& InOutTagContainer)
{
	InOutTagContainer.AppendTags(InTagContainer);

	return true;
}

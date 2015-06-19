// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_GameplayTags.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_ActorBase.h"
#include "GameplayTagAssetInterface.h"

UEnvQueryTest_GameplayTags::UEnvQueryTest_GameplayTags(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Cost = EEnvTestCost::Low;
	SetWorkOnFloatValues(false);

	// To search for GameplayTags, currently we require the item type to be an actor.  Certainly it must at least be a
	// class of some sort to be able to find the interface required.
	ValidItemType = UEnvQueryItemType_ActorBase::StaticClass();
}

bool UEnvQueryTest_GameplayTags::SatisfiesTest(IGameplayTagAssetInterface* ItemGameplayTagAssetInterface) const
{
	// Currently we're requiring that the test only be run on items that implement the interface.  In theory, we could
	// (instead of checking) support correctly passing or failing on items that don't implement the interface or
	// just have a NULL item by testing them as though they have the interface with no gameplay tags.  However, that
	// seems potentially confusing, since certain tests could actually pass.
	check(ItemGameplayTagAssetInterface != NULL);
// 	if (GameplayTags.Num() == 0)
// 	{
// 		return true;
// 	}
// 
// 	if (ItemGameplayTagAssetInterface == NULL)
// 	{
// 		return false;
// 	}
	
	switch (TagsToMatch)
	{
		case EGameplayContainerMatchType::All:
			return ItemGameplayTagAssetInterface->HasAllMatchingGameplayTags(GameplayTags);

		case EGameplayContainerMatchType::Any:
			return ItemGameplayTagAssetInterface->HasAnyMatchingGameplayTags(GameplayTags);

		default:
		{
			UE_LOG(LogBehaviorTree, Warning, TEXT("Invalid value for TagsToMatch (EGameplayContainerMatchType) %d.  Should only be Any or All."), static_cast<int32>(TagsToMatch));
			return false;
		}
	}

	return false;
}

void UEnvQueryTest_GameplayTags::RunTest(FEnvQueryInstance& QueryInstance) const
{
	BoolValue.BindData(QueryInstance.Owner.Get(), QueryInstance.QueryID);
	bool bWantsValid = BoolValue.GetValue();

	// loop through all items
	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		AActor* ItemActor = GetItemActor(QueryInstance, It.GetIndex());
		IGameplayTagAssetInterface* GameplayTagAssetInterface = Cast<IGameplayTagAssetInterface>(ItemActor);
		if (GameplayTagAssetInterface != NULL)
		{
			bool bSatisfiesTest = SatisfiesTest(GameplayTagAssetInterface);

			// bWantsValid is the basically the opposite of bInverseCondition in BTDecorator.  Possibly we should
			// rename to make these more consistent.
			It.SetScore(TestPurpose, FilterType, bSatisfiesTest, bWantsValid);
		}
		else // If no GameplayTagAssetInterface is found, this test doesn't apply at all, so just skip the item.
		{	 // Currently 
			It.SkipItem();
		}
	}
}

FText UEnvQueryTest_GameplayTags::GetDescriptionDetails() const
{
	return GameplayTags.ToMatchingText(TagsToMatch, !BoolValue.DefaultValue);
}

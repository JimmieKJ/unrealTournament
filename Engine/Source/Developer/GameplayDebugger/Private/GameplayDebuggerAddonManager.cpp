// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerPrivatePCH.h"
#include "GameplayDebuggerExtension.h"
#include "GameplayDebuggerAddonManager.h"

FGameplayDebuggerAddonManager::FGameplayDebuggerAddonManager()
{
}

void FGameplayDebuggerAddonManager::RegisterCategory(FName CategoryName, IGameplayDebugger::FOnGetCategory MakeInstanceDelegate, EGameplayDebuggerCategoryState CategoryState, int32 SlotIdx)
{
	FGameplayDebuggerCategoryInfo NewInfo;
	NewInfo.MakeInstanceDelegate = MakeInstanceDelegate;
	NewInfo.CategoryState = CategoryState;
	NewInfo.SlotIdx = SlotIdx;

	CategoryMap.Add(CategoryName, NewInfo);
}

void FGameplayDebuggerAddonManager::UnregisterCategory(FName CategoryName)
{
	CategoryMap.Remove(CategoryName);
}

void FGameplayDebuggerAddonManager::NotifyCategoriesChanged()
{
	struct FSlotInfo
	{
		FName CategoryName;
		int32 CategoryId;
		int32 SlotIdx;

		bool operator<(const FSlotInfo& Other) const { return (SlotIdx == Other.SlotIdx) ? (CategoryName < Other.CategoryName) : (SlotIdx < Other.SlotIdx); }
	};

	TArray<FSlotInfo> AssignList;
	TSet<int32> OccupiedSlots;
	int32 CategoryId = 0;
	for (auto It : CategoryMap)
	{
		FSlotInfo SlotInfo;
		SlotInfo.CategoryId = CategoryId;
		SlotInfo.CategoryName = It.Key;
		SlotInfo.SlotIdx = It.Value.SlotIdx;
		CategoryId++;

		AssignList.Add(SlotInfo);
		OccupiedSlots.Add(It.Value.SlotIdx);
	}

	AssignList.Sort();

	int32 MaxSlotIdx = 0;
	for (int32 Idx = 0; Idx < AssignList.Num(); Idx++)
	{
		FSlotInfo& SlotInfo = AssignList[Idx];
		if (SlotInfo.SlotIdx == INDEX_NONE)
		{
			int32 FreeSlotIdx = 0;
			while (OccupiedSlots.Contains(FreeSlotIdx))
			{
				FreeSlotIdx++;
			}

			SlotInfo.SlotIdx = FreeSlotIdx;
			OccupiedSlots.Add(FreeSlotIdx);
		}

		MaxSlotIdx = FMath::Max(MaxSlotIdx, SlotInfo.SlotIdx);
	}

	SlotMap.Reset();
	SlotNames.Reset();
	SlotMap.AddDefaulted(MaxSlotIdx + 1);
	SlotNames.AddDefaulted(MaxSlotIdx + 1);

	for (int32 Idx = 0; Idx < AssignList.Num(); Idx++)
	{
		FSlotInfo& SlotInfo = AssignList[Idx];

		if (SlotNames[SlotInfo.SlotIdx].Len())
		{
			SlotNames[SlotInfo.SlotIdx] += TEXT('+');
		}

		SlotNames[SlotInfo.SlotIdx] += SlotInfo.CategoryName.ToString();
		SlotMap[SlotInfo.SlotIdx].Add(SlotInfo.CategoryId);
	}

	OnCategoriesChanged.Broadcast();
}

void FGameplayDebuggerAddonManager::CreateCategories(AGameplayDebuggerCategoryReplicator& Owner, TArray<TSharedRef<FGameplayDebuggerCategory> >& CategoryObjects)
{
	UWorld* World = Owner.GetWorld();
	const ENetMode NetMode = World->GetNetMode();
	const bool bHasAuthority = (NetMode != NM_Client);
	const bool bIsLocal = (NetMode != NM_DedicatedServer);
	const bool bIsSimulate = FGameplayDebuggerAddonBase::IsSimulateInEditor();

	TArray<TSharedRef<FGameplayDebuggerCategory> > UnsortedCategories;
	for (auto It : CategoryMap)
	{
		TSharedRef<FGameplayDebuggerCategory> CategoryObjectRef = It.Value.MakeInstanceDelegate.Execute();
		FGameplayDebuggerCategory& CategoryObject = CategoryObjectRef.Get();
		CategoryObject.RepOwner = &Owner;
		CategoryObject.CategoryId = CategoryObjects.Num();
		CategoryObject.CategoryName = It.Key;
		CategoryObject.bHasAuthority = bHasAuthority;
		CategoryObject.bIsLocal = bIsLocal;
		CategoryObject.bIsEnabled =
			(It.Value.CategoryState == EGameplayDebuggerCategoryState::EnabledInGameAndSimulate) ||
			(It.Value.CategoryState == EGameplayDebuggerCategoryState::EnabledInGame && !bIsSimulate) ||
			(It.Value.CategoryState == EGameplayDebuggerCategoryState::EnabledInSimulate && bIsSimulate);

		UnsortedCategories.Add(CategoryObjectRef);
	}

	// sort by slots for drawing order
	for (int32 SlotIdx = 0; SlotIdx < SlotMap.Num(); SlotIdx++)
	{
		for (int32 Idx = 0; Idx < SlotMap[SlotIdx].Num(); Idx++)
		{
			const int32 CategoryId = SlotMap[SlotIdx][Idx];
			CategoryObjects.Add(UnsortedCategories[CategoryId]);
		}
	}
}

void FGameplayDebuggerAddonManager::RegisterExtension(FName ExtensionName, IGameplayDebugger::FOnGetExtension MakeInstanceDelegate)
{
	ExtensionMap.Add(ExtensionName, MakeInstanceDelegate);
}

void FGameplayDebuggerAddonManager::UnregisterExtension(FName ExtensionName)
{
	ExtensionMap.Remove(ExtensionName);
}

void FGameplayDebuggerAddonManager::NotifyExtensionsChanged()
{
	OnExtensionsChanged.Broadcast();
}

void FGameplayDebuggerAddonManager::CreateExtensions(AGameplayDebuggerCategoryReplicator& Replicator, TArray<TSharedRef<FGameplayDebuggerExtension> >& ExtensionObjects)
{
	for (auto It : ExtensionMap)
	{
		TSharedRef<FGameplayDebuggerExtension> ExtensionObjectRef = It.Value.Execute();
		FGameplayDebuggerExtension& ExtensionObject = ExtensionObjectRef.Get();
		ExtensionObject.RepOwner = &Replicator;

		ExtensionObjects.Add(ExtensionObjectRef);
	}
}

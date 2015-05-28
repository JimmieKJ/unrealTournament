// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AbilitySystemComponent.h"
#include "GameplayCueInterface.h"
#include "GameplayTagsModule.h"
#include "GameplayCueSet.h"

UGameplayCueInterface::UGameplayCueInterface(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void IGameplayCueInterface::DispatchBlueprintCustomHandler(AActor* Actor, UFunction* Func, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	GameplayCueInterface_eventBlueprintCustomHandler_Parms Parms;
	Parms.EventType = EventType;
	Parms.Parameters = Parameters;

	Actor->ProcessEvent(Func, &Parms);
}

void IGameplayCueInterface::HandleGameplayCues(AActor *Self, const FGameplayTagContainer& GameplayCueTags, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	for (auto TagIt = GameplayCueTags.CreateConstIterator(); TagIt; ++TagIt)
	{
		HandleGameplayCue(Self, *TagIt, EventType, Parameters);
	}
}

void IGameplayCueInterface::HandleGameplayCue(AActor *Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	// Look up a custom function for this gameplay tag. 
	bool bFoundHandler = false;
	bool bShouldContinue = true;

	UClass* Class = Self->GetClass();
	IGameplayTagsModule& GameplayTagsModule = IGameplayTagsModule::Get();
	FGameplayTagContainer TagAndParentsContainer = GameplayTagsModule.GetGameplayTagsManager().RequestGameplayTagParents(GameplayCueTag);

	Parameters.OriginalTag = GameplayCueTag;

	for (auto InnerTagIt = TagAndParentsContainer.CreateConstIterator(); InnerTagIt && bShouldContinue; ++InnerTagIt)
	{
		UFunction* Func = NULL;
		FName CueName = InnerTagIt->GetTagName();

		Func = Class->FindFunctionByName(CueName, EIncludeSuperFlag::IncludeSuper);
		// If the handler calls ForwardGameplayCueToParent, keep calling functions until one consumes the cue and doesn't forward it
		while (bShouldContinue && Func)
		{			
			Parameters.MatchedTagName = CueName;

			// Reset the forward parameter now, so we can check it after function
			bForwardToParent = false;
			IGameplayCueInterface::DispatchBlueprintCustomHandler(Self, Func, EventType, Parameters);
			
			bShouldContinue = bForwardToParent;
			bFoundHandler = true;
			Func = Func->GetSuperFunction();
		}

		if (bShouldContinue)
		{
			// Native functions cant be named with ".", so look for them with _. 
			FName NativeCueFuncName = *CueName.ToString().Replace(TEXT("."), TEXT("_"));
			Func = Class->FindFunctionByName(NativeCueFuncName, EIncludeSuperFlag::IncludeSuper);

			while (bShouldContinue && Func)
			{
				Parameters.MatchedTagName = CueName; // purposefully returning the . qualified name.

				// Reset the forward parameter now, so we can check it after function
				bForwardToParent = false;
				IGameplayCueInterface::DispatchBlueprintCustomHandler(Self, Func, EventType, Parameters);

				bShouldContinue = bForwardToParent;
				bFoundHandler = true;
				Func = Func->GetSuperFunction();
			}
		}
	}

	if (bShouldContinue)
	{
		TArray<UGameplayCueSet*> Sets;
		GetGameplayCueSets(Sets);
		for (UGameplayCueSet* Set : Sets)
		{
			bShouldContinue = Set->HandleGameplayCue(Self, GameplayCueTag, EventType, Parameters);
			if (!bShouldContinue)
			{
				break;
			}
		}
	}

	if (bShouldContinue)
	{
		Parameters.MatchedTagName = GameplayCueTag.GetTagName();
		GameplayCueDefaultHandler(EventType, Parameters);
	}
}

void IGameplayCueInterface::GameplayCueDefaultHandler(EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	// No default handler, subclasses can implement
}

void IGameplayCueInterface::ForwardGameplayCueToParent()
{
	// Consumed by HandleGameplayCue
	bForwardToParent = true;
}

void FActiveGameplayCue::PreReplicatedRemove(const struct FActiveGameplayCueContainer &InArray)
{
	if (PredictionKey.IsLocalClientKey() == false)
	{
		// If predicted ignore the add/remove
		InArray.Owner->InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::Removed);
		InArray.Owner->UpdateTagMap(GameplayCueTag, -1);
	}
}

void FActiveGameplayCue::PostReplicatedAdd(const struct FActiveGameplayCueContainer &InArray)
{
	if (PredictionKey.IsLocalClientKey() == false)
	{
		// If predicted ignore the add/remove
		InArray.Owner->InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::WhileActive);
		InArray.Owner->UpdateTagMap(GameplayCueTag, 1);
	}
}

void FActiveGameplayCueContainer::AddCue(const FGameplayTag& Tag, const FPredictionKey& PredictionKey)
{
	UWorld* World = Owner->GetWorld();

	// Store the prediction key so the client can investigate it
	FActiveGameplayCue	NewCue;
	NewCue.GameplayCueTag = Tag;
	NewCue.PredictionKey = PredictionKey;
	MarkItemDirty(NewCue);

	GameplayCues.Add(NewCue);
	Owner->UpdateTagMap(Tag, 1);
}

void FActiveGameplayCueContainer::RemoveCue(const FGameplayTag& Tag)
{
	for (int32 idx=0; idx < GameplayCues.Num(); ++idx)
	{
		FActiveGameplayCue& Cue = GameplayCues[idx];

		if (Cue.GameplayCueTag == Tag)
		{
			GameplayCues.RemoveAt(idx);
			MarkArrayDirty();
			Owner->UpdateTagMap(Tag, -1);
			return;
		}
	}
}
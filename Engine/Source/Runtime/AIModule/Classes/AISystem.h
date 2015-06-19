// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Misc/CoreMisc.h"
#include "EngineDefines.h"
#include "Engine/World.h"
#include "AI/AISystemBase.h"
#include "AISystem.generated.h"

class UBehaviorTreeManager;
class UEnvQueryManager;
class UAIPerceptionSystem;
class UAIAsyncTaskBlueprintProxy;
class UAIHotSpotManager;
class UBlackboardData;
class UBlackboardComponent;

UCLASS(config=Engine, defaultconfig)
class AIMODULE_API UAISystem : public UAISystemBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(globalconfig, EditAnywhere, Category = "AISystem", meta = (MetaClass = "AIPerceptionSystem", DisplayName = "Perception System Class"))
	FStringClassReference PerceptionSystemClassName;

	UPROPERTY(globalconfig, EditAnywhere, Category = "AISystem", meta = (MetaClass = "AIHotSpotManager", DisplayName = "AIHotSpotManager Class"))
	FStringClassReference HotSpotManagerClassName;

protected:
	/** Behavior tree manager used by game */
	UPROPERTY(Transient)
	UBehaviorTreeManager* BehaviorTreeManager;

	/** Environment query manager used by game */
	UPROPERTY(Transient)
	UEnvQueryManager* EnvironmentQueryManager;

	UPROPERTY(Transient)
	UAIPerceptionSystem* PerceptionSystem;

	UPROPERTY(Transient)
	TArray<UAIAsyncTaskBlueprintProxy*> AllProxyObjects;

	UPROPERTY(Transient)
	UAIHotSpotManager* HotSpotManager;

	typedef TMultiMap<TWeakObjectPtr<UBlackboardData>, TWeakObjectPtr<UBlackboardComponent>> FBlackboardDataToComponentsMap;

	/** UBlackboardComponent instances that reference the blackboard data definition */
	FBlackboardDataToComponentsMap BlackboardDataToComponentsMap;

	FDelegateHandle ActorSpawnedDelegateHandle;
	
public:
	virtual void BeginDestroy() override;
	
	virtual void PostInitProperties() override;

	// UAISystemBase begin		
	virtual void InitializeActorsForPlay(bool bTimeGotReset) override;
	virtual void WorldOriginLocationChanged(FIntVector OldOriginLocation, FIntVector NewOriginLocation) override;
	virtual void CleanupWorld(bool bSessionEnded = true, bool bCleanupResources = true, UWorld* NewWorld = NULL) override;
	virtual void StartPlay() override;
	// UAISystemBase end
	
	/** Behavior tree manager getter */
	FORCEINLINE UBehaviorTreeManager* GetBehaviorTreeManager() { return BehaviorTreeManager; }
	/** Behavior tree manager const getter */
	FORCEINLINE const UBehaviorTreeManager* GetBehaviorTreeManager() const { return BehaviorTreeManager; }

	/** Behavior tree manager getter */
	FORCEINLINE UEnvQueryManager* GetEnvironmentQueryManager() { return EnvironmentQueryManager; }
	/** Behavior tree manager const getter */
	FORCEINLINE const UEnvQueryManager* GetEnvironmentQueryManager() const { return EnvironmentQueryManager; }

	FORCEINLINE UAIPerceptionSystem* GetPerceptionSystem() { return PerceptionSystem; }
	FORCEINLINE const UAIPerceptionSystem* GetPerceptionSystem() const { return PerceptionSystem; }

	FORCEINLINE UAIHotSpotManager* GetHotSpotManager() { return HotSpotManager; }
	FORCEINLINE const UAIHotSpotManager* GetHotSpotManager() const { return HotSpotManager; }

	FORCEINLINE static UAISystem* GetCurrentSafe(UWorld* World) 
	{ 
		return World != nullptr ? Cast<UAISystem>(World->GetAISystem()) : NULL;
	}

	FORCEINLINE static UAISystem* GetCurrent(UWorld& World)
	{
		return Cast<UAISystem>(World.GetAISystem());
	}

	FORCEINLINE UWorld* GetOuterWorld() const { return Cast<UWorld>(GetOuter()); }

	virtual UWorld* GetWorld() const override { return GetOuterWorld(); }
	
	FORCEINLINE void AddReferenceFromProxyObject(UAIAsyncTaskBlueprintProxy* BlueprintProxy) { AllProxyObjects.AddUnique(BlueprintProxy); }

	FORCEINLINE void RemoveReferenceToProxyObject(UAIAsyncTaskBlueprintProxy* BlueprintProxy) { AllProxyObjects.RemoveSwap(BlueprintProxy); }

	//----------------------------------------------------------------------//
	// cheats
	//----------------------------------------------------------------------//
	UFUNCTION(exec)
	virtual void AIIgnorePlayers();

	UFUNCTION(exec)
	virtual void AILoggingVerbose();

	/** insta-runs EQS query for given Target */
	void RunEQS(const FString& QueryName, UObject* Target);

	/**
	* Iterator for traversing all UBlackboardComponent instances associated
	* with this blackboard data asset. This is a forward only iterator.
	*/
	struct FBlackboardDataToComponentsIterator
	{
	public:
		FBlackboardDataToComponentsIterator(FBlackboardDataToComponentsMap& BlackboardDataToComponentsMap, class UBlackboardData* BlackboardAsset);

		FORCEINLINE FBlackboardDataToComponentsIterator& operator++()
		{
			++GetCurrentIteratorRef();
			TryMoveIteratorToParentBlackboard();
			return *this;
		}
		FORCEINLINE FBlackboardDataToComponentsIterator operator++(int)
		{
			FBlackboardDataToComponentsIterator Tmp(*this);
			++GetCurrentIteratorRef();
			TryMoveIteratorToParentBlackboard();
			return Tmp;
		}

		SAFE_BOOL_OPERATORS(FBlackboardDataToComponentsIterator);
		FORCEINLINE_EXPLICIT_OPERATOR_BOOL() const { return CurrentIteratorIndex < Iterators.Num() && (bool)GetCurrentIteratorRef(); }
		FORCEINLINE bool operator !() const { return !(bool)*this; }

		FORCEINLINE UBlackboardData* Key() const { return GetCurrentIteratorRef().Key().Get(); }
		FORCEINLINE UBlackboardComponent* Value() const { return GetCurrentIteratorRef().Value().Get(); }

	private:
		FORCEINLINE const FBlackboardDataToComponentsMap::TConstKeyIterator& GetCurrentIteratorRef() const { return Iterators[CurrentIteratorIndex]; }
		FORCEINLINE FBlackboardDataToComponentsMap::TConstKeyIterator& GetCurrentIteratorRef() { return Iterators[CurrentIteratorIndex]; }

		void TryMoveIteratorToParentBlackboard()
		{
			if (!GetCurrentIteratorRef() && CurrentIteratorIndex < Iterators.Num() - 1)
			{
				++CurrentIteratorIndex;
				TryMoveIteratorToParentBlackboard(); // keep incrementing until we find a valid iterator.
			}
		}

		int32 CurrentIteratorIndex;

		static const int32 InlineSize = 8;
		TArray<FBlackboardDataToComponentsMap::TConstKeyIterator, TInlineAllocator<InlineSize>> Iterators;
	};

	/**
	* Registers a UBlackboardComponent instance with this blackboard data asset.
	* This will also register the component for each parent UBlackboardData
	* asset. This should be called after the component has been initialized
	* (i.e. InitializeComponent). The user is responsible for calling
	* UnregisterBlackboardComponent (i.e. UninitializeComponent).
	*/
	void RegisterBlackboardComponent(class UBlackboardData& BlackboardAsset, class UBlackboardComponent& BlackboardComp);

	/**
	* Unregisters a UBlackboardComponent instance with this blackboard data
	* asset. This should be called before the component has been uninitialized
	* (i.e. UninitializeComponent).
	*/
	void UnregisterBlackboardComponent(class UBlackboardData& BlackboardAsset, class UBlackboardComponent& BlackboardComp);

	/**
	* Creates a forward only iterator for that will iterate all
	* UBlackboardComponent instances that reference the specified
	* BlackboardAsset and it's parents.
	*/
	FBlackboardDataToComponentsIterator CreateBlackboardDataToComponentsIterator(class UBlackboardData& BlackboardAsset);

protected:
	virtual void OnActorSpawned(AActor* SpawnedActor);
};

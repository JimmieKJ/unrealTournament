// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tickable.h"
#include "AIPerceptionTypes.h"
#include "AIPerceptionSystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAIPerception, Warning, All);

class APawn;

/**
 *	By design checks perception between hostile teams
 */
UCLASS(ClassGroup=AI, config=Game, defaultconfig)
class AIMODULE_API UAIPerceptionSystem : public UObject, public FTickableGameObject
{
	GENERATED_BODY()
		
public:

	UAIPerceptionSystem(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// We need to implement GetWorld() so that any EQS-related blueprints (such as blueprint contexts) can implement
	// GetWorld() and so provide access to blueprint nodes using hidden WorldContextObject parameters.
	virtual UWorld* GetWorld() const override;

	/** [FTickableGameObject] tick function */
	virtual void Tick(float DeltaTime) override;

	/** [FTickableGameObject] always tick, unless it's the default object */
	virtual bool IsTickable() const override { return HasAnyFlags(RF_ClassDefaultObject) == false; }

	/** [FTickableGameObject] tick stats */
	virtual TStatId GetStatId() const override;

protected:	
	AIPerception::FListenerMap ListenerContainer;

	UPROPERTY()
	TArray<UAISense*> Senses;

	UPROPERTY(config, EditAnywhere, Category = Perception)
	float PerceptionAgingRate;

	FActorEndPlaySignature::FDelegate StimuliSourceEndPlayDelegate;

	// not a UPROPERTY on purpose so that we have a control over when stuff gets removed from the map
	TMap<const AActor*, FPerceptionStimuliSource> RegisteredStimuliSources;

	/** gets set to true if as as result of stimuli aging (that's done outside of Tick, on timer)
	 *	one of listeners requires an update. The update, as usual is tone in Tick where 
	 *	bSomeListenersNeedUpdateDueToStimuliAging gets reset to false */
	uint32 bSomeListenersNeedUpdateDueToStimuliAging : 1;

	/** gets set to true when perception system gets notified about a stimuli source's end play */
	uint32 bStimuliSourcesRefreshRequired : 1;

	uint32 bHandlePawnNotification : 1;

	struct FDelayedStimulus
	{
		float DeliveryTimestamp;
		FPerceptionListenerID ListenerId;
		TWeakObjectPtr<AActor> Instigator;
		FAIStimulus Stimulus;
	};

	TArray<FDelayedStimulus> DelayedStimuli;

	struct FPerceptionSourceRegistration
	{
		FAISenseID SenseID;
		TWeakObjectPtr<AActor> Source;

		FPerceptionSourceRegistration(FAISenseID InSenseID, AActor* SourceActor)
			: SenseID(InSenseID), Source(SourceActor)
		{}

		FORCEINLINE bool operator==(const FPerceptionSourceRegistration& Other) const
		{
			return SenseID == Other.SenseID && Source == Other.Source;
		}
	};
	TArray<FPerceptionSourceRegistration> SourcesToRegister;

public:
	/** UObject begin */
	virtual void PostInitProperties() override;
	/* UObject end */

	FORCEINLINE bool IsSenseInstantiated(const FAISenseID& SenseID) const { return SenseID.IsValid() && Senses.IsValidIndex(SenseID) && Senses[SenseID] != nullptr; }

	/** Registers listener if not registered */
	void UpdateListener(UAIPerceptionComponent& Listener);
	void UnregisterListener(UAIPerceptionComponent& Listener);

	template<typename FEventClass>
	void OnEvent(const FEventClass& Event)
	{
		const FAISenseID SenseID = UAISense::GetSenseID<typename FEventClass::FSenseClass>();
		if (Senses.IsValidIndex(SenseID) && Senses[SenseID] != nullptr)
		{
			((typename FEventClass::FSenseClass*)Senses[SenseID])->RegisterEvent(Event);
		}
		// otherwise there's no one interested in this event, skip it.
	}

	template<typename FEventClass>
	static void OnEvent(UWorld* World, const FEventClass& Event)
	{
		UAIPerceptionSystem* PerceptionSys = GetCurrent(World);
		if (PerceptionSys != NULL)
		{
			PerceptionSys->OnEvent(Event);
		}
	}

	UFUNCTION(BlueprintCallable, Category = "AI|Perception")
	void ReportEvent(UAISenseEvent* PerceptionEvent);

	UFUNCTION(BlueprintCallable, Category = "AI|Perception", meta = (HidePin = "WorldContext", DefaultToSelf = "WorldContext"))
	static void ReportPerceptionEvent(UObject* WorldContext, UAISenseEvent* PerceptionEvent);

	template<typename FSenseClass>
	void RegisterSource(AActor& SourceActor);

	void RegisterSourceForSenseClass(TSubclassOf<UAISense> Sense, AActor& Target);

	/** 
	 *	unregisters given actor from the list of active stimuli sources
	 */
	void UnregisterSource(AActor& SourceActor, TSubclassOf<UAISense> Sense = nullptr);

	void RegisterDelayedStimulus(FPerceptionListenerID ListenerId, float Delay, AActor* Instigator, const FAIStimulus& Stimulus);

	static UAIPerceptionSystem* GetCurrent(UObject* WorldContextObject);

	static void MakeNoiseImpl(AActor* NoiseMaker, float Loudness, APawn* NoiseInstigator, const FVector& NoiseLocation);

	UFUNCTION(BlueprintCallable, Category = "AI|Perception", meta = (HidePin = "WorldContext", DefaultToSelf = "WorldContext"))
	static bool RegisterPerceptionStimuliSource(UObject* WorldContext, TSubclassOf<UAISense> Sense, AActor* Target);

	FAISenseID RegisterSenseClass(TSubclassOf<UAISense> SenseClass);

	UFUNCTION(BlueprintCallable, Category = "AI|Perception")
	static TSubclassOf<UAISense> GetSenseClassForStimulus(UObject* WorldContext, const FAIStimulus& Stimulus);
	
protected:
	
	UFUNCTION()
	void OnPerceptionStimuliSourceEndPlay(EEndPlayReason::Type EndPlayReason);
	
	/** requests registration of a given actor as a perception data source for specified sense */
	void RegisterSource(FAISenseID SenseID, AActor& SourceActor);

	void RegisterAllPawnsAsSourcesForSense(FAISenseID SenseID);

	enum EDelayedStimulusSorting 
	{
		RequiresSorting,
		NoNeedToSort,
	};
	/** sorts DelayedStimuli and delivers all the ones that are no longer "in the future"
	 *	@return true if any stimuli has become "current" stimuli (meaning being no longer in future) */
	bool DeliverDelayedStimuli(EDelayedStimulusSorting Sorting);
	void OnNewListener(const FPerceptionListener& NewListener);
	void OnListenerUpdate(const FPerceptionListener& UpdatedListener);
	void OnListenerRemoved(const FPerceptionListener& UpdatedListener);
	void PerformSourceRegistration();

	void AgeStimuli();

	friend class UAISense;
	FORCEINLINE AIPerception::FListenerMap& GetListenersMap() { return ListenerContainer; }

	friend class UAISystem;
	virtual void OnNewPawn(APawn& Pawn);
	virtual void StartPlay();

private:
	/** cached world's timestamp */
	float CurrentTime;

public:

#if !UE_BUILD_SHIPPING
	//----------------------------------------------------------------------//
	// DEBUG
	//----------------------------------------------------------------------//
	TArray<FColor> DebugSenseColors;
	FString PerceptionDebugLegend;	// describes which color means what

	FColor GetSenseDebugColor(FAISenseID SenseID) const;
	FString GetSenseName(FAISenseID SenseID) const;
	FString GetPerceptionDebugLegend() const { return PerceptionDebugLegend; }
#endif 

private:
	FTimerHandle AgeStimuliTimerHandle;
};

//////////////////////////////////////////////////////////////////////////
template<typename FSenseClass>
void UAIPerceptionSystem::RegisterSource(AActor& SourceActor)
{
	FAISenseID SenseID = UAISense::GetSenseID<FSenseClass>();
	if (IsSenseInstantiated(SenseID) == false)
	{
		RegisterSenseClass(FSenseClass::StaticClass());
		SenseID = UAISense::GetSenseID<FSenseClass>();
		check(SenseID.IsValid());
	}
	RegisterSource(SenseID, SourceActor);
}

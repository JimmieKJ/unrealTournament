// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AISystem.h"
#include "Perception/AISense.h"
#include "AIPerceptionSystem.h"

#include "AIPerceptionComponent.generated.h"

class AAIController;
struct FVisualLogEntry;
class UCanvas;
class UAIPerceptionSystem;
class UAISenseConfig;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPerceptionUpdatedDelegate, TArray<AActor*>, UpdatedActors);

struct AIMODULE_API FActorPerceptionInfo
{
	TWeakObjectPtr<AActor> Target;

	TArray<FAIStimulus> LastSensedStimuli;

	/** if != MAX indicated the sense that takes precedense over other senses when it comes
		to determining last stimulus location */
	FAISenseID DominantSense;

	/** indicates whether this Actor is hostile to perception holder */
	uint32 bIsHostile : 1;
	
	FActorPerceptionInfo(AActor* InTarget = NULL)
		: Target(InTarget), DominantSense(FAISenseID::InvalidID())
	{
		for (uint32 Index = 0; Index < FAISenseID::GetSize(); ++Index)
		{
			LastSensedStimuli.Add(FAIStimulus());
		}
	}

	FORCEINLINE_DEBUGGABLE FVector GetLastStimulusLocation(float* OptionalAge = NULL) const 
	{
		FVector Location(FAISystem::InvalidLocation);
		float BestAge = FLT_MAX;
		for (int32 Sense = 0; Sense < LastSensedStimuli.Num(); ++Sense)
		{
			const float Age = LastSensedStimuli[Sense].GetAge();
			if (Age >= 0 && (Age < BestAge 
				|| (Sense == DominantSense && LastSensedStimuli[Sense].WasSuccessfullySensed())))
			{
				BestAge = Age;
				Location = LastSensedStimuli[Sense].StimulusLocation;
			}
		}

		if (OptionalAge)
		{
			*OptionalAge = BestAge;
		}

		return Location;
	}

	/** @note will return FAISystem::InvalidLocation if given sense has never registered related Target actor */
	FORCEINLINE FVector GetStimulusLocation(FAISenseID Sense) const
	{
		return LastSensedStimuli.IsValidIndex(Sense) && LastSensedStimuli[Sense].GetAge() < FAIStimulus::NeverHappenedAge ? LastSensedStimuli[Sense].StimulusLocation : FAISystem::InvalidLocation;
	}

	FORCEINLINE FVector GetReceiverLocation(FAISenseID Sense) const
	{
		return LastSensedStimuli.IsValidIndex(Sense) && LastSensedStimuli[Sense].GetAge() < FAIStimulus::NeverHappenedAge ? LastSensedStimuli[Sense].ReceiverLocation : FAISystem::InvalidLocation;
	}

	FORCEINLINE bool IsSenseRegistered(FAISenseID Sense) const
	{
		return LastSensedStimuli.IsValidIndex(Sense) && LastSensedStimuli[Sense].WasSuccessfullySensed() && (LastSensedStimuli[Sense].GetAge() < FAIStimulus::NeverHappenedAge);
	}
	
	/** takes all "newer" info from Other and absorbs it */
	void Merge(const FActorPerceptionInfo& Other);
};

USTRUCT(BlueprintType, meta = (DisplayName = "Sensed Actor's Data"))
struct FActorPerceptionBlueprintInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "AI|Perception")
	AActor* Target;

	UPROPERTY(BlueprintReadWrite, Category = "AI|Perception")
	TArray<FAIStimulus> LastSensedStimuli;

	UPROPERTY(BlueprintReadWrite, Category = "AI|Perception")
	uint32 bIsHostile : 1;

	FActorPerceptionBlueprintInfo() : Target(NULL), bIsHostile(false)
	{}
	FActorPerceptionBlueprintInfo(const FActorPerceptionInfo& Info);
};

/**
 *	AIPerceptionComponent is used to register as stimuli listener in AIPerceptionSystem
 *	and gathers registered stimuli. UpdatePerception is called when component gets new stimuli (batched)
 */
UCLASS(ClassGroup=AI, HideCategories=(Activation, Collision), meta=(BlueprintSpawnableComponent), config=Game)
class AIMODULE_API UAIPerceptionComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()
	
	static const int32 InitialStimuliToProcessArraySize;

	typedef TMap<AActor*, FActorPerceptionInfo> TActorPerceptionContainer;

protected:
	/** Max distance at which a makenoise(1.0) loudness sound can be heard, regardless of occlusion */
	DEPRECATED(4.8, "This property is deprecated. Please use apropriate sencse config class instead")
	UPROPERTY(VisibleDefaultsOnly, Category = AI)
	float HearingRange;

	/** Max distance at which a makenoise(1.0) loudness sound can be heard if unoccluded (LOSHearingThreshold should be > HearingThreshold) */
	DEPRECATED(4.8, "This property is deprecated. Please use apropriate sencse config class instead")
	UPROPERTY(VisibleDefaultsOnly, Category = AI)
	float LoSHearingRange;

	/** Maximum sight distance to notice a target. */
	DEPRECATED(4.8, "This property is deprecated. Please use apropriate sencse config class instead")
	UPROPERTY(VisibleDefaultsOnly, Category = AI)
	float SightRadius;

	/** Maximum sight distance to see target that has been already seen. */
	DEPRECATED(4.8, "This property is deprecated. Please use apropriate sencse config class instead")
	UPROPERTY(VisibleDefaultsOnly, Category = AI)
	float LoseSightRadius;

	/** How far to the side AI can see, in degrees. Use SetPeripheralVisionAngle to change the value at runtime. */
	DEPRECATED(4.8, "This property is deprecated. Please use apropriate sencse config class instead")
	UPROPERTY(VisibleDefaultsOnly, Category = AI)
	float PeripheralVisionAngle;
		
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "AI Perception")
	TArray<UAISenseConfig*> SensesConfig;

	/** Indicated sense that takes precedence over other senses when determining sensed actor's location. 
	 *	Should be set to one of the sences configured in SensesConfig, or None. */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "AI Perception")
	TSubclassOf<UAISense> DominantSense;
	
	FAISenseID DominantSenseID;

	UPROPERTY(Transient)
	AAIController* AIOwner;

	FPerceptionChannelWhitelist PerceptionFilter;

private:
	TActorPerceptionContainer PerceptualData;
		
protected:	
	struct FStimulusToProcess
	{
		AActor* Source;
		FAIStimulus Stimulus;

		FStimulusToProcess(AActor* InSource, const FAIStimulus& InStimulus)
			: Source(InSource), Stimulus(InStimulus)
		{

		}
	};

	TArray<FStimulusToProcess> StimuliToProcess; 
	
	/** max age of stimulus to consider it "active" (e.g. target is visible) */
	TArray<float> MaxActiveAge;

private:
	uint32 bCleanedUp : 1;

public:

	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	UFUNCTION()
	void OnOwnerEndPlay(EEndPlayReason::Type EndPlayReason);
	
	void GetLocationAndDirection(FVector& Location, FVector& Direction) const;
	const AActor* GetBodyActor() const;
	AActor* GetMutableBodyActor();

	FORCEINLINE const FPerceptionChannelWhitelist GetPerceptionFilter() const { return PerceptionFilter; }

	FGenericTeamId GetTeamIdentifier() const;
	FORCEINLINE FPerceptionListenerID GetListenerId() const { return PerceptionListenerId; }

	FVector GetActorLocation(const AActor& Actor) const;
	FORCEINLINE const FActorPerceptionInfo* GetActorInfo(const AActor& Actor) const { return PerceptualData.Find(&Actor); }
	FORCEINLINE TActorPerceptionContainer::TIterator GetPerceptualDataIterator() { return TActorPerceptionContainer::TIterator(PerceptualData); }
	FORCEINLINE TActorPerceptionContainer::TConstIterator GetPerceptualDataConstIterator() const { return TActorPerceptionContainer::TConstIterator(PerceptualData); }

	void GetHostileActors(TArray<AActor*>& OutActors) const;

	// @note will stop on first age 0 stimulus
	const FActorPerceptionInfo* GetFreshestTrace(const FAISenseID Sense) const;
	
	void SetDominantSense(TSubclassOf<UAISense> InDominantSense);
	FORCEINLINE FAISenseID GetDominantSenseID() const { return DominantSenseID; }
	FORCEINLINE TSubclassOf<UAISense> GetDominantSense() const { return DominantSense; }
	UAISenseConfig* GetSenseConfig(const FAISenseID& SenseID);
	const UAISenseConfig* GetSenseConfig(const FAISenseID& SenseID) const;
	void ConfigureSense(UAISenseConfig& SenseConfig);

	/** Notifies AIPerceptionSystem to update properties for this "stimuli listener" */
	UFUNCTION(BlueprintCallable, Category="AI|Perception")
	void RequestStimuliListenerUpdate();

	void RegisterStimulus(AActor* Source, const FAIStimulus& Stimulus);
	void ProcessStimuli();
	/** returns true if, as result of stimuli aging, this listener needs an update (like if some stimuli expired)*/
	bool AgeStimuli(const float ConstPerceptionAgingRate);
	void ForgetActor(AActor* ActorToForget);

	float GetYoungestStimulusAge(const AActor& Source) const;
	bool HasAnyActiveStimulus(const AActor& Source) const;
	bool HasActiveStimulus(const AActor& Source, FAISenseID Sense) const;

#if !UE_BUILD_SHIPPING
	void DrawDebugInfo(UCanvas* Canvas);
#endif // !UE_BUILD_SHIPPING

#if ENABLE_VISUAL_LOG
	virtual void DescribeSelfToVisLog(FVisualLogEntry* Snapshot) const;
#endif // ENABLE_VISUAL_LOG

	//----------------------------------------------------------------------//
	// blueprint interface
	//----------------------------------------------------------------------//
	UFUNCTION(BlueprintCallable, Category = "AI|Perception")
	void GetPerceivedHostileActors(TArray<AActor*>& OutActors) const;

	/** If SenseToUse is none all actors perceived in any way will get fetched */
	UFUNCTION(BlueprintCallable, Category = "AI|Perception")
	void GetPerceivedActors(TSubclassOf<UAISense> SenseToUse, TArray<AActor*>& OutActors) const;
	
	/** Retrieves whatever has been sensed about given actor */
	UFUNCTION(BlueprintCallable, Category = "AI|Perception")
	bool GetActorsPerception(AActor* Actor, FActorPerceptionBlueprintInfo& Info);

	//////////////////////////////////////////////////////////////////////////
	// Might want to move these to special "BP_AIPerceptionComponent"
	//////////////////////////////////////////////////////////////////////////
	UPROPERTY(BlueprintAssignable)
	FPerceptionUpdatedDelegate OnPerceptionUpdated;

protected:

	void UpdatePerceptionFilter(FAISenseID Channel, bool bNewValue);
	TActorPerceptionContainer& GetPerceptualData() { return PerceptualData; }

	/** called to clean up on owner's end play or destruction */
	virtual void CleanUp();

	void RemoveDeadData();

	/** Updates the stimulus entry in StimulusStore, if NewStimulus is more recent or stronger */
	virtual void RefreshStimulus(FAIStimulus& StimulusStore, const FAIStimulus& NewStimulus);

	/** @note no need to call super implementation, it's there just for some validity checking */
	virtual void HandleExpiredStimulus(FAIStimulus& StimulusStore);
	
private:
	FPerceptionListenerID PerceptionListenerId;

	friend UAIPerceptionSystem;

	void StoreListenerId(FPerceptionListenerID InListenerId) { PerceptionListenerId = InListenerId; }
	void SetMaxStimulusAge(int32 ConfigIndex, float MaxAge);
};


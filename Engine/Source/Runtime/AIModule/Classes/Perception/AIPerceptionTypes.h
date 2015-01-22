// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AITypes.h"
#include "GenericTeamAgentInterface.h"
#include "AITypes.h"
#include "AIPerceptionTypes.generated.h"

class UAISense;
class UAISenseEvent;
class UAISenseConfig;
class UAIPerceptionComponent;
class UWorld;

//////////////////////////////////////////////////////////////////////////
struct AIMODULE_API FAISenseCounter : FAIBasicCounter<uint8>
{};
typedef FAINamedID<FAISenseCounter> FAISenseID;

//////////////////////////////////////////////////////////////////////////
struct AIMODULE_API FPerceptionListenerCounter : FAIBasicCounter<uint32>
{};
typedef FAIGenericID<FPerceptionListenerCounter> FPerceptionListenerID;

//////////////////////////////////////////////////////////////////////////

struct FPerceptionChannelFilter
{
	static const uint32 AllChannels = uint32(-1);

	uint32 AcceptedChannelsMask;

	// by default accept all
	FPerceptionChannelFilter() : AcceptedChannelsMask(AllChannels)
	{}

	void Clear()
	{
		AcceptedChannelsMask = 0;
	}

	FORCEINLINE FPerceptionChannelFilter& FilterOutChannel(FAISenseID Channel)
	{
		AcceptedChannelsMask &= ~(1 << Channel);
		return *this;
	}

	FORCEINLINE FPerceptionChannelFilter& AcceptChannel(FAISenseID Channel)
	{
		AcceptedChannelsMask |= (1 << Channel);
		return *this;
	}

	FORCEINLINE bool ShouldRespondToChannel(FAISenseID Channel) const
	{
		return (AcceptedChannelsMask & (1 << Channel)) != 0;
	}
};

USTRUCT(BlueprintType)
struct AIMODULE_API FAIStimulus
{
	GENERATED_USTRUCT_BODY()

	static const float NeverHappenedAge;

	enum FResult
	{
		SensingSucceeded,
		SensingFailed
	};

protected:
	UPROPERTY(BlueprintReadWrite, Category = "AI|Perception")
	float Age;

	UPROPERTY(BlueprintReadWrite, Category = "AI|Perception")
	float ExpirationAge;
public:
	UPROPERTY(BlueprintReadWrite, Category = "AI|Perception")
	float Strength;
	UPROPERTY(BlueprintReadWrite, Category = "AI|Perception")
	FVector StimulusLocation;
	UPROPERTY(BlueprintReadWrite, Category = "AI|Perception")
	FVector ReceiverLocation;

	FAISenseID Type;
protected:
	uint32 bLastSensingResult:1; // currently used only for marking failed sight tests
	/** this means the stimulus was originally created with a "time limit" and this time has passed. 
	 *	Expiration also results in calling MarkNoLongerSensed */
	uint32 bExpired:1;	
	
public:
	
	/** this is the recommended constructor. Use others if you know what you're doing. */
	FAIStimulus(const UAISense& Sense, float StimulusStrength, const FVector& InStimulusLocation, const FVector& InReceiverLocation, FResult Result = SensingSucceeded);

	FAIStimulus(FAISenseID SenseType, float StimulusStrength, const FVector& InStimulusLocation, const FVector& InReceiverLocation, FResult Result = SensingSucceeded)
		: Age(0.f), ExpirationAge(NeverHappenedAge)
		, Strength(Result == SensingSucceeded ? StimulusStrength : -1.f)
		, StimulusLocation(InStimulusLocation)
		, ReceiverLocation(InReceiverLocation), Type(SenseType), bLastSensingResult(Result == SensingSucceeded), bExpired(false)
	{}

	// default constructor
	FAIStimulus()
		: Age(NeverHappenedAge), ExpirationAge(NeverHappenedAge), Strength(-1.f), StimulusLocation(FAISystem::InvalidLocation)
		, ReceiverLocation(FAISystem::InvalidLocation), Type(FAISenseID::InvalidID()), bLastSensingResult(false), bExpired(false)
	{}

	FAIStimulus& SetExpirationAge(float InExpirationAge) { ExpirationAge = InExpirationAge; return *this; }
	FAIStimulus& SetStimulusAge(float StimulusAge) { Age = StimulusAge; return *this; }
	
	FORCEINLINE float GetAge() const { return Strength > 0 ? Age : NeverHappenedAge; }
	/** @return false when this stimulus is no longer valid, when it is Expired */
	FORCEINLINE bool AgeStimulus(float ConstPerceptionAgingRate) 
	{ 
		Age += ConstPerceptionAgingRate; 
		return Age < ExpirationAge;
	}
	FORCEINLINE bool WasSuccessfullySensed() const { return bLastSensingResult; }
	FORCEINLINE bool IsExpired() const { return bExpired; }
	FORCEINLINE void MarkNoLongerSensed() { bLastSensingResult = false; }
	FORCEINLINE void MarkExpired() { bExpired = true; MarkNoLongerSensed(); }
	FORCEINLINE bool IsActive() const { return WasSuccessfullySensed() == true && GetAge() < NeverHappenedAge; }
};

USTRUCT()
struct AIMODULE_API FAISenseAffiliationFilter
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense")
	uint32 bDetectEnemies : 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense")
	uint32 bDetectNeutrals : 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense")
	uint32 bDetectFriendlies : 1;
	
	uint8 GetAsFlags() const { return (bDetectEnemies << ETeamAttitude::Hostile) | (bDetectNeutrals << ETeamAttitude::Neutral) | (bDetectFriendlies << ETeamAttitude::Friendly); }
};

/** Should contain only cached information common to all senses. Sense-specific data needs to be stored by senses themselves */
struct AIMODULE_API FPerceptionListener
{
	TWeakObjectPtr<UAIPerceptionComponent> Listener;

	FPerceptionChannelFilter Filter;

	FVector CachedLocation;
	FVector CachedDirection;

	FGenericTeamId TeamIdentifier;

private:
	uint32 bHasStimulusToProcess : 1;

	FPerceptionListenerID ListenerID;

	FPerceptionListener();
public:
	FPerceptionListener(UAIPerceptionComponent& InListener);

	void UpdateListenerProperties(UAIPerceptionComponent& Listener);

	bool operator==(const UAIPerceptionComponent* Other) const { return Listener.Get() == Other; }
	bool operator==(const FPerceptionListener& Other) const { return Listener == Other.Listener; }

	void CacheLocation();

	void RegisterStimulus(AActor* Source, const FAIStimulus& Stimulus);

	FORCEINLINE bool HasAnyNewStimuli() const { return bHasStimulusToProcess; }
	void ProcessStimuli();

	FORCEINLINE bool HasSense(FAISenseID SenseID) const { return Filter.ShouldRespondToChannel(SenseID); }

	// used to remove "dead" listeners
	static const FPerceptionListener NullListener;

	FORCEINLINE FPerceptionListenerID GetListenerID() const { return ListenerID; }

	FName GetBodyActorName() const;

	/** Returns pointer to the actor representing this listener's physical body */
	const AActor* GetBodyActor() const;

	const IGenericTeamAgentInterface* GetTeamAgent() const;

private:
	friend class UAIPerceptionSystem;
	FORCEINLINE void SetListenerID(FPerceptionListenerID InListenerID) { ListenerID = InListenerID; }
	FORCEINLINE void MarkForStimulusProcessing() { bHasStimulusToProcess = true; }
};

namespace AIPerception
{
	typedef TMap<FPerceptionListenerID, FPerceptionListener> FListenerMap;
}

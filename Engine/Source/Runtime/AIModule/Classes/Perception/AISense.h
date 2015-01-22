// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AIPerceptionTypes.h"
#include "AISense.generated.h"

class AActor;
class UAIPerceptionSystem;

DECLARE_DELEGATE_OneParam(FOnPerceptionListenerUpdateDelegate, const FPerceptionListener&);

UCLASS(ClassGroup = AI, abstract, config = Engine)
class AIMODULE_API UAISense : public UObject
{
	GENERATED_UCLASS_BODY()

	static const float SuspendNextUpdate;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Debug)
	FColor DebugDrawColor;

	/** Sense name used in debug drawing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Debug)
	FString DebugName;

	/** age past which stimulus of this sense are "forgotten"*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Perception", config)
	float DefaultExpirationAge;
	
private:
	UPROPERTY()
	UAIPerceptionSystem* PerceptionSystemInstance;

	/** then this count reaches 0 sense will be updated */
	float TimeUntilNextUpdate;

	FAISenseID SenseID;

protected:
	/**	If bound will be called when new FPerceptionListener gets registers with AIPerceptionSystem */
	FOnPerceptionListenerUpdateDelegate OnNewListenerDelegate;

	/**	If bound will be called when a FPerceptionListener's in AIPerceptionSystem change */
	FOnPerceptionListenerUpdateDelegate OnListenerUpdateDelegate;

	/**	If bound will be called when a FPerceptionListener's in removed from AIPerceptionSystem */
	FOnPerceptionListenerUpdateDelegate OnListenerRemovedDelegate;
				
public:

	virtual UWorld* GetWorld() const override;

	/** use with caution! Needs to be called before any senses get instantiated or listeners registered. DOES NOT update any perceptions system instances */
	static void HardcodeSenseID(TSubclassOf<UAISense> SenseClass, FAISenseID HardcodedID);

	static FAISenseID GetSenseID(TSubclassOf<UAISense> SenseClass) { return SenseClass ? ((UAISense*)SenseClass->GetDefaultObject())->SenseID : FAISenseID::InvalidID(); }
	template<typename TSense>
	static FAISenseID GetSenseID() 
	{ 
		return GetDefault<TSense>()->GetSenseID();
	}
	FORCEINLINE FAISenseID GetSenseID() const { return SenseID; }

	virtual void PostInitProperties() override;

	/** 
	 *	@return should this sense be ticked now
	 */
	bool ProgressTime(float DeltaSeconds)
	{
		TimeUntilNextUpdate -= DeltaSeconds;
		return TimeUntilNextUpdate <= 0.f;
	}

	void Tick()
	{
		if (TimeUntilNextUpdate <= 0.f)
		{
			TimeUntilNextUpdate = Update();
		}
	}

	//virtual void RegisterSources(TArray<AActor&> SourceActors) {}
	virtual void RegisterSource(AActor& SourceActors){}
	virtual void RegisterWrappedEvent(UAISenseEvent& PerceptionEvent);
	virtual FAISenseID UpdateSenseID();

	FORCEINLINE void OnNewListener(const FPerceptionListener& NewListener) { OnNewListenerDelegate.ExecuteIfBound(NewListener); }
	FORCEINLINE void OnListenerUpdate(const FPerceptionListener& NewListener) { OnListenerUpdateDelegate.ExecuteIfBound(NewListener); }
	FORCEINLINE void OnListenerRemoved(const FPerceptionListener& NewListener) { OnListenerRemovedDelegate.ExecuteIfBound(NewListener); }

	FORCEINLINE float GetDefaultExpirationAge() const { return DefaultExpirationAge; }

protected:
	/** @return time until next update */
	virtual float Update() { return FLT_MAX; }

	/** will result in updating as soon as possible */
	FORCEINLINE void RequestImmediateUpdate() { TimeUntilNextUpdate = 0.f; }

	/** will result in updating in specified number of seconds */
	FORCEINLINE void RequestUpdateInSeconds(float UpdateInSeconds) { TimeUntilNextUpdate = UpdateInSeconds; }

	FORCEINLINE UAIPerceptionSystem* GetPerceptionSystem() { return PerceptionSystemInstance; }

	void SetSenseID(FAISenseID Index);

	/** returning pointer rather then a reference to prevent users from
	 *	accidentally creating copies by creating non-reference local vars */
	AIPerception::FListenerMap* GetListeners();

	/** To be called only for BP-generated classes */
	void ForceSenseID(FAISenseID SenseID);

public:
#if !UE_BUILD_SHIPPING
	//----------------------------------------------------------------------//
	// DEBUG
	//----------------------------------------------------------------------//
	FColor GetDebugColor() const { return DebugDrawColor; }
	FString GetDebugName() const { return DebugName; }
	virtual FString GetDebugLegend() const;
#endif // !UE_BUILD_SHIPPING
};

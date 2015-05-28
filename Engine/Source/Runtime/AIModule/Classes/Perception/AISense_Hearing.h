// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Perception/AIPerceptionSystem.h"
#include "Perception/AISense.h"
#include "AISense_Hearing.generated.h"

class UAISenseConfig_Hearing;

USTRUCT()
struct AIMODULE_API FAINoiseEvent
{	
	GENERATED_USTRUCT_BODY()

	typedef class UAISense_Hearing FSenseClass;

	float Age;

	/** if not set Instigator's location will be used */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sense")
	FVector NoiseLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sense", meta = (UIMin = 0, ClampMin = 0))
	float Loudness;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sense")
	AActor* Instigator;

	FGenericTeamId TeamIdentifier;
		
	FAINoiseEvent();
	FAINoiseEvent(AActor* InInstigator, const FVector& InNoiseLocation, float InLoudness = 1.f);

	/** Verifies and calculates derived data */
	void Compile();
};

UCLASS(ClassGroup=AI, Config=Game)
class AIMODULE_API UAISense_Hearing : public UAISense
{
	GENERATED_UCLASS_BODY()
		
protected:
	UPROPERTY()
	TArray<FAINoiseEvent> NoiseEvents;

	/** Defaults to 0 to have instant notification. Setting to > 0 will result in delaying 
	 *	when AI hears the sound based on the distance from the source */
	UPROPERTY(config)
	float SpeedOfSoundSq;

	struct FDigestedHearingProperties
	{
		float HearingRangeSq;
		float LoSHearingRangeSq;
		uint8 AffiliationFlags;
		uint32 bUseLoSHearing : 1;

		FDigestedHearingProperties(const UAISenseConfig_Hearing& SenseConfig);
		FDigestedHearingProperties();
	};
	TMap<FPerceptionListenerID, FDigestedHearingProperties> DigestedProperties;

public:	
	void RegisterEvent(const FAINoiseEvent& Event);	
	// part of BP interface. Translates PerceptionEvent to FAINoiseEvent and call RegisterEvent(const FAINoiseEvent& Event)
	virtual void RegisterWrappedEvent(UAISenseEvent& PerceptionEvent) override;

	UFUNCTION(BlueprintCallable, Category = "AI|Perception", meta = (HidePin = "WorldContext", DefaultToSelf = "WorldContext"))
	static void ReportNoiseEvent(UObject* WorldContext, FVector NoiseLocation, float Loudness = 1.f, AActor* Instigator = nullptr);

protected:
	virtual float Update() override;

	void OnNewListenerImpl(const FPerceptionListener& NewListener);
	void OnListenerUpdateImpl(const FPerceptionListener& UpdatedListener);
	void OnListenerRemovedImpl(const FPerceptionListener& UpdatedListener);

public:
#if !UE_BUILD_SHIPPING
	//----------------------------------------------------------------------//
	// DEBUG
	//----------------------------------------------------------------------//
	virtual FString GetDebugLegend() const override;
	static FColor GetDebugHearingRangeColor() { return FColor::Yellow; }
	static FColor GetDebugLoSHearingRangeeColor() { return FColorList::Cyan; }
#endif // !UE_BUILD_SHIPPING
};

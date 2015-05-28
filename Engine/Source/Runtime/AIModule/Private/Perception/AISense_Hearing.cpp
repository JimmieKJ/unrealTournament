// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseEvent_Hearing.h"

//----------------------------------------------------------------------//
// FAINoiseEvent
//----------------------------------------------------------------------//
FAINoiseEvent::FAINoiseEvent()
	: Age(0.f), NoiseLocation(FAISystem::InvalidLocation), Loudness(1.f)
	, Instigator(nullptr), TeamIdentifier(FGenericTeamId::NoTeam)
{
}

FAINoiseEvent::FAINoiseEvent(AActor* InInstigator, const FVector& InNoiseLocation, float InLoudness)
	: Age(0.f), NoiseLocation(InNoiseLocation), Loudness(InLoudness)
	, Instigator(InInstigator), TeamIdentifier(FGenericTeamId::NoTeam)
{
	Compile();
}

void FAINoiseEvent::Compile()
{
	TeamIdentifier = FGenericTeamId::GetTeamIdentifier(Instigator);
	if (FAISystem::IsValidLocation(NoiseLocation) == false && Instigator != nullptr)
	{
		NoiseLocation = Instigator->GetActorLocation();
	}
}

//----------------------------------------------------------------------//
// FDigestedHearingProperties
//----------------------------------------------------------------------//
UAISense_Hearing::FDigestedHearingProperties::FDigestedHearingProperties(const UAISenseConfig_Hearing& SenseConfig)
{
	HearingRangeSq = FMath::Square(SenseConfig.HearingRange);
	LoSHearingRangeSq = FMath::Square(SenseConfig.LoSHearingRange);
	AffiliationFlags = SenseConfig.DetectionByAffiliation.GetAsFlags();
	bUseLoSHearing = SenseConfig.bUseLoSHearing;
}

UAISense_Hearing::FDigestedHearingProperties::FDigestedHearingProperties()
	: HearingRangeSq(-1.f), LoSHearingRangeSq(-1.f), AffiliationFlags(-1), bUseLoSHearing(false)
{

}

//----------------------------------------------------------------------//
// UAISense_Hearing
//----------------------------------------------------------------------//
UAISense_Hearing::UAISense_Hearing(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
{
	DebugDrawColor = FColor::Yellow;
	DebugName = TEXT("Hearing");

	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		OnNewListenerDelegate.BindUObject(this, &UAISense_Hearing::OnNewListenerImpl);
		OnListenerUpdateDelegate.BindUObject(this, &UAISense_Hearing::OnListenerUpdateImpl);
		OnListenerRemovedDelegate.BindUObject(this, &UAISense_Hearing::OnListenerRemovedImpl);

		static bool bMakeNoiseInterceptionSetUp = false;
		if (bMakeNoiseInterceptionSetUp == false)
		{
			AActor::SetMakeNoiseDelegate(FMakeNoiseDelegate::CreateStatic(&UAIPerceptionSystem::MakeNoiseImpl));
			bMakeNoiseInterceptionSetUp = true;
		}
	}
}

void UAISense_Hearing::ReportNoiseEvent(UObject* WorldContext, FVector NoiseLocation, float Loudness, AActor* Instigator)
{
	UAIPerceptionSystem* PerceptionSystem = UAIPerceptionSystem::GetCurrent(WorldContext);
	if (PerceptionSystem)
	{
		FAINoiseEvent Event(Instigator, NoiseLocation, Loudness);
		PerceptionSystem->OnEvent(Event);
	}
}

void UAISense_Hearing::OnNewListenerImpl(const FPerceptionListener& NewListener)
{
	check(NewListener.Listener.IsValid());
	const UAISenseConfig_Hearing* SenseConfig = Cast<const UAISenseConfig_Hearing>(NewListener.Listener->GetSenseConfig(GetSenseID()));
	check(SenseConfig);
	const FDigestedHearingProperties PropertyDigest(*SenseConfig);
	DigestedProperties.Add(NewListener.GetListenerID(), PropertyDigest);
}

void UAISense_Hearing::OnListenerUpdateImpl(const FPerceptionListener& UpdatedListener)
{
	// @todo add updating code here
	const FPerceptionListenerID ListenerID = UpdatedListener.GetListenerID();
	
	if (UpdatedListener.HasSense(GetSenseID()))
	{
		const UAISenseConfig_Hearing* SenseConfig = Cast<const UAISenseConfig_Hearing>(UpdatedListener.Listener->GetSenseConfig(GetSenseID()));
		check(SenseConfig);
		FDigestedHearingProperties& PropertiesDigest = DigestedProperties.FindOrAdd(ListenerID);
		PropertiesDigest = FDigestedHearingProperties(*SenseConfig);
	}
	else
	{
		DigestedProperties.FindAndRemoveChecked(ListenerID);
	}
}

void UAISense_Hearing::OnListenerRemovedImpl(const FPerceptionListener& UpdatedListener)
{
	DigestedProperties.FindAndRemoveChecked(UpdatedListener.GetListenerID());
}

float UAISense_Hearing::Update()
{
	AIPerception::FListenerMap& ListenersMap = *GetListeners();
	UAIPerceptionSystem* PerseptionSys = GetPerceptionSystem();

	for (AIPerception::FListenerMap::TIterator ListenerIt(ListenersMap); ListenerIt; ++ListenerIt)
	{
		FPerceptionListener& Listener = ListenerIt->Value;
		
		if (Listener.HasSense(GetSenseID()) == false)
		{
			// skip listeners not interested in this sense
			continue;
		}

		const FDigestedHearingProperties& PropDigest = DigestedProperties[Listener.GetListenerID()];

		for (int32 EventIndex = 0; EventIndex < NoiseEvents.Num(); ++EventIndex)
		{
			const FAINoiseEvent& Event = NoiseEvents[EventIndex];
		
			if (FVector::DistSquared(Event.NoiseLocation, Listener.CachedLocation) > PropDigest.HearingRangeSq * Event.Loudness
				|| FAISenseAffiliationFilter::ShouldSenseTeam(Listener.TeamIdentifier, Event.TeamIdentifier, PropDigest.AffiliationFlags) == false)
			{
				continue;
			}
			// calculate delay and fake it with Age
			const float Delay = SpeedOfSoundSq > 0.f ? FVector::DistSquared(Event.NoiseLocation, Listener.CachedLocation) / SpeedOfSoundSq : 0;
			// pass over to listener to process 			
			PerseptionSys->RegisterDelayedStimulus(Listener.GetListenerID(), Delay, Event.Instigator
				, FAIStimulus(*this, Event.Loudness, Event.NoiseLocation, Listener.CachedLocation) );
		}
	}

	NoiseEvents.Reset();

	// return decides when next tick is going to happen
	return SuspendNextUpdate;
}

void UAISense_Hearing::RegisterEvent(const FAINoiseEvent& Event)
{
	NoiseEvents.Add(Event);

	RequestImmediateUpdate();
}

void UAISense_Hearing::RegisterWrappedEvent(UAISenseEvent& PerceptionEvent)
{
	UAISenseEvent_Hearing* HearingEvent = Cast<UAISenseEvent_Hearing>(&PerceptionEvent);
	ensure(HearingEvent);
	if (HearingEvent)
	{
		RegisterEvent(HearingEvent->GetNoiseEvent());
	}
}


#if !UE_BUILD_SHIPPING
//----------------------------------------------------------------------//
// DEBUG
//----------------------------------------------------------------------//
FString UAISense_Hearing::GetDebugLegend() const
{
	static const FColor HearingColor = GetDebugHearingRangeColor();
	static const FColor LoSHearingColor = GetDebugLoSHearingRangeeColor();

	return FString::Printf(TEXT("{%s} Hearing, {%s} LoS hearing,"), *HearingColor.ToString(), *LoSHearingColor.ToString());
}
#endif // !UE_BUILD_SHIPPING

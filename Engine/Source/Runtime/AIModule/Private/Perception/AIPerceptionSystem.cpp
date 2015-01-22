// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "Perception/AIPerceptionSystem.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Team.h"
#include "Perception/AISense_Touch.h"
#include "Perception/AISense_Prediction.h"
#include "Perception/AISense_Damage.h"

DECLARE_CYCLE_STAT(TEXT("Perception System"),STAT_AI_PerceptionSys,STATGROUP_AI);

DEFINE_LOG_CATEGORY(LogAIPerception);

//----------------------------------------------------------------------//
// UAISenseConfig
//----------------------------------------------------------------------//
FAISenseID UAISenseConfig::GetSenseID() const
{
	TSubclassOf<UAISense> SenseClass = GetSenseImplementation();
	return UAISense::GetSenseID(SenseClass);
}

//----------------------------------------------------------------------//
// UAIPerceptionSystem
//----------------------------------------------------------------------//
UAIPerceptionSystem::UAIPerceptionSystem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, PerceptionAgingRate(0.3f)
	, bSomeListenersNeedUpdateDueToStimuliAging(false)
	, CurrentTime(0.f)
{
}

void UAIPerceptionSystem::RegisterSenseClass(TSubclassOf<UAISense> SenseClass)
{
	check(SenseClass);
	FAISenseID SenseID = UAISense::GetSenseID(SenseClass);
	if (SenseID.IsValid() == false)
	{
		UAISense* SenseCDO = GetMutableDefault<UAISense>(SenseClass);
		SenseID = SenseCDO->UpdateSenseID();

		if (SenseID.IsValid() == false)
		{
			// @todo log a message here
			return;
		}
	}

	if (SenseID.Index >= Senses.Num())
	{
		const int32 ItemsToAdd = SenseID.Index - Senses.Num() + 1;
		Senses.AddZeroed(ItemsToAdd);
#if !UE_BUILD_SHIPPING
		DebugSenseColors.AddZeroed(ItemsToAdd);
#endif
	}

	if (Senses[SenseID] == nullptr)
	{
		Senses[SenseID] = ConstructObject<UAISense>(SenseClass, this);
		check(Senses[SenseID]);
#if !UE_BUILD_SHIPPING
		DebugSenseColors[SenseID] = Senses[SenseID]->GetDebugColor();
		PerceptionDebugLegend += Senses[SenseID]->GetDebugLegend();

		// make senses v-log to perception system's log
		REDIRECT_OBJECT_TO_VLOG(Senses[SenseID], this);
		UE_VLOG(this, LogAIPerception, Log, TEXT("Registering sense %s"), *Senses[SenseID]->GetName());
#endif
	}
}

UWorld* UAIPerceptionSystem::GetWorld() const
{
	return Cast<UWorld>(GetOuter());
}

void UAIPerceptionSystem::PostInitProperties() 
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		UWorld* World = GEngine->GetWorldFromContextObject(GetOuter());
		if (World)
		{
			World->GetTimerManager().SetTimer(this, &UAIPerceptionSystem::AgeStimuli, PerceptionAgingRate, /*inbLoop=*/true);
		}
	}
}

TStatId UAIPerceptionSystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAIPerceptionSystem, STATGROUP_Tickables);
}


void UAIPerceptionSystem::RegisterSource(FAISenseID SenseID, AActor& SourceActor)
{
	ensure(IsSenseInstantiated(SenseID));
	SourcesToRegister.Add(FPerceptionSourceRegistration(SenseID, &SourceActor));
}

void UAIPerceptionSystem::PerformSourceRegistration()
{
	SCOPE_CYCLE_COUNTER(STAT_AI_PerceptionSys);

	for (const auto& PercSource : SourcesToRegister)
	{
		AActor* SourceActor = PercSource.Source.Get();
		if (SourceActor)
		{
			Senses[PercSource.SenseID]->RegisterSource(*SourceActor);
		}
	}

	SourcesToRegister.Reset();
}

void UAIPerceptionSystem::OnNewListener(const FPerceptionListener& NewListener)
{
	UAISense** SenseInstance = Senses.GetData();
	for (int32 SenseID = 0; SenseID < Senses.Num(); ++SenseID, ++SenseInstance)
	{
		// @todo filter out the ones that do not declare using this sense
		if (*SenseInstance != nullptr && NewListener.HasSense((*SenseInstance)->GetSenseID()))
		{
			(*SenseInstance)->OnNewListener(NewListener);
		}
	}
}

void UAIPerceptionSystem::OnListenerUpdate(const FPerceptionListener& UpdatedListener)
{
	UAISense** SenseInstance = Senses.GetData();
	for (int32 SenseID = 0; SenseID < Senses.Num(); ++SenseID, ++SenseInstance)
	{
		if (*SenseInstance != nullptr)
		{
			(*SenseInstance)->OnListenerUpdate(UpdatedListener);
		}
	}
}

void UAIPerceptionSystem::Tick(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_AI_PerceptionSys);

	// if no new stimuli
	// and it's not time to remove stimuli from "know events"

	UWorld* World = GEngine->GetWorldFromContextObject(GetOuter());
	check(World);

	if (World->bPlayersOnly == false)
	{
		// cache it
		CurrentTime = World->GetTimeSeconds();

		bool bNeedsUpdate = false;

		if (SourcesToRegister.Num() > 0)
		{
			PerformSourceRegistration();
		}

		{
			UAISense** SenseInstance = Senses.GetData();
			for (int32 SenseID = 0; SenseID < Senses.Num(); ++SenseID, ++SenseInstance)
			{
				bNeedsUpdate |= *SenseInstance != nullptr && (*SenseInstance)->ProgressTime(DeltaSeconds);
			}
		}

		if (bNeedsUpdate)
		{
			// first update cached location of all listener, and remove invalid listeners
			for (AIPerception::FListenerMap::TIterator ListenerIt(ListenerContainer); ListenerIt; ++ListenerIt)
			{
				if (ListenerIt->Value.Listener.IsValid())
				{
					ListenerIt->Value.CacheLocation();
				}
				else
				{
					OnListenerRemoved(ListenerIt->Value);
					ListenerIt.RemoveCurrent();
				}
			}

			UAISense** SenseInstance = Senses.GetData();
			for (int32 SenseID = 0; SenseID < Senses.Num(); ++SenseID, ++SenseInstance)
			{
				if (*SenseInstance != nullptr)
				{
					(*SenseInstance)->Tick();
				}
			}
		}

		/** no point in soring in no new stimuli was processed */
		bool bStimuliDelivered = DeliverDelayedStimuli(bNeedsUpdate ? RequiresSorting : NoNeedToSort);

		if (bNeedsUpdate || bStimuliDelivered || bSomeListenersNeedUpdateDueToStimuliAging)
		{
			for (AIPerception::FListenerMap::TIterator ListenerIt(ListenerContainer); ListenerIt; ++ListenerIt)
			{
				check(ListenerIt->Value.Listener.IsValid())
				if (ListenerIt->Value.HasAnyNewStimuli())
				{
					ListenerIt->Value.ProcessStimuli();
				}
			}

			bSomeListenersNeedUpdateDueToStimuliAging = false;
		}
	}
}

void UAIPerceptionSystem::AgeStimuli()
{
	// age all stimuli in all listeners by PerceptionAgingRate
	const float ConstPerceptionAgingRate = PerceptionAgingRate;

	for (AIPerception::FListenerMap::TIterator ListenerIt(ListenerContainer); ListenerIt; ++ListenerIt)
	{
		FPerceptionListener& Listener = ListenerIt->Value;
		if (Listener.Listener.IsValid())
		{
			// AgeStimuli will return true if this listener requires an update after stimuli aging
			if (Listener.Listener->AgeStimuli(ConstPerceptionAgingRate))
			{
				Listener.MarkForStimulusProcessing();
				bSomeListenersNeedUpdateDueToStimuliAging = true;
			}
		}
	}
}

UAIPerceptionSystem* UAIPerceptionSystem::GetCurrent(UObject* WorldContextObject)
{
	UWorld* World = Cast<UWorld>(WorldContextObject);

	if (World == nullptr && WorldContextObject != nullptr)
	{
		World = GEngine->GetWorldFromContextObject(WorldContextObject);
	}

	if (World && World->GetAISystem())
	{
		check(Cast<UAISystem>(World->GetAISystem()));
		UAISystem* AISys = (UAISystem*)(World->GetAISystem());

		return AISys->GetPerceptionSystem();
	}

	return nullptr;
}

void UAIPerceptionSystem::UpdateListener(UAIPerceptionComponent& Listener)
{
	SCOPE_CYCLE_COUNTER(STAT_AI_PerceptionSys);

	if (Listener.IsPendingKill())
    {
		UnregisterListener(Listener);
        return;
    }

	const FPerceptionListenerID ListenerId = Listener.GetListenerId();

	if (ListenerId != FPerceptionListenerID::InvalidID())
    {
		FPerceptionListener& ListenerEntry = ListenerContainer[ListenerId];
		ListenerEntry.UpdateListenerProperties(Listener);
		OnListenerUpdate(ListenerEntry);
	}
	else
	{			
		const FPerceptionListenerID NewListenerId = FPerceptionListenerID::GetNextID();
		Listener.StoreListenerId(NewListenerId);
		FPerceptionListener& ListenerEntry = ListenerContainer.Add(NewListenerId, FPerceptionListener(Listener));
		ListenerEntry.CacheLocation();
				
		OnNewListener(ListenerContainer[NewListenerId]);
    }
}

void UAIPerceptionSystem::UnregisterListener(UAIPerceptionComponent& Listener)
{
	SCOPE_CYCLE_COUNTER(STAT_AI_PerceptionSys);

	const FPerceptionListenerID ListenerId = Listener.GetListenerId();

	// can already be removed from ListenerContainer as part of cleaning up 
	// listeners with invalid WeakObjectPtr to UAIPerceptionComponent
	if (ListenerId != FPerceptionListenerID::InvalidID() && ListenerContainer.Contains(ListenerId))
	{
		check(ListenerContainer[ListenerId].Listener.IsValid() == false
			|| ListenerContainer[ListenerId].Listener.Get() == &Listener);
		OnListenerRemoved(ListenerContainer[ListenerId]);
		ListenerContainer.Remove(ListenerId);

		// mark it as unregistered
		Listener.StoreListenerId(FPerceptionListenerID::InvalidID());
	}
}

void UAIPerceptionSystem::OnListenerRemoved(const FPerceptionListener& NewListener)
{
	UAISense** SenseInstance = Senses.GetData();
	for (int32 SenseID = 0; SenseID < Senses.Num(); ++SenseID, ++SenseInstance)
	{
		if (*SenseInstance != nullptr)
		{
			(*SenseInstance)->OnListenerRemoved(NewListener);
		}
	}
}

void UAIPerceptionSystem::RegisterDelayedStimulus(FPerceptionListenerID ListenerId, float Delay, AActor* Instigator, const FAIStimulus& Stimulus)
{
	FDelayedStimulus DelayedStimulus;
	DelayedStimulus.DeliveryTimestamp = CurrentTime + Delay;
	DelayedStimulus.ListenerId = ListenerId;
	DelayedStimulus.Instigator = Instigator;
	DelayedStimulus.Stimulus = Stimulus;
	DelayedStimuli.Add(DelayedStimulus);
}

bool UAIPerceptionSystem::DeliverDelayedStimuli(UAIPerceptionSystem::EDelayedStimulusSorting Sorting)
{
	struct FTimestampSort
	{ 
		bool operator()(const FDelayedStimulus& A, const FDelayedStimulus& B) const
		{
			return A.DeliveryTimestamp < B.DeliveryTimestamp;
		}
	};

	if (DelayedStimuli.Num() <= 0)
	{
		return false;
	}

	if (Sorting == RequiresSorting)
	{
		DelayedStimuli.Sort(FTimestampSort());
	}

	int Index = 0;
	while (Index < DelayedStimuli.Num() && DelayedStimuli[Index].DeliveryTimestamp < CurrentTime)
	{
		FDelayedStimulus& DelayedStimulus = DelayedStimuli[Index];
		
		if (DelayedStimulus.ListenerId != FPerceptionListenerID::InvalidID() && ListenerContainer.Contains(DelayedStimulus.ListenerId))
		{
			FPerceptionListener& ListenerEntry = ListenerContainer[DelayedStimulus.ListenerId];
			// this has been already checked during tick, so if it's no longer the case then it's a bug
			check(ListenerEntry.Listener.IsValid());

			// deliver
			ListenerEntry.RegisterStimulus(DelayedStimulus.Instigator.Get(), DelayedStimulus.Stimulus);
		}

		++Index;
	}

	DelayedStimuli.RemoveAt(0, Index, /*bAllowShrinking=*/false);

	return Index > 0;
}

void UAIPerceptionSystem::MakeNoiseImpl(AActor* NoiseMaker, float Loudness, APawn* NoiseInstigator, const FVector& NoiseLocation)
{
	check(NoiseMaker != nullptr || NoiseInstigator != nullptr);

	UWorld* World = NoiseMaker ? NoiseMaker->GetWorld() : NoiseInstigator->GetWorld();

	UAIPerceptionSystem::OnEvent(World, FAINoiseEvent(NoiseInstigator ? NoiseInstigator : NoiseMaker
			, NoiseLocation
			, Loudness));
}

bool UAIPerceptionSystem::RegisterPerceptionStimuliSource(UObject* WorldContext, TSubclassOf<UAISense> Sense, AActor* Target)
{
	bool bResult = false;
	if (Sense && Target)
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContext);
		if (World && World->GetAISystem())
		{
			UAISystem* AISys = Cast<UAISystem>(World->GetAISystem());
			if (AISys && AISys->GetPerceptionSystem())
			{
				const FAISenseID SenseID = UAISense::GetSenseID(Sense);
				if (AISys->GetPerceptionSystem()->IsSenseInstantiated(SenseID) == false)
				{
					AISys->GetPerceptionSystem()->RegisterSenseClass(Sense);
				}

				AISys->GetPerceptionSystem()->RegisterSource(SenseID, *Target);
				bResult = true;
			}
		}
	}

	return bResult;
}

TSubclassOf<UAISense> UAIPerceptionSystem::GetSenseClassForStimulus(UObject* WorldContext, const FAIStimulus& Stimulus)
{
	TSubclassOf<UAISense> Result = nullptr;
	UAIPerceptionSystem* PercSys = GetCurrent(WorldContext);
	if (PercSys && PercSys->Senses.IsValidIndex(Stimulus.Type))
	{
		Result = PercSys->Senses[Stimulus.Type]->GetClass();
	}

	return Result;
}

//----------------------------------------------------------------------//
// Blueprint API
//----------------------------------------------------------------------//
void UAIPerceptionSystem::ReportEvent(UAISenseEvent* PerceptionEvent)
{
	if (PerceptionEvent)
	{
		const FAISenseID SenseID = PerceptionEvent->GetSenseID();
		if (SenseID.IsValid() && Senses.IsValidIndex(SenseID) && Senses[SenseID] != nullptr)
		{
			Senses[SenseID]->RegisterWrappedEvent(*PerceptionEvent);
		}
		else
		{
			UE_VLOG(this, LogAIPerception, Log, TEXT("Skipping perception event %s since related sense class has not been registered (no listeners)")
				, *PerceptionEvent->GetName());
			PerceptionEvent->DrawToVLog(*this);
		}
	}
}

void UAIPerceptionSystem::ReportPerceptionEvent(UObject* WorldContext, UAISenseEvent* PerceptionEvent)
{
	UAIPerceptionSystem* PerceptionSys = GetCurrent(WorldContext);
	if (PerceptionSys != nullptr)
	{
		PerceptionSys->ReportEvent(PerceptionEvent);
	}
}


#if !UE_BUILD_SHIPPING
FColor UAIPerceptionSystem::GetSenseDebugColor(FAISenseID SenseID) const
{
	return SenseID.IsValid() && Senses.IsValidIndex(SenseID) ? Senses[SenseID]->GetDebugColor() : FColor::Black;
}

FString UAIPerceptionSystem::GetSenseName(FAISenseID SenseID) const
{
	return SenseID.IsValid() && Senses.IsValidIndex(SenseID) ? *Senses[SenseID]->GetDebugName() : TEXT("Invalid");
}
#endif 
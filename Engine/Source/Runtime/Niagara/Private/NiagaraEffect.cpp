// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "NiagaraPrivate.h"
#include "NiagaraEffect.h"



UNiagaraEffect::UNiagaraEffect(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}



UNiagaraEmitterProperties *UNiagaraEffect::AddEmitterProperties(UNiagaraEmitterProperties *Props)
{
	if (Props == nullptr)
	{
		Props = NewObject<UNiagaraEmitterProperties>(this);
	}
	EmitterProps.Add(Props);
	return Props;
}

void UNiagaraEffect::DeleteEmitterProperties(UNiagaraEmitterProperties *Props)
{
	EmitterProps.Remove(Props);
}


void UNiagaraEffect::CreateEffectRendererProps(TSharedPtr<FNiagaraSimulation> Sim)
{
	UClass *RendererProps = Sim->GetEffectRenderer()->GetPropertiesClass();
	if (RendererProps)
	{
		Sim->GetProperties()->RendererProperties = NewObject<UNiagaraEffectRendererProperties>(this, RendererProps);
	}
	else
	{
		Sim->GetProperties()->RendererProperties = nullptr;
	}
	Sim->GetEffectRenderer()->SetRendererProperties(Sim->GetProperties()->RendererProperties);
}

void UNiagaraEffect::PostLoad()
{
	Super::PostLoad();
}

//////////////////////////////////////////////////////////////////////////

TSharedPtr<FNiagaraSimulation> FNiagaraEffectInstance::AddEmitter(UNiagaraEmitterProperties *Properties)
{
	FNiagaraSimulation *SimPtr = new FNiagaraSimulation(Properties, this);
	TSharedPtr<FNiagaraSimulation> Sim = MakeShareable(SimPtr);
	Sim->SetRenderModuleType(Properties->RenderModuleType, Component->GetWorld()->FeatureLevel);
	Emitters.Add(Sim);
	FNiagaraSceneProxy *SceneProxy = static_cast<FNiagaraSceneProxy*>(Component->SceneProxy);
	SceneProxy->UpdateEffectRenderers(this);

	return Sim;
}

void FNiagaraEffectInstance::DeleteEmitter(TSharedPtr<FNiagaraSimulation> Emitter)
{
	Emitters.Remove(Emitter);
	FNiagaraSceneProxy *SceneProxy = static_cast<FNiagaraSceneProxy*>(Component->SceneProxy);
	SceneProxy->UpdateEffectRenderers(this);
}

void FNiagaraEffectInstance::Tick(float DeltaSeconds)
{
	// pass the constants down to the emitter
	// TODO: should probably just pass a pointer to the table
	EffectBounds.Init();

	for (TPair<FNiagaraDataSetID, FNiagaraDataSet>& EventSetPair : ExternalEvents)
	{
		EventSetPair.Value.Tick();
	}

	for (TSharedPtr<FNiagaraSimulation>&it : Emitters)
	{
		UNiagaraEmitterProperties *Props = it->GetProperties().Get();
		check(Props);

		int Duration = Props->EndTime - Props->StartTime;
		int LoopedStartTime = Props->StartTime + Duration*it->GetLoopCount();
		int LoopedEndTime = Props->EndTime + Duration*it->GetLoopCount();

		// manage emitter lifetime
		//
		if ((Props->StartTime == 0.0f && Props->EndTime == 0.0f)
			|| (LoopedStartTime<Age && LoopedEndTime>Age)
			)
		{
			it->SetTickState(NTS_Running);
		}
		else
		{
			// if we're past end time, manage looping; we reset the emitters age constant
			// if it has one
			if (Props->NumLoops > 1 && it->GetLoopCount() < Props->NumLoops)
			{
				it->LoopRestart();
			}
			else
			{
				it->SetTickState(NTS_Dieing);
			}
		}

		//TODO - Handle constants better. Like waaaay better.
		it->GetConstants().Merge(it->GetProperties()->SpawnScriptProps.ExternalConstants);
		it->GetConstants().Merge(it->GetProperties()->UpdateScriptProps.ExternalConstants);
		it->GetConstants().Merge(InstanceConstants);

		it->PreTick();
	}

	for (TSharedPtr<FNiagaraSimulation>&it : Emitters)
	{
		if (it->GetTickState() != NTS_Dead && it->GetTickState() != NTS_Suspended)
		{
			it->Tick(DeltaSeconds);
		}

		EffectBounds += it->GetEffectRenderer()->GetBounds();
	}

	Age += DeltaSeconds;
}

FNiagaraSimulation* FNiagaraEffectInstance::GetEmitter(FString Name)
{
	for (TSharedPtr<FNiagaraSimulation> Sim : Emitters)
	{
		UNiagaraEmitterProperties* Props = Sim->GetProperties().Get();
		if (Props && Props->EmitterName == Name)
		{
			return Sim.Get();//We really need to sort out the ownership of objects in naigara. Am I free to pass a raw ptr here?
		}
	}
	return NULL;
}

void FNiagaraEffectInstance::RenderModuleupdate()
{
	FNiagaraSceneProxy *NiagaraProxy = static_cast<FNiagaraSceneProxy*>(Component->SceneProxy);
	if (NiagaraProxy)
	{
		NiagaraProxy->UpdateEffectRenderers(this);
	}
}

void FNiagaraEffectInstance::InitEmitters(UNiagaraEffect *InAsset)
{
	check(InAsset);
	for (int i = 0; i < InAsset->GetNumEmitters(); i++)
	{
		UNiagaraEmitterProperties *Props = InAsset->GetEmitterProperties(i);
		Props->Init();//Init these to be sure any cached data from the scripts is up to date.

		FNiagaraSimulation *Sim = new FNiagaraSimulation(Props, this);
		Emitters.Add(MakeShareable(Sim));
	}

	for (TSharedPtr<FNiagaraSimulation> Emitter : Emitters)
	{
		Emitter->PostInit();
	}
}

void FNiagaraEffectInstance::ReInitEmitters()
{
	for (TSharedPtr<FNiagaraSimulation> Emitter : Emitters)
	{
		UNiagaraEmitterProperties* Props = Emitter->GetProperties().Get();
		check(Props);
		Props->Init();//Init these to be sure any cached data from the scripts is up to date.
		Emitter->Init();
	}

	for (TSharedPtr<FNiagaraSimulation> Emitter : Emitters)
	{
		Emitter->PostInit();
	}
}

FNiagaraDataSet* FNiagaraEffectInstance::GetDataSet(FNiagaraDataSetID SetID, FName EmitterName)
{
	if (EmitterName == NAME_None)
	{
		if (FNiagaraDataSet* ExternalSet = ExternalEvents.Find(SetID))
		{
			return ExternalSet;
		}
	}
	for (TSharedPtr<FNiagaraSimulation> Emitter : Emitters)
	{
		check(Emitter.IsValid());
		if (Emitter->IsEnabled())
		{
			UNiagaraEmitterProperties* PinnedProps = Emitter->GetProperties().Get();
			check(PinnedProps);
			if (*PinnedProps->EmitterName == EmitterName)
			{
				return Emitter->GetDataSet(SetID);
			}
		}
	}

	return NULL;
}
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Engine/NiagaraEffect.h"



UNiagaraEffect::UNiagaraEffect(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}



FNiagaraEmitterProperties *UNiagaraEffect::AddEmitterProperties()
{
	FNiagaraEmitterProperties *Props = new FNiagaraEmitterProperties();
	EmitterProps.Add(Props);
	return Props;
}



void UNiagaraEffect::CreateEffectRendererProps(TSharedPtr<FNiagaraSimulation> Sim)
{
	UClass *RendererProps = Sim->GetEffectRenderer()->GetPropertiesClass();
	if (RendererProps)
	{
		Sim->GetProperties()->RendererProperties = ConstructObject<UNiagaraEffectRendererProperties>(RendererProps, this);
	}
	else
	{
		Sim->GetProperties()->RendererProperties = nullptr;
	}
}


void UNiagaraEffect::PostLoad()
{
	Super::PostLoad();
	EmitterProps.Empty();
	for (FNiagaraEmitterProperties &Props : EmitterPropsSerialized)
	{
		FNiagaraEmitterProperties *NewProps = new FNiagaraEmitterProperties(Props);
		EmitterProps.Add(NewProps);
	}
}


void UNiagaraEffect::PreSave()
{
	EmitterPropsSerialized.Empty();
	for (FNiagaraEmitterProperties *Props : EmitterProps)
	{
		EmitterPropsSerialized.Add(*Props);
	}
}




TSharedPtr<FNiagaraSimulation> FNiagaraEffectInstance::AddEmitter(FNiagaraEmitterProperties *Properties)
{
	FNiagaraSimulation *SimPtr = new FNiagaraSimulation(Properties);
	TSharedPtr<FNiagaraSimulation> Sim = MakeShareable(SimPtr);

	Sim->SetRenderModuleType(RMT_Sprites, Component->GetWorld()->FeatureLevel);
	Emitters.Add(Sim);
	FNiagaraSceneProxy *SceneProxy = static_cast<FNiagaraSceneProxy*>(Component->SceneProxy);
	SceneProxy->UpdateEffectRenderers(this);

	return Sim;
}
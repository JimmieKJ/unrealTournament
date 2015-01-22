// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NiagaraSimulation.h"
#include "NiagaraEffectRenderer.h"
#include "List.h"
#include "NiagaraEffect.generated.h"

UCLASS(MinimalAPI)
class UNiagaraEffect : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	ENGINE_API FNiagaraEmitterProperties *AddEmitterProperties();

	void Init()
	{
	}


	FNiagaraEmitterProperties *GetEmitterProperties(int Idx)
	{
		check(Idx < EmitterProps.Num());
		return EmitterProps[Idx];
	};

	int GetNumEmitters()
	{
		return EmitterProps.Num();
	}

	ENGINE_API void CreateEffectRendererProps(TSharedPtr<FNiagaraSimulation> Sim);

	virtual void PostLoad() override; 
	virtual void PreSave() override;

private:
	// serialized array of emitter properties
	UPROPERTY()
	TArray<FNiagaraEmitterProperties>EmitterPropsSerialized;

	TArray<FNiagaraEmitterProperties*> EmitterProps;
};


class FNiagaraEmitterProp
{

};



class FNiagaraEffectInstance
{
public:
	explicit FNiagaraEffectInstance(UNiagaraEffect *InAsset, UNiagaraComponent *InComponent)
	{
		Component = InComponent;
		for (int32 i = 0; i < InAsset->GetNumEmitters(); i++)
		{
			FNiagaraSimulation *Sim = new FNiagaraSimulation(InAsset->GetEmitterProperties(i), Component->GetWorld()->FeatureLevel);
			Emitters.Add(MakeShareable(Sim));
		}
		InitRenderModules(Component->GetWorld()->FeatureLevel);
	}

	explicit FNiagaraEffectInstance(UNiagaraEffect *InAsset)
		: Component(nullptr)
	{
		InitEmitters(InAsset);
	}

	void InitEmitters(UNiagaraEffect *InAsset)
	{
		check(InAsset);
		for (int i = 0; i < InAsset->GetNumEmitters(); i++)
		{
			FNiagaraEmitterProperties *Props = InAsset->GetEmitterProperties(i);
			FNiagaraSimulation *Sim = new FNiagaraSimulation(Props);
			Emitters.Add(MakeShareable(Sim));
		}
	}

	void InitRenderModules(ERHIFeatureLevel::Type InFeatureLevel)
	{
		for (TSharedPtr<FNiagaraSimulation> Sim : Emitters)
		{
			if(Sim->GetProperties()->RendererProperties == nullptr)
			{
				Component->GetAsset()->CreateEffectRendererProps(Sim);
			}
			Sim->SetRenderModuleType(Sim->GetProperties()->RenderModuleType, InFeatureLevel);
		}
	}

	void Init(UNiagaraComponent *InComponent)
	{
		if (InComponent->GetAsset())
		{
			Emitters.Empty();
			InitEmitters(InComponent->GetAsset());
		}
		Component = InComponent;
		InitRenderModules(Component->GetWorld()->FeatureLevel);
		RenderModuleupdate();
	}

	ENGINE_API TSharedPtr<FNiagaraSimulation> AddEmitter(FNiagaraEmitterProperties *Properties);

	void SetConstant(FName ConstantName, const float Value)
	{
		Constants.SetOrAdd(ConstantName, Value);
	}

	void SetConstant(FName ConstantName, const FVector4& Value)
	{
		Constants.SetOrAdd(ConstantName, Value);
	}

	void SetConstant(FName ConstantName, const FMatrix& Value)
	{
		Constants.SetOrAdd(ConstantName, Value);
	}


	void Tick(float DeltaSeconds)
	{
		// pass the constants down to the emitter
		// TODO: should probably just pass a pointer to the table
		for (TSharedPtr<FNiagaraSimulation>&it : Emitters)
		{
			it->SetConstants(Constants);
			it->GetConstants().Merge(it->GetProperties()->ExternalConstants);
			it->Tick(DeltaSeconds);
		}
	}

	void RenderModuleupdate()
	{
		FNiagaraSceneProxy *NiagaraProxy = static_cast<FNiagaraSceneProxy*>(Component->SceneProxy);
		if (NiagaraProxy)
		{
			NiagaraProxy->UpdateEffectRenderers(this);
		}
	}

	FNiagaraSimulation *GetEmitter(uint32 idx)
	{
		return Emitters[idx].Get();
	}


	UNiagaraComponent *GetComponent() { return Component; }

	TArray< TSharedPtr<FNiagaraSimulation> > &GetEmitters()	{ return Emitters; }

private:
	UNiagaraComponent *Component;

	/** Local constant table. */
	FNiagaraConstantMap Constants;

	TArray< TSharedPtr<FNiagaraSimulation> > Emitters;


};
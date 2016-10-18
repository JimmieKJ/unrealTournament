// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NiagaraSimulation.h"
#include "NiagaraEmitterProperties.h"
#include "NiagaraEffectRenderer.h"
#include "List.h"
#include "NiagaraEffect.generated.h"


enum NiagaraTickState
{
	ERunning,		// normally running
	ESuspended,		// stop simulating and spawning, still render
	EDieing,		// stop spawning, still simulate and render
	EDead			// no live particles, no new spawning
};

UCLASS()
class NIAGARA_API UNiagaraEffect : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	UNiagaraEmitterProperties* AddEmitterProperties(UNiagaraEmitterProperties *Props = nullptr);
	void DeleteEmitterProperties(UNiagaraEmitterProperties *Props);

	void Init()
	{
	}


	UNiagaraEmitterProperties *GetEmitterProperties(int Idx)
	{
		check(Idx < EmitterProps.Num());
		return EmitterProps[Idx];
	};

	int GetNumEmitters()
	{
		return EmitterProps.Num();
	}

	void CreateEffectRendererProps(TSharedPtr<FNiagaraSimulation> Sim);

	//Begin UObject Interface
	virtual void PostLoad()override;
	//End UObject Interface
private:
	UPROPERTY()
	TArray<UNiagaraEmitterProperties*> EmitterProps;
};



class FNiagaraEffectInstance
{
public:
	explicit FNiagaraEffectInstance(UNiagaraEffect *InAsset, UNiagaraComponent *InComponent)
		: Effect(InAsset)
	{
		Component = InComponent;
		for (int32 i = 0; i < InAsset->GetNumEmitters(); i++)
		{
			FNiagaraSimulation *Sim = new FNiagaraSimulation(InAsset->GetEmitterProperties(i), this, Component->GetWorld()->FeatureLevel);
			Emitters.Add(MakeShareable(Sim));
		}
		InitRenderModules(Component->GetWorld()->FeatureLevel);
	}

	explicit FNiagaraEffectInstance(UNiagaraEffect *InAsset)
		: Component(nullptr)
		, Effect(InAsset)
	{
		InitEmitters(InAsset);
	}

	NIAGARA_API void InitEmitters(UNiagaraEffect *InAsset);
	NIAGARA_API void ReInitEmitters();
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

		Age = 0.0f;
	}

	void ReInit()
	{
		if (Component)
		{
			ReInitEmitters();
			InitRenderModules(Component->GetWorld()->FeatureLevel);
			RenderModuleupdate();
		}
		Age = 0.0f;
	}

	NIAGARA_API TSharedPtr<FNiagaraSimulation> AddEmitter(UNiagaraEmitterProperties *Properties);
	NIAGARA_API void DeleteEmitter(TSharedPtr<FNiagaraSimulation> Emitter);

	void SetConstant(FNiagaraVariableInfo ID, const float Value)
	{
		InstanceConstants.SetOrAdd(ID.Name, Value);
	}

	void SetConstant(FNiagaraVariableInfo ID, const FVector4& Value)
	{
		InstanceConstants.SetOrAdd(ID.Name, Value);
	}

	void SetConstant(FNiagaraVariableInfo ID, const FMatrix& Value)
	{
		InstanceConstants.SetOrAdd(ID.Name, Value);
	}
	
	void Tick(float DeltaSeconds);

	NIAGARA_API void RenderModuleupdate();

	FNiagaraSimulation *GetEmitter(uint32 idx)
	{
		return Emitters[idx].Get();
	}

	FNiagaraSimulation* GetEmitter(FString Name);

	UNiagaraComponent *GetComponent() { return Component; }

	TArray< TSharedPtr<FNiagaraSimulation> > &GetEmitters()	{ return Emitters; }

	FBox GetEffectBounds()	{ return EffectBounds;  }

	/** Gets a data set either from another emitter or one owned by the effect itself. */
	FNiagaraDataSet* GetDataSet(FNiagaraDataSetID SetID, FName EmitterName=NAME_None);
	
private:
	UNiagaraComponent *Component;
	UNiagaraEffect *Effect;
	FBox EffectBounds;
	float Age;

	/** Local constant table. */
	FNiagaraConstants InstanceConstants;
	
	TMap<FNiagaraDataSetID, FNiagaraDataSet> ExternalEvents;

	TArray< TSharedPtr<FNiagaraSimulation> > Emitters;
};
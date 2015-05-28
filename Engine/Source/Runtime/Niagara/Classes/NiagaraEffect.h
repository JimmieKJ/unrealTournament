// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NiagaraSimulation.h"
#include "Engine/NiagaraEffectRenderer.h"
#include "List.h"
#include "Runtime/VectorVM/Public/VectorVMDataObject.h"
#include "NiagaraEffect.generated.h"


UCLASS()
class NIAGARA_API UNiagaraEffect : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	FNiagaraEmitterProperties* AddEmitterProperties();

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

	void CreateEffectRendererProps(TSharedPtr<FNiagaraSimulation> Sim);

	virtual void PostLoad() override; 
	virtual void PreSave() override;

private:
	// serialized array of emitter properties
	UPROPERTY()
	TArray<FNiagaraEmitterProperties>EmitterPropsSerialized;

	TArray<FNiagaraEmitterProperties*> EmitterProps;
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

	NIAGARA_API void InitEmitters(UNiagaraEffect *InAsset);
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

	NIAGARA_API TSharedPtr<FNiagaraSimulation> AddEmitter(FNiagaraEmitterProperties *Properties);

	void SetConstant(FNiagaraVariableInfo ID, const float Value)
	{
		Constants.SetOrAdd(ID, Value);
	}

	void SetConstant(FNiagaraVariableInfo ID, const FVector4& Value)
	{
		Constants.SetOrAdd(ID, Value);
	}

	void SetConstant(FNiagaraVariableInfo ID, const FMatrix& Value)
	{
		Constants.SetOrAdd(ID, Value);
	}

	void SetConstant(FName ConstantName, FNiagaraDataObject *Value)
	{
		Constants.SetOrAdd(ConstantName, Value);
	}



	void Tick(float DeltaSeconds)
	{
		Constants.SetOrAdd(FName(TEXT("EffectGrid")), &VolumeGrid); 

		// pass the constants down to the emitter
		// TODO: should probably just pass a pointer to the table
		EffectBounds.Init();

		for (TSharedPtr<FNiagaraSimulation>&it : Emitters)
		{
			it->SetConstants(Constants);
			it->GetConstants().Merge(it->GetProperties()->ExternalConstants);
			it->Tick(DeltaSeconds);

			EffectBounds += it->GetEffectRenderer()->GetBounds();
		}
	}

	NIAGARA_API void RenderModuleupdate();

	FNiagaraSimulation *GetEmitter(uint32 idx)
	{
		return Emitters[idx].Get();
	}


	UNiagaraComponent *GetComponent() { return Component; }

	TArray< TSharedPtr<FNiagaraSimulation> > &GetEmitters()	{ return Emitters; }

	FBox GetEffectBounds()	{ return EffectBounds;  }
private:
	UNiagaraComponent *Component;
	FBox EffectBounds;

	/** Local constant table. */
	FNiagaraConstantMap Constants;

	TArray< TSharedPtr<FNiagaraSimulation> > Emitters;

	FNiagaraSparseVolumeDataObject VolumeGrid;

};
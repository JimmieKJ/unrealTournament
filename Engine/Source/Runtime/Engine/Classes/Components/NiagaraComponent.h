// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NiagaraComponent.generated.h"

/* Predefined particle attribute names */
const FName ParticleAttr_Position = "Position";
const FName ParticleAttr_Velocity = "Velocity";
const FName ParticleAttr_Color = "Color";



/**
* Contains the simulation data for a single FNiagaraSimulation
* all buffers for particle attributes are stored here for convenient access
*/
class FNiagaraEmitterParticleData
{
public:
	FNiagaraEmitterParticleData()
		: CurrentBuffer(0),
		NumParticles(0)
	{
		AddAttribute("Position");
		AddAttribute("Velocity");
		AddAttribute("Color");
		AddAttribute("Rotation");
		AddAttribute("Age");
	}

	~FNiagaraEmitterParticleData() {}

	void Allocate(uint32 NumExpectedParticles)
	{
		ParticleBuffers[CurrentBuffer].Reset(NumExpectedParticles * AttrMap.Num());
		ParticleBuffers[CurrentBuffer].AddUninitialized(NumExpectedParticles * AttrMap.Num());

		//Can we make this a ring buffer and cut memory use in half?

		ParticleAllocation = NumExpectedParticles;
	}

	const FVector4 *GetAttributeData(FName Name) const
	{
		int32 Offset = AttrMap[Name] * ParticleAllocation;
		return ParticleBuffers[CurrentBuffer].GetData() + Offset;
	}

	FVector4 *GetAttributeDataWrite(FName Name) 
	{
		int32 Offset = AttrMap[Name] * ParticleAllocation;
		return ParticleBuffers[CurrentBuffer].GetData() + Offset;
	}

	


	void AddAttribute(FName NewAttrName)
	{
		uint32 Idx = AttrMap.Num();
		AttrMap.Add(NewAttrName, Idx);
	}

	int GetNumAttributes()				{ return AttrMap.Num(); }

	uint32 GetNumParticles() const		{ return NumParticles; }
	uint32 GetParticleAllocation() const { return ParticleAllocation; }
	void SetNumParticles(uint32 Num)	{ NumParticles = Num;  }
	void SwapBuffers()					{ CurrentBuffer ^= 0x1; }

	FVector4 *GetCurrentBuffer()		{ return ParticleBuffers[CurrentBuffer].GetData(); }
	FVector4 *GetPreviousBuffer()		{ return ParticleBuffers[CurrentBuffer^0x1].GetData(); }

	int GetBytesUsed()	{ return (ParticleBuffers[0].Num() + ParticleBuffers[1].Num()) * 16 + Attributes.Num() * 4; }
private:
	uint32 CurrentBuffer, NumParticles, ParticleAllocation;
	TArray<FVector4> ParticleBuffers[2];
	TArray<FVector4*> Attributes;
	TMap<FName, uint32> AttrMap;
};



/**
* UNiagaraComponent is the primitive component for a Niagara effect.
* @see ANiagaraActor
* @see UNiagaraEffect
*/
UCLASS()
class ENGINE_API UNiagaraComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()
private:
	UPROPERTY()
	//class UNiagaraEffect *Effect;
	class UNiagaraEffect *Asset;
	class FNiagaraEffectInstance *EffectInstance;

	// Begin UActorComponent interface.
protected:
	virtual void OnRegister() override;
	virtual void OnUnregister()  override;
	virtual void SendRenderDynamicData_Concurrent() override;
public:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	// End UActorComponent interface.

	// Begin UPrimitiveComponent Interface
	virtual int32 GetNumMaterials() const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	// End UPrimitiveComponent Interface

	void SetAsset(UNiagaraEffect *InAsset);
	UNiagaraEffect *GetAsset() const { return Asset; }

	FNiagaraEffectInstance *GetEffectInstance()	const { return EffectInstance; }
	void SetEffectInstance(FNiagaraEffectInstance *InInstance)	{ EffectInstance = InInstance; }

	// Begin UObject interface.
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	// End UObject interface.
};






/**
* Scene proxy for drawing niagara particle simulations.
*/
class FNiagaraSceneProxy : public FPrimitiveSceneProxy
{
public:

	FNiagaraSceneProxy(const UNiagaraComponent* InComponent);
	~FNiagaraSceneProxy();

	/** Called on render thread to assign new dynamic data */
	void SetDynamicData_RenderThread(struct FNiagaraDynamicDataBase* NewDynamicData);
	TArray<class NiagaraEffectRenderer*> &GetEffectRenderers() { return EffectRenderers; }
	void AddEffectRenderer(NiagaraEffectRenderer *Renderer)	{ EffectRenderers.Add(Renderer); }
	ENGINE_API void UpdateEffectRenderers(FNiagaraEffectInstance *InEffect);

private:
	void ReleaseRenderThreadResources();

	// FPrimitiveSceneProxy interface.
	virtual void CreateRenderThreadResources() override;

	virtual void OnActorPositionChanged() override;
	virtual void OnTransformChanged() override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)  override;

	/*
	virtual bool CanBeOccluded() const override
	{
	return !MaterialRelevance.bDisableDepthTest;
	}
	*/
	virtual uint32 GetMemoryFootprint() const override;

	uint32 GetAllocatedSize() const;


private:
	//class NiagaraEffectRenderer *EffectRenderer;
	TArray<class NiagaraEffectRenderer *>EffectRenderers;
};

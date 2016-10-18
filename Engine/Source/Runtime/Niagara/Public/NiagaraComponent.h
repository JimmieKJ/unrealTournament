// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"

#include "NiagaraComponent.generated.h"

class UNiagaraEffect;
class FNiagaraEffectInstance;

/**
* UNiagaraComponent is the primitive component for a Niagara effect.
* @see ANiagaraActor
* @see UNiagaraEffect
*/
UCLASS()
class NIAGARA_API UNiagaraComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()
private:
	UPROPERTY()
	UNiagaraEffect* Asset;

	TSharedPtr<FNiagaraEffectInstance> EffectInstance;

	//~ Begin UActorComponent Interface.
protected:
	virtual void OnRegister() override;
	virtual void OnUnregister()  override;
	virtual void SendRenderDynamicData_Concurrent() override;
public:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual const UObject* AdditionalStatObject() const override;
	//~ End UActorComponent Interface.

	//~ Begin UPrimitiveComponent Interface
	virtual int32 GetNumMaterials() const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface

	void SetAsset(UNiagaraEffect* InAsset);
	UNiagaraEffect* GetAsset() const { return Asset; }

	TSharedPtr<FNiagaraEffectInstance> GetEffectInstance()	const { return EffectInstance; }
	void SetEffectInstance(TSharedPtr<FNiagaraEffectInstance> InInstance)	{ EffectInstance = InInstance; }

	//~ Begin UObject Interface.
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface.

	static const TArray<FNiagaraVariableInfo>& GetSystemConstants();
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
	TArray<class NiagaraEffectRenderer*>& GetEffectRenderers() { return EffectRenderers; }
	void AddEffectRenderer(NiagaraEffectRenderer* Renderer)	{ EffectRenderers.Add(Renderer); }
	void UpdateEffectRenderers(FNiagaraEffectInstance* InEffect);

private:
	void ReleaseRenderThreadResources();

	//~ Begin FPrimitiveSceneProxy Interface.
	virtual void CreateRenderThreadResources() override;

	virtual void OnTransformChanged() override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	/*
	virtual bool CanBeOccluded() const override
	{
	return !MaterialRelevance.bDisableDepthTest;
	}
	*/
	virtual uint32 GetMemoryFootprint() const override;

	uint32 GetAllocatedSize() const;


private:
	//class NiagaraEffectRenderer* EffectRenderer;
	TArray<class NiagaraEffectRenderer*>EffectRenderers;
};

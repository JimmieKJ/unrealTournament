// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraPrivate.h"
#include "NiagaraComponent.h"
#include "NiagaraScript.h"
#include "VectorVM.h"
#include "ParticleHelper.h"
#include "Particles/ParticleResources.h"
#include "NiagaraEffectRenderer.h"
#include "NiagaraEffect.h"
#include "NiagaraSimulation.h"
#include "MeshBatch.h"
#include "SceneUtils.h"
#include "ComponentReregisterContext.h"

DECLARE_CYCLE_STAT(TEXT("Gen Verts"),STAT_NiagaraGenerateVertices,STATGROUP_Niagara);

DEFINE_LOG_CATEGORY(LogNiagara);

FNiagaraSceneProxy::FNiagaraSceneProxy(const UNiagaraComponent* InComponent)
		:	FPrimitiveSceneProxy(InComponent)
{
	UpdateEffectRenderers(InComponent->GetEffectInstance().Get());
}

void FNiagaraSceneProxy::UpdateEffectRenderers(FNiagaraEffectInstance* InEffect)
{
	EffectRenderers.Empty();
	if (InEffect)
	{
		for (TSharedPtr<FNiagaraSimulation>Emitter : InEffect->GetEmitters())
		{
			AddEffectRenderer(Emitter->GetEffectRenderer());
		}
	}
}

FNiagaraSceneProxy::~FNiagaraSceneProxy()
{
	ReleaseRenderThreadResources();
}

/** Called on render thread to assign new dynamic data */
void FNiagaraSceneProxy::SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData)
{
	for (NiagaraEffectRenderer* Renderer : EffectRenderers)
	{
		if (Renderer)
		{
			Renderer->SetDynamicData_RenderThread(NewDynamicData);
		}
	}
	return;
}


void FNiagaraSceneProxy::ReleaseRenderThreadResources()
{
	for (NiagaraEffectRenderer* Renderer : EffectRenderers)
	{
		if (Renderer)
		{
			Renderer->ReleaseRenderThreadResources();
		}
	}
	return;
}

// FPrimitiveSceneProxy interface.
void FNiagaraSceneProxy::CreateRenderThreadResources()
{
	for (NiagaraEffectRenderer* Renderer : EffectRenderers)
	{
		if (Renderer)
		{
			Renderer->CreateRenderThreadResources();
		}
	}
	return;
}


void FNiagaraSceneProxy::OnTransformChanged()
{
	//WorldSpacePrimitiveUniformBuffer.ReleaseResource();
}

FPrimitiveViewRelevance FNiagaraSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Relevance;
	Relevance.bDynamicRelevance = true;

	for (NiagaraEffectRenderer* Renderer : EffectRenderers)
	{
		if (Renderer)
		{
			Relevance |= Renderer->GetViewRelevance(View, this);
		}
	}
	return Relevance;
}


uint32 FNiagaraSceneProxy::GetMemoryFootprint() const
{ 
	return (sizeof(*this) + GetAllocatedSize()); 
}

uint32 FNiagaraSceneProxy::GetAllocatedSize() const
{ 
	uint32 DynamicDataSize = 0;
	for (NiagaraEffectRenderer* Renderer : EffectRenderers)
	{
		if (Renderer)
		{
			DynamicDataSize += Renderer->GetDynamicDataSize();
		}
	}
	return FPrimitiveSceneProxy::GetAllocatedSize() + DynamicDataSize;
}


void FNiagaraSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	for (NiagaraEffectRenderer* Renderer : EffectRenderers)
	{
		if (Renderer)
		{
			Renderer->GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector, this);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

UNiagaraComponent::UNiagaraComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
}


void UNiagaraComponent::TickComponent(float DeltaSeconds, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
//	EmitterAge += DeltaSeconds;

	if (EffectInstance.Get())
	{ 
		static FNiagaraVariableInfo Const_Zero(TEXT("ZERO"), ENiagaraDataType::Vector);
		static FNiagaraVariableInfo Const_DeltaTime(TEXT("Delta Time"), ENiagaraDataType::Vector);
		static FNiagaraVariableInfo Const_EmitterPos(TEXT("Emitter Position"), ENiagaraDataType::Vector);
		static FNiagaraVariableInfo Const_EmitterAge(TEXT("Emitter Age"), ENiagaraDataType::Vector);
		static FNiagaraVariableInfo Const_EmitterX(TEXT("Emitter X Axis"), ENiagaraDataType::Vector);
		static FNiagaraVariableInfo Const_EmitterY(TEXT("Emitter Y Axis"), ENiagaraDataType::Vector);
		static FNiagaraVariableInfo Const_EmitterZ(TEXT("Emitter Z Axis"), ENiagaraDataType::Vector);
		static FNiagaraVariableInfo Const_EmitterTransform(TEXT("Emitter Transform"), ENiagaraDataType::Matrix);
		//Todo, open this up to the UI and setting via code and BPs.
		EffectInstance->SetConstant(Const_Zero, FVector4(0.0f, 0.0f, 0.0f, 0.0f));	// zero constant
		EffectInstance->SetConstant(Const_DeltaTime, FVector4(DeltaSeconds, DeltaSeconds, DeltaSeconds, DeltaSeconds));
		EffectInstance->SetConstant(Const_EmitterPos, FVector4(ComponentToWorld.GetTranslation()));
		EffectInstance->SetConstant(Const_EmitterX, FVector4(ComponentToWorld.GetUnitAxis(EAxis::X)));
		EffectInstance->SetConstant(Const_EmitterY, FVector4(ComponentToWorld.GetUnitAxis(EAxis::Y)));
		EffectInstance->SetConstant(Const_EmitterZ, FVector4(ComponentToWorld.GetUnitAxis(EAxis::Z)));
		EffectInstance->SetConstant(Const_EmitterTransform, ComponentToWorld.ToMatrixWithScale());

		EffectInstance->Tick(DeltaSeconds);
	}

	UpdateComponentToWorld();
	MarkRenderDynamicDataDirty();
}

const UObject* UNiagaraComponent::AdditionalStatObject() const
{
	return Asset;
}

void UNiagaraComponent::OnRegister()
{
	Super::OnRegister();

	if (!EffectInstance.IsValid() && Asset)
	{
		EffectInstance = MakeShareable(new FNiagaraEffectInstance(Asset, this));
	}

	if (EffectInstance.IsValid())
	{
		EffectInstance->Init(this);

		// initialize all render modules
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			FChangeNiagaraRenderModule,
			FNiagaraEffectInstance*, InEffect, this->EffectInstance.Get(),
			UNiagaraComponent*, InComponent, this,
			{
			for (TSharedPtr<FNiagaraSimulation> Emitter : InEffect->GetEmitters())
			{
				Emitter->SetRenderModuleType(Emitter->GetProperties()->RenderModuleType, InComponent->GetWorld()->FeatureLevel);
			}
			InEffect->RenderModuleupdate();
		}
		);
		FlushRenderingCommands();
	}
	VectorVM::Init();
}


void UNiagaraComponent::OnUnregister()
{
	Super::OnUnregister();
}

void UNiagaraComponent::SendRenderDynamicData_Concurrent()
{
	if (EffectInstance.IsValid() && SceneProxy)
	{
		FNiagaraSceneProxy* NiagaraProxy = static_cast<FNiagaraSceneProxy*>(SceneProxy);
		for (int32 i = 0; i < EffectInstance->GetEmitters().Num(); i++)
		{
			TSharedPtr<FNiagaraSimulation> Emitter = EffectInstance->GetEmitters()[i];
			NiagaraEffectRenderer* Renderer = Emitter->GetEffectRenderer();
			if (Renderer)
			{
				if (Emitter->IsEnabled())
				{
					FNiagaraDynamicDataBase* DynamicData = Renderer->GenerateVertexData(Emitter->GetData());

					ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
						FSendNiagaraDynamicData,
						NiagaraEffectRenderer*, EffectRenderer, Emitter->GetEffectRenderer(),
						FNiagaraDynamicDataBase*, DynamicData, DynamicData,
						{
						EffectRenderer->SetDynamicData_RenderThread(DynamicData);
					});
				}
				else
				{
					ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
						FSendNiagaraDynamicData,
						NiagaraEffectRenderer*, EffectRenderer, Emitter->GetEffectRenderer(),
						{
							EffectRenderer->SetDynamicData_RenderThread(nullptr);
						});
				}
			}
		}
	}

}

int32 UNiagaraComponent::GetNumMaterials() const
{
	return 0;
}


FBoxSphereBounds UNiagaraComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox SimBounds(ForceInit);
	if (EffectInstance.IsValid())
	{
		FBoxSphereBounds BSBounds(EffectInstance->GetEffectBounds());
		//BSBounds.Origin = LocalToWorld.GetLocation();
		return BSBounds;
	}
	return FBoxSphereBounds();
}

FPrimitiveSceneProxy* UNiagaraComponent::CreateSceneProxy()
{
	FNiagaraSceneProxy* Proxy = new FNiagaraSceneProxy(this);
	Proxy->UpdateEffectRenderers(EffectInstance.Get());
	return Proxy;
}

#if WITH_EDITOR
void UNiagaraComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FComponentReregisterContext ReregisterContext(this);
}
#endif // WITH_EDITOR



void UNiagaraComponent::SetAsset(UNiagaraEffect* InAsset)
{
	Asset = InAsset;

	EffectInstance = MakeShareable(new FNiagaraEffectInstance(Asset, this));
}

const TArray<FNiagaraVariableInfo>& UNiagaraComponent::GetSystemConstants()
{
	static TArray<FNiagaraVariableInfo> SystemConstants;
	if (SystemConstants.Num() == 0)
	{
		static FNiagaraVariableInfo Const_Zero(TEXT("ZERO"), ENiagaraDataType::Vector);
		static FNiagaraVariableInfo Const_DeltaTime(TEXT("Delta Time"), ENiagaraDataType::Vector);
		static FNiagaraVariableInfo Const_EmitterPos(TEXT("Emitter Position"), ENiagaraDataType::Vector);
		static FNiagaraVariableInfo Const_EmitterAge(TEXT("Emitter Age"), ENiagaraDataType::Vector);
		static FNiagaraVariableInfo Const_EmitterX(TEXT("Emitter X Axis"), ENiagaraDataType::Vector);
		static FNiagaraVariableInfo Const_EmitterY(TEXT("Emitter Y Axis"), ENiagaraDataType::Vector);
		static FNiagaraVariableInfo Const_EmitterZ(TEXT("Emitter Z Axis"), ENiagaraDataType::Vector);
		static FNiagaraVariableInfo Const_EmitterTransform(TEXT("Emitter Transform"), ENiagaraDataType::Matrix);
		SystemConstants.Add(Const_Zero);
		SystemConstants.Add(Const_DeltaTime);
		SystemConstants.Add(Const_EmitterPos);
		SystemConstants.Add(Const_EmitterAge);
		SystemConstants.Add(Const_EmitterX);
		SystemConstants.Add(Const_EmitterY);
		SystemConstants.Add(Const_EmitterZ);
		SystemConstants.Add(Const_EmitterTransform);
	}
	return SystemConstants;
}
FNiagaraDataSetID FNiagaraDataSetID::DeathEvent(TEXT("Death"),ENiagaraDataSetType::Event);
FNiagaraDataSetID FNiagaraDataSetID::SpawnEvent(TEXT("Spawn"), ENiagaraDataSetType::Event);
FNiagaraDataSetID FNiagaraDataSetID::CollisionEvent(TEXT("Collision"), ENiagaraDataSetType::Event);
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraEffectRenderer.h: Base class for Niagara render modules
==============================================================================*/
#pragma once

#include "SceneUtils.h"
#include "ParticleHelper.h"
#include "ParticleVertexFactory.h"
#include "ParticleBeamTrailVertexFactory.h"
#include "NiagaraComponent.h"
#include "NiagaraSimulation.h"
#include "NiagaraEffectRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"


/** Struct used to pass dynamic data from game thread to render thread */
struct FNiagaraDynamicDataBase
{
};


class SimpleTimer
{
public:
	SimpleTimer()
	{
		StartTime = FPlatformTime::Seconds() * 1000.0;
	}

	double GetElapsedMilliseconds()
	{
		return (FPlatformTime::Seconds()*1000.0) - StartTime;
	}

	~SimpleTimer()
	{
	}

private:
	double StartTime;
};


/**
* Base class for Niagara effect renderers. Effect renderers handle generating vertex data for and
* drawing of simulation data coming out of FNiagaraSimulation instances.
*/
class NiagaraEffectRenderer
{
public:
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const = 0;

	virtual void SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData) = 0;
	virtual void CreateRenderThreadResources() = 0;
	virtual void ReleaseRenderThreadResources() = 0;
	virtual FNiagaraDynamicDataBase *GenerateVertexData(const FNiagaraDataSet &Data) = 0;
	virtual int GetDynamicDataSize() = 0;

	virtual bool HasDynamicData() = 0;

	virtual bool SetMaterialUsage() = 0;

	virtual const TArray<FNiagaraVariableInfo>& GetRequiredAttributes() = 0;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View, const FNiagaraSceneProxy *SceneProxy)
	{
		FPrimitiveViewRelevance Result;
		bool bHasDynamicData = HasDynamicData();

		Result.bDrawRelevance = bHasDynamicData && SceneProxy->IsShown(View) && View->Family->EngineShowFlags.Particles;
		Result.bShadowRelevance = bHasDynamicData && SceneProxy->IsShadowCast(View);
		Result.bDynamicRelevance = bHasDynamicData;
		if (bHasDynamicData && View->Family->EngineShowFlags.Bounds)
		{
			Result.bOpaqueRelevance = true;
		}
		MaterialRelevance.SetPrimitiveViewRelevance(Result);

		return Result;
	}


	UMaterial *GetMaterial()	{ return Material; }
	void SetMaterial(UMaterial *InMaterial, ERHIFeatureLevel::Type FeatureLevel)
	{
		Material = InMaterial;
		if (!Material || !SetMaterialUsage())
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}
		check(Material);
		MaterialRelevance = Material->GetRelevance(FeatureLevel);
	}

	virtual ~NiagaraEffectRenderer() {}

	virtual UClass *GetPropertiesClass() = 0;
	virtual void SetRendererProperties(UNiagaraEffectRendererProperties *Props) = 0;

	float GetCPUTimeMS() { return CPUTimeMS; }
	FBox GetBounds()	{ return CachedBounds;  }

protected:
	NiagaraEffectRenderer()	
		: TickState(NTS_Running)

	{
		Material = UMaterial::GetDefaultMaterial(MD_Surface);
	}

	mutable float CPUTimeMS;
	UMaterial* Material;
	FBox CachedBounds;

private:
	FMaterialRelevance MaterialRelevance;
	ENiagaraTickState TickState;
};




/**
* NiagaraEffectRendererSprites renders an FNiagaraSimulation as sprite particles
*/
class NIAGARA_API NiagaraEffectRendererSprites : public NiagaraEffectRenderer
{
public:	

	explicit NiagaraEffectRendererSprites(ERHIFeatureLevel::Type FeatureLevel, UNiagaraEffectRendererProperties *Props);
	~NiagaraEffectRendererSprites()
	{
		ReleaseRenderThreadResources();
	}


	virtual void ReleaseRenderThreadResources() override;

	// FPrimitiveSceneProxy interface.
	virtual void CreateRenderThreadResources() override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const override;
	virtual bool SetMaterialUsage() override;
	/** Update render data buffer from attributes */
	FNiagaraDynamicDataBase *GenerateVertexData(const FNiagaraDataSet &Data) override;

	virtual void SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData) override;
	int GetDynamicDataSize() override;
	bool HasDynamicData() override;

	UClass *GetPropertiesClass() override { return UNiagaraSpriteRendererProperties::StaticClass(); }
	void SetRendererProperties(UNiagaraEffectRendererProperties *Props) override { Properties = Cast<UNiagaraSpriteRendererProperties>(Props); }

	virtual const TArray<FNiagaraVariableInfo>& GetRequiredAttributes() override;

private:
	UNiagaraSpriteRendererProperties *Properties;
	struct FNiagaraDynamicDataSprites *DynamicDataRender;
	mutable TUniformBuffer<FPrimitiveUniformShaderParameters> WorldSpacePrimitiveUniformBuffer;
	class FParticleSpriteVertexFactory* VertexFactory;
};





/**
* NiagaraEffectRendererRibbon renders an FNiagaraSimulation as a ribbon connecting all particles
* in order by particle age.
*/
class NIAGARA_API NiagaraEffectRendererRibbon : public NiagaraEffectRenderer
{
public:
	NiagaraEffectRendererRibbon(ERHIFeatureLevel::Type FeatureLevel, UNiagaraEffectRendererProperties *Props);
	~NiagaraEffectRendererRibbon()
	{
		ReleaseRenderThreadResources();
	}

	virtual void ReleaseRenderThreadResources() override;
	virtual void CreateRenderThreadResources() override;


	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const override;

	virtual bool SetMaterialUsage() override;

	/** Update render data buffer from attributes */
	FNiagaraDynamicDataBase *GenerateVertexData(const FNiagaraDataSet &Data) override;

	void AddRibbonVert(TArray<FParticleBeamTrailVertex>& RenderData, FVector ParticlePos, const FNiagaraDataSet &Data, FVector2D UV1,
		const FVector4 &Color, const FVector4 &Age, const FVector4 &Rotation)
	{
		FParticleBeamTrailVertex& NewVertex = *new(RenderData)FParticleBeamTrailVertex;
		NewVertex.Position = ParticlePos;
		NewVertex.OldPosition = NewVertex.Position;
		NewVertex.Color = FLinearColor(Color);
		NewVertex.ParticleId = 0;
		NewVertex.RelativeTime = Age.X;
		NewVertex.Size = FVector2D(Rotation.Y, Rotation.Z);
		NewVertex.Rotation = Rotation.X;
		NewVertex.SubImageIndex = 0.f;
		NewVertex.Tex_U = UV1.X;
		NewVertex.Tex_V = UV1.Y;
		NewVertex.Tex_U2 = UV1.X;
		NewVertex.Tex_V2 = UV1.Y;
	}



	virtual void SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData) override;
	int GetDynamicDataSize() override;
	bool HasDynamicData() override;

	virtual const TArray<FNiagaraVariableInfo>& GetRequiredAttributes() override;

	UClass *GetPropertiesClass() override { return UNiagaraRibbonRendererProperties::StaticClass(); }
	void SetRendererProperties(UNiagaraEffectRendererProperties *Props) override { Properties = Cast<UNiagaraRibbonRendererProperties>(Props); }

private:
	class FParticleBeamTrailVertexFactory *VertexFactory;
	UNiagaraRibbonRendererProperties *Properties;
	struct FNiagaraDynamicDataRibbon *DynamicDataRender;
	mutable TUniformBuffer<FPrimitiveUniformShaderParameters> WorldSpacePrimitiveUniformBuffer;
};






/**
* NiagaraEffectRendererSprites renders an FNiagaraSimulation as sprite particles
*/
class NIAGARA_API NiagaraEffectRendererMeshes : public NiagaraEffectRenderer
{
public:

	explicit NiagaraEffectRendererMeshes(ERHIFeatureLevel::Type FeatureLevel, UNiagaraEffectRendererProperties *Props);
	~NiagaraEffectRendererMeshes()
	{
		ReleaseRenderThreadResources();
	}


	virtual void ReleaseRenderThreadResources() override;

	// FPrimitiveSceneProxy interface.
	virtual void CreateRenderThreadResources() override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const override;
	virtual bool SetMaterialUsage() override;
	/** Update render data buffer from attributes */
	FNiagaraDynamicDataBase *GenerateVertexData(const FNiagaraDataSet &Data) override;

	virtual void SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData) override;
	int GetDynamicDataSize() override;
	bool HasDynamicData() override;

	UClass *GetPropertiesClass() override { return UNiagaraMeshRendererProperties::StaticClass(); }
	void SetRendererProperties(UNiagaraEffectRendererProperties *Props) override { Properties = Cast<UNiagaraMeshRendererProperties>(Props); }
	virtual const TArray<FNiagaraVariableInfo>& GetRequiredAttributes() override;

	void SetupVertexFactory(FMeshParticleVertexFactory *InVertexFactory, const FStaticMeshLODResources& LODResources) const;
private:
	UNiagaraMeshRendererProperties *Properties;
	struct FNiagaraDynamicDataMesh *DynamicDataRender;
	mutable TUniformBuffer<FPrimitiveUniformShaderParameters> WorldSpacePrimitiveUniformBuffer;
	class FMeshParticleVertexFactory* VertexFactory;
};

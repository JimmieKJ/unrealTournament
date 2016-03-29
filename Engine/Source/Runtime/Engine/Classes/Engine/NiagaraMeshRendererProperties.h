// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraEffectRendererProperties.h"
#include "StaticMeshResources.h"
#include "NiagaraMeshRendererProperties.generated.h"

UCLASS()
class UNiagaraMeshRendererProperties : public UNiagaraEffectRendererProperties
{
public:
	GENERATED_UCLASS_BODY()

	UNiagaraMeshRendererProperties()
	{
		ParticleMesh = nullptr;
	}

	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
	{
		if (ParticleMesh && PropertyChangedEvent.Property->GetName() == "ParticleMesh")
		{
			const FStaticMeshLODResources& LODModel = ParticleMesh->RenderData->LODResources[0];
			for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
			{
				const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];
				UMaterialInterface *Material = ParticleMesh->GetMaterial(Section.MaterialIndex);
				FMaterialRenderProxy* MaterialProxy = Material->GetRenderProxy(false, false);
				Material->CheckMaterialUsage(MATUSAGE_MeshParticles);
			}
		}
	}

	UPROPERTY(EditAnywhere, Category = "Mesh Rendering")
	UStaticMesh *ParticleMesh;
};

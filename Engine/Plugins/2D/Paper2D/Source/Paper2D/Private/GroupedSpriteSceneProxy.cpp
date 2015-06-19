// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"
#include "GroupedSpriteSceneProxy.h"
#include "PaperGroupedSpriteComponent.h"

//////////////////////////////////////////////////////////////////////////
// FGroupedSpriteSceneProxy

FSpriteRenderSection& FGroupedSpriteSceneProxy::FindOrAddSection(FSpriteDrawCallRecord& InBatch, UMaterialInterface* InMaterial)
{
	// Check the existing sections, starting with the most recent
	for (int32 SectionIndex = BatchedSections.Num() - 1; SectionIndex >= 0; --SectionIndex)
	{
		FSpriteRenderSection& TestSection = BatchedSections[SectionIndex];

		if (TestSection.Material == InMaterial)
		{
			if (TestSection.BaseTexture == InBatch.BaseTexture)
			{
				if (TestSection.AdditionalTextures == InBatch.AdditionalTextures)
				{
					return TestSection;
				}
			}
		}
	}

	// Didn't find a matching section, create one
	FSpriteRenderSection& NewSection = *(new (BatchedSections) FSpriteRenderSection());
	NewSection.Material = InMaterial;
	NewSection.BaseTexture = InBatch.BaseTexture;
	NewSection.AdditionalTextures = InBatch.AdditionalTextures;
	NewSection.VertexOffset = VertexBuffer.Vertices.Num();

	return NewSection;
}

FGroupedSpriteSceneProxy::FGroupedSpriteSceneProxy(UPaperGroupedSpriteComponent* InComponent)
	: FPaperRenderSceneProxy(InComponent)
	, MyComponent(InComponent)
{
	MaterialRelevance = InComponent->GetMaterialRelevance(GetScene().GetFeatureLevel());

	NumInstances = InComponent->PerInstanceSpriteData.Num();

	BodySetupTransforms.Reserve(NumInstances);
	BodySetups.Reserve(NumInstances);

	for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; ++InstanceIndex)
	{
		const FSpriteInstanceData InstanceData = InComponent->PerInstanceSpriteData[InstanceIndex];

		TWeakObjectPtr<class UBodySetup> BodySetup;
		if (InstanceData.SourceSprite != nullptr)
		{
			FSpriteDrawCallRecord Record;
			Record.BuildFromSprite(InstanceData.SourceSprite);

			UMaterialInterface* SpriteMaterial = InComponent->GetMaterial(InstanceData.MaterialIndex);

			FSpriteRenderSection& Section = FindOrAddSection(Record, SpriteMaterial);
			
			
			const int32 NumNewVerts = Record.RenderVerts.Num();
			Section.NumVertices += NumNewVerts;

			const FColor VertColor(InstanceData.VertexColor);
			for (const FVector4& SourceVert : Record.RenderVerts)
			{
				const FVector LocalPos((PaperAxisX * SourceVert.X) + (PaperAxisY * SourceVert.Y));
				const FVector ComponentSpacePos = InstanceData.Transform.TransformPosition(LocalPos);
				const FVector2D UV(SourceVert.Z, SourceVert.W);

				new (VertexBuffer.Vertices) FPaperSpriteVertex(ComponentSpacePos, UV, VertColor);
			}

			if (InComponent->InstanceBodies.IsValidIndex(InstanceIndex))
			{
				if (FBodyInstance* BodyInstance = InComponent->InstanceBodies[InstanceIndex])
				{
					BodySetup = BodyInstance->BodySetup;
				}
			}
		}

		BodySetupTransforms.Add(InstanceData.Transform);
		BodySetups.Add(BodySetup);
	}
	
	if (VertexBuffer.Vertices.Num() > 0)
	{
		// Init the vertex factory
		MyVertexFactory.Init(&VertexBuffer);

		// Enqueue initialization of render resources
		BeginInitResource(&VertexBuffer);
		BeginInitResource(&MyVertexFactory);
	}
}

void FGroupedSpriteSceneProxy::DebugDrawCollision(const FSceneView* View, int32 ViewIndex, FMeshElementCollector& Collector, bool bDrawSolid) const
{
	for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; ++InstanceIndex)
	{
		if (UBodySetup* BodySetup = BodySetups[InstanceIndex].Get())
		{
			const FColor CollisionColor = FColor(157, 149, 223, 255);
			const FMatrix GeomTransform = BodySetupTransforms[InstanceIndex] * GetLocalToWorld();
			DebugDrawBodySetup(View, ViewIndex, Collector, BodySetup, GeomTransform, CollisionColor, bDrawSolid);
		}
	}
}

void FGroupedSpriteSceneProxy::SetOneBodySetup_RenderThread(int32 InstanceIndex, UBodySetup* NewSetup)
{
	BodySetups[InstanceIndex] = NewSetup;
}

void FGroupedSpriteSceneProxy::SetAllBodySetups_RenderThread(TArray<TWeakObjectPtr<UBodySetup>> Setups)
{
	check(Setups.Num() == NumInstances);
	BodySetups = Setups;
}

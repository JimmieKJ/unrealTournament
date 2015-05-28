// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"

// FPreviewScene derived helpers for rendering
#include "ThumbnailHelpers.h"
#include "EngineModule.h"
#include "RendererInterface.h"
#include "Engine/DestructibleMesh.h"

UDestructibleMeshThumbnailRenderer::UDestructibleMeshThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ThumbnailScene = nullptr;
}

void UDestructibleMeshThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas)
{
	UDestructibleMesh* DestructibleMesh = Cast<UDestructibleMesh>(Object);
	if (DestructibleMesh != nullptr)
	{
		if ( ThumbnailScene == nullptr )
		{
			ThumbnailScene = new FDestructibleMeshThumbnailScene();
		}

		ThumbnailScene->SetDestructibleMesh(DestructibleMesh);
		FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues( RenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game) )
			.SetWorldTimes(FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime));

		ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
		ViewFamily.EngineShowFlags.MotionBlur = 0;
		ViewFamily.EngineShowFlags.LOD = 0;

		ThumbnailScene->GetView(&ViewFamily, X, Y, Width, Height);
		GetRendererModule().BeginRenderingViewFamily(Canvas,&ViewFamily);
		ThumbnailScene->SetDestructibleMesh(nullptr);
	}
}

void UDestructibleMeshThumbnailRenderer::BeginDestroy()
{
	if ( ThumbnailScene != nullptr )
	{
		delete ThumbnailScene;
		ThumbnailScene = nullptr;
	}

	Super::BeginDestroy();
}

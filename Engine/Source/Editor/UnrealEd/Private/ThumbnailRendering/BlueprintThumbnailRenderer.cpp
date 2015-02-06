// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"

// FPreviewScene derived helpers for rendering
#include "ThumbnailHelpers.h"
#include "EngineModule.h"
#include "RendererInterface.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

UBlueprintThumbnailRenderer::UBlueprintThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ThumbnailScene = nullptr;
}

bool UBlueprintThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	if (ThumbnailScene == nullptr)
	{
		ThumbnailScene = new FBlueprintThumbnailScene();
	}

	UBlueprint* Blueprint = Cast<UBlueprint>(Object);

	// Only visualize actor based blueprints
	if (Blueprint && Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()))
	{
		// Try to find any visible primitive components in the native class' CDO
		AActor* CDO = Blueprint->GeneratedClass->GetDefaultObject<AActor>();

		TInlineComponentArray<UActorComponent*> Components;
		CDO->GetComponents(Components);

		for (auto CompIt = Components.CreateConstIterator(); CompIt; ++CompIt)
		{
			if (ThumbnailScene->IsValidComponentForVisualization(*CompIt))
			{
				return true;
			}
		}

		// Try to find any visible primitive components in the simple construction script
		// Do this for all parent blueprint generated classes as well
		UBlueprint* BlueprintToHarvestComponents = Blueprint;
		TSet<UBlueprint*> AllVisitedBlueprints;
		while (BlueprintToHarvestComponents)
		{
			AllVisitedBlueprints.Add(BlueprintToHarvestComponents);

			if (BlueprintToHarvestComponents->SimpleConstructionScript)
			{
				TArray<USCS_Node*> AllNodes = BlueprintToHarvestComponents->SimpleConstructionScript->GetAllNodes();

				for (auto NodeIt = AllNodes.CreateConstIterator(); NodeIt; ++NodeIt)
				{
					if (ThumbnailScene->IsValidComponentForVisualization((*NodeIt)->ComponentTemplate))
					{
						return true;
					}
				}
			}

			UClass* ParentClass = BlueprintToHarvestComponents->ParentClass;
			BlueprintToHarvestComponents = nullptr;

			// If the parent class was a blueprint generated class, check it's simple construction script components as well
			if (ParentClass)
			{
				UBlueprint* ParentBlueprint = Cast<UBlueprint>(ParentClass->ClassGeneratedBy);

				// Also make sure we haven't visited the blueprint already. This would only happen if there was a loop of parent classes.
				if (ParentBlueprint && !AllVisitedBlueprints.Contains(ParentBlueprint))
				{
					BlueprintToHarvestComponents = ParentBlueprint;
				}
			}
		}
	}

	return false;
}

void UBlueprintThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas)
{
	UBlueprint* Blueprint = Cast<UBlueprint>(Object);
	if (Blueprint != nullptr)
	{
		if ( ThumbnailScene == nullptr )
		{
			ThumbnailScene = new FBlueprintThumbnailScene();
		}

		ThumbnailScene->SetBlueprint(Blueprint);
		FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues( RenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game) )
			.SetWorldTimes(FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime));

		ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
		ViewFamily.EngineShowFlags.MotionBlur = 0;

		ThumbnailScene->GetView(&ViewFamily, X, Y, Width, Height);
		GetRendererModule().BeginRenderingViewFamily(Canvas,&ViewFamily);
		ThumbnailScene->SetBlueprint(nullptr);
	}
}

void UBlueprintThumbnailRenderer::BeginDestroy()
{
	if ( ThumbnailScene != nullptr )
	{
		delete ThumbnailScene;
		ThumbnailScene = nullptr;
	}

	Super::BeginDestroy();
}

void UBlueprintThumbnailRenderer::BlueprintChanged(class UBlueprint* Blueprint)
{
	if ( ThumbnailScene != nullptr )
	{
		ThumbnailScene->BlueprintChanged(Blueprint);
	}
}
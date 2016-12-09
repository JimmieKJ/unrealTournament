// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Engine/World.h"
#include "SceneInterface.h"
#include "Components/StaticMeshComponent.h"
#include "Model.h"
#include "ComponentRecreateRenderStateContext.h"
#include "Engine/PreCullTrianglesVolume.h"

APreCullTrianglesVolume::APreCullTrianglesVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void PreCullTrianglesExec(UWorld* InWorld)
{
	TArray<UStaticMeshComponent*> ComponentsToPreCull;

	for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
	{
		UStaticMeshComponent* Component = *It;

		if (!Component->IsPendingKill() 
			&& Component->Mobility == EComponentMobility::Static
			&& Component->GetOwner() 
			&& InWorld->ContainsActor(Component->GetOwner()) 
			&& Component->GetStaticMesh()
			&& Component->IsRegistered()
			&& Component->SceneProxy)
		{
			ComponentsToPreCull.Add(Component);
		}
	}

	TArray<TArray<FPlane> > CullVolumes;

	for (TObjectIterator<APreCullTrianglesVolume> It; It; ++It)
	{
		APreCullTrianglesVolume* CullVolume = *It;

		if (InWorld->ContainsActor(CullVolume) && !CullVolume->IsPendingKill())
		{
			TArray<FPlane> Planes;
			CullVolume->Brush->GetSurfacePlanes(CullVolume, Planes);

			CullVolumes.Add(Planes);
		}
	}

	InWorld->Scene->PreCullStaticMeshes(ComponentsToPreCull, CullVolumes);
	FGlobalComponentRecreateRenderStateContext Context;
}

FAutoConsoleCommandWithWorld GPreCullIndexBuffersCmd(
	TEXT("r.PreCullIndexBuffers"),
	TEXT("Runs a slow operation to remove any static mesh triangles that are invisible or inside a precull volume."),
	FConsoleCommandWithWorldDelegate::CreateStatic(PreCullTrianglesExec)
	);

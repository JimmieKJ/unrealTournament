// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
TextureStreamingBuild.cpp : Contains definitions to build texture streaming data.
=============================================================================*/

#include "EnginePrivate.h"
#include "DebugViewModeMaterialProxy.h"
#include "ShaderCompiler.h"

ENGINE_API void BuildTextureStreamingData(UWorld* InWorld)
{
	if (!InWorld) return;

#if WITH_EDITORONLY_DATA
	FlushRenderingCommands();
	
	double StartTime = FPlatformTime::Seconds();

	// ====================================================
	// Build Shaders to compute tex coord scale per texture
	// ====================================================

	FDebugViewModeMaterialProxy::ClearAllShaders();

	for (int32 LevelIndex = 0; LevelIndex < InWorld->GetNumLevels(); LevelIndex++)
	{
		ULevel* Level = InWorld->GetLevel(LevelIndex);

		TArray<UObject*> ObjectsInOuter;
		GetObjectsWithOuter(Level, ObjectsInOuter);

		for (int32 Index = 0; Index < ObjectsInOuter.Num(); Index++)
		{
			UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(ObjectsInOuter[Index]);
			if (!Primitive) continue;

			TArray<UMaterialInterface*> Materials;
			Primitive->GetUsedMaterials(Materials);

			for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
			{
				UMaterialInterface* MaterialInterface = Materials[MaterialIndex];
				if (!MaterialInterface) continue;

				FDebugViewModeMaterialProxy::AddShader(MaterialInterface, EMaterialShaderMapUsage::DebugViewModeTexCoordScale);
			}
		}
	}
	GShaderCompilingManager->FinishAllCompilation();

	// ====================================================
	// Build per primitive data
	// ====================================================

	for (int32 LevelIndex = 0; LevelIndex < InWorld->GetNumLevels(); LevelIndex++)
	{
		ULevel* Level = InWorld->GetLevel(LevelIndex);

		TArray<UObject*> ObjectsInOuter;
		GetObjectsWithOuter(Level, ObjectsInOuter);

		for (int32 Index = 0; Index < ObjectsInOuter.Num(); Index++)
		{
			UStaticMeshComponent* Primitive = Cast<UStaticMeshComponent>(ObjectsInOuter[Index]);
			if (!Primitive) continue;

			Primitive->UpdateStreamingTextureInfos();
		}
	}

	// ====================================================
	// Update texture streaming data
	// ====================================================

	ULevel::BuildStreamingData(InWorld);

	// ====================================================
	// Reregister everything for debug view modes to reflect changes
	// ====================================================

	for (int32 LevelIndex = 0; LevelIndex < InWorld->GetNumLevels(); LevelIndex++)
	{
		ULevel* Level = InWorld->GetLevel(LevelIndex);
		if (!Level) continue;
		
		for (AActor* Actor : Level->Actors)
		{
			if (!Actor) continue;

			Actor->ReregisterAllComponents();
		}
	}
	UE_LOG(LogLevel, Display, TEXT("Build TextureStreaming took %.3f seconds."), FPlatformTime::Seconds() - StartTime);
#else
	UE_LOG(LogLevel, Fatal,TEXT("Build TextureStreaming should not be called on a console"));
#endif
}

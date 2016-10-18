// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
TextureStreamingBuild.cpp : Contains definitions to build texture streaming data.
=============================================================================*/

#include "EnginePrivate.h"
#include "DebugViewModeMaterialProxy.h"
#include "ShaderCompiler.h"
#include "TextureStreamingHelpers.h"

DEFINE_LOG_CATEGORY(TextureStreamingBuild);

bool WaitForShaderCompilation(const FText& Message, FSlowTask& BuildTextureStreamingTask)
{
	const int32 NumShadersToBeCompiled = GShaderCompilingManager->GetNumRemainingJobs();
	int32 RemainingShaders = NumShadersToBeCompiled;
	if (NumShadersToBeCompiled > 0)
	{
		FScopedSlowTask SlowTask((float)NumShadersToBeCompiled, Message);

		while (RemainingShaders > 0)
		{
			FPlatformProcess::Sleep(0.01f);
			GShaderCompilingManager->ProcessAsyncResults(false, true);

			const int32 RemainingShadersThisFrame = GShaderCompilingManager->GetNumRemainingJobs();
			const int32 NumberOfShadersCompiledThisFrame = RemainingShaders - RemainingShadersThisFrame;

			SlowTask.EnterProgressFrame((float)NumberOfShadersCompiledThisFrame);
			BuildTextureStreamingTask.EnterProgressFrame((float)NumberOfShadersCompiledThisFrame / (float)NumShadersToBeCompiled);
			if (GWarn->ReceivedUserCancel()) return false;

			RemainingShaders = RemainingShadersThisFrame;
		}
	}
	else
	{
		BuildTextureStreamingTask.EnterProgressFrame();
		if (GWarn->ReceivedUserCancel()) return false;
	}

	// Extra safety to make sure every shader map is updated
	GShaderCompilingManager->FinishAllCompilation();
	return true;
}

void GetTextureStreamingPrimitives(ULevel* InLevel, TArray<UPrimitiveComponent*>& OutComponents, bool bOnlyThoseRequiringStreamingData = true)
{
	if (InLevel)
	{
		for (AActor* Actor : InLevel->Actors)
		{
			if (!Actor) continue;

			TInlineComponentArray<UPrimitiveComponent*> Primitives;
			Actor->GetComponents<UPrimitiveComponent>(Primitives);

			for (UPrimitiveComponent* Primitive : Primitives)
			{
				if (!bOnlyThoseRequiringStreamingData || Primitive->RequiresStreamingTextureData())
				{
					OutComponents.Push(Primitive);
			}
		}
	}
}
}

void GetTextureStreamingPrimitives(UWorld* InWorld, TArray<UPrimitiveComponent*>& OutComponents)
{
	for (int32 LevelIndex = 0; LevelIndex < InWorld->GetNumLevels(); LevelIndex++)
	{
		GetTextureStreamingPrimitives(InWorld->GetLevel(LevelIndex), OutComponents);
	}
}

int32 GetNumTextureStreamingPrimitives(UWorld* InWorld)
{
	int32 Count = 0;
	for (int32 LevelIndex = 0; LevelIndex < InWorld->GetNumLevels(); LevelIndex++)
	{
		ULevel* Level = InWorld->GetLevel(LevelIndex);
		if (!Level) continue;
		for (AActor* Actor : Level->Actors)
		{
			if (!Actor) continue;
			TInlineComponentArray<UPrimitiveComponent*> Primitives;
			Actor->GetComponents<UPrimitiveComponent>(Primitives);

			for (UPrimitiveComponent* Primitive : Primitives)
			{
				if (Primitive->RequiresStreamingTextureData())
				{
					++Count;
				}
			}
		}
	}
	return Count;
}


#define LOCTEXT_NAMESPACE "TextureStreamingBuild"

/**
 * Build Shaders to compute tex coord scale per texture.
 *
 * @param InWorld			The world to build streaming for.
 * @param QualityLevel		The quality level for the shaders.
 * @param FeatureLevel		The feature level for the shaders.
 * @param TexCoordScales	The map to store material interfaces used by the current primitive world.
 * @param MaterialToLevels	The map to bind a material to all levels where primitives are refering it.
 * @param bIncremental		true if the build should only update components that have no data.
 * @return true if the operation is a success, false if it was canceled.
 */
ENGINE_API bool BuildTextureStreamingShaders(UWorld* InWorld, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, OUT FTexCoordScaleMap& TexCoordScales, OUT FMaterialToLevelsMap& MaterialToLevels, bool bIncremental, FSlowTask& BuildTextureStreamingTask)
{
#if WITH_EDITORONLY_DATA

	if (!InWorld || !GShaderCompilingManager) return false;

	const bool bUseNewMetrics = CVarStreamingUseNewMetrics.GetValueOnGameThread() != 0;
	const double StartTime = FPlatformTime::Seconds();

	FlushRenderingCommands();

	if (!WaitForShaderCompilation(LOCTEXT("TextureStreamingBuild_FinishPendingShadersCompilation", "Waiting For Pending Shaders Compilation"), BuildTextureStreamingTask))
	{
		return false;
	}

	if (!bIncremental)
	{
		FDebugViewModeMaterialProxy::ClearAllShaders();
	}
	else // Otherwise, if there are no shaders, then we need to rebuild them all.
	{
		bIncremental = FDebugViewModeMaterialProxy::HasAnyShaders();
	}

	// Without the new metrics, the shaders will are not compiled, making the texcoord scale viewmode non functional.
	if (bUseNewMetrics && AllowDebugViewPS(DVSM_MaterialTexCoordScalesAnalysis, GetFeatureLevelShaderPlatform(FeatureLevel)))
	{
		const float NumPrimitiveComponents = (float)GetNumTextureStreamingPrimitives(InWorld);

		{
			FScopedSlowTask SlowTask(NumPrimitiveComponents, (LOCTEXT("TextureStreamingBuild_ParsingPrimitiveMaterials ", "Parsing Primitive Materials")));
			for (int32 LevelIndex = 0; LevelIndex < InWorld->GetNumLevels(); LevelIndex++)
			{
				ULevel* Level = InWorld->GetLevel(LevelIndex);

			TArray<UPrimitiveComponent*> Components;
				GetTextureStreamingPrimitives(Level, Components);
			for (UPrimitiveComponent* Primitive : Components)
			{
				SlowTask.EnterProgressFrame();
					BuildTextureStreamingTask.EnterProgressFrame(1.f / NumPrimitiveComponents);
				if (GWarn->ReceivedUserCancel())
				{
					FDebugViewModeMaterialProxy::ClearAllShaders();
					return false;
				}

				// When only updating (viewmode path), skip primitives that already have data.
					if (bIncremental && Primitive->HasStreamingSectionData(true))
					continue;

				TArray<UMaterialInterface*> Materials;
				Primitive->GetUsedMaterials(Materials);

				for (UMaterialInterface* MaterialInterface : Materials)
				{
					if (!MaterialInterface) continue;

					// Landscape material resources can not be used. See logic in FLandscapeMaterialResource::ShouldCache().
					const FMaterial* Material = MaterialInterface->GetMaterialResource(FeatureLevel);
					if (!Material || Material->IsUsedWithLandscape())
					{
						UE_LOG(TextureStreamingBuild, Verbose, TEXT("Landscape material %s not supported, skipping shader"), *MaterialInterface->GetName());
						continue;
					}

					if (!TexCoordScales.Contains(MaterialInterface))
					{
						TexCoordScales.Add(MaterialInterface);
							FDebugViewModeMaterialProxy::AddShader(MaterialInterface, QualityLevel, FeatureLevel, EMaterialShaderMapUsage::DebugViewModeTexCoordScale);
						}
						MaterialToLevels.FindOrAdd(MaterialInterface).AddUnique(Level);
					}
				}
			}
		}

		if (WaitForShaderCompilation(LOCTEXT("TextureStreamingBuild_CompTexCoordScaleShaders", "Compiling Shaders For TexCoord Scale Analysis "), BuildTextureStreamingTask))
		{
			// Check The validity of all shaders, removing invalid entries
			FDebugViewModeMaterialProxy::ValidateAllShaders(TexCoordScales);
			return true;
		}
		else
		{
			FDebugViewModeMaterialProxy::ClearAllShaders();
			return false;
		}
	}
	else
	{
		return true; // Nothing to build but this is not an error.
	}

#else
	UE_LOG(TextureStreamingBuild, Fatal,TEXT("Build Texture Streaming Shaders should not be called on a console"));
	return false;
#endif
}

ENGINE_API bool UpdateComponentStreamingSectionData(UWorld* InWorld, const FTexCoordScaleMap& InTexCoordScales, bool bIncremental, FSlowTask& BuildTextureStreamingTask)
{
#if WITH_EDITORONLY_DATA
	if (!InWorld) return false;
	const double StartTime = FPlatformTime::Seconds();

	// ====================================================
	// Build per primitive data
	// ====================================================
	const float NumPrimitiveComponents = (float)GetNumTextureStreamingPrimitives(InWorld);
	FScopedSlowTask SlowTask(NumPrimitiveComponents, (LOCTEXT("TextureStreamingBuild_ComponentDataUpdate", "Updating Component Data")));

	for (int32 LevelIndex = 0; LevelIndex < InWorld->GetNumLevels(); LevelIndex++)
	{
		TArray<UPrimitiveComponent*> Components;
		GetTextureStreamingPrimitives(InWorld->GetLevel(LevelIndex), Components);
		for (UPrimitiveComponent* Primitive : Components)
			{
				SlowTask.EnterProgressFrame();
				BuildTextureStreamingTask.EnterProgressFrame(1.f / NumPrimitiveComponents);
				if (GWarn->ReceivedUserCancel()) return false;

			if (bIncremental && Primitive->HasStreamingSectionData(InTexCoordScales.Num() > 0))
					continue;

				Primitive->UpdateStreamingSectionData(InTexCoordScales);
		}
	}
	UE_LOG(TextureStreamingBuild, Display, TEXT("Update Texture Streaming Data took %.3f seconds."), FPlatformTime::Seconds() - StartTime);
	return true;
#else
	return false;
#endif
}

ENGINE_API bool BuildTextureStreamingData(UWorld* InWorld, const FTexCoordScaleMap& InTexCoordScales, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, FSlowTask& BuildTextureStreamingTask)
{
#if WITH_EDITORONLY_DATA
	if (!InWorld) return false;
	const double StartTime = FPlatformTime::Seconds();

	// ====================================================
	// Build per primitive data
	// ====================================================
	const float NumPrimitiveComponents = (float)GetNumTextureStreamingPrimitives(InWorld);
	FScopedSlowTask SlowTask(NumPrimitiveComponents, (LOCTEXT("TextureStreamingBuild_ComponentDataUpdate", "Updating Component Data")));

	for (int32 LevelIndex = 0; LevelIndex < InWorld->GetNumLevels(); LevelIndex++)
	{
		ULevel* Level = InWorld->GetLevel(LevelIndex);
		if (!Level) continue;

		TArray<UTexture2D*> LevelTextures;

		TArray<UPrimitiveComponent*> Components;
		GetTextureStreamingPrimitives(Level, Components, false);
		for (UPrimitiveComponent* Primitive : Components)
		{
			// Update all primitives as this could clear unwanted data.
			Primitive->UpdateStreamingTextureData(LevelTextures, InTexCoordScales, QualityLevel, FeatureLevel);

			if (Primitive->RequiresStreamingTextureData())
			{
				// Slow task only accounts for primitive having streaming data.
				SlowTask.EnterProgressFrame();
				BuildTextureStreamingTask.EnterProgressFrame(1.f / NumPrimitiveComponents);

				if (GWarn->ReceivedUserCancel()) 
					return false;

				UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Primitive);
				if (StaticMeshComponent && StaticMeshComponent->StaticMesh && StaticMeshComponent->StaticMesh->GetLightingGuid().IsValid())
				{
					// Add to the build Guids all used static meshes.
					Level->TextureStreamingBuildGuids.Add(StaticMeshComponent->StaticMesh->GetLightingGuid());
				}
			}
		}

		// If a component was updated, or  the level data needs reset.
		if (Level->bTextureStreamingRotationChanged || Level->StreamingTextureGuids.Num() || LevelTextures.Num())
		{
			Level->bTextureStreamingRotationChanged = false;
			Level->StreamingTextureGuids.Empty(LevelTextures.Num());
			Level->MarkPackageDirty();

			// Reset LevelIndex to default for next use and build the level Guid array.
			for (int32 TextureIndex = 0; TextureIndex < LevelTextures.Num(); ++TextureIndex)
			{
				UTexture2D* Texture2D = LevelTextures[TextureIndex];
				Level->StreamingTextureGuids.Push(Texture2D->GetLightingGuid());
				Texture2D->LevelIndex = INDEX_NONE;
			}
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
			Actor->MarkComponentsRenderStateDirty();
		}
	}
	UE_LOG(TextureStreamingBuild, Display, TEXT("Build Texture Streaming took %.3f seconds."), FPlatformTime::Seconds() - StartTime);
	return true;
#else
	UE_LOG(TextureStreamingBuild, Fatal,TEXT("Build Texture Streaming should not be called on a console"));
	return false;
#endif
}

#undef LOCTEXT_NAMESPACE

static uint32 PackRelativeBox(const FVector& RefOrigin, const FVector& RefExtent, const FVector& Origin, const FVector& Extent)
{
	const FVector RefMin = RefOrigin - RefExtent;
	// 15.5 and 31.5 have the / 2 scale included 
	const FVector PackScale = FVector(15.5f, 15.5f, 31.5f) / RefExtent.ComponentMax(FVector(KINDA_SMALL_NUMBER, KINDA_SMALL_NUMBER, KINDA_SMALL_NUMBER));

	const FVector Min = Origin - Extent;
	const FVector Max = Origin + Extent;

	const FVector RelMin = (Min - RefMin) * PackScale;
	const FVector RelMax = (Max - RefMin) * PackScale;

	const uint32 PackedMinX = (uint32)FMath::Clamp<int32>(FMath::FloorToInt(RelMin.X), 0, 31);
	const uint32 PackedMinY = (uint32)FMath::Clamp<int32>(FMath::FloorToInt(RelMin.Y), 0, 31);
	const uint32 PackedMinZ = (uint32)FMath::Clamp<int32>(FMath::FloorToInt(RelMin.Z), 0, 63);

	const uint32 PackedMaxX = (uint32)FMath::Clamp<int32>(FMath::CeilToInt(RelMax.X), 0, 31);
	const uint32 PackedMaxY = (uint32)FMath::Clamp<int32>(FMath::CeilToInt(RelMax.Y), 0, 31);
	const uint32 PackedMaxZ = (uint32)FMath::Clamp<int32>(FMath::CeilToInt(RelMax.Z), 0, 63);

	return PackedMinX | (PackedMinY << 5) | (PackedMinZ << 10) | (PackedMaxX << 16) | (PackedMaxY << 21) | (PackedMaxZ << 26);
}

static void UnpackRelativeBox(const FVector& InRefOrigin, const FVector& InRefExtent, uint32 InPackedRelBox, FVector& OutOrigin, FVector& OutExtent)
{
	const uint32 PackedMinX = InPackedRelBox & 31;
	const uint32 PackedMinY = (InPackedRelBox >> 5) & 31;
	const uint32 PackedMinZ = (InPackedRelBox >> 10) & 63;

	const uint32 PackedMaxX = (InPackedRelBox >> 16) & 31;
	const uint32 PackedMaxY = (InPackedRelBox >> 21) & 31;
	const uint32 PackedMaxZ = (InPackedRelBox >> 26) & 63;

	const FVector RefMin = InRefOrigin - InRefExtent;
	// 15.5 and 31.5 have the / 2 scale included 
	const FVector UnpackScale = InRefExtent.ComponentMax(FVector(KINDA_SMALL_NUMBER, KINDA_SMALL_NUMBER, KINDA_SMALL_NUMBER)) / FVector(15.5f, 15.5f, 31.5f);

	const FVector Min = FVector((float)PackedMinX, (float)PackedMinY, (float)PackedMinZ) * UnpackScale + RefMin;
	const FVector Max = FVector((float)PackedMaxX, (float)PackedMaxY, (float)PackedMaxZ) * UnpackScale + RefMin;

	OutOrigin = .5 * (Min + Max);
	OutExtent = .5 * (Max - Min);
}

void FStreamingTexturePrimitiveInfo::UnPackFrom(UTexture2D* InTexture, float ExtraScale, const FBoxSphereBounds& RefBounds, const FStreamingTextureBuildInfo& Info, bool bUseRelativeBox)
{
	check(InTexture->LevelIndex == Info.TextureLevelIndex);
	Texture = InTexture;

	if (bUseRelativeBox)
	{
		UnpackRelativeBox(RefBounds.Origin, RefBounds.BoxExtent, Info.PackedRelativeBox, Bounds.Origin, Bounds.BoxExtent);
		Bounds.SphereRadius = Bounds.BoxExtent.Size();
	}
	else
	{
		Bounds = RefBounds;
	}

	TexelFactor = Info.TexelFactor * ExtraScale;
}

void FStreamingTextureBuildInfo::PackFrom(TArray<UTexture2D*>& LevelTextures, const FBoxSphereBounds& RefBounds, const FStreamingTexturePrimitiveInfo& Info)
{
	PackedRelativeBox = PackRelativeBox(RefBounds.Origin, RefBounds.BoxExtent, Info.Bounds.Origin, Info.Bounds.BoxExtent);

	UTexture2D* Texture2D = CastChecked<UTexture2D>(Info.Texture);
	if (Texture2D->LevelIndex == INDEX_NONE)
	{
		// If this is the first time this texture gets processed in the packing process, encode it.
		Texture2D->LevelIndex = LevelTextures.Add(Texture2D);
	}
	TextureLevelIndex = (uint16)Texture2D->LevelIndex;

	TexelFactor = Info.TexelFactor;
}


FStreamingTextureLevelContext::FStreamingTextureLevelContext(const ULevel* InLevel) :
	bUseRelativeBoxes(InLevel ? !InLevel->bTextureStreamingRotationChanged : false),
	ComponentTimestamp(0),
	ComponentBounds(ForceInitToZero),
	ComponentPrecomputedDataScale(1.f),
	ComponentFallbackScale(1.f)
{
	if (InLevel)
	{
		for (int32 TextureIndex = 0; TextureIndex < InLevel->StreamingTextureGuids.Num(); ++TextureIndex)
		{
			TextureGuidToLevelIndex.Add(InLevel->StreamingTextureGuids[TextureIndex], TextureIndex);
		}
		BoundStates.AddZeroed(InLevel->StreamingTextureGuids.Num());
	}
}

FStreamingTextureLevelContext::~FStreamingTextureLevelContext()
{
	// Reset the level indices for the next use.
	for (UTexture2D* Texture : ProcessedTextures)
	{
		Texture->LevelIndex = INDEX_NONE;
	}
}

void FStreamingTextureLevelContext::BindComponent(const TArray<FStreamingTextureBuildInfo>* BuildData, const FBoxSphereBounds& Bounds, float PrecomputedDataScale, float FallbackScale)
{
	// First processed component must use a timestamp > 0  so that default value of 0 is invalid.
	++ComponentTimestamp;

	ComponentBounds = Bounds;
	ComponentPrecomputedDataScale = PrecomputedDataScale;
	ComponentFallbackScale = FallbackScale;
	ComponentBuildData = BuildData;

	if (BuildData)
	{
		for (int32 Index = 0; Index < BuildData->Num(); ++Index)
		{
			int32 TextureLevelIndex = (*BuildData)[Index].TextureLevelIndex;
			if (BoundStates.IsValidIndex(TextureLevelIndex))
			{
				FTextureBoundState& BoundState = BoundStates[TextureLevelIndex];
				BoundState.BuildDataIndex = Index;
				BoundState.BuildDataTimestamp = ComponentTimestamp;
			}
		}
	}
}

void FStreamingTextureLevelContext::Process(const TArray<UTexture*>& InTextures, TArray<FStreamingTexturePrimitiveInfo>& OutInfos)
{
	for (int32 TextureIndex = 0; TextureIndex < InTextures.Num(); ++TextureIndex)
	{
		UTexture2D* Texture2D = Cast<UTexture2D>(InTextures[TextureIndex]);
		if (!Texture2D) continue; // Only interested in 2d texture for streaming

		if (Texture2D->LevelIndex == INDEX_NONE)
		{
			int32* LevelIndex = TextureGuidToLevelIndex.Find(Texture2D->GetLightingGuid());
			if (LevelIndex)
			{
				Texture2D->LevelIndex = *LevelIndex;
			}
			else // If it is not in build data, add an entry so that we can process it in a uniform way.
			{
				Texture2D->LevelIndex = BoundStates.AddZeroed();
			}
			ProcessedTextures.Push(Texture2D);
		}

		FTextureBoundState& BoundState = BoundStates[Texture2D->LevelIndex];

		// Ignore this texture if it has already been handled for the current component.
		if (BoundState.Timestamp != ComponentTimestamp)
		{
			FStreamingTexturePrimitiveInfo& StreamingTexture = *new(OutInfos) FStreamingTexturePrimitiveInfo;

			// Check if there is any precomputed data
			if (BoundState.BuildDataTimestamp == ComponentTimestamp && ComponentBuildData)
			{
				// The ExtraScale is only applied to the precomputed data as we assume it was already 
				StreamingTexture.UnPackFrom(Texture2D, ComponentPrecomputedDataScale, ComponentBounds, (*ComponentBuildData)[BoundState.BuildDataIndex], bUseRelativeBoxes);
			}
			else // Otherwise add default entry.
			{
				StreamingTexture.Texture = Texture2D;
				StreamingTexture.Bounds = ComponentBounds;
				StreamingTexture.TexelFactor = ComponentFallbackScale;
			}

			// Mark this texture as processed for this component.
			BoundState.Timestamp = ComponentTimestamp;
		}
	}
}

void CheckTextureStreamingBuild(ULevel* InLevel)
{
#if WITH_EDITORONLY_DATA
	if (!InLevel) return;

	int32 NumTextureStreamingUnbuiltComponents = 0;

	TSet<FGuid> BuildGuilds;
	for (const FGuid& Guid : InLevel->TextureStreamingBuildGuids)
	{
		BuildGuilds.Add(Guid);
	}

	TArray<UPrimitiveComponent*> Components;
	GetTextureStreamingPrimitives(InLevel, Components);
	for (UPrimitiveComponent* Primitive : Components)
	{
		//@todo : Non transactional primitives, like the one created from blueprints, fail to save/load their built data.
		const bool bHasMissingData = !!(Primitive->GetFlags() & RF_Transactional) && !Primitive->HasStreamingTextureData();
		TArray<UTexture*> Textures;

		TArray<UMaterialInterface*> Materials;
		Primitive->GetUsedMaterials(Materials);

		for (const UMaterialInterface* MaterialInterface : Materials)
		{
			if (MaterialInterface)
			{
				TArray<FGuid> MaterialGuids;
				MaterialInterface->GetLightingGuidChain(false, MaterialGuids);

				for (const FGuid& MaterialGuid : MaterialGuids)
				{
					BuildGuilds.Remove(MaterialGuid);
				}

				// If there is no data, then we will have to parse the texture to check whether there are streaming textures there.
				if (bHasMissingData)
				{
					MaterialInterface->GetUsedTextures(Textures, EMaterialQualityLevel::Num, false, GMaxRHIFeatureLevel, false);
				} 
			}
		}

		// If there is no data, check that the material actually uses streaming textures before marking the Texture Streaming Build invalid.
		if (bHasMissingData && Textures.Num() > 0)
		{
			for (UTexture* Texture : Textures)
			{
				if (IsStreamingTexture(Cast<UTexture2D>(Texture)))
				{
					++NumTextureStreamingUnbuiltComponents;
					break;
				}
			}
		}

		const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Primitive);
		if (StaticMeshComponent && StaticMeshComponent->StaticMesh)
		{
			BuildGuilds.Remove(StaticMeshComponent->StaticMesh->GetLightingGuid());
		}
	}

	// All guids must have been found, otherwise, it means the resource changed.
	if (NumTextureStreamingUnbuiltComponents != InLevel->NumTextureStreamingUnbuiltComponents || BuildGuilds.Num() != InLevel->NumTextureStreamingDirtyResources)
	{
		InLevel->NumTextureStreamingUnbuiltComponents = NumTextureStreamingUnbuiltComponents;
		InLevel->NumTextureStreamingDirtyResources = BuildGuilds.Num();

		// Don't mark package dirty as we avoid marking package dirty unless user changes something.
		// InLevel->MarkPackageDirty();
	}

#endif
}

void CheckTextureStreamingBuild(UWorld* InWorld)
{
#if WITH_EDITORONLY_DATA
	if (!InWorld) return;

	InWorld->NumTextureStreamingUnbuiltComponents = 0;
	InWorld->NumTextureStreamingDirtyResources = 0;

	if (CVarStreamingCheckBuildStatus.GetValueOnAnyThread() > 0)
	{
		for (int32 LevelIndex = 0; LevelIndex < InWorld->GetNumLevels(); LevelIndex++)
		{
			ULevel* Level = InWorld->GetLevel(LevelIndex);
			if (!Level) continue;

			CheckTextureStreamingBuild(Level);

			InWorld->NumTextureStreamingUnbuiltComponents += Level->NumTextureStreamingUnbuiltComponents;
			InWorld->NumTextureStreamingDirtyResources += Level->NumTextureStreamingDirtyResources;
		}
	}

#endif
}

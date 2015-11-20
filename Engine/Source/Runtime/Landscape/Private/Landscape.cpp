// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Landscape.cpp: Terrain rendering
=============================================================================*/

#include "Landscape.h"
#include "EditorSupportDelegates.h"
#include "LandscapeDataAccess.h"
#include "LandscapeRender.h"
#include "LandscapeRenderMobile.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "MapErrors.h"
#include "DerivedDataCacheInterface.h"
#include "TargetPlatform.h"
#include "LandscapeMeshCollisionComponent.h"
#include "LandscapeMaterialInstanceConstant.h"
#include "LandscapeSplinesComponent.h"
#include "LandscapeInfo.h"
#include "LandscapeInfoMap.h"
#include "LandscapeLayerInfoObject.h"
#include "LightMap.h"
#include "ShadowMap.h"
#include "Engine/CollisionProfile.h"
#include "Materials/Material.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "AI/Navigation/NavigationSystem.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "LandscapeMeshProxyComponent.h"
#include "LandscapeMeshProxyActor.h"
#include "Materials/MaterialExpressionLandscapeLayerWeight.h"
#include "Materials/MaterialExpressionLandscapeLayerSwitch.h"
#include "Materials/MaterialExpressionLandscapeLayerSample.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Materials/MaterialExpressionLandscapeVisibilityMask.h"

#if WITH_EDITOR
#include "MaterialUtilities.h"
#endif

/** Landscape stats */

DEFINE_STAT(STAT_LandscapeDynamicDrawTime);
DEFINE_STAT(STAT_LandscapeStaticDrawLODTime);
DEFINE_STAT(STAT_LandscapeVFDrawTime);
DEFINE_STAT(STAT_LandscapeComponents);
DEFINE_STAT(STAT_LandscapeDrawCalls);
DEFINE_STAT(STAT_LandscapeTriangles);
DEFINE_STAT(STAT_LandscapeVertexMem);
DEFINE_STAT(STAT_LandscapeComponentMem);

// Set this to 0 to disable landscape cooking and thus disable it on device.
#define ENABLE_LANDSCAPE_COOKING 1

#define LOCTEXT_NAMESPACE "Landscape"

static void PrintNumLandscapeShadows()
{
	int32 NumComponents = 0;
	int32 NumShadowCasters = 0;
	for (TObjectIterator<ULandscapeComponent> It; It; ++It)
	{
		ULandscapeComponent* LC = *It;
		NumComponents++;
		if (LC->CastShadow && LC->bCastDynamicShadow)
		{
			NumShadowCasters++;
		}
	}
	UE_LOG(LogConsoleResponse, Display, TEXT("%d/%d landscape components cast shadows"), NumShadowCasters, NumComponents);
}

FAutoConsoleCommand CmdPrintNumLandscapeShadows(
	TEXT("ls.PrintNumLandscapeShadows"),
	TEXT("Prints the number of landscape components that cast shadows."),
	FConsoleCommandDelegate::CreateStatic(PrintNumLandscapeShadows)
	);

ULandscapeComponent::ULandscapeComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
, GrassData(MakeShareable(new FLandscapeComponentGrassData()))
{
	SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	bGenerateOverlapEvents = false;
	CastShadow = true;
	// by default we want to see the Landscape shadows even in the far shadow cascades
	bCastFarShadow = true;
	bUseAsOccluder = true;
	bAllowCullDistanceVolume = false;
	CollisionMipLevel = 0;
	StaticLightingResolution = 0.f; // Default value 0 means no overriding

	HeightmapScaleBias = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
	WeightmapScaleBias = FVector4(0.0f, 0.0f, 0.0f, 1.0f);

	bBoundsChangeTriggersStreamingDataRebuild = true;
	ForcedLOD = -1;
	LODBias = 0;
#if WITH_EDITORONLY_DATA
	LightingLODBias = -1; // -1 Means automatic LOD calculation based on ForcedLOD + LODBias
#endif

	Mobility = EComponentMobility::Static;

#if WITH_EDITORONLY_DATA
	EditToolRenderData = NULL;
#endif

	LpvBiasMultiplier = 0.0f; // Bias is 0 for landscape, since it's single sided
}

ULandscapeComponent::~ULandscapeComponent()
{
}

void ULandscapeComponent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	ULandscapeComponent* This = CastChecked<ULandscapeComponent>(InThis);
	if (This->LightMap != NULL)
	{
		This->LightMap->AddReferencedObjects(Collector);
	}
	if (This->ShadowMap != NULL)
	{
		This->ShadowMap->AddReferencedObjects(Collector);
	}
	Super::AddReferencedObjects(This, Collector);
}

#if WITH_EDITOR

void ULandscapeComponent::BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	if (!TargetPlatform->SupportsFeature(ETargetPlatformFeatures::VertexShaderTextureSampling))
	{
		CheckGenerateLandscapePlatformData(true);
	}
}

void ULandscapeComponent::CheckGenerateLandscapePlatformData(bool bIsCooking)
{
#if ENABLE_LANDSCAPE_COOKING
	// Calculate hash of source data and skip generation if the data we have in memory is unchanged
	FBufferArchive ComponentStateAr;
	SerializeStateHashes(ComponentStateAr);
	uint32 Hash[5];
	FSHA1::HashBuffer(ComponentStateAr.GetData(), ComponentStateAr.Num(), (uint8*)Hash);

	FGuid NewSourceHash = FGuid(Hash[0] ^ Hash[4], Hash[1], Hash[2], Hash[3]);

	bool bGenerateVertexData = true;
	bool bGeneratePixelData = true;

	// Skip generation if the source hash matches
	if (MobileDataSourceHash.IsValid() && MobileDataSourceHash == NewSourceHash)
	{
		if (MobileMaterialInterface != nullptr && MobileWeightNormalmapTexture != nullptr)
		{
			bGeneratePixelData = false;
		}

		if (PlatformData.HasValidPlatformData())
		{
			bGenerateVertexData = false;
		}
		else
		if (PlatformData.LoadFromDDC(NewSourceHash))
		{
			bGenerateVertexData = false;
		}
	}

	if (bGenerateVertexData)
	{
		GeneratePlatformVertexData();
		if (bIsCooking)
		{
			PlatformData.SaveToDDC(NewSourceHash);
		}
	}

	if (bGeneratePixelData)
	{
		GeneratePlatformPixelData();
	}

	MobileDataSourceHash = NewSourceHash;
#endif
}
#endif

void ULandscapeComponent::Serialize(FArchive& Ar)
{
	if (Ar.IsCooking() && !Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::VertexShaderTextureSampling))
	{
		// These properties are not used for ES2 so we back them up and clear them before serializing them.
		UTexture2D* BackupHeightmapTexture = nullptr;
		UTexture2D* BackupXYOffsetmapTexture = nullptr;
		UMaterialInstanceConstant* BackupMaterialInstance = nullptr;
		TArray<UTexture2D*> BackupWeightmapTextures;
			
		Exchange(HeightmapTexture, BackupHeightmapTexture);
		Exchange(BackupXYOffsetmapTexture, XYOffsetmapTexture);
		Exchange(BackupMaterialInstance, MaterialInstance);
		Exchange(BackupWeightmapTextures, WeightmapTextures);

		Super::Serialize(Ar);

		Exchange(HeightmapTexture, BackupHeightmapTexture);
		Exchange(BackupXYOffsetmapTexture, XYOffsetmapTexture);
		Exchange(BackupMaterialInstance, MaterialInstance);
		Exchange(BackupWeightmapTextures, WeightmapTextures);
	}
	else
	{
		Super::Serialize(Ar);
	}

	Ar << LightMap;
	Ar << ShadowMap;

	if (Ar.UE4Ver() >= VER_UE4_SERIALIZE_LANDSCAPE_GRASS_DATA)
	{
		// Share the shared ref so PIE can share this data
		if (Ar.GetPortFlags() & PPF_DuplicateForPIE)
		{
			if (Ar.IsSaving())
			{
				PTRINT GrassDataPointer = (PTRINT)&GrassData;
				Ar << GrassDataPointer;
			}
			else
			{
				PTRINT GrassDataPointer;
				Ar << GrassDataPointer;
				// Duplicate shared reference
				GrassData = *(TSharedRef<FLandscapeComponentGrassData, ESPMode::ThreadSafe>*)GrassDataPointer;
			}
		}
		else
		{
			Ar << GrassData.Get();
		}
	}

#if WITH_EDITOR
	if (Ar.IsTransacting())
	{
		if (EditToolRenderData)
		{
			Ar << EditToolRenderData->SelectedType;
		}
		else
		{
			int32 TempV = 0;
			Ar << TempV;
		}
	}
#endif

	bool bCooked = false;

	if (Ar.UE4Ver() >= VER_UE4_LANDSCAPE_PLATFORMDATA_COOKING && !HasAnyFlags(RF_ClassDefaultObject))
	{
		bCooked = Ar.IsCooking();
		// Save a bool indicating whether this is cooked data
		// This is needed when loading cooked data, to know to serialize differently
		Ar << bCooked;
	}

	if (FPlatformProperties::RequiresCookedData() && !bCooked && Ar.IsLoading())
	{
		UE_LOG(LogLandscape, Fatal, TEXT("This platform requires cooked packages, and this landscape does not contain cooked data %s."), *GetName());
	}

#if ENABLE_LANDSCAPE_COOKING
	if (bCooked)
	{
		// Saving for cooking path
		if (Ar.IsCooking() && !Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::VertexShaderTextureSampling))
		{
			bool bValid = ensure(PlatformData.HasValidPlatformData());
			Ar << bValid;
			if (bValid)
			{
				Ar << PlatformData;
			}
			if (Ar.UE4Ver() >= VER_UE4_SERIALIZE_LANDSCAPE_ES2_TEXTURES)
			{
				Ar << MobileMaterialInterface;
				Ar << MobileWeightNormalmapTexture;
			}
		}
		else if (!FPlatformProperties::SupportsVertexShaderTextureSampling())
		{
			// Loading cooked data path
			bool bValid = false;
			Ar << bValid;

			if (bValid)
			{
				Ar << PlatformData;
			}
			if (Ar.UE4Ver() >= VER_UE4_SERIALIZE_LANDSCAPE_ES2_TEXTURES)
			{
				Ar << MobileMaterialInterface;
				Ar << MobileWeightNormalmapTexture;
			}
		}
		if (Ar.UE4Ver() >= VER_UE4_LANDSCAPE_GRASS_COOKING && Ar.UE4Ver() < VER_UE4_SERIALIZE_LANDSCAPE_GRASS_DATA)
		{
			// deal with previous cooked FGrassMap data
			int32 NumChannels = 0;
			Ar << NumChannels;
			if (NumChannels)
			{
				TArray<uint8> OldData;
				OldData.BulkSerialize(Ar);
			}
		}
	}
#endif
}

SIZE_T ULandscapeComponent::GetResourceSize(EResourceSizeMode::Type Mode)
{
	return Super::GetResourceSize(Mode) + GrassData->GetAllocatedSize();
}

#if WITH_EDITOR
UMaterialInterface* ULandscapeComponent::GetLandscapeMaterial() const
{
	if (OverrideMaterial)
	{
		return OverrideMaterial;
	}
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	if (Proxy)
	{
		return Proxy->GetLandscapeMaterial();
	}
	return UMaterial::GetDefaultMaterial(MD_Surface);
}

UMaterialInterface* ULandscapeComponent::GetLandscapeHoleMaterial() const
{
	if (OverrideHoleMaterial)
	{
		return OverrideHoleMaterial;
	}
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	if (Proxy)
	{
		return Proxy->GetLandscapeHoleMaterial();
	}
	return nullptr;
}

bool ULandscapeComponent::ComponentHasVisibilityPainted() const
{
	for (const FWeightmapLayerAllocationInfo& Allocation : WeightmapLayerAllocations)
	{
		if (Allocation.LayerInfo == ALandscapeProxy::VisibilityLayer)
		{
			return true;
		}
	}

	return false;
}

FString ULandscapeComponent::GetLayerAllocationKey(UMaterialInterface* LandscapeMaterial, bool bMobile /*= false*/) const
{
	if (!LandscapeMaterial)
	{
		return FString();
	}

	FString Result = LandscapeMaterial->GetPathName();

	// Sort the allocations
	TArray<FString> LayerStrings;
	for (int32 LayerIdx = 0; LayerIdx < WeightmapLayerAllocations.Num(); LayerIdx++)
	{
		LayerStrings.Add(FString::Printf(TEXT("_%s_%d"), *WeightmapLayerAllocations[LayerIdx].GetLayerName().ToString(), bMobile ? 0 : WeightmapLayerAllocations[LayerIdx].WeightmapTextureIndex));
	}
	/**
	 * Generate a key for this component's layer allocations to use with MaterialInstanceConstantMap.
	 */
	LayerStrings.Sort(TGreater<FString>());

	for (int32 LayerIdx = 0; LayerIdx < LayerStrings.Num(); LayerIdx++)
	{
		Result += LayerStrings[LayerIdx];
	}
	return Result;
}

void ULandscapeComponent::GetLayerDebugColorKey(int32& R, int32& G, int32& B) const
{
	ULandscapeInfo* Info = GetLandscapeInfo(false);
	if (Info)
	{
		R = INDEX_NONE, G = INDEX_NONE, B = INDEX_NONE;

		for (auto It = Info->Layers.CreateConstIterator(); It; It++)
		{
			const FLandscapeInfoLayerSettings& LayerStruct = *It;
			if (LayerStruct.DebugColorChannel > 0
				&& LayerStruct.LayerInfoObj)
			{
				for (int32 LayerIdx = 0; LayerIdx < WeightmapLayerAllocations.Num(); LayerIdx++)
				{
					if (WeightmapLayerAllocations[LayerIdx].LayerInfo == LayerStruct.LayerInfoObj)
					{
						if (LayerStruct.DebugColorChannel & 1) // R
						{
							R = (WeightmapLayerAllocations[LayerIdx].WeightmapTextureIndex * 4 + WeightmapLayerAllocations[LayerIdx].WeightmapTextureChannel);
						}
						if (LayerStruct.DebugColorChannel & 2) // G
						{
							G = (WeightmapLayerAllocations[LayerIdx].WeightmapTextureIndex * 4 + WeightmapLayerAllocations[LayerIdx].WeightmapTextureChannel);
						}
						if (LayerStruct.DebugColorChannel & 4) // B
						{
							B = (WeightmapLayerAllocations[LayerIdx].WeightmapTextureIndex * 4 + WeightmapLayerAllocations[LayerIdx].WeightmapTextureChannel);
						}
						break;
					}
				}
			}
		}
	}
}
#endif	//WITH_EDITOR

ULandscapeMeshCollisionComponent::ULandscapeMeshCollisionComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// make landscape always create? 
	bAlwaysCreatePhysicsState = true;
}

ULandscapeInfo::ULandscapeInfo(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
#if WITH_EDITOR
	bIsValid = false;
#endif
}

#if WITH_EDITOR
void ULandscapeInfo::UpdateDebugColorMaterial()
{
	FlushRenderingCommands();
	//GWarn->BeginSlowTask( *FString::Printf(TEXT("Compiling layer color combinations for %s"), *GetName()), true);

	for (auto It = XYtoComponentMap.CreateIterator(); It; ++It)
	{
		ULandscapeComponent* Comp = It.Value();
		if (Comp && Comp->EditToolRenderData)
		{
			Comp->EditToolRenderData->UpdateDebugColorMaterial();
		}
	}
	FlushRenderingCommands();
	//GWarn->EndSlowTask();
}

void ULandscapeComponent::PostLoad()
{
	Super::PostLoad();

	ALandscapeProxy* LandscapeProxy = GetLandscapeProxy();
	if (ensure(LandscapeProxy))
	{
		// Ensure that the component's lighting settings matches the actor's.
		bCastStaticShadow = LandscapeProxy->bCastStaticShadow;
		bCastShadowAsTwoSided = LandscapeProxy->bCastShadowAsTwoSided;

		// check SectionBaseX/Y are correct
		int32 CheckSectionBaseX = FMath::RoundToInt(RelativeLocation.X) + LandscapeProxy->LandscapeSectionOffset.X;
		int32 CheckSectionBaseY = FMath::RoundToInt(RelativeLocation.Y) + LandscapeProxy->LandscapeSectionOffset.Y;
		if (CheckSectionBaseX != SectionBaseX ||
			CheckSectionBaseY != SectionBaseY)
		{
			UE_LOG(LogLandscape, Warning, TEXT("LandscapeComponent SectionBaseX disagrees with its location, attempted automated fix: '%s', %d,%d vs %d,%d."),
				*GetFullName(), SectionBaseX, SectionBaseY, CheckSectionBaseX, CheckSectionBaseY);
			SectionBaseX = CheckSectionBaseX;
			SectionBaseY = CheckSectionBaseY;
		}
	}

#if WITH_EDITOR
	if (GIsEditor && !HasAnyFlags(RF_ClassDefaultObject))
	{
		// This is to ensure that component relative location is exact section base offset value
		float CheckRelativeLocationX = float(SectionBaseX - LandscapeProxy->LandscapeSectionOffset.X);
		float CheckRelativeLocationY = float(SectionBaseY - LandscapeProxy->LandscapeSectionOffset.Y);
		if (CheckRelativeLocationX != RelativeLocation.X || 
			CheckRelativeLocationY != RelativeLocation.Y)
		{
			UE_LOG(LogLandscape, Warning, TEXT("LandscapeComponent RelativeLocation disagrees with its section base, attempted automated fix: '%s', %f,%f vs %f,%f."),
				*GetFullName(), RelativeLocation.X, RelativeLocation.Y, CheckRelativeLocationX, CheckRelativeLocationY);
			RelativeLocation.X = CheckRelativeLocationX;
			RelativeLocation.Y = CheckRelativeLocationY;
		}
						
		// Remove standalone flags from data textures to ensure data is unloaded in the editor when reverting an unsaved level.
		// Previous version of landscape set these flags on creation.
		if (HeightmapTexture && HeightmapTexture->HasAnyFlags(RF_Standalone))
		{
			HeightmapTexture->ClearFlags(RF_Standalone);
		}
		for (int32 Idx = 0; Idx < WeightmapTextures.Num(); Idx++)
		{
			if (WeightmapTextures[Idx] && WeightmapTextures[Idx]->HasAnyFlags(RF_Standalone))
			{
				WeightmapTextures[Idx]->ClearFlags(RF_Standalone);
			}
		}
	}
#endif

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		// If we're loading on a platform that doesn't require cooked data, but *only* supports OpenGL ES, preload data from the DDC
		if (!FPlatformProperties::RequiresCookedData() && GMaxRHIFeatureLevel <= ERHIFeatureLevel::ES3_1)
		{
			// Only check the DDC if we don't already have it loaded
			if (!PlatformData.HasValidPlatformData())
			{
				// Attempt to load the ES2 landscape data from the DDC
				if (!PlatformData.LoadFromDDC(StateId))
				{
					// Height Data is available after loading height map, so need to defer it
					UE_LOG(LogLandscape, Warning, TEXT("Attempt to access the DDC when there is none available on component '%s'."), *GetFullName());
				}
			}
		}
	}

#if WITH_EDITOR
	if (GIsEditor && !HasAnyFlags(RF_ClassDefaultObject))
	{
		// Move the MICs and Textures back to the Package if they're currently in the level
		// Moving them into the level caused them to be duplicated when running PIE, which is *very very slow*, so we've reverted that change
		// Also clear the public flag to avoid various issues, e.g. generating and saving thumbnails that can never be seen
		{
			ULevel* Level = GetLevel();
			if (ensure(Level))
			{
				TArray<UObject*> ObjectsToMoveFromLevelToPackage;
				GetGeneratedTexturesAndMaterialInstances(ObjectsToMoveFromLevelToPackage);

				UPackage* MyPackage = GetOutermost();
				for (auto* Obj : ObjectsToMoveFromLevelToPackage)
				{
					Obj->ClearFlags(RF_Public);
					if (Obj->GetOuter() == Level)
					{
						Obj->Rename(NULL, MyPackage, REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
					}
				}
			}
		}
	}
#endif
}

#endif // WITH_EDITOR

ALandscape::ALandscape(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bIsProxy = false;

#if WITH_EDITORONLY_DATA
	bLockLocation = false;
#endif // WITH_EDITORONLY_DATA
}

ALandscape* ALandscape::GetLandscapeActor()
{
	return this;
}

ALandscapeProxy::ALandscapeProxy(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bReplicates = false;
	NetUpdateFrequency = 10.0f;
	bHidden = false;
	bReplicateMovement = false;
	bCanBeDamaged = false;
	// by default we want to see the Landscape shadows even in the far shadow cascades
	bCastFarShadow = true;

	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent0"));
	RootComponent = SceneComponent;
	RootComponent->RelativeScale3D = FVector(128.0f, 128.0f, 256.0f); // Old default scale, preserved for compatibility. See ULandscapeEditorObject::NewLandscape_Scale
	RootComponent->Mobility = EComponentMobility::Static;
	LandscapeSectionOffset = FIntPoint::ZeroValue;

	StaticLightingResolution = 1.0f;
	StreamingDistanceMultiplier = 1.0f;
	bIsProxy = true;
	MaxLODLevel = -1;
#if WITH_EDITORONLY_DATA
	bLockLocation = true;
	bIsMovingToLevel = false;
	bStaticSectionOffset = true;
#endif // WITH_EDITORONLY_DATA
	LODDistanceFactor = 1.0f;
	LODFalloff = ELandscapeLODFalloff::Linear;
	bCastStaticShadow = true;
	bCastShadowAsTwoSided = false;
	bUsedForNavigation = true;
	CollisionThickness = 16;
	BodyInstance.SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
#if WITH_EDITORONLY_DATA
	MaxPaintedLayersPerComponent = 0;
#endif

#if WITH_EDITOR
	if (VisibilityLayer == NULL)
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<ULandscapeLayerInfoObject> DataLayer;
			FConstructorStatics()
				: DataLayer(TEXT("LandscapeLayerInfoObject'/Engine/EditorLandscapeResources/DataLayer.DataLayer'"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		VisibilityLayer = ConstructorStatics.DataLayer.Get();
		check(VisibilityLayer);
#if WITH_EDITORONLY_DATA
		// This layer should be no weight blending
		VisibilityLayer->bNoWeightBlend = true;
#endif
		VisibilityLayer->AddToRoot();
	}
#endif
}

ALandscape* ALandscapeProxy::GetLandscapeActor()
{
#if WITH_EDITORONLY_DATA
	return LandscapeActor.Get();
#else
	return 0;
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
ULandscapeInfo* ALandscapeProxy::GetLandscapeInfo(bool bSpawnNewActor /*= true*/)
{
	// LandscapeInfo generate
	if (GIsEditor)
	{
		UWorld* OwningWorld = GetWorld();
		if (OwningWorld)
		{
			auto &LandscapeInfoMap = GetLandscapeInfoMap(OwningWorld);

			ULandscapeInfo* LandscapeInfo = LandscapeInfoMap.Map.FindRef(LandscapeGuid);
			if (!LandscapeInfo && bSpawnNewActor && !HasAnyFlags(RF_BeginDestroyed))
			{
				LandscapeInfo = NewObject<ULandscapeInfo>(OwningWorld, NAME_None, RF_Transactional | RF_Transient);
				LandscapeInfoMap.Modify(false);
				LandscapeInfoMap.Map.Add(LandscapeGuid, LandscapeInfo);
			}

			return LandscapeInfo;
		}
	}

	return NULL;
}
#endif

ALandscape* ULandscapeComponent::GetLandscapeActor() const
{
	ALandscapeProxy* Landscape = GetLandscapeProxy();
	if (Landscape)
	{
		return Landscape->GetLandscapeActor();
	}
	return NULL;
}

ULevel* ULandscapeComponent::GetLevel() const
{
	AActor* MyOwner = GetOwner();
	return MyOwner ? MyOwner->GetLevel() : NULL;
}

void ULandscapeComponent::GetGeneratedTexturesAndMaterialInstances(TArray<UObject*>& OutTexturesAndMaterials) const
{
	if (HeightmapTexture)
	{
		OutTexturesAndMaterials.Add(HeightmapTexture);
	}

	for (auto* Tex : WeightmapTextures)
	{
		OutTexturesAndMaterials.Add(Tex);
	}

	if (XYOffsetmapTexture)
	{
		OutTexturesAndMaterials.Add(XYOffsetmapTexture);
	}

	for (UMaterialInstance* CurrentMIC = MaterialInstance; CurrentMIC && CurrentMIC->IsA<ULandscapeMaterialInstanceConstant>(); CurrentMIC = Cast<UMaterialInstance>(CurrentMIC->Parent))
	{
		OutTexturesAndMaterials.Add(CurrentMIC);

		// Sometimes weight map is not registered in the WeightmapTextures, so
		// we need to get it from here.
		if (CurrentMIC->IsA<ULandscapeMaterialInstanceConstant>())
		{
			auto* LandscapeMIC = Cast<ULandscapeMaterialInstanceConstant>(CurrentMIC);

			auto* WeightmapPtr = LandscapeMIC->TextureParameterValues.FindByPredicate(
				[](const FTextureParameterValue& ParamValue)
			{
				static const FName WeightmapParamName("Weightmap0");
				return ParamValue.ParameterName == WeightmapParamName;
			}
			);

			if (WeightmapPtr != nullptr &&
				!OutTexturesAndMaterials.Contains(WeightmapPtr->ParameterValue))
			{
				OutTexturesAndMaterials.Add(WeightmapPtr->ParameterValue);
			}
		}
	}
}

ALandscapeProxy* ULandscapeComponent::GetLandscapeProxy() const
{
	return CastChecked<ALandscapeProxy>(GetOuter());
}

FIntPoint ULandscapeComponent::GetSectionBase() const
{
	return FIntPoint(SectionBaseX, SectionBaseY);
}

void ULandscapeComponent::SetSectionBase(FIntPoint InSectionBase)
{
	SectionBaseX = InSectionBase.X;
	SectionBaseY = InSectionBase.Y;
}

#if WITH_EDITOR
ULandscapeInfo* ULandscapeComponent::GetLandscapeInfo(bool bSpawnNewActor /*= true*/) const
{
	if (GetLandscapeProxy())
	{
		return GetLandscapeProxy()->GetLandscapeInfo(bSpawnNewActor);
	}
	return NULL;
}
#endif

void ULandscapeComponent::BeginDestroy()
{
	Super::BeginDestroy();

#if WITH_EDITOR
	if (EditToolRenderData != NULL)
	{
		// Ask render thread to destroy EditToolRenderData
		EditToolRenderData->Cleanup();
		EditToolRenderData = NULL;
	}

	if (GIsEditor && !HasAnyFlags(RF_ClassDefaultObject))
	{
		ALandscapeProxy* Proxy = GetLandscapeProxy();

		// Remove any weightmap allocations from the Landscape Actor's map
		for (int32 LayerIdx = 0; LayerIdx < WeightmapLayerAllocations.Num(); LayerIdx++)
		{
			int32 WeightmapIndex = WeightmapLayerAllocations[LayerIdx].WeightmapTextureIndex;
			if (WeightmapTextures.IsValidIndex(WeightmapIndex))
			{
				UTexture2D* WeightmapTexture = WeightmapTextures[WeightmapIndex];
				FLandscapeWeightmapUsage* Usage = Proxy->WeightmapUsageMap.Find(WeightmapTexture);
				if (Usage != NULL)
				{
					Usage->ChannelUsage[WeightmapLayerAllocations[LayerIdx].WeightmapTextureChannel] = NULL;

					if (Usage->FreeChannelCount() == 4)
					{
						Proxy->WeightmapUsageMap.Remove(WeightmapTexture);
					}
				}
			}
		}
	}
#endif
}

FPrimitiveSceneProxy* ULandscapeComponent::CreateSceneProxy()
{
	const auto FeatureLevel = GetWorld()->FeatureLevel;
	FPrimitiveSceneProxy* Proxy = NULL;
	if (FeatureLevel >= ERHIFeatureLevel::SM4)
	{
#if WITH_EDITOR
		if (EditToolRenderData == NULL)
		{
			EditToolRenderData = new FLandscapeEditToolRenderData(this);
		}
		Proxy = new FLandscapeComponentSceneProxy(this, EditToolRenderData);
#else
		Proxy = new FLandscapeComponentSceneProxy(this, NULL);
#endif
	}
	else // i.e. (FeatureLevel <= ERHIFeatureLevel::ES3_1)
	{
#if WITH_EDITOR
		// See if we need to cook platform data for ES2 preview in editor
		// We can't always pre-cook it in PostLoad/Serialize etc because landscape can be edited
		CheckGenerateLandscapePlatformData(false);

		if (PlatformData.HasValidPlatformData())
		{
			if (EditToolRenderData == NULL)
			{
				EditToolRenderData = new FLandscapeEditToolRenderData(this);
			}

			Proxy = new FLandscapeComponentSceneProxyMobile(this, EditToolRenderData);
		}
#else
		if (PlatformData.HasValidPlatformData())
		{
			Proxy = new FLandscapeComponentSceneProxyMobile(this, NULL);
		}
#endif
	}
	return Proxy;
}

void ULandscapeComponent::DestroyComponent(bool bPromoteChildren/*= false*/)
{
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	if (Proxy)
	{
		Proxy->LandscapeComponents.Remove(this);
	}

	Super::DestroyComponent(bPromoteChildren);
}

FBoxSphereBounds ULandscapeComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return FBoxSphereBounds(CachedLocalBox.TransformBy(LocalToWorld));
}

void ULandscapeComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	ULandscapeInfo* Info = GetLandscapeInfo(false);
	if (Info)
	{
		Info->RegisterActorComponent(this);
	}
#endif
}

void ULandscapeComponent::OnUnregister()
{
	Super::OnUnregister();

#if WITH_EDITOR
	ULandscapeInfo* Info = GetLandscapeInfo(false);
	if (Info)
	{
		Info->UnregisterActorComponent(this);
	}
#endif
}

void ULandscapeComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const
{
	if (MaterialInstance != NULL)
	{
		OutMaterials.Add(MaterialInstance);
	}
}

void ALandscapeProxy::RegisterAllComponents()
{
	Super::RegisterAllComponents();

	// Landscape was added to world 
	// We might need to update shared landscape data
	if (GIsEditor && GetWorld() && !GetWorld()->IsPlayInEditor())
	{
		GEngine->DeferredCommands.AddUnique(TEXT("UpdateLandscapeEditorData"));
	}
}

void ALandscapeProxy::UnregisterAllComponents()
{
	Super::UnregisterAllComponents();

	// Landscape was removed from world 
	// We might need to update shared landscape data
	if (GEngine && GIsEditor && GetWorld() && !GetWorld()->IsPlayInEditor())
	{
		GEngine->DeferredCommands.AddUnique(TEXT("UpdateLandscapeEditorData"));
	}
}

// FLandscapeWeightmapUsage serializer
FArchive& operator<<(FArchive& Ar, FLandscapeWeightmapUsage& U)
{
	return Ar << U.ChannelUsage[0] << U.ChannelUsage[1] << U.ChannelUsage[2] << U.ChannelUsage[3];
}

#if WITH_EDITORONLY_DATA
FArchive& operator<<(FArchive& Ar, FLandscapeAddCollision& U)
{
	return Ar << U.Corners[0] << U.Corners[1] << U.Corners[2] << U.Corners[3];
}
#endif // WITH_EDITORONLY_DATA

FArchive& operator<<(FArchive& Ar, FLandscapeLayerStruct*& L)
{
	if (L)
	{
		Ar << L->LayerInfoObj;
#if WITH_EDITORONLY_DATA
		return Ar << L->ThumbnailMIC;
#else
		return Ar;
#endif // WITH_EDITORONLY_DATA
	}
	return Ar;
}

void ULandscapeInfo::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// We do not serialize XYtoComponentMap as we don't want these references to hold a component in memory.
	// The references are automatically cleaned up by the components' BeginDestroy method.
	if (Ar.IsTransacting())
	{
		Ar << XYtoComponentMap;
#if WITH_EDITORONLY_DATA
		Ar << XYtoAddCollisionMap;
#endif
		Ar << SelectedComponents;
		Ar << SelectedRegion;
		Ar << SelectedRegionComponents;
	}
}

void ALandscape::PostLoad()
{
	if (!LandscapeGuid.IsValid())
	{
		LandscapeGuid = FGuid::NewGuid();
	}

	Super::PostLoad();
}

void ULandscapeInfo::BeginDestroy()
{
	Super::BeginDestroy();

#if WITH_EDITOR
	if (DataInterface)
	{
		delete DataInterface;
		DataInterface = NULL;
	}
#endif
}

#if WITH_EDITOR

void ALandscape::CheckForErrors()
{
}

#endif

void ALandscapeProxy::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITOR
	if (Ar.IsTransacting())
	{
		Ar << WeightmapUsageMap;
	}
#endif
}

void ALandscapeProxy::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	ALandscapeProxy* This = CastChecked<ALandscapeProxy>(InThis);

	Super::AddReferencedObjects(InThis, Collector);

	Collector.AddReferencedObjects(This->MaterialInstanceConstantMap, This);

	for (auto It = This->WeightmapUsageMap.CreateIterator(); It; ++It)
	{
		Collector.AddReferencedObject(It.Key(), This);
		Collector.AddReferencedObject(It.Value().ChannelUsage[0], This);
		Collector.AddReferencedObject(It.Value().ChannelUsage[1], This);
		Collector.AddReferencedObject(It.Value().ChannelUsage[2], This);
		Collector.AddReferencedObject(It.Value().ChannelUsage[3], This);
	}
}

#if WITH_EDITOR
void ALandscapeProxy::PreEditUndo()
{
	Super::PreEditUndo();

	// PostEditUndo doesn't get called when undoing a create action
	// and PreEditUndo doesn't get called when undoing a delete action
	// so this code needs to be in both!
	if (GIsEditor && GetWorld() && !GetWorld()->IsPlayInEditor())
	{
		GEngine->DeferredCommands.AddUnique(TEXT("UpdateLandscapeEditorData"));
	}
}

void ALandscapeProxy::PostEditUndo()
{
	Super::PostEditUndo();

	if (GIsEditor && GetWorld() && !GetWorld()->IsPlayInEditor())
	{
		GEngine->DeferredCommands.AddUnique(TEXT("UpdateLandscapeEditorData"));
	}
}

void ULandscapeInfo::PostEditUndo()
{
	Super::PostEditUndo();

	if (GIsEditor && GetWorld() && !GetWorld()->IsPlayInEditor())
	{
		GEngine->DeferredCommands.AddUnique(TEXT("UpdateLandscapeEditorData"));
	}
}

FName FLandscapeInfoLayerSettings::GetLayerName() const
{
	checkSlow(LayerInfoObj == NULL || LayerInfoObj->LayerName == LayerName);

	return LayerName;
}

FLandscapeEditorLayerSettings& FLandscapeInfoLayerSettings::GetEditorSettings() const
{
	check(LayerInfoObj);

	ULandscapeInfo* LandscapeInfo = Owner->GetLandscapeInfo();
	return LandscapeInfo->GetLayerEditorSettings(LayerInfoObj);
}

FLandscapeEditorLayerSettings& ULandscapeInfo::GetLayerEditorSettings(ULandscapeLayerInfoObject* LayerInfo) const
{
	auto& LayerSettingsList = GetLandscapeProxy()->EditorLayerSettings;
	FLandscapeEditorLayerSettings* EditorLayerSettings = LayerSettingsList.FindByKey(LayerInfo);
	if (EditorLayerSettings)
	{
		return *EditorLayerSettings;
	}
	else
	{
		int32 Index = LayerSettingsList.Add(LayerInfo);
		return LayerSettingsList[Index];
	}
}

void ULandscapeInfo::CreateLayerEditorSettingsFor(ULandscapeLayerInfoObject* LayerInfo)
{
	ALandscape* Landscape = LandscapeActor.Get();
	if (Landscape != NULL)
	{
		FLandscapeEditorLayerSettings* EditorLayerSettings = Landscape->EditorLayerSettings.FindByKey(LayerInfo);
		if (EditorLayerSettings == NULL)
		{
			Landscape->Modify();
			Landscape->EditorLayerSettings.Add(LayerInfo);
		}
	}

	for (auto It = Proxies.CreateConstIterator(); It; ++It)
	{
		ALandscapeProxy* Proxy = *It;
		FLandscapeEditorLayerSettings* EditorLayerSettings = Proxy->EditorLayerSettings.FindByKey(LayerInfo);
		if (EditorLayerSettings == NULL)
		{
			Proxy->Modify();
			Proxy->EditorLayerSettings.Add(LayerInfo);
		}
	}
}

ULandscapeLayerInfoObject* ULandscapeInfo::GetLayerInfoByName(FName LayerName, ALandscapeProxy* Owner /*= NULL*/) const
{
	ULandscapeLayerInfoObject* LayerInfo = NULL;
	for (int32 j = 0; j < Layers.Num(); j++)
	{
		if (Layers[j].LayerInfoObj && Layers[j].LayerInfoObj->LayerName == LayerName
			&& (Owner == NULL || Layers[j].Owner == Owner))
		{
			LayerInfo = Layers[j].LayerInfoObj;
		}
	}
	return LayerInfo;
}

int32 ULandscapeInfo::GetLayerInfoIndex(ULandscapeLayerInfoObject* LayerInfo, ALandscapeProxy* Owner /*= NULL*/) const
{
	for (int32 j = 0; j < Layers.Num(); j++)
	{
		if (Layers[j].LayerInfoObj && Layers[j].LayerInfoObj == LayerInfo
			&& (Owner == NULL || Layers[j].Owner == Owner))
		{
			return j;
		}
	}

	return INDEX_NONE;
}

int32 ULandscapeInfo::GetLayerInfoIndex(FName LayerName, ALandscapeProxy* Owner /*= NULL*/) const
{
	for (int32 j = 0; j < Layers.Num(); j++)
	{
		if (Layers[j].GetLayerName() == LayerName
			&& (Owner == NULL || Layers[j].Owner == Owner))
		{
			return j;
		}
	}

	return INDEX_NONE;
}


bool ULandscapeInfo::UpdateLayerInfoMap(ALandscapeProxy* Proxy /*= NULL*/, bool bInvalidate /*= false*/)
{
	bool bHasCollision = false;
	if (GIsEditor)
	{
		bIsValid = !bInvalidate;

		if (Proxy)
		{
			if (bInvalidate)
			{
				// this is a horribly dangerous combination of parameters...

				for (int32 i = 0; i < Layers.Num(); i++)
				{
					if (Layers[i].Owner == Proxy)
					{
						Layers.RemoveAt(i--);
					}
				}
			}
			else // Proxy && !bInvalidate
			{
				TArray<FName> LayerNames = Proxy->GetLayersFromMaterial();

				// Validate any existing layer infos owned by this proxy
				for (int32 i = 0; i < Layers.Num(); i++)
				{
					if (Layers[i].Owner == Proxy)
					{
						Layers[i].bValid = LayerNames.Contains(Layers[i].GetLayerName());
					}
				}

				// Add placeholders for any unused material layers
				for (int32 i = 0; i < LayerNames.Num(); i++)
				{
					int32 LayerInfoIndex = GetLayerInfoIndex(LayerNames[i]);
					if (LayerInfoIndex == INDEX_NONE)
					{
						FLandscapeInfoLayerSettings LayerSettings(LayerNames[i], Proxy);
						LayerSettings.bValid = true;
						Layers.Add(LayerSettings);
					}
				}

				// Populate from layers used in components
				for (int32 ComponentIndex = 0; ComponentIndex < Proxy->LandscapeComponents.Num(); ComponentIndex++)
				{
					ULandscapeComponent* Component = Proxy->LandscapeComponents[ComponentIndex];

					// Add layers from per-component override materials
					if (Component->OverrideMaterial != NULL)
					{
						TArray<FName> ComponentLayerNames = Proxy->GetLayersFromMaterial(Component->OverrideMaterial);
						for (int32 i = 0; i < ComponentLayerNames.Num(); i++)
						{
							int32 LayerInfoIndex = GetLayerInfoIndex(ComponentLayerNames[i]);
							if (LayerInfoIndex == INDEX_NONE)
							{
								FLandscapeInfoLayerSettings LayerSettings(ComponentLayerNames[i], Proxy);
								LayerSettings.bValid = true;
								Layers.Add(LayerSettings);
							}
						}
					}

					for (int32 AllocationIndex = 0; AllocationIndex < Component->WeightmapLayerAllocations.Num(); AllocationIndex++)
					{
						ULandscapeLayerInfoObject* LayerInfo = Component->WeightmapLayerAllocations[AllocationIndex].LayerInfo;
						if (LayerInfo)
						{
							int32 LayerInfoIndex = GetLayerInfoIndex(LayerInfo);
							bool bValid = LayerNames.Contains(LayerInfo->LayerName);

							if (LayerInfoIndex != INDEX_NONE)
							{
								FLandscapeInfoLayerSettings& LayerSettings = Layers[LayerInfoIndex];

								// Valid layer infos take precedence over invalid ones
								// Landscape Actors take precedence over Proxies
								if ((bValid && !LayerSettings.bValid)
									|| (bValid == LayerSettings.bValid && !Proxy->bIsProxy))
								{
									LayerSettings.Owner = Proxy;
									LayerSettings.bValid = bValid;
									LayerSettings.ThumbnailMIC = NULL;
								}
							}
							else
							{
								// handle existing placeholder layers
								LayerInfoIndex = GetLayerInfoIndex(LayerInfo->LayerName);
								if (LayerInfoIndex != INDEX_NONE)
								{
									FLandscapeInfoLayerSettings& LayerSettings = Layers[LayerInfoIndex];

									//if (LayerSettings.Owner == Proxy)
									{
										LayerSettings.Owner = Proxy;
										LayerSettings.LayerInfoObj = LayerInfo;
										LayerSettings.bValid = bValid;
										LayerSettings.ThumbnailMIC = NULL;
									}
								}
								else
								{
									FLandscapeInfoLayerSettings LayerSettings(LayerInfo, Proxy);
									LayerSettings.bValid = bValid;
									Layers.Add(LayerSettings);
								}
							}
						}
					}
				}

				// Add any layer infos cached in the actor
				Proxy->EditorLayerSettings.Remove(NULL);
				for (int32 i = 0; i < Proxy->EditorLayerSettings.Num(); i++)
				{
					FLandscapeEditorLayerSettings& LayerInfo = Proxy->EditorLayerSettings[i];
					if (LayerNames.Contains(LayerInfo.LayerInfoObj->LayerName))
					{
						// intentionally using the layer name here so we don't add layer infos from
						// the cache that have the same name as an actual assignment from a component above
						int32 LayerInfoIndex = GetLayerInfoIndex(LayerInfo.LayerInfoObj->LayerName);
						if (LayerInfoIndex != INDEX_NONE)
						{
							FLandscapeInfoLayerSettings& LayerSettings = Layers[LayerInfoIndex];
							if (LayerSettings.LayerInfoObj == NULL)
							{
								LayerSettings.Owner = Proxy;
								LayerSettings.LayerInfoObj = LayerInfo.LayerInfoObj;
								LayerSettings.bValid = true;
							}
						}
					}
					else
					{
						Proxy->Modify();
						Proxy->EditorLayerSettings.RemoveAt(i--);
					}
				}
			}
		}
		else // !Proxy
		{
			Layers.Empty();

			if (!bInvalidate)
			{
				if (LandscapeActor.IsValid() && !LandscapeActor->IsPendingKillPending())
				{
					checkSlow(LandscapeActor->GetLandscapeInfo() == this);
					UpdateLayerInfoMap(LandscapeActor.Get(), false);
				}
				for (auto It = Proxies.CreateIterator(); It; ++It)
				{
					ALandscapeProxy* LandscapeProxy = *It;
					checkSlow(LandscapeProxy && LandscapeProxy->GetLandscapeInfo() == this);
					UpdateLayerInfoMap(LandscapeProxy, false);
				}
			}
		}

		//if (GCallbackEvent)
		//{
		//	GCallbackEvent->Send( CALLBACK_EditorPostModal );
		//}
	}
	return bHasCollision;
}
#endif

void ALandscapeProxy::PostLoad()
{
	Super::PostLoad();

	// Temporary
	if (ComponentSizeQuads == 0 && LandscapeComponents.Num() > 0)
	{
		ULandscapeComponent* Comp = LandscapeComponents[0];
		if (Comp)
		{
			ComponentSizeQuads = Comp->ComponentSizeQuads;
			SubsectionSizeQuads = Comp->SubsectionSizeQuads;
			NumSubsections = Comp->NumSubsections;
		}
	}

	if (IsTemplate() == false)
	{
		BodyInstance.FixupData(this);
	}

#if WITH_EDITOR
	if ((GetLinker() && (GetLinker()->UE4Ver() < VER_UE4_LANDSCAPE_COMPONENT_LAZY_REFERENCES)) || LandscapeComponents.Num() != CollisionComponents.Num())
	{
		// Need to clean up invalid collision components
		RecreateCollisionComponents();
	}

	if (GetLinker() && GetLinker()->UE4Ver() < VER_UE4_LANDSCAPE_STATIC_SECTION_OFFSET)
	{
		bStaticSectionOffset = false;
	}

	EditorLayerSettings.Remove(NULL);

	if (EditorCachedLayerInfos_DEPRECATED.Num() > 0)
	{
		for (int32 i = 0; i < EditorCachedLayerInfos_DEPRECATED.Num(); i++)
		{
			EditorLayerSettings.Add(EditorCachedLayerInfos_DEPRECATED[i]);
		}
		EditorCachedLayerInfos_DEPRECATED.Empty();
	}

	if (GIsEditor && !GetWorld()->IsPlayInEditor())
	{
		// defer LandscapeInfo setup
		GEngine->DeferredCommands.AddUnique(TEXT("UpdateLandscapeEditorData -warnings"));
	}
#endif
}

#if WITH_EDITOR
void ALandscapeProxy::Destroyed()
{
	Super::Destroyed();

	if (GIsEditor && !GetWorld()->IsGameWorld())
	{
		ULandscapeInfo::RecreateLandscapeInfo(GetWorld(), false);

		if (SplineComponent)
		{
			SplineComponent->ModifySplines();
		}

		TotalComponentsNeedingGrassMapRender -= NumComponentsNeedingGrassMapRender;
		NumComponentsNeedingGrassMapRender = 0;
		TotalTexturesToStreamForVisibleGrassMapRender -= NumTexturesToStreamForVisibleGrassMapRender;
		NumTexturesToStreamForVisibleGrassMapRender = 0;
	}
}

void ALandscapeProxy::GetSharedProperties(ALandscapeProxy* Landscape)
{
	if (GIsEditor && Landscape)
	{
		Modify();

		LandscapeGuid = Landscape->LandscapeGuid;

		//@todo UE4 merge, landscape, this needs work
		RootComponent->SetRelativeScale3D(Landscape->GetRootComponent()->GetComponentToWorld().GetScale3D());

		//PrePivot = Landscape->PrePivot;
		StaticLightingResolution = Landscape->StaticLightingResolution;
		bCastStaticShadow = Landscape->bCastStaticShadow;
		bCastShadowAsTwoSided = Landscape->bCastShadowAsTwoSided;
		ComponentSizeQuads = Landscape->ComponentSizeQuads;
		NumSubsections = Landscape->NumSubsections;
		SubsectionSizeQuads = Landscape->SubsectionSizeQuads;
		MaxLODLevel = Landscape->MaxLODLevel;
		LODDistanceFactor = Landscape->LODDistanceFactor;
		LODFalloff = Landscape->LODFalloff;
		CollisionMipLevel = Landscape->CollisionMipLevel;
		if (!LandscapeMaterial)
		{
			LandscapeMaterial = Landscape->LandscapeMaterial;
		}
		if (!LandscapeHoleMaterial)
		{
			LandscapeHoleMaterial = Landscape->LandscapeHoleMaterial;
		}
		if (LandscapeMaterial == Landscape->LandscapeMaterial)
		{
			EditorLayerSettings = Landscape->EditorLayerSettings;
		}
		if (!DefaultPhysMaterial)
		{
			DefaultPhysMaterial = Landscape->DefaultPhysMaterial;
		}
		LightmassSettings = Landscape->LightmassSettings;
	}
}

void ALandscapeProxy::ConditionalAssignCommonProperties(ALandscape* Landscape)
{
	if (Landscape == nullptr)
	{
		return;
	}
	
	bool bUpdated = false;
	
	if (MaxLODLevel != Landscape->MaxLODLevel)
	{
		MaxLODLevel = Landscape->MaxLODLevel;
		bUpdated = true;
	}
	
	if (LODDistanceFactor != Landscape->LODDistanceFactor)
	{
		LODDistanceFactor = Landscape->LODDistanceFactor;
		bUpdated = true;
	}
	
	if (LODFalloff != Landscape->LODFalloff)
	{
		LODFalloff = Landscape->LODFalloff;
		bUpdated = true;
	}

	if (bUpdated)
	{
		MarkPackageDirty();
	}
}

FTransform ALandscapeProxy::LandscapeActorToWorld() const
{
	FTransform TM = ActorToWorld();
	// Add this proxy landscape section offset to obtain landscape actor transform
	TM.AddToTranslation(TM.TransformVector(-FVector(LandscapeSectionOffset)));
	return TM;
}

void ALandscapeProxy::SetAbsoluteSectionBase(FIntPoint InSectionBase)
{
	FIntPoint Difference = InSectionBase - LandscapeSectionOffset;
	LandscapeSectionOffset = InSectionBase;

	for (int32 CompIdx = 0; CompIdx < LandscapeComponents.Num(); CompIdx++)
	{
		ULandscapeComponent* Comp = LandscapeComponents[CompIdx];
		if (Comp)
		{
			FIntPoint AbsoluteSectionBase = Comp->GetSectionBase() + Difference;
			Comp->SetSectionBase(AbsoluteSectionBase);
			Comp->RecreateRenderState_Concurrent();
		}
	}

	for (int32 CompIdx = 0; CompIdx < CollisionComponents.Num(); CompIdx++)
	{
		ULandscapeHeightfieldCollisionComponent* Comp = CollisionComponents[CompIdx];
		if (Comp)
		{
			FIntPoint AbsoluteSectionBase = Comp->GetSectionBase() + Difference;
			Comp->SetSectionBase(AbsoluteSectionBase);
		}
	}
}

FIntPoint ALandscapeProxy::GetSectionBaseOffset() const
{
	return LandscapeSectionOffset;
}

void ALandscapeProxy::RecreateComponentsState()
{
	for (int32 ComponentIndex = 0; ComponentIndex < LandscapeComponents.Num(); ComponentIndex++)
	{
		ULandscapeComponent* Comp = LandscapeComponents[ComponentIndex];
		if (Comp)
		{
			Comp->UpdateComponentToWorld();
			Comp->UpdateCachedBounds();
			Comp->UpdateBounds();
			Comp->RecreateRenderState_Concurrent(); // @todo UE4 jackp just render state needs update?
		}
	}

	for (int32 ComponentIndex = 0; ComponentIndex < CollisionComponents.Num(); ComponentIndex++)
	{
		ULandscapeHeightfieldCollisionComponent* Comp = CollisionComponents[ComponentIndex];
		if (Comp)
		{
			Comp->UpdateComponentToWorld();
			Comp->RecreatePhysicsState();
		}
	}
}

UMaterialInterface* ALandscapeProxy::GetLandscapeMaterial() const
{
	if (LandscapeMaterial)
	{
		return LandscapeMaterial;
	}
	else if (LandscapeActor)
	{
		return LandscapeActor->GetLandscapeMaterial();
	}
	return UMaterial::GetDefaultMaterial(MD_Surface);
}

UMaterialInterface* ALandscapeProxy::GetLandscapeHoleMaterial() const
{
	if (LandscapeHoleMaterial)
	{
		return LandscapeHoleMaterial;
	}
	else if (ALandscape* Landscape = LandscapeActor.Get())
	{
		return Landscape->GetLandscapeHoleMaterial();
	}
	return nullptr;
}

UMaterialInterface* ALandscape::GetLandscapeMaterial() const
{
	if (LandscapeMaterial)
	{
		return LandscapeMaterial;
	}
	return UMaterial::GetDefaultMaterial(MD_Surface);
}

UMaterialInterface* ALandscape::GetLandscapeHoleMaterial() const
{
	if (LandscapeHoleMaterial)
	{
		return LandscapeHoleMaterial;
	}
	return nullptr;
}

void ALandscape::PreSave()
{
	Super::PreSave();
	//ULandscapeInfo* Info = GetLandscapeInfo();
	//if (GIsEditor && Info && !IsRunningCommandlet())
	//{
	//	for (TSet<ALandscapeProxy*>::TIterator It(Info->Proxies); It; ++It)
	//	{
	//		ALandscapeProxy* Proxy = *It;
	//		if (!ensure(Proxy->LandscapeActor == this))
	//		{
	//			Proxy->LandscapeActor = this;
	//			Proxy->GetSharedProperties(this);
	//		}
	//	}
	//}
}

ALandscapeProxy* ULandscapeInfo::GetLandscapeProxyForLevel(ULevel* Level) const
{
	ALandscape* Landscape = LandscapeActor.Get();

	if (Landscape && Landscape->GetLevel() == Level)
	{
		return Landscape;
	}

	for (ALandscapeProxy* LandscapeProxy : Proxies)
	{
		if (LandscapeProxy && LandscapeProxy->GetLevel() == Level)
		{
			return LandscapeProxy;
		}
	}

	return nullptr;
}

ALandscapeProxy* ULandscapeInfo::GetCurrentLevelLandscapeProxy(bool bRegistered) const
{
	ALandscape* Landscape = LandscapeActor.Get();

	if (Landscape && (!bRegistered || Landscape->GetRootComponent()->IsRegistered()))
	{
		UWorld* LandscapeWorld = Landscape->GetWorld();
		if (LandscapeWorld &&
			LandscapeWorld->GetCurrentLevel() == Landscape->GetOuter())
		{
			return Landscape;
		}
	}

	for (auto It = Proxies.CreateConstIterator(); It; ++It)
	{
		ALandscapeProxy* LandscapeProxy = (*It);

		if (LandscapeProxy && (!bRegistered || LandscapeProxy->GetRootComponent()->IsRegistered()))
		{
			UWorld* ProxyWorld = LandscapeProxy->GetWorld();
			if (ProxyWorld &&
				ProxyWorld->GetCurrentLevel() == LandscapeProxy->GetOuter())
			{
				return LandscapeProxy;
			}
		}
	}

	return NULL;
}

ALandscapeProxy* ULandscapeInfo::GetLandscapeProxy() const
{
	// Mostly this Proxy used to calculate transformations
	// in Editor all proxies of same landscape actor have root components in same locations
	// so it doesn't really matter which proxy we return here

	// prefer LandscapeActor in case it is loaded
	ALandscape* Landscape = LandscapeActor.Get();
	if (Landscape != NULL &&
		Landscape->GetRootComponent()->IsRegistered())
	{
		return Landscape;
	}

	// prefer current level proxy 
	ALandscapeProxy* Proxy = GetCurrentLevelLandscapeProxy(true);
	if (Proxy != NULL)
	{
		return Proxy;
	}

	// any proxy in the world
	for (auto It = Proxies.CreateConstIterator(); It; ++It)
	{
		Proxy = (*It);
		if (Proxy != NULL &&
			Proxy->GetRootComponent()->IsRegistered())
		{
			return Proxy;
		}
	}

	return NULL;
}

void ULandscapeInfo::RegisterActor(ALandscapeProxy* Proxy, bool bMapCheck)
{
	// do not pass here invalid actors
	check(Proxy->GetLandscapeGuid().IsValid());
	UWorld* OwningWorld = CastChecked<UWorld>(GetOuter());

	// in case this Info object is not initialized yet
	// initialized it with properties from passed actor
	if (LandscapeGuid.IsValid() == false ||
		(GetLandscapeProxy() == NULL && ensure(LandscapeGuid == Proxy->GetLandscapeGuid())))
	{
		LandscapeGuid = Proxy->GetLandscapeGuid();
		ComponentSizeQuads = Proxy->ComponentSizeQuads;
		ComponentNumSubsections = Proxy->NumSubsections;
		SubsectionSizeQuads = Proxy->SubsectionSizeQuads;
		DrawScale = Proxy->GetRootComponent()->RelativeScale3D;
	}

	// check that passed actor matches all shared parameters
	check(LandscapeGuid == Proxy->GetLandscapeGuid());
	check(ComponentSizeQuads == Proxy->ComponentSizeQuads);
	check(ComponentNumSubsections == Proxy->NumSubsections);
	check(SubsectionSizeQuads == Proxy->SubsectionSizeQuads);

	if (!DrawScale.Equals(Proxy->GetRootComponent()->RelativeScale3D))
	{
		UE_LOG(LogLandscape, Warning, TEXT("Landscape proxy (%s) scale (%s) does not match to main actor scale (%s)."),
			*Proxy->GetName(), *Proxy->GetRootComponent()->RelativeScale3D.ToCompactString(), *DrawScale.ToCompactString());
	}

	// register
	ALandscape* Landscape = Cast<ALandscape>(Proxy);
	if (Landscape)
	{
		LandscapeActor = Landscape;
		// In world composition user is not allowed to move landscape in editor, only through WorldBrowser 
		LandscapeActor->bLockLocation = (OwningWorld->WorldComposition != NULL);

		// update proxies reference actor
		for (auto It = Proxies.CreateConstIterator(); It; ++It)
		{
			(*It)->LandscapeActor = LandscapeActor;
			(*It)->ConditionalAssignCommonProperties(Landscape);
		}
	}
	else
	{
		Proxies.Add(Proxy);
		Proxy->LandscapeActor = LandscapeActor;
		Proxy->ConditionalAssignCommonProperties(LandscapeActor.Get());
	}

	//
	// add proxy components to the XY map
	//
	for (int32 CompIdx = 0; CompIdx < Proxy->LandscapeComponents.Num(); ++CompIdx)
	{
		RegisterActorComponent(Proxy->LandscapeComponents[CompIdx], bMapCheck);
	}
}

void ULandscapeInfo::UnregisterActor(ALandscapeProxy* Proxy)
{
	ALandscape* Landscape = Cast<ALandscape>(Proxy);
	if (Landscape)
	{
		check(LandscapeActor.Get() == Landscape);
		LandscapeActor = nullptr;

		// update proxies reference to landscape actor
		for (auto It = Proxies.CreateConstIterator(); It; ++It)
		{
			(*It)->LandscapeActor = nullptr;
		}
	}
	else
	{
		Proxies.Remove(Proxy);
		Proxy->LandscapeActor = nullptr;
	}

	// remove proxy components from the XY map
	for (int32 CompIdx = 0; CompIdx < Proxy->LandscapeComponents.Num(); ++CompIdx)
	{
		UnregisterActorComponent(Proxy->LandscapeComponents[CompIdx]);
	}

	XYtoComponentMap.Compact();
}

void ULandscapeInfo::RegisterActorComponent(ULandscapeComponent* Component, bool bMapCheck)
{
	// Do not register components which are not part of the world
	if (Component == NULL ||
		Component->IsRegistered() == false)
	{
		return;
	}

	check(Component != NULL);
	FIntPoint ComponentKey = Component->GetSectionBase() / Component->ComponentSizeQuads;
	auto RegisteredComponent = XYtoComponentMap.FindRef(ComponentKey);

	if (RegisteredComponent == NULL)
	{
		XYtoComponentMap.Add(ComponentKey, Component);
	}
	else if (bMapCheck)
	{
		ALandscapeProxy* OurProxy = Component->GetLandscapeProxy();
		ALandscapeProxy* ExistingProxy = RegisteredComponent->GetLandscapeProxy();
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ProxyName1"), FText::FromString(OurProxy->GetName()));
		Arguments.Add(TEXT("LevelName1"), FText::FromString(OurProxy->GetLevel()->GetOutermost()->GetName()));
		Arguments.Add(TEXT("ProxyName2"), FText::FromString(ExistingProxy->GetName()));
		Arguments.Add(TEXT("LevelName2"), FText::FromString(ExistingProxy->GetLevel()->GetOutermost()->GetName()));
		Arguments.Add(TEXT("XLocation"), Component->GetSectionBase().X);
		Arguments.Add(TEXT("YLocation"), Component->GetSectionBase().Y);
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(OurProxy))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("MapCheck_Message_LandscapeComponentPostLoad_Warning", "Landscape {ProxyName1} of {LevelName1} has overlapping render components with {ProxyName2} of {LevelName2} at location ({XLocation}, {YLocation})."), Arguments)))
#if WITH_EDITOR
			->AddToken(FActionToken::Create(LOCTEXT("MapCheck_RemoveDuplicateLandscapeComponent", "Delete Duplicate"), LOCTEXT("MapCheck_RemoveDuplicateLandscapeComponentDesc", "Deletes the duplicate landscape component."), FOnActionTokenExecuted::CreateUObject(OurProxy, &ALandscapeProxy::RemoveOverlappingComponent, Component), true))
#endif
			->AddToken(FMapErrorToken::Create(FMapErrors::LandscapeComponentPostLoad_Warning));

		// Show MapCheck window
		FMessageLog("MapCheck").Open(EMessageSeverity::Warning);
	}

	// Update Selected Components/Regions
#if WITH_EDITOR
	if (Component->EditToolRenderData != NULL && Component->EditToolRenderData->SelectedType)
	{
		if (Component->EditToolRenderData->SelectedType & FLandscapeEditToolRenderData::ST_COMPONENT)
		{
			SelectedComponents.Add(Component);
		}
		else if (Component->EditToolRenderData->SelectedType & FLandscapeEditToolRenderData::ST_REGION)
		{
			SelectedRegionComponents.Add(Component);
		}
	}
#endif
}

void ULandscapeInfo::UnregisterActorComponent(ULandscapeComponent* Component)
{
	check(Component != NULL);

	FIntPoint ComponentKey = Component->GetSectionBase() / Component->ComponentSizeQuads;
	auto RegisteredComponent = XYtoComponentMap.FindRef(ComponentKey);

	if (RegisteredComponent == Component)
	{
		XYtoComponentMap.Remove(ComponentKey);
	}

	SelectedComponents.Remove(Component);
	SelectedRegionComponents.Remove(Component);
}

void ULandscapeInfo::Reset()
{
	LandscapeActor.Reset();

	Proxies.Empty();
	XYtoComponentMap.Empty();
	XYtoAddCollisionMap.Empty();

	//SelectedComponents.Empty();
	//SelectedRegionComponents.Empty();
	//SelectedRegion.Empty();
}

void ULandscapeInfo::FixupProxiesTransform()
{
	ALandscape* Landscape = LandscapeActor.Get();

	if (Landscape == NULL ||
		Landscape->GetRootComponent()->IsRegistered() == false)
	{
		return;
	}

	// Make sure section offset of all proxies is multiple of ALandscapeProxy::ComponentSizeQuads
	for (auto It = Proxies.CreateConstIterator(); It; ++It)
	{
		ALandscapeProxy* Proxy = *It;
		FIntPoint LandscapeSectionOffset = Proxy->LandscapeSectionOffset - Landscape->LandscapeSectionOffset;
		FIntPoint LandscapeSectionOffsetRem(
			LandscapeSectionOffset.X % Proxy->ComponentSizeQuads, 
			LandscapeSectionOffset.Y % Proxy->ComponentSizeQuads);

		if (LandscapeSectionOffsetRem.X != 0 || LandscapeSectionOffsetRem.Y != 0)
		{
			FIntPoint NewLandscapeSectionOffset = Proxy->LandscapeSectionOffset - LandscapeSectionOffsetRem;
			
			UE_LOG(LogLandscape, Warning, TEXT("Landscape section base is not multiple of component size, attempted automated fix: '%s', %d,%d vs %d,%d."),
					*Proxy->GetFullName(), Proxy->LandscapeSectionOffset.X, Proxy->LandscapeSectionOffset.Y, NewLandscapeSectionOffset.X, NewLandscapeSectionOffset.Y);

			Proxy->SetAbsoluteSectionBase(NewLandscapeSectionOffset);
		}
	}

	FTransform LandscapeTM = Landscape->LandscapeActorToWorld();
	// Update transformations of all linked landscape proxies
	for (auto It = Proxies.CreateConstIterator(); It; ++It)
	{
		ALandscapeProxy* Proxy = *It;
		FTransform ProxyRelativeTM(FVector(Proxy->LandscapeSectionOffset));
		FTransform ProxyTransform = ProxyRelativeTM*LandscapeTM;

		if (!Proxy->GetTransform().Equals(ProxyTransform))
		{
			Proxy->SetActorTransform(ProxyTransform);
			Proxy->RecreateComponentsState();

			// Let other systems know that an actor was moved
			GEngine->BroadcastOnActorMoved(Proxy);
		}
	}
}

void ULandscapeInfo::FixupProxiesWeightmaps()
{
	if (LandscapeActor.IsValid())
	{
		LandscapeActor->WeightmapUsageMap.Empty();
		for (int32 CompIdx = 0; CompIdx < LandscapeActor->LandscapeComponents.Num(); ++CompIdx)
		{
			ULandscapeComponent* Comp = LandscapeActor->LandscapeComponents[CompIdx];
			if (Comp)
			{
				Comp->FixupWeightmaps();
			}
		}
	}

	for (auto It = Proxies.CreateConstIterator(); It; ++It)
	{
		ALandscapeProxy* Proxy = (*It);
		Proxy->WeightmapUsageMap.Empty();
		for (int32 CompIdx = 0; CompIdx < Proxy->LandscapeComponents.Num(); ++CompIdx)
		{
			ULandscapeComponent* Comp = Proxy->LandscapeComponents[CompIdx];
			if (Comp)
			{
				Comp->FixupWeightmaps();
			}
		}
	}
}

//
// This handles legacy behavior of landscapes created under world composition
// We adjust landscape section offsets and set a flag inside landscape that sections offset are static and should never be touched again
//
void AdjustLandscapeSectionOffsets(UWorld* InWorld, const TArray<ALandscapeProxy*> InLandscapeList)
{
	// We interested only in registered actors
	TArray<ALandscapeProxy*> RegisteredLandscapeList = InLandscapeList.FilterByPredicate([](ALandscapeProxy* Proxy) {
		return Proxy->GetRootComponent()->IsRegistered();
	});

	// Main Landscape actor should act as origin of global components grid
	int32 LandcapeIndex = RegisteredLandscapeList.IndexOfByPredicate([](ALandscapeProxy* Proxy) {
		return Proxy->IsA<ALandscape>();
	});
	ALandscapeProxy* StaticLandscape = RegisteredLandscapeList.IsValidIndex(LandcapeIndex) ? RegisteredLandscapeList[LandcapeIndex] : nullptr;

	if (StaticLandscape && !StaticLandscape->bStaticSectionOffset)
	{
		StaticLandscape->SetAbsoluteSectionBase(FIntPoint::ZeroValue);
	}

	// In case there is no main landscape actor loaded try use any landscape that already has static offsets
	if (StaticLandscape == nullptr)
	{
		LandcapeIndex = RegisteredLandscapeList.IndexOfByPredicate([](ALandscapeProxy* Proxy) {
			return Proxy->bStaticSectionOffset;
		});
		StaticLandscape = RegisteredLandscapeList.IsValidIndex(LandcapeIndex) ? RegisteredLandscapeList[LandcapeIndex] : nullptr;
		// Otherwise offsets will stay variable
	}

	ALandscapeProxy* OriginLandscape = StaticLandscape;

	for (ALandscapeProxy* Proxy : RegisteredLandscapeList)
	{
		if (OriginLandscape == nullptr)
		{
			OriginLandscape = Proxy;
		}
		else if (!Proxy->bStaticSectionOffset)
		{
			// Calculate section offset based on relative position from "origin" landscape
			FVector Offset = Proxy->GetActorLocation() - OriginLandscape->GetActorLocation();
			FVector	DrawScale = OriginLandscape->GetRootComponent()->RelativeScale3D;

			FIntPoint QuadsSpaceOffset;
			QuadsSpaceOffset.X = FMath::RoundToInt(Offset.X / DrawScale.X);
			QuadsSpaceOffset.Y = FMath::RoundToInt(Offset.Y / DrawScale.Y);
			Proxy->SetAbsoluteSectionBase(QuadsSpaceOffset + OriginLandscape->LandscapeSectionOffset);

			if (StaticLandscape)
			{
				Proxy->bStaticSectionOffset = true;
			}
		}
	}
}

void ULandscapeInfo::RecreateLandscapeInfo(UWorld* InWorld, bool bMapCheck)
{
	check(InWorld);

	auto& LandscapeInfoMap = GetLandscapeInfoMap(InWorld);

	// reset all LandscapeInfo objects
	for (auto It = LandscapeInfoMap.Map.CreateIterator(); It; ++It)
	{
		It.Value()->Modify();
		It.Value()->Reset();
	}

	TMap<FGuid, TArray<ALandscapeProxy*>> ValidLandscapesMap;
	// Gather all valid landscapes in the world
	for (TActorIterator<ALandscapeProxy> It(InWorld); It; ++It)
	{
		ALandscapeProxy* Proxy = *It;
		if (Proxy
			&& Proxy->GetLevel()
			&& Proxy->GetLevel()->bIsVisible
			&& Proxy->HasAnyFlags(RF_BeginDestroyed) == false
			&& Proxy->IsPendingKill() == false
			&& Proxy->IsPendingKillPending() == false)
		{
			ValidLandscapesMap.FindOrAdd(Proxy->GetLandscapeGuid()).Add(Proxy);
		}
	}

	// Handle legacy landscape data under world composition
	if (InWorld->WorldComposition)
	{
		for (auto It = ValidLandscapesMap.CreateIterator(); It; ++It)
		{
			AdjustLandscapeSectionOffsets(InWorld, It.Value());
		}
	}

	// Register landscapes in global landscape map
	for (auto It = ValidLandscapesMap.CreateIterator(); It; ++It)
	{
		auto& LandscapeList = It.Value();
		for (ALandscapeProxy* Proxy : LandscapeList)
		{
			Proxy->GetLandscapeInfo(true)->RegisterActor(Proxy, bMapCheck);
		}
	}

	// Remove empty entries from global LandscapeInfo map
	for (auto It = LandscapeInfoMap.Map.CreateIterator(); It; ++It)
	{
		if (It.Value()->GetLandscapeProxy() == NULL)
		{
			It.Value()->MarkPendingKill();
			It.RemoveCurrent();
		}
	}

	// Update layer info maps
	for (auto It = LandscapeInfoMap.Map.CreateIterator(); It; ++It)
	{
		ULandscapeInfo* LandscapeInfo = It.Value();
		if (LandscapeInfo)
		{
			LandscapeInfo->UpdateLayerInfoMap();
		}
	}

	// Update add collision
	for (auto It = LandscapeInfoMap.Map.CreateIterator(); It; ++It)
	{
		ULandscapeInfo* LandscapeInfo = It.Value();
		if (LandscapeInfo)
		{
			LandscapeInfo->UpdateAllAddCollisions();
		}
	}

	// We need to inform Landscape editor tools about LandscapeInfo updates
	FEditorSupportDelegates::WorldChange.Broadcast();
}


#endif

void ULandscapeComponent::PostInitProperties()
{
	Super::PostInitProperties();

	// Create a new guid in case this is a newly created component
	// If not, this guid will be overwritten when serialized
	FPlatformMisc::CreateGuid(StateId);
}

void ULandscapeComponent::PostDuplicate(bool bDuplicateForPIE)
{
	if (!bDuplicateForPIE)
	{
		// Reset the StateId on duplication since it needs to be unique for each capture.
		// PostDuplicate covers direct calls to StaticDuplicateObject, but not actor duplication (see PostEditImport)
		FPlatformMisc::CreateGuid(StateId);
	}
}

// Generate a new guid to force a recache of all landscape derived data
#define LANDSCAPE_FULL_DERIVEDDATA_VER			TEXT("016D326F3A954BBA9CCDFA00CEFA31E9")

FString FLandscapeComponentDerivedData::GetDDCKeyString(const FGuid& StateId)
{
	return FDerivedDataCacheInterface::BuildCacheKey(TEXT("LS_FULL"), LANDSCAPE_FULL_DERIVEDDATA_VER, *StateId.ToString());
}

void FLandscapeComponentDerivedData::InitializeFromUncompressedData(const TArray<uint8>& UncompressedData)
{
	int32 UncompressedSize = UncompressedData.Num() * UncompressedData.GetTypeSize();

	TArray<uint8> TempCompressedMemory;
	// Compressed can be slightly larger than uncompressed
	TempCompressedMemory.Empty(UncompressedSize * 4 / 3);
	TempCompressedMemory.AddUninitialized(UncompressedSize * 4 / 3);
	int32 CompressedSize = TempCompressedMemory.Num() * TempCompressedMemory.GetTypeSize();

	verify(FCompression::CompressMemory(
		(ECompressionFlags)(COMPRESS_ZLIB | COMPRESS_BiasMemory),
		TempCompressedMemory.GetData(),
		CompressedSize,
		UncompressedData.GetData(),
		UncompressedSize));

	// Note: change LANDSCAPE_FULL_DERIVEDDATA_VER when modifying the serialization layout
	FMemoryWriter FinalArchive(CompressedLandscapeData, true);
	FinalArchive << UncompressedSize;
	FinalArchive << CompressedSize;
	FinalArchive.Serialize(TempCompressedMemory.GetData(), CompressedSize);
}

void FLandscapeComponentDerivedData::GetUncompressedData(TArray<uint8>& OutUncompressedData)
{
	check(CompressedLandscapeData.Num() > 0);

	FMemoryReader Ar(CompressedLandscapeData);

	// Note: change LANDSCAPE_FULL_DERIVEDDATA_VER when modifying the serialization layout
	int32 UncompressedSize;
	Ar << UncompressedSize;

	int32 CompressedSize;
	Ar << CompressedSize;

	TArray<uint8> CompressedData;
	CompressedData.Empty(CompressedSize);
	CompressedData.AddUninitialized(CompressedSize);
	Ar.Serialize(CompressedData.GetData(), CompressedSize);

	OutUncompressedData.Empty(UncompressedSize);
	OutUncompressedData.AddUninitialized(UncompressedSize);

	verify(FCompression::UncompressMemory((ECompressionFlags)COMPRESS_ZLIB, OutUncompressedData.GetData(), UncompressedSize, CompressedData.GetData(), CompressedSize));

	if (FPlatformProperties::RequiresCookedData())
	{
		// free the compressed data now that we have the uncompressed version if running with cooked data
		CompressedLandscapeData.Empty();
	}
}

FArchive& operator<<(FArchive& Ar, FLandscapeComponentDerivedData& Data)
{
	check(!Ar.IsSaving() || Data.CompressedLandscapeData.Num() > 0);
	return Ar << Data.CompressedLandscapeData;
}

bool FLandscapeComponentDerivedData::LoadFromDDC(const FGuid& StateId)
{
	return GetDerivedDataCacheRef().GetSynchronous(*GetDDCKeyString(StateId), CompressedLandscapeData);
}

void FLandscapeComponentDerivedData::SaveToDDC(const FGuid& StateId)
{
	check(CompressedLandscapeData.Num() > 0);
	GetDerivedDataCacheRef().Put(*GetDDCKeyString(StateId), CompressedLandscapeData);
}

void WorldDestroyEventFunction(UWorld* World)
{
	World->PerModuleDataObjects.RemoveAll(
		[](UObject* Object)
	{
		return Object->IsA(ULandscapeInfoMap::StaticClass());
	}
	);
}

#if WITH_EDITORONLY_DATA

ULandscapeInfoMap& GetLandscapeInfoMap(UWorld* World)
{
	ULandscapeInfoMap *FoundObject = nullptr;

	World->PerModuleDataObjects.FindItemByClass(&FoundObject);

	checkf(FoundObject, TEXT("ULandscapInfoMap object was not created for this UWorld."));

	return *FoundObject;
}

#endif // WITH_EDITORONLY_DATA

void LandscapeMaterialsParameterValuesGetter(FStaticParameterSet &OutStaticParameterSet, UMaterialInstance* Material)
{
	if (Material->Parent)
	{
		UMaterial *ParentMaterial = Material->Parent->GetMaterial();

		TArray<FName> ParameterNames;
		TArray<FGuid> Guids;
		ParentMaterial->GetAllParameterNames<UMaterialExpressionLandscapeLayerWeight>(ParameterNames, Guids);
		ParentMaterial->GetAllParameterNames<UMaterialExpressionLandscapeLayerSwitch>(ParameterNames, Guids);
		ParentMaterial->GetAllParameterNames<UMaterialExpressionLandscapeLayerSample>(ParameterNames, Guids);
		ParentMaterial->GetAllParameterNames<UMaterialExpressionLandscapeLayerBlend>(ParameterNames, Guids);
		ParentMaterial->GetAllParameterNames<UMaterialExpressionLandscapeVisibilityMask>(ParameterNames, Guids);

		OutStaticParameterSet.TerrainLayerWeightParameters.AddZeroed(ParameterNames.Num());
		for (int32 ParameterIdx = 0; ParameterIdx < ParameterNames.Num(); ParameterIdx++)
		{
			FStaticTerrainLayerWeightParameter& ParentParameter = OutStaticParameterSet.TerrainLayerWeightParameters[ParameterIdx];
			FName ParameterName = ParameterNames[ParameterIdx];
			FGuid ExpressionId = Guids[ParameterIdx];
			int32 WeightmapIndex = INDEX_NONE;

			ParentParameter.bOverride = false;
			ParentParameter.ParameterName = ParameterName;
			//get the settings from the parent in the MIC chain
			if (Material->Parent->GetTerrainLayerWeightParameterValue(ParameterName, WeightmapIndex, ExpressionId))
			{
				ParentParameter.WeightmapIndex = WeightmapIndex;
			}
			ParentParameter.ExpressionGUID = ExpressionId;

			// if the SourceInstance is overriding this parameter, use its settings
			for (int32 WeightParamIdx = 0; WeightParamIdx < Material->GetStaticParameters().TerrainLayerWeightParameters.Num(); WeightParamIdx++)
			{
				const FStaticTerrainLayerWeightParameter &TerrainLayerWeightParam = Material->GetStaticParameters().TerrainLayerWeightParameters[WeightParamIdx];

				if (ParameterName == TerrainLayerWeightParam.ParameterName)
				{
					ParentParameter.bOverride = TerrainLayerWeightParam.bOverride;
					if (TerrainLayerWeightParam.bOverride)
					{
						ParentParameter.WeightmapIndex = TerrainLayerWeightParam.WeightmapIndex;
					}
				}
			}
		}
	}
}

bool LandscapeMaterialsParameterSetUpdater(FStaticParameterSet &StaticParameterSet, UMaterial* ParentMaterial)
{
	return UpdateParameterSet<FStaticTerrainLayerWeightParameter, UMaterialExpressionLandscapeLayerWeight>(StaticParameterSet.TerrainLayerWeightParameters, ParentMaterial);
}

void ALandscapeProxy::Tick(float DeltaSeconds)
{
	if (!IsPendingKillPending() && !HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_Unreachable | RF_PendingKill | RF_ClassDefaultObject | RF_AsyncLoading))
	{
		// this is NOT an actor tick, it is a FTickableGameObject tick
		// the super tick is for an actor tick...
		//Super::Tick(DeltaSeconds);

#if WITH_EDITOR
		UWorld* World = GetWorld();

		if (GIsEditor && World && !World->IsPlayInEditor())
		{
			UpdateBakedTextures();
		}
#endif

		TickGrass();
	}
}

ALandscapeProxy::~ALandscapeProxy()
{
	for (int32 Index = 0; Index < AsyncFoliageTasks.Num(); Index++)
	{
		FAsyncTask<FAsyncGrassTask>* Task = AsyncFoliageTasks[Index];
		Task->EnsureCompletion(true);
		FAsyncGrassTask& Inner = Task->GetTask();
		delete Task;
	}
	AsyncFoliageTasks.Empty();

#if WITH_EDITOR
	TotalComponentsNeedingGrassMapRender -= NumComponentsNeedingGrassMapRender;
	NumComponentsNeedingGrassMapRender = 0;
	TotalTexturesToStreamForVisibleGrassMapRender -= NumTexturesToStreamForVisibleGrassMapRender;
	NumTexturesToStreamForVisibleGrassMapRender = 0;
#endif
}
//
// ALandscapeMeshProxyActor
//
ALandscapeMeshProxyActor::ALandscapeMeshProxyActor(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bCanBeDamaged = false;

	LandscapeMeshProxyComponent = CreateDefaultSubobject<ULandscapeMeshProxyComponent>(TEXT("LandscapeMeshProxyComponent0"));
	LandscapeMeshProxyComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	LandscapeMeshProxyComponent->Mobility = EComponentMobility::Static;
	LandscapeMeshProxyComponent->bGenerateOverlapEvents = false;

	RootComponent = LandscapeMeshProxyComponent;
}

//
// ULandscapeMeshProxyComponent
//
ULandscapeMeshProxyComponent::ULandscapeMeshProxyComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

#if WITH_EDITOR

void ULandscapeComponent::SerializeStateHashes(FArchive& Ar)
{
	if (MaterialInstance)
	{
		Ar << MaterialInstance->GetMaterial()->StateId;
	}

	FGuid HeightmapGuid = HeightmapTexture->Source.GetId();
	Ar << HeightmapGuid;
	for (auto WeightmapTexture : WeightmapTextures)
	{
		FGuid WeightmapGuid = WeightmapTexture->Source.GetId();
		Ar << WeightmapGuid;
	}
}

void ALandscapeProxy::UpdateBakedTextures()
{
	// See if we can render
	UWorld* World = GetWorld();
	if (!GIsEditor || GUsingNullRHI || !World || World->IsGameWorld() || World->FeatureLevel < ERHIFeatureLevel::SM4)
	{
		return;
	}

	if (UpdateBakedTexturesCountdown-- > 0)
	{
		return;
	}

	// Check if we can want to generate landscape GI data
	static const auto DistanceFieldCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GenerateMeshDistanceFields"));
	static const auto LandscapeGICVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GenerateLandscapeGIData"));
	if (DistanceFieldCVar->GetValueOnGameThread() == 0 || LandscapeGICVar->GetValueOnGameThread() == 0)
	{
		// Clear out any existing GI textures
		for (ULandscapeComponent* Component : LandscapeComponents)
		{
			if (Component->GIBakedBaseColorTexture != nullptr)
			{
				Component->BakedTextureMaterialGuid.Invalidate();
				Component->GIBakedBaseColorTexture = nullptr;
				Component->MarkRenderStateDirty();
			}
		}

		// Don't check if we need to update anything for another 60 frames
		UpdateBakedTexturesCountdown = 60;

		return;
	}
	
	// Stores the components and their state hash data for a single atlas
	struct FBakedTextureSourceInfo
	{
		// pointer as FMemoryWriter caches the address of the FBufferArchive, and this struct could be relocated on a realloc.
		FBufferArchive* ComponentStateAr;
		TArray<ULandscapeComponent*> Components;

		FBakedTextureSourceInfo()
		{
			ComponentStateAr = new FBufferArchive();
		}
		~FBakedTextureSourceInfo()
		{
			delete ComponentStateAr;
		}
	};

	// Group components by heightmap texture
	TMap<UTexture2D*, FBakedTextureSourceInfo> ComponentsByHeightmap;
	for (ULandscapeComponent* Component : LandscapeComponents)
	{
		FBakedTextureSourceInfo& Info = ComponentsByHeightmap.FindOrAdd(Component->HeightmapTexture);
		Info.Components.Add(Component);
		Component->SerializeStateHashes(*Info.ComponentStateAr);
	}

	TotalComponentsNeedingTextureBaking -= NumComponentsNeedingTextureBaking;
	NumComponentsNeedingTextureBaking = 0;
	int32 NumGenerated = 0;

	for (auto It = ComponentsByHeightmap.CreateConstIterator(); It; ++It)
	{
		const FBakedTextureSourceInfo& Info = It.Value();

		bool bCanBake = true;
		for (ULandscapeComponent* Component : Info.Components)
		{
			// not registered; ignore this component
			if (!Component->SceneProxy)
			{
				continue;
			}

			// Check we can render the material
			UMaterialInstance* MaterialInstance = Component->MaterialInstance;
			if (!MaterialInstance)
			{
				// Cannot render this component yet as it doesn't have a material; abandon the atlas for this heightmap
				bCanBake = false;
				break;
			}

			FMaterialResource* MaterialResource = MaterialInstance->GetMaterialResource(World->FeatureLevel);
			if (!MaterialResource || !MaterialResource->HasValidGameThreadShaderMap())
			{
				// Cannot render this component yet as its shaders aren't compiled; abandon the atlas for this heightmap
				bCanBake = false;
				break;
			}
		}

		if (bCanBake)
		{
			// Calculate a combined Guid-like ID we can use for this component
			uint32 Hash[5];
			FSHA1::HashBuffer(Info.ComponentStateAr->GetData(), Info.ComponentStateAr->Num(), (uint8*)Hash);
			FGuid CombinedStateId = FGuid(Hash[0] ^ Hash[4], Hash[1], Hash[2], Hash[3]);

			bool bNeedsBake = false;
			for (ULandscapeComponent* Component : Info.Components)
			{
				if (Component->BakedTextureMaterialGuid != CombinedStateId)
				{
					bNeedsBake = true;
					break;
				}
			}
			
			if (bNeedsBake)
			{
				// We throttle, baking only one atlas per frame
				if (NumGenerated > 0)
				{
					NumComponentsNeedingTextureBaking += Info.Components.Num();
				}
				else
				{
					UTexture2D* HeightmapTexture = It.Key();
					// 1/8 the res of the heightmap
					FIntPoint AtlasSize(HeightmapTexture->GetSizeX() >> 3, HeightmapTexture->GetSizeY() >> 3);

					TArray<FColor> AtlasSamples;
					AtlasSamples.AddZeroed(AtlasSize.X * AtlasSize.Y);

					for (ULandscapeComponent* Component : Info.Components)
					{
						// not registered; ignore this component
						if (!Component->SceneProxy)
						{
							continue;
						}

						int32 ComponentSamples = (SubsectionSizeQuads + 1) * NumSubsections;
						check(FMath::IsPowerOfTwo(ComponentSamples));

						int32 BakeSize = ComponentSamples >> 3;
						TArray<FColor> Samples;
						if (FMaterialUtilities::ExportBaseColor(Component, BakeSize, Samples))
						{
							int32 AtlasOffsetX = FMath::RoundToInt(Component->HeightmapScaleBias.Z * (float)HeightmapTexture->GetSizeX()) >> 3;
							int32 AtlasOffsetY = FMath::RoundToInt(Component->HeightmapScaleBias.W * (float)HeightmapTexture->GetSizeY()) >> 3;
							for (int32 y = 0; y < BakeSize; y++)
							{
								FMemory::Memcpy(&AtlasSamples[(y + AtlasOffsetY)*AtlasSize.X + AtlasOffsetX], &Samples[y*BakeSize], sizeof(FColor)* BakeSize);
							}
							NumGenerated++;
						}
					}
					UTexture2D* AtlasTexture = FMaterialUtilities::CreateTexture(GetOutermost(), HeightmapTexture->GetName() + TEXT("_BaseColor"), AtlasSize, AtlasSamples, TC_Default, TEXTUREGROUP_World, RF_Public, true, CombinedStateId);
					AtlasTexture->MarkPackageDirty();

					for (ULandscapeComponent* Component : Info.Components)
					{
						Component->BakedTextureMaterialGuid = CombinedStateId;
						Component->GIBakedBaseColorTexture = AtlasTexture;
						Component->MarkRenderStateDirty();
					}
				}
			}
		}
	}

	TotalComponentsNeedingTextureBaking += NumComponentsNeedingTextureBaking;

	if (NumGenerated == 0)
	{
		// Don't check if we need to update anything for another 60 frames
		UpdateBakedTexturesCountdown = 60;
	}
}
#endif

void ALandscapeProxy::InvalidateGeneratedComponentData(const TSet<ULandscapeComponent*>& Components)
{
	TMap<ALandscapeProxy*, TSet<ULandscapeComponent*>> ByProxy;
	for (auto Component : Components)
	{
		Component->BakedTextureMaterialGuid.Invalidate();

		ByProxy.FindOrAdd(Component->GetLandscapeProxy()).Add(Component);
	}
	for (auto It = ByProxy.CreateConstIterator(); It; ++It)
	{
		It.Key()->FlushGrassComponents(&It.Value());
	}
}

#undef LOCTEXT_NAMESPACE
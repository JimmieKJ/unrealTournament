// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialInterface.cpp: UMaterialInterface implementation.
=============================================================================*/

#include "Materials/MaterialInterface.h"
#include "RenderingThread.h"
#include "PrimitiveViewRelevance.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Texture2D.h"
#include "Engine/SubsurfaceProfile.h"
#include "Engine/TextureStreamingTypes.h"
#include "Interfaces/ITargetPlatform.h"
#include "Components.h"

//////////////////////////////////////////////////////////////////////////

UEnum* UMaterialInterface::SamplerTypeEnum = nullptr;

//////////////////////////////////////////////////////////////////////////

/** Copies the material's relevance flags to a primitive's view relevance flags. */
void FMaterialRelevance::SetPrimitiveViewRelevance(FPrimitiveViewRelevance& OutViewRelevance) const
{
	OutViewRelevance.bOpaqueRelevance = bOpaque;
	OutViewRelevance.bMaskedRelevance = bMasked;
	OutViewRelevance.bDistortionRelevance = bDistortion;
	OutViewRelevance.bSeparateTranslucencyRelevance = bSeparateTranslucency;
	OutViewRelevance.bMobileSeparateTranslucencyRelevance = bMobileSeparateTranslucency;
	OutViewRelevance.bNormalTranslucencyRelevance = bNormalTranslucency;
	OutViewRelevance.ShadingModelMaskRelevance = ShadingModelMask;
	OutViewRelevance.bUsesGlobalDistanceField = bUsesGlobalDistanceField;
	OutViewRelevance.bUsesWorldPositionOffset = bUsesWorldPositionOffset;
	OutViewRelevance.bDecal = bDecal;
	OutViewRelevance.bTranslucentSurfaceLighting = bTranslucentSurfaceLighting;
	OutViewRelevance.bUsesSceneDepth = bUsesSceneDepth;
}

//////////////////////////////////////////////////////////////////////////

UMaterialInterface::UMaterialInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		InitDefaultMaterials();
		AssertDefaultMaterialsExist();

		if (SamplerTypeEnum == nullptr)
		{
			SamplerTypeEnum = FindObject<UEnum>(NULL, TEXT("/Script/Engine.EMaterialSamplerType"));
			check(SamplerTypeEnum);
		}

		SetLightingGuid();
	}
}

void UMaterialInterface::PostLoad()
{
	Super::PostLoad();
	PostLoadDefaultMaterials();
}

void UMaterialInterface::GetUsedTexturesAndIndices(TArray<UTexture*>& OutTextures, TArray< TArray<int32> >& OutIndices, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel) const
{
	GetUsedTextures(OutTextures, QualityLevel, false, FeatureLevel, false);
	OutIndices.AddDefaulted(OutTextures.Num());
}

FMaterialRelevance UMaterialInterface::GetRelevance_Internal(const UMaterial* Material, ERHIFeatureLevel::Type InFeatureLevel) const
{
	if(Material)
	{
		const FMaterialResource* MaterialResource = Material->GetMaterialResource(InFeatureLevel);
		const bool bIsTranslucent = IsTranslucentBlendMode((EBlendMode)GetBlendMode());

		EMaterialShadingModel ShadingModel = GetShadingModel();
		EMaterialDomain Domain = (EMaterialDomain)MaterialResource->GetMaterialDomain();
		bool bDecal = (Domain == MD_DeferredDecal);

		// Determine the material's view relevance.
		FMaterialRelevance MaterialRelevance;

		MaterialRelevance.ShadingModelMask = 1 << ShadingModel;

		if(bDecal)
		{
			MaterialRelevance.bDecal = bDecal;
			// we rely on FMaterialRelevance defaults are 0
		}
		else
		{
			MaterialRelevance.bOpaque = !bIsTranslucent;
			MaterialRelevance.bMasked = IsMasked();
			MaterialRelevance.bDistortion = MaterialResource->IsDistorted();
			MaterialRelevance.bSeparateTranslucency = bIsTranslucent && Material->bEnableSeparateTranslucency;
			MaterialRelevance.bMobileSeparateTranslucency = bIsTranslucent && Material->bEnableMobileSeparateTranslucency;
			MaterialRelevance.bNormalTranslucency = bIsTranslucent && !Material->bEnableSeparateTranslucency;
			MaterialRelevance.bDisableDepthTest = bIsTranslucent && Material->bDisableDepthTest;		
			MaterialRelevance.bOutputsVelocityInBasePass = Material->bOutputVelocityOnBasePass;	
			MaterialRelevance.bUsesGlobalDistanceField = MaterialResource->UsesGlobalDistanceField_GameThread();
			MaterialRelevance.bUsesWorldPositionOffset = MaterialResource->UsesWorldPositionOffset_GameThread();
			ETranslucencyLightingMode TranslucencyLightingMode = MaterialResource->GetTranslucencyLightingMode();
			MaterialRelevance.bTranslucentSurfaceLighting = bIsTranslucent && (TranslucencyLightingMode == TLM_SurfacePerPixelLighting || TranslucencyLightingMode == TLM_Surface);
			MaterialRelevance.bUsesSceneDepth = MaterialResource->MaterialUsesSceneDepthLookup_GameThread();
		}
		return MaterialRelevance;
	}
	else
	{
		return FMaterialRelevance();
	}
}

FMaterialRelevance UMaterialInterface::GetRelevance(ERHIFeatureLevel::Type InFeatureLevel) const
{
	// Find the interface's concrete material.
	const UMaterial* Material = GetMaterial();
	return GetRelevance_Internal(Material, InFeatureLevel);
}

FMaterialRelevance UMaterialInterface::GetRelevance_Concurrent(ERHIFeatureLevel::Type InFeatureLevel) const
{
	// Find the interface's concrete material.
	TMicRecursionGuard RecursionGuard;
	const UMaterial* Material = GetMaterial_Concurrent(RecursionGuard);
	return GetRelevance_Internal(Material, InFeatureLevel);
}

int32 UMaterialInterface::GetWidth() const
{
	return ME_PREV_THUMBNAIL_SZ+(ME_STD_BORDER*2);
}

int32 UMaterialInterface::GetHeight() const
{
	return ME_PREV_THUMBNAIL_SZ+ME_CAPTION_HEIGHT+(ME_STD_BORDER*2);
}


void UMaterialInterface::SetForceMipLevelsToBeResident( bool OverrideForceMiplevelsToBeResident, bool bForceMiplevelsToBeResidentValue, float ForceDuration, int32 CinematicTextureGroups )
{
	TArray<UTexture*> Textures;
	
	GetUsedTextures(Textures, EMaterialQualityLevel::Num, false, ERHIFeatureLevel::Num, true);
	for ( int32 TextureIndex=0; TextureIndex < Textures.Num(); ++TextureIndex )
	{
		UTexture2D* Texture = Cast<UTexture2D>(Textures[TextureIndex]);
		if ( Texture )
		{
			Texture->SetForceMipLevelsToBeResident( ForceDuration, CinematicTextureGroups );
			if (OverrideForceMiplevelsToBeResident)
			{
				Texture->bForceMiplevelsToBeResident = bForceMiplevelsToBeResidentValue;
			}
		}
	}
}

void UMaterialInterface::RecacheAllMaterialUniformExpressions()
{
	// For each interface, reacache its uniform parameters
	for( TObjectIterator<UMaterialInterface> MaterialIt; MaterialIt; ++MaterialIt )
	{
		MaterialIt->RecacheUniformExpressions();
	}
}

bool UMaterialInterface::IsReadyForFinishDestroy()
{
	bool bIsReady = Super::IsReadyForFinishDestroy();
	bIsReady = bIsReady && ParentRefFence.IsFenceComplete(); 
	return bIsReady;
}

void UMaterialInterface::BeginDestroy()
{
	ParentRefFence.BeginFence();

	Super::BeginDestroy();
}

void UMaterialInterface::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	SetLightingGuid();
}

#if WITH_EDITOR
void UMaterialInterface::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// flush the lighting guid on all changes
	SetLightingGuid();

	LightmassSettings.EmissiveBoost = FMath::Max(LightmassSettings.EmissiveBoost, 0.0f);
	LightmassSettings.DiffuseBoost = FMath::Max(LightmassSettings.DiffuseBoost, 0.0f);
	LightmassSettings.ExportResolutionScale = FMath::Clamp(LightmassSettings.ExportResolutionScale, 0.0f, 16.0f);

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UMaterialInterface::GetLightingGuidChain(bool bIncludeTextures, TArray<FGuid>& OutGuids) const
{
#if WITH_EDITORONLY_DATA
	OutGuids.Add(LightingGuid);
#endif // WITH_EDITORONLY_DATA
}

bool UMaterialInterface::GetVectorParameterValue(FName ParameterName, FLinearColor& OutValue) const
{
	// is never called but because our system wants a UMaterialInterface instance we cannot use "virtual =0"
	return false;
}

bool UMaterialInterface::GetScalarParameterValue(FName ParameterName, float& OutValue) const
{
	// is never called but because our system wants a UMaterialInterface instance we cannot use "virtual =0"
	return false;
}

bool UMaterialInterface::GetScalarCurveParameterValue(FName ParameterName, FInterpCurveFloat& OutValue) const
{
	return false;
}

bool UMaterialInterface::GetVectorCurveParameterValue(FName ParameterName, FInterpCurveVector& OutValue) const
{
	return false;
}

bool UMaterialInterface::GetLinearColorParameterValue(FName ParameterName, FLinearColor& OutValue) const
{
	return false;
}

bool UMaterialInterface::GetLinearColorCurveParameterValue(FName ParameterName, FInterpCurveLinearColor& OutValue) const
{
	return false;
}

bool UMaterialInterface::GetTextureParameterValue(FName ParameterName, UTexture*& OutValue) const
{
	return false;
}

bool UMaterialInterface::GetTextureParameterOverrideValue(FName ParameterName, UTexture*& OutValue) const
{
	return false;
}

bool UMaterialInterface::GetFontParameterValue(FName ParameterName,class UFont*& OutFontValue,int32& OutFontPage) const
{
	return false;
}

bool UMaterialInterface::GetRefractionSettings(float& OutBiasValue) const
{
	return false;
}

bool UMaterialInterface::GetParameterDesc(FName ParamaterName,FString& OutDesc) const
{
	return false;
}
bool UMaterialInterface::GetGroupName(FName ParamaterName,FName& OutDesc) const
{
	return false;
}

UMaterial* UMaterialInterface::GetBaseMaterial()
{
	return GetMaterial();
}

bool DoesMaterialUseTexture(const UMaterialInterface* Material,const UTexture* CheckTexture)
{
	//Do not care if we're running dedicated server
	if (FPlatformProperties::IsServerOnly())
	{
		return false;
	}

	TArray<UTexture*> Textures;
	Material->GetUsedTextures(Textures, EMaterialQualityLevel::Num, true, GMaxRHIFeatureLevel, true);
	for (int32 i = 0; i < Textures.Num(); i++)
	{
		if (Textures[i] == CheckTexture)
		{
			return true;
		}
	}
	return false;
}

float UMaterialInterface::GetOpacityMaskClipValue() const
{
	return 0.0f;
}

EBlendMode UMaterialInterface::GetBlendMode() const
{
	return BLEND_Opaque;
}

bool UMaterialInterface::IsTwoSided() const
{
	return false;
}

bool UMaterialInterface::IsDitheredLODTransition() const
{
	return false;
}

bool UMaterialInterface::IsTranslucencyWritingCustomDepth() const
{
	return false;
}

bool UMaterialInterface::IsMasked() const
{
	return false;
}

bool UMaterialInterface::IsDeferredDecal() const
{
	return false;
}

EMaterialShadingModel UMaterialInterface::GetShadingModel() const
{
	return MSM_DefaultLit;
}

USubsurfaceProfile* UMaterialInterface::GetSubsurfaceProfile_Internal() const
{
	return NULL;
}

void UMaterialInterface::SetFeatureLevelToCompile(ERHIFeatureLevel::Type FeatureLevel, bool bShouldCompile)
{
	uint32 FeatureLevelBit = (1 << FeatureLevel);
	if (bShouldCompile)
	{
		FeatureLevelsToForceCompile |= FeatureLevelBit;
	}
	else
	{
		FeatureLevelsToForceCompile &= (~FeatureLevelBit);
	}
}

uint32 UMaterialInterface::FeatureLevelsForAllMaterials = 0;

void UMaterialInterface::SetGlobalRequiredFeatureLevel(ERHIFeatureLevel::Type FeatureLevel, bool bShouldCompile)
{
	uint32 FeatureLevelBit = (1 << FeatureLevel);
	if (bShouldCompile)
	{
		FeatureLevelsForAllMaterials |= FeatureLevelBit;
	}
	else
	{
		FeatureLevelsForAllMaterials &= (~FeatureLevelBit);
	}
}


uint32 UMaterialInterface::GetFeatureLevelsToCompileForRendering() const
{
	return FeatureLevelsToForceCompile | GetFeatureLevelsToCompileForAllMaterials();
}


void UMaterialInterface::UpdateMaterialRenderProxy(FMaterialRenderProxy& Proxy)
{
	// no 0 pointer
	check(&Proxy);

	EMaterialShadingModel MaterialShadingModel = GetShadingModel();

	// for better performance we only update SubsurfaceProfileRT if the feature is used
	if (MaterialShadingModel == MSM_SubsurfaceProfile)
	{
		FSubsurfaceProfileStruct Settings;

		USubsurfaceProfile* LocalSubsurfaceProfile = GetSubsurfaceProfile_Internal();
		
		if (LocalSubsurfaceProfile)
		{
			Settings = LocalSubsurfaceProfile->Settings;
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
			UpdateMaterialRenderProxySubsurface,
			const FSubsurfaceProfileStruct, Settings, Settings,
			USubsurfaceProfile*, LocalSubsurfaceProfile, LocalSubsurfaceProfile,
			FMaterialRenderProxy&, Proxy, Proxy,
		{
			uint32 AllocationId = 0;

			if (LocalSubsurfaceProfile)
			{
				AllocationId = GSubsurfaceProfileTextureObject.AddOrUpdateProfile(Settings, LocalSubsurfaceProfile);

				check(AllocationId >= 0 && AllocationId <= 255);
			}
			Proxy.SetSubsurfaceProfileRT(LocalSubsurfaceProfile);
		});
	}
}

namespace Algo
{
	template<typename T, typename PredType>
	int32 BinarySearch(const TArray<T>& Container, PredType Pred)
	{
		int32 Min = 0; // Min is included
		int32 Max = Container.Num(); // Max is excluded

		while (Min != Max)
		{
			const int32 Curr = (Min + Max) / 2;
			const int32 Comp = Pred(Container[Curr]);
			if (Comp < 0) // Pred < Ele
			{
				Max = Curr;
			}
			else if (Comp > 0) // Pred > Ele
			{
				Min = Curr + 1;
			}
			else // Pred == Ele
			{
				return Curr;
			}
		}
		return INDEX_NONE;
	}
}

bool FMaterialTextureInfo::IsValid(bool bCheckTextureIndex) const
{ 
#if WITH_EDITORONLY_DATA
	if (bCheckTextureIndex && (TextureIndex < 0 || TextureIndex >= TEXSTREAM_MAX_NUM_TEXTURES_PER_MATERIAL))
	{
		return false;
	}
#endif
	return TextureName != NAME_None && SamplingScale > SMALL_NUMBER && UVChannelIndex >= 0 && UVChannelIndex < TEXSTREAM_MAX_NUM_UVCHANNELS; 
}

void UMaterialInterface::SortTextureStreamingData(bool bForceSort, bool bFinalSort)
{
#if WITH_EDITORONLY_DATA
	// In cook that was already done in the save.
	if (!FPlatformProperties::RequiresCookedData() && (!bTextureStreamingDataSorted || bForceSort))
	{
		for (int32 Index = 0; Index < TextureStreamingData.Num(); ++Index)
		{
			FMaterialTextureInfo& TextureData = TextureStreamingData[Index];
			UObject* Texture = TextureData.TextureReference.TryLoad();

			// In the final data it must also be a streaming texture, to make the data leaner.
			if (Texture && (!bFinalSort || IsStreamingTexture(Cast<UTexture2D>(Texture))))
			{
				TextureData.TextureName = Texture->GetFName();
			}
			else if (bFinalSort) // In the final sort we remove null names as they will never match.
			{
				TextureStreamingData.RemoveAtSwap(Index);
				--Index;
			}
			else
			{
				TextureData.TextureName = NAME_None;
			}
		}
		// Sort by name to be compatible with FindTextureStreamingDataIndexRange
		TextureStreamingData.Sort([](const FMaterialTextureInfo& Lhs, const FMaterialTextureInfo& Rhs) { return Lhs.TextureName < Rhs.TextureName; });
		bTextureStreamingDataSorted = true;
	}
#endif
}

extern 	TAutoConsoleVariable<int32> CVarStreamingUseMaterialData;

bool UMaterialInterface::FindTextureStreamingDataIndexRange(FName TextureName, int32& LowerIndex, int32& HigherIndex) const
{
	struct FNameSearch
	{
		FName Name;
		FNameSearch(FName InName) : Name(InName) {}
		FORCEINLINE int32 operator()(const FMaterialTextureInfo& Rhs) const { return Name.Compare(Rhs.TextureName); }
	};

#if WITH_EDITORONLY_DATA
	// Because of redirectors (when textures are renammed), the texture names might be invalid and we need to udpate the data at every load.
	// Normally we would do that in the post load, but since the process needs to resolve the StringAssetReference, this is forbidden at that place.
	// As a workaround, we do it on demand. Note that this is not required in cooked build as it is done in the presave.
	const_cast<UMaterialInterface*>(this)->SortTextureStreamingData(false, false);
#endif

#if !UE_BUILD_SHIPPING
	if (CVarStreamingUseMaterialData.GetValueOnGameThread() == 0)
	{
		return false;
	}
#endif

	const int32 MatchingIndex = Algo::BinarySearch(TextureStreamingData, FNameSearch(TextureName));
	if (MatchingIndex != INDEX_NONE)
	{
		// Find the range of entries for this texture. 
		// This is possible because the same texture could be bound to several register and also be used with different sampling UV.
		LowerIndex = MatchingIndex;
		HigherIndex = MatchingIndex;
		while (LowerIndex > 0 && TextureStreamingData[LowerIndex - 1].TextureName == TextureName)
		{
			--LowerIndex;
		}
		while (HigherIndex + 1 < TextureStreamingData.Num() && TextureStreamingData[HigherIndex + 1].TextureName == TextureName)
		{
			++HigherIndex;
		}
		return true;
	}
	return false;
}

void UMaterialInterface::SetTextureStreamingData(const TArray<FMaterialTextureInfo>& InTextureStreamingData)
{
	TextureStreamingData = InTextureStreamingData;
	SortTextureStreamingData(true, false);
}

float UMaterialInterface::GetTextureDensity(FName TextureName, const FMeshUVChannelInfo& UVChannelData) const
{
	ensure(UVChannelData.bInitialized);

	int32 LowerIndex = INDEX_NONE;
	int32 HigherIndex = INDEX_NONE;
	if (FindTextureStreamingDataIndexRange(TextureName, LowerIndex, HigherIndex))
	{
		// Compute the max, at least one entry will be valid. 
		float MaxDensity = 0;
		for (int32 Index = LowerIndex; Index <= HigherIndex; ++Index)
		{
			const FMaterialTextureInfo& MatchingData = TextureStreamingData[Index];
			ensure(MatchingData.IsValid() && MatchingData.TextureName == TextureName);
			MaxDensity = FMath::Max<float>(UVChannelData.LocalUVDensities[MatchingData.UVChannelIndex] / MatchingData.SamplingScale, MaxDensity);
		}
		return MaxDensity;
	}

	// Otherwise return 0 to indicate the data is not found.
	return 0;
}

bool UMaterialInterface::UseAnyStreamingTexture() const
{
	TArray<UTexture*> Textures;
	GetUsedTextures(Textures, EMaterialQualityLevel::Num, true, ERHIFeatureLevel::Num, true);

	for (UTexture* Texture : Textures)
	{
		if (IsStreamingTexture(Cast<UTexture2D>(Texture)))
		{
			return true;
		}
	}
	return false;
}

void UMaterialInterface::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);

	if (TargetPlatform && TargetPlatform->RequiresCookedData())
	{
		SortTextureStreamingData(true, true);
	}
}


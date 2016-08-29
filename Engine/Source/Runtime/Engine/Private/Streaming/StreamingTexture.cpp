// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
StreamingTexture.cpp: Definitions of classes used for texture.
=============================================================================*/

#include "EnginePrivate.h"
#include "StreamingTexture.h"
#include "StreamingManagerTexture.h"
#include "Engine/LightMapTexture2D.h"
#include "Engine/ShadowMapTexture2D.h"

FStreamingTexture::FStreamingTexture(UTexture2D* InTexture, const int32 NumStreamedMips[TEXTUREGROUP_MAX], const FTextureStreamingSettings& Settings)
: Texture(InTexture)
{
	UpdateStaticData(Settings);
	UpdateDynamicData(NumStreamedMips, Settings);

	InstanceRemovedTimestamp = -FLT_MAX;
	LastRenderTimeRefCountTimestamp = -FLT_MAX;
	LastRenderTimeRefCount = 0;
	DynamicBoostFactor = 1.f;

	bForceFullyLoadHeuristic = false;
	NumMissingMips = 0;
	bLooksLowRes = false;
	VisibleWantedMips = MinAllowedMips;
	HiddenWantedMips = MinAllowedMips;
	RetentionPriority = 0;
	BudgetedMips = MinAllowedMips;
	LoadOrderPriority = 0;
	WantedMips = MinAllowedMips;
}

void FStreamingTexture::UpdateStaticData(const FTextureStreamingSettings& Settings)
{
	if (Texture)
	{
		LODGroup = (TextureGroup)Texture->LODGroup;
		NumNonStreamingMips = Texture->GetNumNonStreamingMips();
		MipCount = Texture->GetNumMips();
		BudgetMipBias = 0;
		BoostFactor = GetExtraBoost(LODGroup, Settings);

		bIsCharacterTexture = (LODGroup == TEXTUREGROUP_Character || LODGroup == TEXTUREGROUP_CharacterSpecular || LODGroup == TEXTUREGROUP_CharacterNormalMap);
		bIsTerrainTexture = (LODGroup == TEXTUREGROUP_Terrain_Heightmap || LODGroup == TEXTUREGROUP_Terrain_Weightmap);

		for (int32 MipIndex=0; MipIndex < MAX_TEXTURE_MIP_COUNT; ++MipIndex)
		{
			TextureSizes[MipIndex] = Texture->CalcTextureMemorySize(FMath::Min(MipIndex + 1, MipCount));
		}
	}
	else
	{
		LODGroup = TEXTUREGROUP_World;
		NumNonStreamingMips = 0;
		MipCount = 0;
		BudgetMipBias = 0;
		BoostFactor = 1.f;

		bIsCharacterTexture = false;
		bIsTerrainTexture = false;

		for (int32 MipIndex=0; MipIndex < MAX_TEXTURE_MIP_COUNT; ++MipIndex)
		{
			TextureSizes[MipIndex] = 0;
		}
	}
}

void FStreamingTexture::UpdateDynamicData(const int32 NumStreamedMips[TEXTUREGROUP_MAX], const FTextureStreamingSettings& Settings)
{
	// Note that those values are read from the async task and must not be assigned temporary values!!
	if (Texture)
	{
		UpdateStreamingStatus();

		// The last render time of this texture. Can be FLT_MAX when texture has no resource.
		const float LastRenderTimeForTexture = Texture->GetLastRenderTimeForStreaming();
		LastRenderTime = (FApp::GetCurrentTime() > LastRenderTimeForTexture) ? float( FApp::GetCurrentTime() - LastRenderTimeForTexture ) : 0.0f;

		bForceFullyLoad = Texture->ShouldMipLevelsBeForcedResident() || LODGroup == TEXTUREGROUP_Skybox || (LODGroup == TEXTUREGROUP_HierarchicalLOD && Settings.HLODStrategy == 2);

		const int32 NumCinematicMipLevels = (bForceFullyLoad && Texture->bUseCinematicMipLevels) ? Texture->NumCinematicMipLevels : 0;

		int32 LODBias = 0;
		if (!Settings.bUseAllMips)
		{
			LODBias = FMath::Max<int32>(Texture->GetCachedLODBias() - NumCinematicMipLevels, 0);

			// Reduce the max allowed resolution according to LODBias if the texture group allows it.
			if (IsMaxResolutionAffectedByGlobalBias())
			{
				LODBias += Settings.GlobalMipBiasAsInt();
			}

			LODBias += BudgetMipBias;
		}

		// The max mip count is affected by the texture bias and cinematic bias settings.
		MaxAllowedMips = FMath::Clamp<int32>(FMath::Min<int32>(MipCount - LODBias, GMaxTextureMipCount), NumNonStreamingMips, MipCount);

		if (LODGroup == TEXTUREGROUP_HierarchicalLOD && Settings.HLODStrategy == 1)
		{
			MinAllowedMips = FMath::Clamp<int32>(MaxAllowedMips - 1, NumNonStreamingMips, MaxAllowedMips);
		}
		else if (NumStreamedMips[LODGroup] > 0)
		{
			MinAllowedMips = FMath::Clamp<int32>(MipCount - NumStreamedMips[LODGroup], NumNonStreamingMips, MaxAllowedMips);
		}
		else
		{
			MinAllowedMips = NumNonStreamingMips;
		}
	}
	else
	{
		bReadyForStreaming = false;
		bInFlight = false;
		bCancelRequestAttempted = false;
		bForceFullyLoad = false;
		ResidentMips = 0;
		RequestedMips = 0;
		MinAllowedMips = 0;
		MaxAllowedMips = 0;
		LastRenderTime = FLT_MAX;	
	}
}

void FStreamingTexture::UpdateStreamingStatus()
{
	if (Texture)
	{
		bReadyForStreaming = Texture->IsReadyForStreaming();
		if (bReadyForStreaming)
		{
			bInFlight = Texture->UpdateStreamingStatus(true);
		}
		else
		{
			ensure(Texture->PendingMipChangeRequestStatus.GetValue() <= TexState_ReadyFor_Requests); // Could also be TexState_InProgress_Initialization
			bInFlight = false;
		}

		 // Allow a future cancel request only once the texture has returned to a fully ready state.
		if (bReadyForStreaming && !bInFlight)
		{
			bCancelRequestAttempted = false;
		}


		// This must be updated after UpdateStreamingStatus
		ResidentMips = Texture->ResidentMips;
		RequestedMips = Texture->RequestedMips;

		ensure(bInFlight || ResidentMips == RequestedMips);
	}
	else
	{
		bReadyForStreaming = false;
		bInFlight = false;
		bCancelRequestAttempted = false;
	}
}

float FStreamingTexture::GetExtraBoost(TextureGroup	LODGroup, const FTextureStreamingSettings& Settings)
{
	// When accurate distance computation, we need to relax the distance otherwhise it get's too conservative. (ex 513 goes to 1024)
	const float DistanceScale = Settings.bUseNewMetrics ? .71f : 1.f;

	if (LODGroup == TEXTUREGROUP_Terrain_Heightmap || LODGroup == TEXTUREGROUP_Terrain_Weightmap) 
	{
		// Terrain are not affected by any kind of scale. Important since instance can use hardcoded resolution.
		// Used the Distance Scale from the new metrics is not big enough to affect which mip gets selected.
		return DistanceScale;
	}
	else if (LODGroup == TEXTUREGROUP_Lightmap)
	{
		return FMath::Min<float>(DistanceScale, GLightmapStreamingFactor);
	}
	else if (LODGroup == TEXTUREGROUP_Shadowmap)
	{
		return FMath::Min<float>(DistanceScale, GShadowmapStreamingFactor);
	}
	else
	{
		return DistanceScale * Settings.GlobalMipBiasAsScale();
	}
}

int32 FStreamingTexture::GetWantedMipsFromSize(float Size) const
{
	float WantedMipsFloat = 1.0f + FMath::Log2(FMath::Max(1.f, Size));
	int32 WantedMipsInt = FMath::CeilToInt(WantedMipsFloat);
	return FMath::Clamp<int32>(WantedMipsInt, MinAllowedMips, MaxAllowedMips);
}

/** Set the wanted mips from the async task data */
void FStreamingTexture::SetPerfectWantedMips_Async(float MaxSize, float MaxSize_VisibleOnly, bool InLooksLowRes, const FTextureStreamingSettings& Settings)
{
	bForceFullyLoadHeuristic = (MaxSize == FLT_MAX || MaxSize_VisibleOnly == FLT_MAX);
	VisibleWantedMips = GetWantedMipsFromSize(MaxSize_VisibleOnly);
	bLooksLowRes = InLooksLowRes; // Things like lightmaps, HLOD and close instances.

	// Terrain, Forced Fully Load and Things that already look bad are not affected by hidden scale.
	if (bIsTerrainTexture || bForceFullyLoadHeuristic || bLooksLowRes)
	{
		HiddenWantedMips = GetWantedMipsFromSize(MaxSize);
		NumMissingMips = 0; // No impact for terrains as there are not allowed to drop mips.
	}
	else
	{
		HiddenWantedMips = GetWantedMipsFromSize(MaxSize * Settings.HiddenPrimitiveScale);
		// NumMissingMips contains the number of mips not loaded because of HiddenPrimitiveScale. When out of budget, those texture will be considered as already sacrificed.
		NumMissingMips = FMath::Max<int32>(GetWantedMipsFromSize(MaxSize) - FMath::Max<int32>(VisibleWantedMips, HiddenWantedMips), 0);
	}
}

/**
 * Once the wanted mips are computed, the async task will check if everything fits in the budget.
 *  This only consider the highest mip that will be requested eventually, so that slip requests are stables.
 */
int64 FStreamingTexture::UpdateRetentionPriority_Async()
{
	// Reserve the budget for the max mip that will be loaded eventually (ignore the effect of split requests)
	BudgetedMips = GetPerfectWantedMips();
	RetentionPriority = 0;

	if (Texture)
	{
		const bool bShouldKeep = bIsTerrainTexture || bForceFullyLoadHeuristic || bLooksLowRes;
		const bool bIsSmall   = GetSize(BudgetedMips) <= 200 * 1024; 
		const bool bIsVisible = VisibleWantedMips >= HiddenWantedMips; // Whether the first mip dropped would be a visible mip or not.

		// Here we try to have a minimal amount of priority flags for last render time to be meaningless.
		// We mostly want thing not seen from a long time to go first to avoid repeating load / unload patterns.

		if (bShouldKeep)						RetentionPriority += 1024; // Keep forced fully load as much as possible.
		if (bIsVisible)							RetentionPriority += 512; // Keep visible things as much as possible.
		if (bIsCharacterTexture || bIsSmall)	RetentionPriority += 256; // Try to keep character of small texture as they don't pay off.
		if (!bIsVisible)						RetentionPriority += FMath::Clamp<int32>(255 - (int32)LastRenderTime, 1, 255); // Keep last visible first.

		return GetSize(BudgetedMips);
	}
	else
	{
		return 0;
	}
}

int64 FStreamingTexture::DropMaxResolution_Async(int32 NumDroppedMips)
{
	if (Texture)
	{
		// Don't drop bellow min allowed mips. Also ensure that MinAllowedMips < MaxAllowedMips in order allow the BudgetMipBias to reset.
		NumDroppedMips = FMath::Min<int32>(MaxAllowedMips - MinAllowedMips - 1, NumDroppedMips);

		if (NumDroppedMips > 0)
		{
			// Decrease MaxAllowedMips and increase BudgetMipBias (as it should include it)
			MaxAllowedMips -= NumDroppedMips;
			BudgetMipBias += NumDroppedMips;

			if (BudgetedMips > MaxAllowedMips)
			{
				const int64 FreedMemory = GetSize(BudgetedMips) - GetSize(MaxAllowedMips);

				BudgetedMips = MaxAllowedMips;
				VisibleWantedMips = FMath::Min<int32>(VisibleWantedMips, MaxAllowedMips);
				HiddenWantedMips = FMath::Min<int32>(HiddenWantedMips, MaxAllowedMips);

				return FreedMemory;
			}
		}
		else // If we can't reduce resolution, still drop a mip if possible to free memory (eventhough it won't be persistent)
		{
			return DropOneMip_Async();
		}
	}
	return 0;
}

int64 FStreamingTexture::DropOneMip_Async()
{
	if (Texture && BudgetedMips > MinAllowedMips)
	{
		--BudgetedMips;
		return GetSize(BudgetedMips + 1) - GetSize(BudgetedMips);
	}
	else
	{
		return 0;
	}
}

int64 FStreamingTexture::KeepOneMip_Async()
{
	if (Texture && BudgetedMips < FMath::Min<int32>(ResidentMips, MaxAllowedMips))
	{
		++BudgetedMips;
		return GetSize(BudgetedMips) - GetSize(BudgetedMips - 1);
	}
	else
	{
		return 0;
	}
}

bool FStreamingTexture::UpdateLoadOrderPriority_Async()
{
	LoadOrderPriority = 0;
	WantedMips = BudgetedMips;

	// If the entry is valid and we need to send a new request to load/drop the right mip.
	if (bReadyForStreaming && Texture && WantedMips != RequestedMips)
	{
		const bool bIsVisible			= WantedMips <= VisibleWantedMips; // Otherwise it means we are loading mips that are only usefull for non visible primitives.
		const bool bMustLoadFirst		= bForceFullyLoadHeuristic || bIsTerrainTexture || bIsCharacterTexture;
		const bool bMipIsImportant		= WantedMips - ResidentMips > (bLooksLowRes ? 1 : 2);

		if (bIsVisible)				LoadOrderPriority += 1024;
		if (bMustLoadFirst)			LoadOrderPriority += 512; 
		if (bMipIsImportant)		LoadOrderPriority += 256;
		if (!bIsVisible)			LoadOrderPriority += FMath::Clamp<int32>(255 - (int32)LastRenderTime, 1, 255);

		return true;
	}
	else
	{
		return false;
	}
}

void FStreamingTexture::CancelPendingMipChangeRequest()
{
	if (Texture && bReadyForStreaming && bInFlight && !bCancelRequestAttempted)
	{
		// Try to cancel at most once per request.
		bCancelRequestAttempted = true;
		Texture->CancelPendingMipChangeRequest();
	}
}

void FStreamingTexture::StreamWantedMips(FStreamingManagerTexture& Manager)
{
	if (Texture && bReadyForStreaming && !bInFlight && WantedMips != ResidentMips)
	{
		ensure(ResidentMips == RequestedMips);

		if (Texture->PendingMipChangeRequestStatus.GetValue() == TexState_ReadyFor_Requests)
		{
			Texture->RequestedMips = WantedMips;

			FTexture2DResource* Texture2DResource = (FTexture2DResource*)Texture->Resource;
			Texture2DResource->BeginUpdateMipCount( (bForceFullyLoadHeuristic || bIsTerrainTexture || bIsCharacterTexture) && WantedMips <= VisibleWantedMips );

			UpdateStreamingStatus();

			TrackTextureEvent( this, Texture, bForceFullyLoadHeuristic != 0, &Manager );
		}
		else
		{
			// Did UpdateStreamingTextures() miss a texture? Should never happen!
			UE_LOG(LogContentStreaming, Warning, TEXT("BeginUpdateMipCount failure! PendingMipChangeRequestStatus == %d, Resident=%d, Requested=%d, Wanted=%d"), Texture->PendingMipChangeRequestStatus.GetValue(), Texture->ResidentMips, Texture->RequestedMips, WantedMips );
		}
	}
}

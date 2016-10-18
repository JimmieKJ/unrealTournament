// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
StreamingTexture.h: Definitions of classes used for texture streaming.
=============================================================================*/

#pragma once

#include "TextureStreamingHelpers.h"

/*-----------------------------------------------------------------------------
	FStreamingTexture, the streaming system's version of UTexture2D.
-----------------------------------------------------------------------------*/

/** Self-contained structure to manage a streaming texture, possibly on a separate thread. */
struct FStreamingTexture
{
	FStreamingTexture(UTexture2D* InTexture, const int32 NumStreamedMips[TEXTUREGROUP_MAX], const FTextureStreamingSettings& Settings);

	/** Update data that should not change unless changing settings. */
	void UpdateStaticData(const FTextureStreamingSettings& Settings);

	/** Update data that the engine could change through gameplay. */
	void UpdateDynamicData(const int32 NumStreamedMips[TEXTUREGROUP_MAX], const FTextureStreamingSettings& Settings);

	/** Lightweight version of UpdateDynamicData. */
	void UpdateStreamingStatus();

	/**
	 * Returns the amount of memory used by the texture given a specified number of mip-maps, in bytes.
	 *
	 * @param MipCount	Number of mip-maps to account for
	 * @return			Total amount of memory used for the specified mip-maps, in bytes
	 */
	int32 GetSize( int32 InMipCount ) const
	{
		check(InMipCount >= 0 && InMipCount <= MAX_TEXTURE_MIP_COUNT);
		return TextureSizes[ InMipCount - 1 ];
	}

	static float GetExtraBoost(TextureGroup	LODGroup, const FTextureStreamingSettings& Settings);

	FORCEINLINE int32 GetWantedMipsFromSize(float Size) const;

	/** Set the wanted mips from the async task data */
	void SetPerfectWantedMips_Async(float MaxSize, float MaxSize_VisibleOnly, bool InLooksLowRes, const FTextureStreamingSettings& Settings);

	/** Init BudgetedMip and update RetentionPriority. Returns the size that would be taken if all budgeted mips where loaded. */
	int64 UpdateRetentionPriority_Async();

	/** Reduce the maximum allowed resolution by 1 mip. Return the size freed by doing so. */
	int64 DropMaxResolution_Async(int32 NumDroppedMips);

	/** Reduce BudgetedMip by 1 and return the size freed by doing so. */
	int64 DropOneMip_Async();

	/** Increase BudgetedMip by 1, up to resident mips, and return the size taken. */
	int64 KeepOneMip_Async();

	float GetMaxAllowedSize() { return (float)(0x1 << (MaxAllowedMips - 1)); }

	/** Init load order. Return wether this texture has any load/unload request */
	bool UpdateLoadOrderPriority_Async();
	
	void CancelPendingMipChangeRequest();
	void StreamWantedMips(FStreamingManagerTexture& Manager);

	FORCEINLINE int32 GetPerfectWantedMips() const { return FMath::Max<int32>(VisibleWantedMips,  HiddenWantedMips); }

	// Whether this texture can be affected by Global Bias and Budget Bias per texture.
	FORCEINLINE bool IsMaxResolutionAffectedByGlobalBias() const { return LODGroup != TEXTUREGROUP_HierarchicalLOD && !bIsTerrainTexture; }

	/***************************************************************
	 * Members initialized when this is constructed => NEVER CHANGES
	 ***************************************************************/

	/** Texture to manage. Note that this becomes null when the texture is removed. */
	UTexture2D*		Texture;

	/** Cached texture group. */
	TextureGroup	LODGroup;

	/** Cached number of mipmaps that are not allowed to stream. */
	int32			NumNonStreamingMips;

	/** Cached number of mip-maps in the UTexture2D mip array (including the base mip) */
	int32			MipCount;

	/** Sum of all boost factors that applies to this texture. */
	float			BoostFactor;

	/** Cached memory sizes for each possible mipcount. */
	int32			TextureSizes[MAX_TEXTURE_MIP_COUNT + 1];

	/** Whether the texture is ready to be streamed in/out (cached from IsReadyForStreaming()). */
	uint32			bIsCharacterTexture : 1;

	/** Whether the texture should be forcibly fully loaded. */
	uint32			bIsTerrainTexture : 1;


	/***********************************************************************************
	 * Cached dynamic members that need constant update => UPDATES IN UpdateCachedInfo()
	 ***********************************************************************************/

	/** Whether the texture is ready to be streamed in/out (cached from IsReadyForStreaming()). */
	uint32			bReadyForStreaming : 1;

	/** Whether the texture should be forcibly fully loaded. */
	uint32			bForceFullyLoad : 1;

	/** Cached number of mip-maps in memory (including the base mip) */
	int32			ResidentMips;
	/** Min number of mip-maps requested by the streaming system. */
	int32			RequestedMips;

	/** Min mip to be requested by the streaming  */
	int32			MinAllowedMips;
	/** Max mip to be requested by the streaming  */
	int32			MaxAllowedMips;

	/** How much game time has elapsed since the texture was bound for rendering. Based on FApp::GetCurrentTime(). */
	float			LastRenderTime;

	/*****************************************************************************************
	 * Helper data set by the streamer to handle special cases => CHANGES ANYTIME (gamethread)
	 *****************************************************************************************/

	/** Whether the texture is currently being streamed in/out. */
	uint32			bInFlight : 1;

	/** Whether an attemp to cancel the inflight request has been attempted. */
	uint32			bCancelRequestAttempted : 1;

	/** If non-zero, the most recent time an instance location was removed for this texture. */
	double			InstanceRemovedTimestamp;

	/** Set to FApp::GetCurrentTime() every time LastRenderTimeRefCount is modified. */
	double			LastRenderTimeRefCountTimestamp;

	/** Current number of instances that need LRT heuristics for this texture. */
	int32			LastRenderTimeRefCount;

	/** Extra gameplay boost factor. Reset after every update. */
	float			DynamicBoostFactor;

	/****************************************************************
	 * Data generated by the async task. CHANGES ANYTIME (taskthread)
	 ****************************************************************/
	
	/** Same as force fully load, but takes into account component settings. */
	uint32			bForceFullyLoadHeuristic : 1; 

	/** Same as force fully load, but takes into account component settings. */
	uint32			bLooksLowRes : 1; 

	/** How many mips are missing to satisfy ideal quality because of max size limitation. Used to prevent sacrificing mips that are already visually sacrificed. */
	int32			NumMissingMips;

	/** Max wanted mips for visible instances.*/
	int32			VisibleWantedMips;

	/** Wanted mips for non visible instances.*/
	int32			HiddenWantedMips;

	/** Retention priority used to sacrifice mips when out of budget. */
	int32			RetentionPriority;
	/** The max allowed mips (based on Visible and Hidden wanted mips) in order to fit in budget. */
	int32			BudgetedMips;

	/** The load request priority. */
	int32			LoadOrderPriority;	
	/** The mip that will be requested. Note, that some texture are loaded using split requests, so that the first request can be smaller than budgeted mip. */
	int32			WantedMips;

	/** A persistent bias applied to this texture. Increase whenever the streamer needs to make sacrifices to fit in budget */
	int32			BudgetMipBias;
};

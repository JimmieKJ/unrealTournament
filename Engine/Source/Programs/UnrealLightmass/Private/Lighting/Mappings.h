// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LockFreeList.h"

namespace Lightmass
{

/** A mapping between world-space surfaces and a static lighting cache. */
class FStaticLightingMapping : public virtual FRefCountedObject, public FStaticLightingMappingData
{
public:
	/** The mesh associated with the mapping, guaranteed to be valid (non-NULL) after import. */
	class FStaticLightingMesh* Mesh;

	/** Whether the mapping has been processed. */
	volatile int32 bProcessed;

	/** If true, the mapping is being padded */
	bool bPadded;

	/** A static indicating that debug borders should be used around padded mappings. */
	static bool s_bShowLightmapBorders;

protected:

	/** The irradiance photons which are cached on this mapping */
	TArray<const class FIrradiancePhoton*> CachedIrradiancePhotons;

	/** Approximate direct lighting ached on this mapping, used by final gather rays. */
	TArray<FLinearColor> CachedDirectLighting;

public:

 	/** Initialization constructor. */
	FStaticLightingMapping() :
		  bProcessed(false)
		, bPadded(false)
	{
	}

	/** Virtual destructor. */
	virtual ~FStaticLightingMapping() {}

	/** @return If the mapping is a texture mapping, returns a pointer to this mapping as a texture mapping.  Otherwise, returns NULL. */
	virtual class FStaticLightingTextureMapping* GetTextureMapping() 
	{
		return NULL;
	}

	virtual const FStaticLightingTextureMapping* GetTextureMapping() const
	{
		return NULL;
	}

	/**
	 * Returns the relative processing cost used to sort tasks from slowest to fastest.
	 *
	 * @return	relative processing cost or 0 if unknown
	 */
	virtual float GetProcessingCost() const
	{
		return 0;
	}

	/** Accesses a cached photon at the given vertex, if one exists. */
	virtual const class FIrradiancePhoton* GetCachedIrradiancePhoton(
		int32 VertexIndex,
		const struct FStaticLightingVertex& Vertex, 
		const class FStaticLightingSystem& System, 
		bool bDebugThisLookup, 
		FLinearColor& OutDirectLighting) const = 0;

	uint32 GetIrradiancePhotonCacheBytes() const { return CachedIrradiancePhotons.GetAllocatedSize(); }

	virtual void Import( class FLightmassImporter& Importer );

	friend class FStaticLightingSystem;
};

/** A mapping between world-space surfaces and static lighting cache textures. */
class FStaticLightingTextureMapping : public FStaticLightingMapping, public FStaticLightingTextureMappingData
{
public:

	FStaticLightingTextureMapping() : 
		NumOutstandingCacheTasks(0),
		NumOutstandingInterpolationTasks(0)
	{}

	// FStaticLightingMapping interface.
	virtual FStaticLightingTextureMapping* GetTextureMapping()
	{
		return this;
	}

	virtual const FStaticLightingTextureMapping* GetTextureMapping() const
	{
		return this;
	}

	/**
	 * Returns the relative processing cost used to sort tasks from slowest to fastest.
	 *
	 * @return	relative processing cost or 0 if unknown
	 */
	virtual float GetProcessingCost() const
	{
		return SizeX * SizeY;
	}

	/** Accesses a cached photon at the given vertex, if one exists. */
	virtual const class FIrradiancePhoton* GetCachedIrradiancePhoton(
		int32 VertexIndex,
		const FStaticLightingVertex& Vertex, 
		const FStaticLightingSystem& System, 
		bool bDebugThisLookup, 
		FLinearColor& OutDirectLighting) const;

	virtual void Import( class FLightmassImporter& Importer );

	/** The padded size of the mapping */
	int32 CachedSizeX;
	int32 CachedSizeY;

	/** The sizes that CachedIrradiancePhotons were stored with */
	int32 IrradiancePhotonCacheSizeX;
	int32 IrradiancePhotonCacheSizeY;

	/** Counts how many cache tasks this mapping needs completed. */
	volatile int32 NumOutstandingCacheTasks;

	/** List of completed cache tasks for this mapping. */
	FLockFreeVoidPointerListBase CompletedCacheIndirectLightingTasks;

	/** Counts how many interpolation tasks this mapping needs completed. */
	volatile int32 NumOutstandingInterpolationTasks;

	/** List of completed interpolation tasks for this mapping. */
	FLockFreeVoidPointerListBase CompletedInterpolationTasks;
};
 
} //namespace Lightmass

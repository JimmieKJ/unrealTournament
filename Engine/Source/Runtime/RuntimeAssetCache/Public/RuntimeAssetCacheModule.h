// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Modules/ModuleInterface.h"

/** Forward declarations. */
class FRuntimeAssetCacheInterface;

RUNTIMEASSETCACHE_API FRuntimeAssetCacheInterface& GetRuntimeAssetCache();

/**
* Module for the RuntimeAssetCache.
*/
class FRuntimeAssetCacheModuleInterface : public IModuleInterface
{
public:
	/**
	 * Gets runtime asset cache.
	 * @return Runtime asset cache.
	 */
	virtual FRuntimeAssetCacheInterface& GetRuntimeAssetCache() = 0;
};

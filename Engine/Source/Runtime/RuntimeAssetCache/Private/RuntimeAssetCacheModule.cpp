// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "RuntimeAssetCachePrivatePCH.h"
#include "RuntimeAssetCacheModule.h"
#include "RuntimeAssetCache.h"
#include "ModuleManager.h"
#include "EngineMinimal.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(RuntimeAssetCache)

FRuntimeAssetCacheInterface& GetRuntimeAssetCache()
{
	static FRuntimeAssetCacheModuleInterface& Module = FModuleManager::LoadModuleChecked<FRuntimeAssetCacheModuleInterface>("RuntimeAssetCache");
	static FRuntimeAssetCacheInterface& Interface = Module.GetRuntimeAssetCache();
	return Interface;
}

/**
 * Class implementing RuntimeAssetCache module interface. This is split
 * to solve circular dependency when building UHT.
 */
class FRuntimeAssetCacheModule : public FRuntimeAssetCacheModuleInterface
{
public:
	/**
	* Gets runtime asset cache.
	* @return Runtime asset cache.
	*/
	virtual FRuntimeAssetCacheInterface& GetRuntimeAssetCache() override
	{
		static FRuntimeAssetCache RuntimeAssetCache = FRuntimeAssetCache();
		return RuntimeAssetCache;
	}

	void StartupModule() override
	{
		FWorldDelegates::OnWorldTickStart.AddStatic(&FRuntimeAssetCacheModule::TickRuntimeAssetCache);
	}

	static void TickRuntimeAssetCache(ELevelTick TickType, float DeltaSeconds)
	{
		static FRuntimeAssetCacheModuleInterface& Module = FModuleManager::LoadModuleChecked<FRuntimeAssetCacheModuleInterface>("RuntimeAssetCache");
		static FRuntimeAssetCacheInterface& Interface = Module.GetRuntimeAssetCache();
		Interface.Tick();
	}
};

IMPLEMENT_MODULE(FRuntimeAssetCacheModule, RuntimeAssetCache);

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "FoliagePrivate.h"
#include "FoliageInstanceBase.h"
#include "InstancedFoliage.h"
#include "Engine/WorldComposition.h"

#if WITH_EDITORONLY_DATA

FFoliageInstanceBaseInfo::FFoliageInstanceBaseInfo()
	: CachedLocation(FVector::ZeroVector)
	, CachedRotation(FRotator::ZeroRotator)
	, CachedDrawScale(1.0f, 1.0f, 1.0f)
{}

FFoliageInstanceBaseInfo::FFoliageInstanceBaseInfo(UActorComponent* InComponent)
	: BasePtr(InComponent)
	, CachedLocation(FVector::ZeroVector)
	, CachedRotation(FRotator::ZeroRotator)
	, CachedDrawScale(1.0f, 1.0f, 1.0f)
{
	UpdateLocationFromComponent(InComponent);
}

void FFoliageInstanceBaseInfo::UpdateLocationFromComponent(UActorComponent* InComponent)
{
	if (InComponent)
	{
		AActor* Owner = Cast<AActor>(InComponent->GetOuter());
		if (Owner)
		{
			const USceneComponent* RootComponent = Owner->GetRootComponent();
			if (RootComponent)
			{
				CachedLocation = RootComponent->RelativeLocation;
				CachedRotation = RootComponent->RelativeRotation;
				CachedDrawScale = RootComponent->RelativeScale3D;
			}
		}
	}
}

FArchive& operator << (FArchive& Ar, FFoliageInstanceBaseInfo& BaseInfo)
{
	Ar << BaseInfo.BasePtr;
	Ar << BaseInfo.CachedLocation;
	Ar << BaseInfo.CachedRotation;
	Ar << BaseInfo.CachedDrawScale;

	return Ar;
}

//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////
FFoliageInstanceBaseId FFoliageInstanceBaseCache::InvalidBaseId = INDEX_NONE;

FFoliageInstanceBaseCache::FFoliageInstanceBaseCache()
	: NextBaseId(1)
{
}

FArchive& operator << (FArchive& Ar, FFoliageInstanceBaseCache& InstanceBaseCache)
{
	Ar << InstanceBaseCache.NextBaseId;
	Ar << InstanceBaseCache.InstanceBaseMap;
	Ar << InstanceBaseCache.InstanceBaseLevelMap;

	if (Ar.IsTransacting())
	{
		Ar << InstanceBaseCache.InstanceBaseInvMap;
	}
	else if (!Ar.IsSaving())
	{
		InstanceBaseCache.InstanceBaseInvMap.Empty();
		for (const auto& Pair : InstanceBaseCache.InstanceBaseMap)
		{
			const FFoliageInstanceBaseInfo& BaseInfo = Pair.Value;
			InstanceBaseCache.InstanceBaseInvMap.Add(BaseInfo.BasePtr, Pair.Key);
		}
	}

	return Ar;
}

FFoliageInstanceBaseId FFoliageInstanceBaseCache::AddInstanceBaseId(UActorComponent* InComponent)
{
	FFoliageInstanceBaseId BaseId = FFoliageInstanceBaseCache::InvalidBaseId;
	if (InComponent && !InComponent->IsCreatedByConstructionScript())
	{
		BaseId = GetInstanceBaseId(InComponent);
		if (BaseId == FFoliageInstanceBaseCache::InvalidBaseId)
		{
			BaseId = NextBaseId++;

			FFoliageInstanceBaseInfo BaseInfo(InComponent);
			InstanceBaseMap.Add(BaseId, BaseInfo);
			InstanceBaseInvMap.Add(BaseInfo.BasePtr, BaseId);

			check(InstanceBaseMap.Num() == InstanceBaseInvMap.Num());

			ULevel* ComponentLevel = InComponent->GetComponentLevel();
			if (ComponentLevel)
			{
				UWorld* ComponentWorld = Cast<UWorld>(ComponentLevel->GetOuter());
				if (ComponentWorld)
				{
					auto WorldKey = TAssetPtr<UWorld>(ComponentWorld);
					InstanceBaseLevelMap.FindOrAdd(WorldKey).Add(BaseInfo.BasePtr);
				}
			}
		}
	}

	return BaseId;
}

FFoliageInstanceBaseId FFoliageInstanceBaseCache::GetInstanceBaseId(UActorComponent* InComponent) const
{
	FFoliageInstanceBasePtr BasePtr = InComponent;
	return GetInstanceBaseId(BasePtr);
}

FFoliageInstanceBaseId FFoliageInstanceBaseCache::GetInstanceBaseId(const FFoliageInstanceBasePtr& BasePtr) const
{
	const FFoliageInstanceBaseId* BaseId = InstanceBaseInvMap.Find(BasePtr);
	if (!BaseId)
	{
		return FFoliageInstanceBaseCache::InvalidBaseId;
	}
	
	return *BaseId;
}

FFoliageInstanceBasePtr FFoliageInstanceBaseCache::GetInstanceBasePtr(FFoliageInstanceBaseId BaseId) const
{
	return GetInstanceBaseInfo(BaseId).BasePtr;
}

FFoliageInstanceBaseInfo FFoliageInstanceBaseCache::GetInstanceBaseInfo(FFoliageInstanceBaseId BaseId) const
{
	return InstanceBaseMap.FindRef(BaseId);
}

FFoliageInstanceBaseInfo FFoliageInstanceBaseCache::UpdateInstanceBaseInfoTransform(UActorComponent* InComponent)
{
	auto BaseId = GetInstanceBaseId(InComponent);
	if (BaseId != FFoliageInstanceBaseCache::InvalidBaseId)
	{
		auto* BaseInfo = InstanceBaseMap.Find(BaseId);
		check(BaseInfo);
		BaseInfo->UpdateLocationFromComponent(InComponent);
		return *BaseInfo;
	}

	return FFoliageInstanceBaseInfo();
}

void FFoliageInstanceBaseCache::CompactInstanceBaseCache(AInstancedFoliageActor* IFA)
{
	UWorld* World = IFA->GetWorld();
	if (!World || World->IsGameWorld())
	{
		return;
	}

	FFoliageInstanceBaseCache& Cache = IFA->InstanceBaseCache;
	
	TSet<FFoliageInstanceBaseId> BasesInUse;
	for (auto& FoliageMeshPair : IFA->FoliageMeshes)
	{
		for (const auto& Pair : FoliageMeshPair.Value->ComponentHash)
		{
			if (Pair.Key != FFoliageInstanceBaseCache::InvalidBaseId)
			{
				BasesInUse.Add(Pair.Key);
			}
		}
	}
	
	// Look for any removed maps
	TSet<FFoliageInstanceBasePtr> InvalidBasePtrs;
	for (auto& Pair : Cache.InstanceBaseLevelMap)
	{
		const auto& WorldAsset = Pair.Key;
		
		bool bExists = (WorldAsset == World);
		// Check sub-levels
		if (!bExists)
		{
			const FName PackageName = FName(*FPackageName::ObjectPathToPackageName(WorldAsset.ToStringReference().ToString()));
			if (World->WorldComposition)
			{
				bExists = World->WorldComposition->DoesTileExists(PackageName);
			}
			else
			{
				bExists = (World->GetLevelStreamingForPackageName(PackageName) != nullptr);
			}
		}

		if (!bExists)
		{
			InvalidBasePtrs.Append(Pair.Value);
			Cache.InstanceBaseLevelMap.Remove(Pair.Key);
		}
		else
		{
			// Remove dead links
			for (int32 i = Pair.Value.Num()-1; i >= 0; --i)
			{
				// Base needs to be removed if it's not in use by existing instances or component was removed
				if (Pair.Value[i].IsNull() || !BasesInUse.Contains(Cache.GetInstanceBaseId(Pair.Value[i])))
				{
					InvalidBasePtrs.Add(Pair.Value[i]);
					Pair.Value.RemoveAt(i);
				}
			}

			if (Pair.Value.Num() == 0)
			{
				Cache.InstanceBaseLevelMap.Remove(Pair.Key);
			}
		}
	}
	
	TSet<FFoliageInstanceBaseId> InvalidBaseIds;
	Cache.InstanceBaseInvMap.Empty();
	// Look for any removed base components
	for (const auto& Pair : Cache.InstanceBaseMap)
	{
		const FFoliageInstanceBaseInfo& BaseInfo = Pair.Value;
		if (InvalidBasePtrs.Contains(BaseInfo.BasePtr))
		{
			InvalidBaseIds.Add(Pair.Key);
			Cache.InstanceBaseMap.Remove(Pair.Key);
		}
		else
		{
			// Regenerate inverse map
			check(!Cache.InstanceBaseInvMap.Contains(BaseInfo.BasePtr));
			Cache.InstanceBaseInvMap.Add(BaseInfo.BasePtr, Pair.Key);
		}
	}

	if (InvalidBaseIds.Num())
	{
		for (auto& Pair : IFA->FoliageMeshes)
		{
			auto& MeshInfo = Pair.Value;
			MeshInfo->ComponentHash.Empty();
			int32 InstanceIdx = 0;
			
			for (FFoliageInstance& Instance : MeshInfo->Instances)
			{
				if (InvalidBaseIds.Contains(Instance.BaseId))
				{
					Instance.BaseId = FFoliageInstanceBaseCache::InvalidBaseId;
				}

				MeshInfo->ComponentHash.FindOrAdd(Instance.BaseId).Add(InstanceIdx);
				InstanceIdx++;
			}
		}

		Cache.InstanceBaseMap.Compact();
		Cache.InstanceBaseLevelMap.Compact();
	}
}

void FFoliageInstanceBaseCache::UpdateInstanceBaseCachedTransforms()
{
	for (auto& Pair : InstanceBaseMap)
	{
		FFoliageInstanceBaseInfo& BaseInfo = Pair.Value;
		BaseInfo.UpdateLocationFromComponent(BaseInfo.BasePtr.Get());
	}
}

#endif//WITH_EDITORONLY_DATA

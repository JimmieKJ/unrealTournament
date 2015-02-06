// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	InstancedFoliage.h: Instanced foliage type definitions.
  =============================================================================*/

#pragma once

//
// Forward declarations.
//
class UInstancedStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class AInstancedFoliageActor;
class UFoliageType;
struct FFoliageInstanceHash;

/**
 * Flags stored with each instance
 */
enum EFoliageInstanceFlags
{
	FOLIAGE_AlignToNormal	= 0x00000001,
	FOLIAGE_NoRandomYaw		= 0x00000002,
	FOLIAGE_Readjusted		= 0x00000004,
	FOLIAGE_InstanceDeleted	= 0x00000008,	// Used only for migration from pre-HierarchicalISM foliage.
};

/**
 *	FFoliageInstancePlacementInfo - placement info an individual instance
 */
struct FFoliageInstancePlacementInfo
{
	FVector Location;
	FRotator Rotation;
	FRotator PreAlignRotation;
	FVector DrawScale3D;
	float ZOffset;
	uint32 Flags;

	FFoliageInstancePlacementInfo()
		: Location(0.f, 0.f, 0.f)
		, Rotation(0, 0, 0)
		, PreAlignRotation(0, 0, 0)
		, DrawScale3D(1.f, 1.f, 1.f)
		, ZOffset(0.f)
		, Flags(0)
	{}
};

/**
 *	FFoliageInstance - editor info an individual instance
 */
struct FFoliageInstance : public FFoliageInstancePlacementInfo
{
	UActorComponent* Base;

	FFoliageInstance()
	: Base(NULL)
	{}


	friend FArchive& operator<<(FArchive& Ar, FFoliageInstance& Instance);

	FTransform GetInstanceWorldTransform() const
	{
		return FTransform(Rotation, Location, DrawScale3D);
	}

	void AlignToNormal(const FVector& InNormal, float AlignMaxAngle = 0.f)
	{
		Flags |= FOLIAGE_AlignToNormal;

		FRotator AlignRotation = InNormal.Rotation();
		// Static meshes are authored along the vertical axis rather than the X axis, so we add 90 degrees to the static mesh's Pitch.
		AlignRotation.Pitch -= 90.f;
		// Clamp its value inside +/- one rotation
		AlignRotation.Pitch = FRotator::NormalizeAxis(AlignRotation.Pitch);

		// limit the maximum pitch angle if it's > 0.
		if (AlignMaxAngle > 0.f)
		{
			int32 MaxPitch = AlignMaxAngle;
			if (AlignRotation.Pitch > MaxPitch)
			{
				AlignRotation.Pitch = MaxPitch;
			}
			else if (AlignRotation.Pitch < -MaxPitch)
			{
				AlignRotation.Pitch = -MaxPitch;
			}
		}

		PreAlignRotation = Rotation;
		Rotation = FRotator(FQuat(AlignRotation) * FQuat(Rotation));
	}
};

/**
 * FFoliageComponentHashInfo
 * Cached instance list and component location info stored in the ComponentHash.
 * Used for moving quick updates after operations on components with foliage painted on them.
 */
struct FFoliageComponentHashInfo
{
	// tors
	FFoliageComponentHashInfo()
		: CachedLocation(0, 0, 0)
		, CachedRotation(0, 0, 0)
		, CachedDrawScale(1, 1, 1)
	{}

	FFoliageComponentHashInfo(UActorComponent* InComponent)
		: CachedLocation(0, 0, 0)
		, CachedRotation(0, 0, 0)
		, CachedDrawScale(1, 1, 1)
	{
		UpdateLocationFromActor(InComponent);
	}

	// Cache the location and rotation from the actor
	void UpdateLocationFromActor(UActorComponent* InComponent)
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

	// serializer
	friend FArchive& operator<<(FArchive& Ar, FFoliageComponentHashInfo& ComponentHashInfo);

	FVector CachedLocation;
	FRotator CachedRotation;
	FVector CachedDrawScale;
	TSet<int32> Instances;
};

/**
 *	FFoliageMeshInfo - editor info for all matching foliage meshes
 */
struct FFoliageMeshInfo
{
	UHierarchicalInstancedStaticMeshComponent* Component;

#if WITH_EDITORONLY_DATA
	// Allows us to detect if FoliageType was updated while this level wasn't loaded
	FGuid FoliageTypeUpdateGuid;

	// Editor-only placed instances
	TArray<FFoliageInstance> Instances;

	// Transient, editor-only locality hash of instances
	TUniquePtr<FFoliageInstanceHash> InstanceHash;

	// Transient, editor-only set of instances per component
	TMap<UActorComponent*, FFoliageComponentHashInfo> ComponentHash;

	// Transient, editor-only list of selected instances.
	TSet<int32> SelectedIndices;
#endif

	ENGINE_API FFoliageMeshInfo();

	~FFoliageMeshInfo() // =default
	{ }

	FFoliageMeshInfo(FFoliageMeshInfo&& Other)
		// even VC++2013 doesn't support "=default" on move constructors
		: Component(Other.Component)
#if WITH_EDITORONLY_DATA
		, FoliageTypeUpdateGuid(MoveTemp(Other.FoliageTypeUpdateGuid))
		, Instances(MoveTemp(Other.Instances))
		, InstanceHash(MoveTemp(Other.InstanceHash))
		, ComponentHash(MoveTemp(Other.ComponentHash))
		, SelectedIndices(MoveTemp(Other.SelectedIndices))
#endif
	{ }

	FFoliageMeshInfo& operator=(FFoliageMeshInfo&& Other)
		// even VC++2013 doesn't support "=default" on move assignment
	{
		Component = Other.Component;
#if WITH_EDITORONLY_DATA
		FoliageTypeUpdateGuid = MoveTemp(Other.FoliageTypeUpdateGuid);
		Instances = MoveTemp(Other.Instances);
		InstanceHash = MoveTemp(Other.InstanceHash);
		ComponentHash = MoveTemp(Other.ComponentHash);
		SelectedIndices = MoveTemp(Other.SelectedIndices);
#endif

		return *this;
	}

#if WITH_EDITOR
	ENGINE_API void AddInstance(AInstancedFoliageActor* InIFA, UFoliageType* InSettings, const FFoliageInstance& InNewInstance);
	ENGINE_API void RemoveInstances(AInstancedFoliageActor* InIFA, const TArray<int32>& InInstancesToRemove);
	ENGINE_API void PreMoveInstances(AInstancedFoliageActor* InIFA, const TArray<int32>& InInstancesToMove);
	ENGINE_API void PostMoveInstances(AInstancedFoliageActor* InIFA, const TArray<int32>& InInstancesMoved);
	ENGINE_API void PostUpdateInstances(AInstancedFoliageActor* InIFA, const TArray<int32>& InInstancesUpdated, bool bReAddToHash = false);
	ENGINE_API void DuplicateInstances(AInstancedFoliageActor* InIFA, UFoliageType* InSettings, const TArray<int32>& InInstancesToDuplicate);
	ENGINE_API void GetInstancesInsideSphere(const FSphere& Sphere, TArray<int32>& OutInstances);
	ENGINE_API bool CheckForOverlappingSphere(const FSphere& Sphere);
	ENGINE_API bool CheckForOverlappingInstanceExcluding(int32 TestInstanceIdx, float Radius, TSet<int32>& ExcludeInstances);

	// Destroy existing clusters and reassign all instances to new clusters
	ENGINE_API void ReallocateClusters(AInstancedFoliageActor* InIFA, UFoliageType* InSettings);

	ENGINE_API void ReapplyInstancesToComponent();

	ENGINE_API void SelectInstances(AInstancedFoliageActor* InIFA, bool bSelect, TArray<int32>& Instances);

	// Get the number of placed instances
	ENGINE_API int32 GetInstanceCount() const;

	// For debugging. Validate state after editing.
	void CheckValid();
#endif

	friend FArchive& operator<<(FArchive& Ar, FFoliageMeshInfo& MeshInfo);

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	FFoliageMeshInfo(const FFoliageMeshInfo& Other) = delete;
	const FFoliageMeshInfo& operator=(const FFoliageMeshInfo& Other) = delete;
#else
private:
	FFoliageMeshInfo(const FFoliageMeshInfo& Other);
	const FFoliageMeshInfo& operator=(const FFoliageMeshInfo& Other);
#endif
};

//
// FFoliageInstanceHash
//

#define FOLIAGE_HASH_CELL_BITS 9	// 512x512 grid

struct FFoliageInstanceHash
{
private:
	const int32 HashCellBits;
	TMultiMap<uint64, int32> CellMap;

	uint64 MakeKey(int32 CellX, int32 CellY)
	{
		return ((uint64)(*(uint32*)(&CellX)) << 32) | (*(uint32*)(&CellY) & 0xffffffff);
	}

	uint64 MakeKey(const FVector& Location)
	{
		return  MakeKey(FMath::FloorToInt(Location.X) >> HashCellBits, FMath::FloorToInt(Location.Y) >> HashCellBits);
	}

	// Locality map
	//TMap<uint64, TSet<int32>> CellMap;
public:
	FFoliageInstanceHash(int32 InHashCellBits = FOLIAGE_HASH_CELL_BITS)
	:	HashCellBits(InHashCellBits)
	{}

	void InsertInstance(const FVector& InstanceLocation, int32 InstanceIndex)
	{
		uint64 Key = MakeKey(InstanceLocation);

		CellMap.AddUnique(Key, InstanceIndex);
	}

	void RemoveInstance(const FVector& InstanceLocation, int32 InstanceIndex)
	{
		uint64 Key = MakeKey(InstanceLocation);
		
		int32 RemoveCount = CellMap.RemoveSingle(Key, InstanceIndex);
		check(RemoveCount == 1);
	}

	void GetInstancesOverlappingBox(const FBox& InBox, TArray<int32>& OutInstanceIndices)
	{
		int32 MinX = FMath::FloorToInt(InBox.Min.X) >> HashCellBits;
		int32 MinY = FMath::FloorToInt(InBox.Min.Y) >> HashCellBits;
		int32 MaxX = FMath::FloorToInt(InBox.Max.X) >> HashCellBits;
		int32 MaxY = FMath::FloorToInt(InBox.Max.Y) >> HashCellBits;

		for (int32 y = MinY; y <= MaxY; y++)
		{
			for (int32 x = MinX; x <= MaxX; x++)
			{
				uint64 Key = MakeKey(x, y);
				CellMap.MultiFind(Key, OutInstanceIndices);
			}
		}
	}

	TArray<int32> GetInstancesOverlappingBox(const FBox& InBox)
	{
		TArray<int32> Result;
		GetInstancesOverlappingBox(InBox, Result);
		return Result;
	}

#if UE_BUILD_DEBUG
	void CheckInstanceCount(int32 InCount)
	{
		check(CellMap.Num() == InCount);
	}
#endif

	void Empty()
	{
		CellMap.Empty();
	}

	friend FArchive& operator<<(FArchive& Ar, FFoliageInstanceHash& Hash)
	{
		if (Ar.UE4Ver() < VER_UE4_FOLIAGE_SETTINGS_TYPE)
		{
			Hash.CellMap.Reset();

			TMap<uint64, TSet<int32>> OldCellMap;
			Ar << OldCellMap;
			for (auto& CellPair : OldCellMap)
			{
				for (int32 Idx : CellPair.Value)
				{
					Hash.CellMap.AddUnique(CellPair.Key, Idx);
				}
			}
		}
		else
		{
			Ar << Hash.CellMap;
		}

		return Ar;
	}
};


/** InstancedStaticMeshInstance hit proxy */
struct HInstancedStaticMeshInstance : public HHitProxy
{
	UInstancedStaticMeshComponent* Component;
	int32 InstanceIndex;

	DECLARE_HIT_PROXY(ENGINE_API);
	HInstancedStaticMeshInstance(UInstancedStaticMeshComponent* InComponent, int32 InInstanceIndex) : HHitProxy(HPP_World), Component(InComponent), InstanceIndex(InInstanceIndex) {}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	virtual EMouseCursor::Type GetMouseCursor()
	{
		return EMouseCursor::CardinalCross;
	}
};

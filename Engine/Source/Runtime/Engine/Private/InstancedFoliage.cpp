// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	InstancedFoliage.cpp: Instanced foliage implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "StaticMeshResources.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "MapErrors.h"
#include "Foliage/InstancedFoliageActor.h"
#include "Foliage/FoliageType_InstancedStaticMesh.h"
#include "Components/ModelComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Serialization/CustomVersion.h"

#define LOCTEXT_NAMESPACE "InstancedFoliage"

#define DO_FOLIAGE_CHECK			0			// whether to validate foliage data during editing.
#define FOLIAGE_CHECK_TRANSFORM		0			// whether to compare transforms between render and painting data.


DEFINE_LOG_CATEGORY_STATIC(LogInstancedFoliage, Log, All);

IMPLEMENT_HIT_PROXY(HInstancedStaticMeshInstance, HHitProxy);

// Custom serialization version for all packages containing Instance Foliage
struct FFoliageCustomVersion
{
	enum Type
	{
		// Before any version changes were made in the plugin
		BeforeCustomVersionWasAdded = 0,
		// Converted to use HierarchicalInstancedStaticMeshComponent
		FoliageUsingHierarchicalISMC = 1,
		// Changed Component to not RF_Transactional
		HierarchicalISMCNonTransactional = 2,
		// Added FoliageTypeUpdateGuid
		AddedFoliageTypeUpdateGuid = 3,
		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FFoliageCustomVersion() {}
};

const FGuid FFoliageCustomVersion::GUID(0x430C4D19, 0x71544970, 0x87699B69, 0xDF90B0E5);
// Register the custom version with core
FCustomVersionRegistration GRegisterFoliageCustomVersion(FFoliageCustomVersion::GUID, FFoliageCustomVersion::LatestVersion, TEXT("FoliageVer"));


//
// Serializers for struct data
//

FArchive& operator<<(FArchive& Ar, FFoliageInstance& Instance)
{
	Ar << Instance.Base;
	Ar << Instance.Location;
	Ar << Instance.Rotation;
	Ar << Instance.DrawScale3D;

	if (Ar.CustomVer(FFoliageCustomVersion::GUID) < FFoliageCustomVersion::FoliageUsingHierarchicalISMC)
	{
		int32 OldClusterIndex;
		Ar << OldClusterIndex;
		Ar << Instance.PreAlignRotation;
		Ar << Instance.Flags;

		if (OldClusterIndex == INDEX_NONE)
		{
			// When converting, we need to skip over any instance that was previously deleted but still in the Instances array.
			Instance.Flags |= FOLIAGE_InstanceDeleted;
		}
	}
	else
	{
		Ar << Instance.PreAlignRotation;
		Ar << Instance.Flags;
	}
	
	Ar << Instance.ZOffset;

	return Ar;
}

/**
*	FFoliageInstanceCluster_Deprecated
*/
struct FFoliageInstanceCluster_Deprecated
{
	UInstancedStaticMeshComponent* ClusterComponent;
	FBoxSphereBounds Bounds;

#if WITH_EDITORONLY_DATA
	TArray<int32> InstanceIndices;	// index into editor editor Instances array
#endif

	friend FArchive& operator<<(FArchive& Ar, FFoliageInstanceCluster_Deprecated& OldCluster)
	{
		check(Ar.CustomVer(FFoliageCustomVersion::GUID) < FFoliageCustomVersion::FoliageUsingHierarchicalISMC);

		Ar << OldCluster.Bounds;
		Ar << OldCluster.ClusterComponent;

#if WITH_EDITORONLY_DATA
		if (!Ar.ArIsFilterEditorOnly ||
			Ar.UE4Ver() < VER_UE4_FOLIAGE_SETTINGS_TYPE)
		{
			Ar << OldCluster.InstanceIndices;
		}
#endif

		return Ar;
	}
};

FArchive& operator<<(FArchive& Ar, FFoliageMeshInfo& MeshInfo)
{
	if (Ar.CustomVer(FFoliageCustomVersion::GUID) >= FFoliageCustomVersion::FoliageUsingHierarchicalISMC)
	{
		Ar << MeshInfo.Component;
	}
	else
	{
		TArray<FFoliageInstanceCluster_Deprecated> OldInstanceClusters;
		Ar << OldInstanceClusters;
	}
	
#if WITH_EDITORONLY_DATA
	if ((!Ar.ArIsFilterEditorOnly || Ar.UE4Ver() < VER_UE4_FOLIAGE_SETTINGS_TYPE) && 
		(!(Ar.GetPortFlags() & PPF_DuplicateForPIE)))
	{
		Ar << MeshInfo.Instances;
	}

	if (!Ar.ArIsFilterEditorOnly && Ar.CustomVer(FFoliageCustomVersion::GUID) >= FFoliageCustomVersion::AddedFoliageTypeUpdateGuid)
	{
		Ar << MeshInfo.FoliageTypeUpdateGuid;
	}

	// Serialize the transient data for undo.
	if (Ar.IsTransacting())
	{
		Ar << *MeshInfo.InstanceHash;
		Ar << MeshInfo.ComponentHash;
		Ar << MeshInfo.SelectedIndices;
	}
#endif

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FFoliageComponentHashInfo& ComponentHashInfo)
{
	return Ar << ComponentHashInfo.CachedLocation << ComponentHashInfo.CachedRotation << ComponentHashInfo.CachedDrawScale << ComponentHashInfo.Instances;
}

//
// UFoliageType
//

UFoliageType::UFoliageType(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Density = 100.0f;
	Radius = 0.0f;
	AlignToNormal = true;
	RandomYaw = true;
	UniformScale = true;
	ScaleMinX = 1.0f;
	ScaleMinY = 1.0f;
	ScaleMinZ = 1.0f;
	ScaleMaxX = 1.0f;
	ScaleMaxY = 1.0f;
	ScaleMaxZ = 1.0f;
	AlignMaxAngle = 0.0f;
	RandomPitchAngle = 0.0f;
	GroundSlope = 45.0f;
	HeightMin = -262144.0f;
	HeightMax = 262144.0f;
	ZOffsetMin = 0.0f;
	ZOffsetMax = 0.0f;
	LandscapeLayer = NAME_None;
	DisplayOrder = 0;
	IsSelected = false;
	ShowNothing = false;
	ShowPaintSettings = true;
	ShowInstanceSettings = false;
	ReapplyDensityAmount = 1.0f;
	CollisionWithWorld = false;
	CollisionScale = FVector(0.9f, 0.9f, 0.9f);
	VertexColorMask = FOLIAGEVERTEXCOLORMASK_Disabled;
	VertexColorMaskThreshold = 0.5f;

	bEnableStaticLighting = true;
	CastShadow = true;
	bCastDynamicShadow = true;
	bCastStaticShadow = true;
	bAffectDynamicIndirectLighting = false;
	bAffectDistanceFieldLighting = true;
	bCastShadowAsTwoSided = false;
	bReceivesDecals = false;

	bOverrideLightMapRes = false;
	OverriddenLightMapRes = 8;

	BodyInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	UpdateGuid = FGuid::NewGuid();
}

UFoliageType_InstancedStaticMesh::UFoliageType_InstancedStaticMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Mesh = nullptr;
}

#if WITH_EDITOR
void UFoliageType::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Ensure that OverriddenLightMapRes is a factor of 4
	OverriddenLightMapRes = OverriddenLightMapRes > 4 ? OverriddenLightMapRes + 3 & ~3 : 4;

	UpdateGuid = FGuid::NewGuid();

	// Notify any currently-loaded InstancedFoliageActors
	if (IsFoliageReallocationRequiredForPropertyChange(PropertyChangedEvent))
	{
		for (TObjectIterator<AInstancedFoliageActor> It(RF_ClassDefaultObject | RF_PendingKill); It; ++It)
		{
			It->NotifyFoliageTypeChanged(this);
		}
	}
}
#endif

//
// FFoliageMeshInfo
//
FFoliageMeshInfo::FFoliageMeshInfo()
	: Component(nullptr)
#if WITH_EDITOR
	, InstanceHash(GIsEditor ? new FFoliageInstanceHash() : nullptr)
#endif
{ }


#if WITH_EDITOR

void FFoliageMeshInfo::CheckValid()
{
#if DO_FOLIAGE_CHECK
	int32 ClusterTotal = 0;
	int32 ComponentTotal = 0;

	for (FFoliageInstanceCluster& Cluster : InstanceClusters)
	{
		check(Cluster.ClusterComponent != nullptr);
		ClusterTotal += Cluster.InstanceIndices.Num();
		ComponentTotal += Cluster.ClusterComponent->PerInstanceSMData.Num();
	}

	check(ClusterTotal == ComponentTotal);

	int32 FreeTotal = 0;
	int32 InstanceTotal = 0;
	for (int32 InstanceIdx = 0; InstanceIdx < Instances.Num(); InstanceIdx++)
	{
		if (Instances[InstanceIdx].ClusterIndex != -1)
		{
			InstanceTotal++;
		}
		else
		{
			FreeTotal++;
		}
	}

	check( ClusterTotal == InstanceTotal );
	check( FreeInstanceIndices.Num() == FreeTotal );

	InstanceHash->CheckInstanceCount(InstanceTotal);

	int32 ComponentHashTotal = 0;
	for( TMap<UActorComponent*, FFoliageComponentHashInfo >::TConstIterator It(ComponentHash); It; ++It )
	{
		ComponentHashTotal += It.Value().Instances.Num();
	}
	check( ComponentHashTotal == InstanceTotal);

#if FOLIAGE_CHECK_TRANSFORM
	// Check transforms match up with editor data
	int32 MismatchCount = 0;
	for( int32 ClusterIdx=0;ClusterIdx<InstanceClusters.Num();ClusterIdx++ )
	{
		TArray<int32> Indices = InstanceClusters(ClusterIdx).InstanceIndices;
		UInstancedStaticMeshComponent* Comp = InstanceClusters(ClusterIdx).ClusterComponent;
		for( int32 InstIdx=0;InstIdx<Indices.Num();InstIdx++ )
		{
			int32 InstanceIdx = Indices(InstIdx);

			FTransform InstanceToWorldEd = Instances(InstanceIdx).GetInstanceTransform();
			FTransform InstanceToWorldCluster = Comp->PerInstanceSMData(InstIdx).Transform * Comp->GetComponentToWorld();

			if( !InstanceToWorldEd.Equals(InstanceToWorldCluster) )
			{
				Comp->PerInstanceSMData(InstIdx).Transform = InstanceToWorldEd.ToMatrixWithScale();
				MismatchCount++;
			}
		}
	}

	if( MismatchCount != 0 )
	{
		UE_LOG(LogInstancedFoliage, Log, TEXT("%s: transform mismatch: %d"), *InstanceClusters(0).ClusterComponent->StaticMesh->GetName(), MismatchCount);
	}
#endif

#endif
}

void FFoliageMeshInfo::AddInstance(AInstancedFoliageActor* InIFA, UFoliageType* InSettings, const FFoliageInstance& InNewInstance)
{
	InIFA->Modify();

	if (Component == nullptr)
	{
		Component = ConstructObject<UHierarchicalInstancedStaticMeshComponent>(UHierarchicalInstancedStaticMeshComponent::StaticClass(), InIFA, NAME_None, RF_Transactional);

		Component->Mobility = InSettings->bEnableStaticLighting ? EComponentMobility::Static : EComponentMobility::Movable;

		Component->StaticMesh = InSettings->GetStaticMesh();
		Component->bSelectable = true;
		Component->bHasPerInstanceHitProxies = true;
		Component->InstancingRandomSeed = FMath::Rand();
		Component->InstanceStartCullDistance = InSettings->StartCullDistance;
		Component->InstanceEndCullDistance = InSettings->EndCullDistance;

		Component->CastShadow = InSettings->CastShadow;
		Component->bCastDynamicShadow = InSettings->bCastDynamicShadow;
		Component->bCastStaticShadow = InSettings->bCastStaticShadow;
		Component->bAffectDynamicIndirectLighting = InSettings->bAffectDynamicIndirectLighting;
		Component->bAffectDistanceFieldLighting = InSettings->bAffectDistanceFieldLighting;
		Component->bCastShadowAsTwoSided = InSettings->bCastShadowAsTwoSided;
		Component->bReceivesDecals = InSettings->bReceivesDecals;
		Component->bOverrideLightMapRes = InSettings->bOverrideLightMapRes;
		Component->OverriddenLightMapRes = InSettings->OverriddenLightMapRes;

		Component->BodyInstance.CopyBodyInstancePropertiesFrom(&InSettings->BodyInstance);

		Component->AttachTo(InIFA->GetRootComponent());
		Component->RegisterComponent();

		// Use only instance translation as a component transform
		Component->SetWorldTransform(InIFA->GetRootComponent()->ComponentToWorld);

		// Add the new component to the transaction buffer so it will get destroyed on undo
		Component->Modify();
		// We don't want to track changes to instances later so we mark it as non-transactional
		Component->ClearFlags(RF_Transactional);
	}
	else
	{
		Component->InvalidateLightingCache();
	}

	// Add the instance taking either a free slot or adding a new item.
	int32 InstanceIndex = Instances.AddUninitialized();

	FFoliageInstance& AddedInstance = Instances[InstanceIndex];
	AddedInstance = InNewInstance;

	// Add the instance to the hash
	InstanceHash->InsertInstance(InNewInstance.Location, InstanceIndex);
	FFoliageComponentHashInfo* ComponentHashInfo = ComponentHash.Find(InNewInstance.Base);
	if (ComponentHashInfo == nullptr)
	{
		ComponentHashInfo = &ComponentHash.Add(InNewInstance.Base, FFoliageComponentHashInfo(InNewInstance.Base));
	}
	ComponentHashInfo->Instances.Add(InstanceIndex);
	
	// Calculate transform for the instance
	FTransform InstanceToWorld = InNewInstance.GetInstanceWorldTransform();

	// Add the instance to the component
	Component->AddInstanceWorldSpace(InstanceToWorld);

	// Update PrimitiveComponent's culling distance taking into account the radius of the bounds, as
	// it is based on the center of the component's bounds.
//	float CullDistance = InSettings->EndCullDistance > 0 ? (float)InSettings->EndCullDistance + BestCluster->Bounds.SphereRadius : 0.f;
//	Component->LDMaxDrawDistance = CullDistance;
//	Component->CachedMaxDrawDistance = CullDistance;

	CheckValid();
}

void FFoliageMeshInfo::RemoveInstances(AInstancedFoliageActor* InIFA, const TArray<int32>& InInstancesToRemove)
{
	if (InInstancesToRemove.Num())
	{
		check(Component);
		InIFA->Modify();

		TSet<int32> InstancesToRemove;
		for (int32 Instance : InInstancesToRemove)
		{
			InstancesToRemove.Add(Instance);
		}

		while(InstancesToRemove.Num())
		{
			// Get an item from the set for processing
			auto It = InstancesToRemove.CreateConstIterator();
			int32 InstanceIndex = *It;		
			int32 InstanceIndexToRemove = InstanceIndex;

			FFoliageInstance& Instance = Instances[InstanceIndex];

			// remove from hash
			InstanceHash->RemoveInstance(Instance.Location, InstanceIndex);
		    FFoliageComponentHashInfo* ComponentHashInfo = ComponentHash.Find(Instance.Base);
		    if (ComponentHashInfo)
		    {
			    ComponentHashInfo->Instances.Remove(InstanceIndex);
			    if (ComponentHashInfo->Instances.Num() == 0)
			    {
				    // Remove the component from the component hash if this is the last instance.
				    ComponentHash.Remove(Instance.Base);
			    }
		    }

			// remove from the component
			Component->RemoveInstance(InstanceIndex);

			// Remove it from the selection.
			SelectedIndices.Remove(InstanceIndex);

			// remove from instances array
			Instances.RemoveAtSwap(InstanceIndex);

			// update hashes for swapped instance
			if (InstanceIndex != Instances.Num() && Instances.Num())
			{
				// Instance hash
				FFoliageInstance& SwappedInstance = Instances[InstanceIndex];
				InstanceHash->RemoveInstance(SwappedInstance.Location, Instances.Num());
				InstanceHash->InsertInstance(SwappedInstance.Location, InstanceIndex);

				// Component hash
				if (SwappedInstance.Base)
				{
					FFoliageComponentHashInfo* ComponentHashInfo = ComponentHash.Find(SwappedInstance.Base);
					check(ComponentHashInfo);
					ComponentHashInfo->Instances.Remove(Instances.Num());
					ComponentHashInfo->Instances.Add(InstanceIndex);
				}

				// Selection
				if (SelectedIndices.Contains(Instances.Num()))
				{
					SelectedIndices.Remove(Instances.Num());
					SelectedIndices.Add(InstanceIndex);
				}

				// Removal list
				if (InstancesToRemove.Contains(Instances.Num()))
				{
					// The item from the end of the array that we swapped in to InstanceIndex is also on the list to remove.
					// Remove the item at the end of the array and leave InstanceIndex in the removal list.
					InstanceIndexToRemove = Instances.Num();
				}
			}

			// Remove the removed item from the removal list
			InstancesToRemove.Remove(InstanceIndexToRemove);
		}

		InIFA->CheckSelection();

		CheckValid();
	}
}

void FFoliageMeshInfo::PreMoveInstances(AInstancedFoliageActor* InIFA, const TArray<int32>& InInstancesToMove)
{
	// Remove instances from the hash
	for (TArray<int32>::TConstIterator It(InInstancesToMove); It; ++It)
	{
		int32 InstanceIndex = *It;
		const FFoliageInstance& Instance = Instances[InstanceIndex];
		InstanceHash->RemoveInstance(Instance.Location, InstanceIndex);
	}
}


void FFoliageMeshInfo::PostUpdateInstances(AInstancedFoliageActor* InIFA, const TArray<int32>& InInstancesUpdated, bool bReAddToHash)
{
	if (InInstancesUpdated.Num())
	{
		check(Component);

		for (TArray<int32>::TConstIterator It(InInstancesUpdated); It; ++It)
		{
			int32 InstanceIndex = *It;
			const FFoliageInstance& Instance = Instances[InstanceIndex];

			FTransform InstanceToWorld = Instance.GetInstanceWorldTransform();

			Component->UpdateInstanceTransform(InstanceIndex, InstanceToWorld, true);

			// Re-add instance to the hash if requested
			if (bReAddToHash)
			{
				InstanceHash->InsertInstance(Instance.Location, InstanceIndex);
			}
		}

		Component->InvalidateLightingCache();
		Component->MarkRenderStateDirty();
	}
}

void FFoliageMeshInfo::PostMoveInstances(AInstancedFoliageActor* InIFA, const TArray<int32>& InInstancesMoved)
{
	PostUpdateInstances(InIFA, InInstancesMoved, true);
}

void FFoliageMeshInfo::DuplicateInstances(AInstancedFoliageActor* InIFA, UFoliageType* InSettings, const TArray<int32>& InInstancesToDuplicate)
{
	for (int32 InstanceIndex : InInstancesToDuplicate)
	{
		const FFoliageInstance TempInstance = Instances[InstanceIndex];
		AddInstance(InIFA, InSettings, TempInstance);
	}
}

/* Get the number of placed instances */
int32 FFoliageMeshInfo::GetInstanceCount() const
{
	return Instances.Num();
}

// Destroy existing clusters and reassign all instances to new clusters
void FFoliageMeshInfo::ReallocateClusters(AInstancedFoliageActor* InIFA, UFoliageType* InSettings)
{
	// Detach all components
	InIFA->UnregisterAllComponents();

	if (Component != nullptr)
	{
		Component->bAutoRegister = false;
		Component = nullptr;
	}

	// Remove everything
	TArray<FFoliageInstance> OldInstances;
	Exchange(Instances, OldInstances);
	InstanceHash->Empty();
	ComponentHash.Empty();
	SelectedIndices.Empty();

	// Copy the UpdateGuid from the foliage type
	FoliageTypeUpdateGuid = InSettings->UpdateGuid;

	// Re-add
	for (FFoliageInstance& Instance : OldInstances)
	{
		if ((Instance.Flags & FOLIAGE_InstanceDeleted) == 0)
		{
			AddInstance(InIFA, InSettings, Instance);
		}
	}

	InIFA->RegisterAllComponents();
}

void FFoliageMeshInfo::ReapplyInstancesToComponent()
{
	if (Component)
	{
		Component->UnregisterComponent();
		Component->ClearInstances();

		for (auto& Instance : Instances)
		{
			Component->AddInstanceWorldSpace(Instance.GetInstanceWorldTransform());
		}

		if (SelectedIndices.Num())
		{
			if (Component->SelectedInstances.Num() != Component->PerInstanceSMData.Num())
			{
				Component->SelectedInstances.Init(false, Component->PerInstanceSMData.Num());
			}
			for (int32 i : SelectedIndices)
			{
				Component->SelectedInstances[i] = true;
			}
		}

		Component->RegisterComponent();
	}
}


// Update settings in the clusters based on the current settings (eg culling distance, collision, ...)
//void FFoliageMeshInfo::UpdateClusterSettings(AInstancedFoliageActor* InIFA)
//{
//	for (FFoliageInstanceCluster& Cluster : InstanceClusters)
//	{
//		UInstancedStaticMeshComponent* ClusterComponent = Cluster.ClusterComponent;
//		ClusterComponent->MarkRenderStateDirty();
//
//		// Copy settings
//		ClusterComponent->InstanceStartCullDistance = Settings->StartCullDistance;
//		ClusterComponent->InstanceEndCullDistance = Settings->EndCullDistance;
//
//		// Update PrimitiveComponent's culling distance taking into account the radius of the bounds, as
//		// it is based on the center of the component's bounds.
//		float CullDistance = Settings->EndCullDistance > 0 ? (float)Settings->EndCullDistance + Cluster.Bounds.SphereRadius : 0.f;
//		ClusterComponent->LDMaxDrawDistance = CullDistance;
//		ClusterComponent->CachedMaxDrawDistance = CullDistance;
//	}
//
//	InIFA->MarkComponentsRenderStateDirty();
//}

void FFoliageMeshInfo::GetInstancesInsideSphere(const FSphere& Sphere, TArray<int32>& OutInstances)
{
	auto TempInstances = InstanceHash->GetInstancesOverlappingBox(FBox::BuildAABB(Sphere.Center, FVector(Sphere.W)));
	for (int32 Idx : TempInstances)
	{
		if (FSphere(Instances[Idx].Location, 0.f).IsInside(Sphere))
		{
			OutInstances.Add(Idx);
		}
	}
}

// Returns whether or not there is are any instances overlapping the sphere specified
bool FFoliageMeshInfo::CheckForOverlappingSphere(const FSphere& Sphere)
{
	auto TempInstances = InstanceHash->GetInstancesOverlappingBox(FBox::BuildAABB(Sphere.Center, FVector(Sphere.W)));
	for (int32 Idx : TempInstances)
	{
		if (FSphere(Instances[Idx].Location, 0.f).IsInside(Sphere))
		{
			return true;
		}
	}
	return false;
}

// Returns whether or not there is are any instances overlapping the instance specified, excluding the set of instances provided
bool FFoliageMeshInfo::CheckForOverlappingInstanceExcluding(int32 TestInstanceIdx, float Radius, TSet<int32>& ExcludeInstances)
{
	FSphere Sphere(Instances[TestInstanceIdx].Location, Radius);

	auto TempInstances = InstanceHash->GetInstancesOverlappingBox(FBox::BuildAABB(Sphere.Center, FVector(Sphere.W)));
	for (int32 Idx : TempInstances)
	{
		if (Idx != TestInstanceIdx && !ExcludeInstances.Contains(Idx) && FSphere(Instances[Idx].Location, 0.f).IsInside(Sphere))
		{
			return true;
		}
	}
	return false;
}

void FFoliageMeshInfo::SelectInstances(AInstancedFoliageActor* InIFA, bool bSelect, TArray<int32>& InInstances)
{
	if (InInstances.Num())
	{
		check(Component);
		InIFA->Modify();

		if (bSelect)
		{
			// Apply selections to the component
			Component->MarkRenderStateDirty();

			if (Component->SelectedInstances.Num() != Component->PerInstanceSMData.Num())
			{
				Component->SelectedInstances.Init(false, Component->PerInstanceSMData.Num());
			}

			for (int32 i : InInstances)
			{
				SelectedIndices.Add(i);
				Component->SelectedInstances[i] = true;
			}
		}
		else
		{
			if (Component->SelectedInstances.Num())
			{
				Component->MarkRenderStateDirty();

				for (int32 i : InInstances)
				{
					SelectedIndices.Remove(i);
					Component->SelectedInstances[i] = false;
				}
			}
		}
	}
}

#endif	//WITH_EDITOR

//
// AInstancedFoliageActor
//
AInstancedFoliageActor::AInstancedFoliageActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	USceneComponent* SceneComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("RootComponent0"));
	RootComponent = SceneComponent;
	RootComponent->Mobility = EComponentMobility::Static;
	
	SetActorEnableCollision(true);
#if WITH_EDITORONLY_DATA
	bListedInSceneOutliner = false;
#endif // WITH_EDITORONLY_DATA
	PrimaryActorTick.bCanEverTick = false;
}

#if WITH_EDITOR
AInstancedFoliageActor* AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(UWorld* InWorld, bool bCreateIfNone)
{
	for (TActorIterator<AInstancedFoliageActor> It(InWorld); It; ++It)
	{
		AInstancedFoliageActor* InstancedFoliageActor = *It;
		if (InstancedFoliageActor)
		{
			if (InstancedFoliageActor->GetLevel()->IsCurrentLevel() && !InstancedFoliageActor->IsPendingKill())
			{
				return InstancedFoliageActor;
			}
		}
	}

	return bCreateIfNone ? InWorld->SpawnActor<AInstancedFoliageActor>() : nullptr;
}


AInstancedFoliageActor* AInstancedFoliageActor::GetInstancedFoliageActorForLevel(ULevel* InLevel)
{
	if (!InLevel)
	{
		return nullptr;
	}

	for (int32 ActorIndex = 0; ActorIndex < InLevel->Actors.Num(); ++ActorIndex)
	{
		AInstancedFoliageActor* InstancedFoliageActor = Cast<AInstancedFoliageActor>(InLevel->Actors[ActorIndex]);
		if (InstancedFoliageActor && !InstancedFoliageActor->IsPendingKill())
		{
			return InstancedFoliageActor;
		}
	}
	return nullptr;
}

void AInstancedFoliageActor::MoveInstancesForMovedComponent(UActorComponent* InComponent)
{
	bool bUpdatedInstances = false;
	bool bFirst = true;

	for (auto& MeshPair : FoliageMeshes)
	{
		FFoliageMeshInfo& MeshInfo = *MeshPair.Value;
		FFoliageComponentHashInfo* ComponentHashInfo = MeshInfo.ComponentHash.Find(InComponent);
		if (ComponentHashInfo)
		{
			if (bFirst)
			{
				bFirst = false;
				Modify();
			}

			FVector OldLocation = ComponentHashInfo->CachedLocation;
			FRotator OldRotation = ComponentHashInfo->CachedRotation;
			FVector OldDrawScale = ComponentHashInfo->CachedDrawScale;
			ComponentHashInfo->UpdateLocationFromActor(InComponent);

			for (int32 InstanceIndex : ComponentHashInfo->Instances)
			{
				FFoliageInstance& Instance = MeshInfo.Instances[InstanceIndex];

				MeshInfo.InstanceHash->RemoveInstance(Instance.Location, InstanceIndex);

				// Apply change
				FMatrix DeltaTransform =
					FRotationMatrix(Instance.Rotation) *
					FTranslationMatrix(Instance.Location) *
					FTranslationMatrix(-OldLocation) *
					FInverseRotationMatrix(OldRotation) *
					FScaleMatrix(ComponentHashInfo->CachedDrawScale / OldDrawScale) *
					FRotationMatrix(ComponentHashInfo->CachedRotation) *
					FTranslationMatrix(ComponentHashInfo->CachedLocation);

				// Extract rotation and position
				Instance.Location = DeltaTransform.GetOrigin();
				Instance.Rotation = DeltaTransform.Rotator();

				// Apply render data
				check(MeshInfo.Component);
				MeshInfo.Component->UpdateInstanceTransform(InstanceIndex, Instance.GetInstanceWorldTransform(), true);

				// Re-add the new instance location to the hash
				MeshInfo.InstanceHash->InsertInstance(Instance.Location, InstanceIndex);
			}
		}
	}
}

void AInstancedFoliageActor::DeleteInstancesForComponent(UActorComponent* InComponent)
{
	for (auto& MeshPair : FoliageMeshes)
	{
		FFoliageMeshInfo& MeshInfo = *MeshPair.Value;
		const FFoliageComponentHashInfo* ComponentHashInfo = MeshInfo.ComponentHash.Find(InComponent);
		if (ComponentHashInfo)
		{
			MeshInfo.RemoveInstances(this, ComponentHashInfo->Instances.Array());
		}
	}
}

void AInstancedFoliageActor::MoveInstancesForComponentToCurrentLevel(UActorComponent* InComponent)
{
	AInstancedFoliageActor* NewIFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(InComponent->GetWorld());

	for (auto& MeshPair : FoliageMeshes)
	{
		FFoliageMeshInfo& MeshInfo = *MeshPair.Value;
		UFoliageType* FoliageType = MeshPair.Key;

		// Duplicate the foliage type if it's not shared
		if (FoliageType->GetOutermost() == GetOutermost())
		{
			FoliageType = (UFoliageType*)StaticDuplicateObject(FoliageType, NewIFA, nullptr, RF_AllFlags & ~(RF_Standalone | RF_Public));
		}
		
		const FFoliageComponentHashInfo* ComponentHashInfo = MeshInfo.ComponentHash.Find(InComponent);
		if (ComponentHashInfo)
		{
			FFoliageMeshInfo* NewMeshInfo = NewIFA->FindOrAddMesh(FoliageType);

			// Add the foliage to the new level
			for (int32 InstanceIndex : ComponentHashInfo->Instances)
			{
				NewMeshInfo->AddInstance(NewIFA, FoliageType, MeshInfo.Instances[InstanceIndex]);
			}

			// Remove from old level
			MeshInfo.RemoveInstances(this, ComponentHashInfo->Instances.Array());
		}
	}
}

void AInstancedFoliageActor::MoveInstancesToNewComponent(UPrimitiveComponent* InOldComponent, UPrimitiveComponent* InNewComponent)
{
	AInstancedFoliageActor* NewIFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(InNewComponent->GetTypedOuter<ULevel>());
	if (!NewIFA)
	{
		NewIFA = InNewComponent->GetWorld()->SpawnActor<AInstancedFoliageActor>();
	}
	check(NewIFA);

	for (auto& MeshPair : FoliageMeshes)
	{
		FFoliageMeshInfo& MeshInfo = *MeshPair.Value;
		UFoliageType* FoliageType = MeshPair.Key;

		// Duplicate the foliage type if it's not shared
		if (FoliageType->GetOutermost() == GetOutermost())
		{
			FoliageType = (UFoliageType*)StaticDuplicateObject(FoliageType, NewIFA, nullptr, RF_AllFlags & ~(RF_Standalone | RF_Public));
		}

		const FFoliageComponentHashInfo* ComponentHashInfo = MeshInfo.ComponentHash.Find(InOldComponent);
		if (ComponentHashInfo)
		{
			// For same level can just remap the instances, otherwise we have to do a more complex move
			if (NewIFA == this)
			{
				FFoliageComponentHashInfo NewComponentHashInfo = MoveTemp(*ComponentHashInfo);
				NewComponentHashInfo.UpdateLocationFromActor(InNewComponent);

				// Update the instances
				for (int32 InstanceIndex : NewComponentHashInfo.Instances)
				{
					MeshInfo.Instances[InstanceIndex].Base = InNewComponent;
				}

				// Update the hash
				MeshInfo.ComponentHash.Add(InNewComponent, MoveTemp(NewComponentHashInfo));
				MeshInfo.ComponentHash.Remove(InOldComponent);
			}
			else
			{
				FFoliageMeshInfo* NewMeshInfo = NewIFA->FindOrAddMesh(FoliageType);

				// Add the foliage to the new level
				for (int32 InstanceIndex : ComponentHashInfo->Instances)
				{
					FFoliageInstance NewInstance = MeshInfo.Instances[InstanceIndex];
					NewInstance.Base = InNewComponent;
					NewMeshInfo->AddInstance(NewIFA, FoliageType, NewInstance);
				}

				// Remove from old level
				MeshInfo.RemoveInstances(this, ComponentHashInfo->Instances.Array());
				check(!MeshInfo.ComponentHash.Contains(InOldComponent));
			}
		}
	}
}

TMap<UFoliageType*, TArray<const FFoliageInstancePlacementInfo*>> AInstancedFoliageActor::GetInstancesForComponent(UActorComponent* InComponent)
{
	TMap<UFoliageType*, TArray<const FFoliageInstancePlacementInfo*>> Result;

	for (auto& MeshPair : FoliageMeshes)
	{
		const FFoliageMeshInfo& MeshInfo = *MeshPair.Value;

		const FFoliageComponentHashInfo* ComponentHashInfo = MeshInfo.ComponentHash.Find(InComponent);
		if (ComponentHashInfo)
		{
			TArray<const FFoliageInstancePlacementInfo*>& Array = Result.Add(MeshPair.Key, TArray<const FFoliageInstancePlacementInfo*>());
			Array.Empty(ComponentHashInfo->Instances.Num());

			for (int32 InstanceIndex : ComponentHashInfo->Instances)
			{
				const FFoliageInstancePlacementInfo* Instance = &MeshInfo.Instances[InstanceIndex];
				Array.Add(Instance);
			}
		}
	}

	return Result;
}

FFoliageMeshInfo* AInstancedFoliageActor::FindMesh(UFoliageType* InType)
{
	TUniqueObj<FFoliageMeshInfo>* MeshInfoEntry = FoliageMeshes.Find(InType);
	FFoliageMeshInfo* MeshInfo = MeshInfoEntry ? &MeshInfoEntry->Get() : nullptr;
	return MeshInfo;
}

FFoliageMeshInfo* AInstancedFoliageActor::FindOrAddMesh(UFoliageType* InType)
{
	TUniqueObj<FFoliageMeshInfo>* MeshInfoEntry = FoliageMeshes.Find(InType);
	FFoliageMeshInfo* MeshInfo = MeshInfoEntry ? &MeshInfoEntry->Get() : AddMesh(InType);
	return MeshInfo;
}

FFoliageMeshInfo* AInstancedFoliageActor::AddMesh(UStaticMesh* InMesh, UFoliageType** OutSettings, const UFoliageType_InstancedStaticMesh* DefaultSettings)
{
	check(GetSettingsForMesh(InMesh) == nullptr);

	MarkPackageDirty();

	UFoliageType_InstancedStaticMesh* Settings = nullptr;
#if WITH_EDITORONLY_DATA
	if (DefaultSettings)
	{
		// TODO: Can't we just use this directly?
		Settings = DuplicateObject<UFoliageType_InstancedStaticMesh>(DefaultSettings, this);
	}
	else
#endif
	{
		Settings = ConstructObject<UFoliageType_InstancedStaticMesh>(UFoliageType_InstancedStaticMesh::StaticClass(), this);
	}
	Settings->Mesh = InMesh;

	FFoliageMeshInfo* MeshInfo = AddMesh(Settings);

	const FBoxSphereBounds MeshBounds = InMesh->GetBounds();

	Settings->MeshBounds = MeshBounds;

	// Make bottom only bound
	FBox LowBound = MeshBounds.GetBox();
	LowBound.Max.Z = LowBound.Min.Z + (LowBound.Max.Z - LowBound.Min.Z) * 0.1f;

	float MinX = FLT_MAX, MaxX = FLT_MIN, MinY = FLT_MAX, MaxY = FLT_MIN;
	Settings->LowBoundOriginRadius = FVector::ZeroVector;

	if (InMesh->RenderData)
	{
		FPositionVertexBuffer& PositionVertexBuffer = InMesh->RenderData->LODResources[0].PositionVertexBuffer;
		for (uint32 Index = 0; Index < PositionVertexBuffer.GetNumVertices(); ++Index)
		{
			const FVector& Pos = PositionVertexBuffer.VertexPosition(Index);
			if (Pos.Z < LowBound.Max.Z)
			{
				MinX = FMath::Min(MinX, Pos.X);
				MinY = FMath::Min(MinY, Pos.Y);
				MaxX = FMath::Max(MaxX, Pos.X);
				MaxY = FMath::Max(MaxY, Pos.Y);
			}
		}
	}

	Settings->LowBoundOriginRadius = FVector((MinX + MaxX), (MinY + MaxY), FMath::Sqrt(FMath::Square(MaxX - MinX) + FMath::Square(MaxY - MinY))) * 0.5f;

	if (OutSettings)
	{
		*OutSettings = Settings;
	}

	return MeshInfo;
}

FFoliageMeshInfo* AInstancedFoliageActor::AddMesh(UFoliageType* InType)
{
	check(FoliageMeshes.Find(InType) == nullptr);

	MarkPackageDirty();

	if (InType->DisplayOrder == 0)
	{
		int32 MaxDisplayOrder = 0;
		for (auto& MeshPair : FoliageMeshes)
		{
			if (MeshPair.Key->DisplayOrder > MaxDisplayOrder)
			{
				MaxDisplayOrder = MeshPair.Key->DisplayOrder;
			}
		}
		InType->DisplayOrder = MaxDisplayOrder + 1;
	}

	FFoliageMeshInfo* MeshInfo = &*FoliageMeshes.Add(InType);
	MeshInfo->FoliageTypeUpdateGuid = InType->UpdateGuid;
	InType->IsSelected = true;

	return MeshInfo;
}

void AInstancedFoliageActor::RemoveMesh(UFoliageType* InSettings)
{
	Modify();
	MarkPackageDirty();
	UnregisterAllComponents();

	// Remove all components for this mesh from the Components array.
	FFoliageMeshInfo* MeshInfo = FindMesh(InSettings);
	if (MeshInfo && MeshInfo->Component)
	{
		MeshInfo->Component->bAutoRegister = false;
	}

	FoliageMeshes.Remove(InSettings);
	RegisterAllComponents();
	CheckSelection();
}

UFoliageType* AInstancedFoliageActor::GetSettingsForMesh(UStaticMesh* InMesh, FFoliageMeshInfo** OutMeshInfo)
{
	UFoliageType* Type = nullptr;
	FFoliageMeshInfo* MeshInfo = nullptr;

	for (auto& MeshPair : FoliageMeshes)
	{
		UFoliageType* Settings = MeshPair.Key;
		if (Settings && Settings->GetStaticMesh() == InMesh)
		{
			Type = MeshPair.Key;
			MeshInfo = &*MeshPair.Value;
			break;
		}
	}

	if (OutMeshInfo)
	{
		*OutMeshInfo = MeshInfo;
	}
	return Type;
}

void AInstancedFoliageActor::SelectInstance(UInstancedStaticMeshComponent* InComponent, int32 InInstanceIndex, bool bToggle)
{
	Modify();

	// If we're not toggling, we need to first deselect everything else
	if (!bToggle)
	{
		for (auto& MeshPair : FoliageMeshes)
		{
			FFoliageMeshInfo& MeshInfo = *MeshPair.Value;

			if (MeshInfo.SelectedIndices.Num() > 0)
			{
				check(MeshInfo.Component);
				if (MeshInfo.Component->SelectedInstances.Num() > 0)
				{
					MeshInfo.Component->SelectedInstances.Empty();
					MeshInfo.Component->MarkRenderStateDirty();
				}
			}

			MeshInfo.SelectedIndices.Empty();
		}
	}

	if (InComponent)
	{
		UFoliageType* Type = nullptr;
		FFoliageMeshInfo* MeshInfo = nullptr;

		Type = GetSettingsForMesh(InComponent->StaticMesh, &MeshInfo);

		if (MeshInfo)
		{
			if (InComponent == MeshInfo->Component)
			{
				bool bIsSelected = MeshInfo->SelectedIndices.Contains(InInstanceIndex);

				// Deselect if it's already selected.
				if (InInstanceIndex < InComponent->SelectedInstances.Num())
				{
					InComponent->SelectedInstances[InInstanceIndex] = false;
					InComponent->MarkRenderStateDirty();
				}

				if (bIsSelected)
				{
					MeshInfo->SelectedIndices.Remove(InInstanceIndex);
				}

				if (bToggle && bIsSelected)
				{
					if (SelectedMesh == Type && MeshInfo->SelectedIndices.Num() == 0)
					{
						SelectedMesh = nullptr;
					}
				}
				else
				{
					// Add the selection
					if (InComponent->SelectedInstances.Num() < InComponent->PerInstanceSMData.Num())
					{
						InComponent->SelectedInstances.Init(false, InComponent->PerInstanceSMData.Num());
					}
					InComponent->SelectedInstances[InInstanceIndex] = true;
					InComponent->MarkRenderStateDirty();

					SelectedMesh = Type;
					MeshInfo->SelectedIndices.Add(InInstanceIndex);
				}
			}
		}
	}

	CheckSelection();

}

void AInstancedFoliageActor::PostEditUndo()
{
	Super::PostEditUndo();

	FlushRenderingCommands();
	for (auto& MeshPair : FoliageMeshes)
	{
		FFoliageMeshInfo& MeshInfo = *MeshPair.Value;
		MeshInfo.ReapplyInstancesToComponent();
	}
}

void AInstancedFoliageActor::ApplySelectionToComponents(bool bApply)
{
	bool bNeedsUpdate = false;

	for (auto& MeshPair : FoliageMeshes)
	{
		FFoliageMeshInfo& MeshInfo = *MeshPair.Value;

		if (bApply)
		{
			if (MeshInfo.SelectedIndices.Num() > 0)
			{
				check(MeshInfo.Component);

				// Apply any selections in the component
				MeshInfo.Component->SelectedInstances.Init(false, MeshInfo.Component->PerInstanceSMData.Num());
				MeshInfo.Component->MarkRenderStateDirty();
				for (int32 i : MeshInfo.SelectedIndices)
				{
					MeshInfo.Component->SelectedInstances[i] = true;
				}
			}
		}
		else		
		{
			if (MeshInfo.Component && MeshInfo.Component->SelectedInstances.Num() > 0)
			{
				// remove any selections in the component
				MeshInfo.Component->SelectedInstances.Empty();
				MeshInfo.Component->MarkRenderStateDirty();
			}
		}
	}
}

void AInstancedFoliageActor::CheckSelection()
{
	// Check if we have to change the selection.
	if (SelectedMesh != nullptr)
	{
		FFoliageMeshInfo* MeshInfo = FindMesh(SelectedMesh);
		if (MeshInfo && MeshInfo->SelectedIndices.Num() > 0)
		{
			return;
		}
	}

	SelectedMesh = nullptr;

	// Try to find a new selection
	for (auto& MeshPair : FoliageMeshes)
	{
		FFoliageMeshInfo& MeshInfo = *MeshPair.Value;
		if (MeshInfo.SelectedIndices.Num() > 0)
		{
			SelectedMesh = MeshPair.Key;
			return;
		}
	}
}

FVector AInstancedFoliageActor::GetSelectionLocation()
{
	FVector Result(0, 0, 0);

	if (SelectedMesh != nullptr)
	{
		FFoliageMeshInfo* MeshInfo = FindMesh(SelectedMesh);
		if (MeshInfo && MeshInfo->SelectedIndices.Num() > 0)
		{
			Result = MeshInfo->Instances[MeshInfo->SelectedIndices.Array()[0]].Location;
		}
	}

	return Result;
}

void AInstancedFoliageActor::MapRebuild()
{
	// Map rebuild may have modified the BSP's ModelComponents and thrown the previous ones away.
	// Most BSP-painted foliage is attached to a Brush's UModelComponent which persist across rebuilds,
	// but any foliage attached directly to the level BSP's ModelComponents will need to try to find a new base.

	TMap<UFoliageType*, TArray<FFoliageInstance>> NewInstances;
	TArray<UModelComponent*> RemovedModelComponents;
	UWorld* World = GetWorld();
	check(World);

	// For each foliage brush, represented by the mesh/info pair
	for (auto& MeshPair : FoliageMeshes)
	{
		// each target component has some foliage instances
		FFoliageMeshInfo const& MeshInfo = *MeshPair.Value;
		UFoliageType* Settings = MeshPair.Key;
		check(Settings);

		for (auto& ComponentFoliagePair : MeshInfo.ComponentHash)
		{
			// BSP components are UModelComponents - they are the only ones we need to change
			UModelComponent* TargetComponent = Cast<UModelComponent>(ComponentFoliagePair.Key);
			// Check if it's part of a brush. We only need to fix up model components that are part of the level BSP.
			if (TargetComponent && Cast<ABrush>(TargetComponent->GetOuter()) == nullptr)
			{
				// Delete its instances later
				RemovedModelComponents.Add(TargetComponent);

				FFoliageComponentHashInfo const& FoliageInfo = ComponentFoliagePair.Value;

				// We have to test each instance to see if we can migrate it across
				for (int32 InstanceIdx : FoliageInfo.Instances)
				{
					// Use a line test against the world. This is not very reliable as we don't know the original trace direction.
					check(MeshInfo.Instances.IsValidIndex(InstanceIdx));
					FFoliageInstance const& Instance = MeshInfo.Instances[InstanceIdx];

					FFoliageInstance NewInstance = Instance;

					FTransform InstanceToWorld = Instance.GetInstanceWorldTransform();
					FVector Down(-FVector::UpVector);
					FVector Start(InstanceToWorld.TransformPosition(FVector::UpVector));
					FVector End(InstanceToWorld.TransformPosition(Down));

					FHitResult Result;
					bool bHit = World->LineTraceSingle(Result, Start, End, FCollisionQueryParams(true), FCollisionObjectQueryParams(ECC_WorldStatic));

					if (bHit && Result.Component.IsValid() && Result.Component->IsA(UModelComponent::StaticClass()))
					{
						NewInstance.Base = CastChecked<UPrimitiveComponent>(Result.Component.Get());
						NewInstances.FindOrAdd(Settings).Add(NewInstance);
					}
				}
			}
		}
	}

	// Remove all existing & broken instances & component references.
	for (UModelComponent* Component : RemovedModelComponents)
	{
		DeleteInstancesForComponent(Component);
	}

	// And then finally add our new instances to the correct target components.
	for (auto& NewInstancePair : NewInstances)
	{
		UFoliageType* Settings = NewInstancePair.Key;
		check(Settings);
		FFoliageMeshInfo& MeshInfo = *FindOrAddMesh(Settings);
		for (FFoliageInstance& Instance : NewInstancePair.Value)
		{
			MeshInfo.AddInstance(this, Settings, Instance);
		}
	}
}

void AInstancedFoliageActor::ApplyLevelTransform(const FTransform& LevelTransform)
{
	// Apply transform to foliage editor only data
	if (GIsEditor)
	{
		for (auto& MeshPair : FoliageMeshes)
		{
			FFoliageMeshInfo& MeshInfo = *MeshPair.Value;
		
			MeshInfo.InstanceHash->Empty();
			for (int32 InstanceIdx = 0; InstanceIdx < MeshInfo.Instances.Num(); InstanceIdx++)
			{
				FFoliageInstance& Instance = MeshInfo.Instances[InstanceIdx];
				FTransform NewTransform = Instance.GetInstanceWorldTransform() * LevelTransform;
			
				Instance.Location		= NewTransform.GetLocation();
				Instance.Rotation		= NewTransform.GetRotation().Rotator();
				Instance.DrawScale3D	= NewTransform.GetScale3D();
			
				// Rehash instance location
				MeshInfo.InstanceHash->InsertInstance(Instance.Location, InstanceIdx);
			}

			for (auto It = MeshInfo.ComponentHash.CreateIterator(); It; ++It)
			{
				// We assume here that component we painted foliage on, was transformed as well
				FFoliageComponentHashInfo& Info = It.Value();
				Info.UpdateLocationFromActor(It.Key());
			}
		}
	}
}

#endif // WITH_EDITOR

struct FFoliageMeshInfo_Old
{
	TArray<FFoliageInstanceCluster_Deprecated> InstanceClusters;
	TArray<FFoliageInstance> Instances;
	UFoliageType_InstancedStaticMesh* Settings; // Type remapped via +ActiveClassRedirects
};
FArchive& operator<<(FArchive& Ar, FFoliageMeshInfo_Old& MeshInfo)
{
	Ar << MeshInfo.InstanceClusters;
	Ar << MeshInfo.Instances;
	Ar << MeshInfo.Settings;

	return Ar;
}

void AInstancedFoliageActor::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FFoliageCustomVersion::GUID);

	if (Ar.UE4Ver() < VER_UE4_FOLIAGE_SETTINGS_TYPE)
	{
		TMap<UStaticMesh*, FFoliageMeshInfo_Old> OldFoliageMeshes;
		Ar << OldFoliageMeshes;
		for (auto& OldMeshInfo : OldFoliageMeshes)
		{
			FFoliageMeshInfo NewMeshInfo;

#if WITH_EDITORONLY_DATA
			NewMeshInfo.Instances = MoveTemp(OldMeshInfo.Value.Instances);
#endif

			UFoliageType_InstancedStaticMesh* FoliageType = OldMeshInfo.Value.Settings;
			if (FoliageType == nullptr)
			{
				// If the Settings object was null, eg the user forgot to save their settings asset, create a new one.
				FoliageType = ConstructObject<UFoliageType_InstancedStaticMesh>(UFoliageType_InstancedStaticMesh::StaticClass(), this);
			}

			if (FoliageType->Mesh == nullptr)
			{
				FoliageType->Modify();
				FoliageType->Mesh = OldMeshInfo.Key;
			}
			else if (FoliageType->Mesh != OldMeshInfo.Key)
			{
				// If mesh doesn't match (two meshes sharing the same settings object?) then we need to duplicate as that is no longer supported
				FoliageType = (UFoliageType_InstancedStaticMesh*)StaticDuplicateObject(FoliageType, this, nullptr, RF_AllFlags & ~(RF_Standalone | RF_Public));
				FoliageType->Mesh = OldMeshInfo.Key;
			}
#if WITH_EDITORONLY_DATA
			NewMeshInfo.FoliageTypeUpdateGuid = FoliageType->UpdateGuid;
#endif
			FoliageMeshes.Add(FoliageType, TUniqueObj<FFoliageMeshInfo>(MoveTemp(NewMeshInfo)));
		}
	}
	else
	{
		Ar << FoliageMeshes;
	}

	// Clean up any old cluster components and convert to hierarchical instanced foliage.
	if (Ar.CustomVer(FFoliageCustomVersion::GUID) < FFoliageCustomVersion::FoliageUsingHierarchicalISMC)
	{
		TInlineComponentArray<UInstancedStaticMeshComponent*> ClusterComponents;
		GetComponents(ClusterComponents);
		for (UInstancedStaticMeshComponent* Component : ClusterComponents)
		{
			Component->bAutoRegister = false;
		}
	}
}

void AInstancedFoliageActor::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if (GetLinkerUE4Version() < VER_UE4_DISALLOW_FOLIAGE_ON_BLUEPRINTS)
	{
		for (auto& MeshPair : FoliageMeshes)
		{
			for (FFoliageInstance& Instance : MeshPair.Value->Instances)
			{
				// Clear out the Base for any instances based on blueprint-created components,
				// as those components will be destroyed when the construction scripts are
				// re-run, leaving dangling references and causing crashes (woo!)
				if (Instance.Base && Instance.Base->IsCreatedByConstructionScript())
				{
					Instance.Base = NULL;
				}
			}
		}
	}
#endif

#if WITH_EDITOR
	if (GIsEditor)
	{
		{
			bool bContainsNull = FoliageMeshes.Remove(nullptr) > 0;
			if (bContainsNull)
			{
				FMessageLog("MapCheck").Warning()
					->AddToken(FUObjectToken::Create(this))
					->AddToken(FTextToken::Create(LOCTEXT("MapCheck_Message_FoliageMissingStaticMesh", "Foliage instances for a missing static mesh have been removed.")))
					->AddToken(FMapErrorToken::Create(FMapErrors::FoliageMissingStaticMesh));
				while (bContainsNull)
				{
					bContainsNull = FoliageMeshes.Remove(nullptr) > 0;
				}
			}
		}
		for (auto& MeshPair : FoliageMeshes)
		{
			// Find the per-mesh info matching the mesh.
			FFoliageMeshInfo& MeshInfo = *MeshPair.Value;
			UFoliageType* FoliageType = MeshPair.Key;

			// Update foliage components if the foliage settings object was changed while the level was not loaded.
			if (MeshInfo.FoliageTypeUpdateGuid != FoliageType->UpdateGuid)
			{
				if (MeshInfo.FoliageTypeUpdateGuid.IsValid())
				{
					MeshInfo.ReallocateClusters(this, MeshPair.Key);
				}
				MeshInfo.FoliageTypeUpdateGuid = FoliageType->UpdateGuid;
			}

			// Update the hash.
			for (int32 InstanceIdx = 0; InstanceIdx < MeshInfo.Instances.Num(); InstanceIdx++)
			{
				FFoliageInstance& Instance = MeshInfo.Instances[InstanceIdx];
				{
					// Add valid instances to the hash.
					MeshInfo.InstanceHash->InsertInstance(Instance.Location, InstanceIdx);
					FFoliageComponentHashInfo* ComponentHashInfo = MeshInfo.ComponentHash.Find(Instance.Base);
					if (ComponentHashInfo == nullptr)
					{
						ComponentHashInfo = &MeshInfo.ComponentHash.Add(Instance.Base, FFoliageComponentHashInfo(Instance.Base));
					}
					ComponentHashInfo->Instances.Add(InstanceIdx);
				}
			}

			// Convert to Heirarchical foliage
			if (GetLinkerCustomVersion(FFoliageCustomVersion::GUID) < FFoliageCustomVersion::FoliageUsingHierarchicalISMC)
			{
				MeshInfo.ReallocateClusters(this, MeshPair.Key);
			}

			if (GetLinkerCustomVersion(FFoliageCustomVersion::GUID) < FFoliageCustomVersion::HierarchicalISMCNonTransactional)
			{
				if (MeshInfo.Component)
				{
					MeshInfo.Component->ClearFlags(RF_Transactional);
				}
			}
		}
	}
#endif
}

#if WITH_EDITOR
void AInstancedFoliageActor::NotifyFoliageTypeChanged(UFoliageType* FoliageType)
{
	FFoliageMeshInfo* MeshInfo = FindMesh(FoliageType);
	if (MeshInfo)
	{
		MeshInfo->ReallocateClusters(this, FoliageType);
	}
}
#endif

//
// Serialize all our UObjects for RTGC 
//
void AInstancedFoliageActor::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	AInstancedFoliageActor* This = CastChecked<AInstancedFoliageActor>(InThis);
	if (This->SelectedMesh)
	{
		Collector.AddReferencedObject(This->SelectedMesh, This);
	}

	for (auto& MeshPair : This->FoliageMeshes)
	{
		Collector.AddReferencedObject(MeshPair.Key, This);
		FFoliageMeshInfo& MeshInfo = *MeshPair.Value;

#if WITH_EDITORONLY_DATA
		for (FFoliageInstance& Instance : MeshInfo.Instances)
		{
			if (Instance.Base != nullptr)
			{
				Collector.AddReferencedObject(Instance.Base, This);
			}
		}
#endif

		if (MeshInfo.Component)
		{
			Collector.AddReferencedObject(MeshInfo.Component, This);
		}
	}
	Super::AddReferencedObjects(This, Collector);
}

/** InstancedStaticMeshInstance hit proxy */
void HInstancedStaticMeshInstance::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Component);
}

void AInstancedFoliageActor::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	Super::ApplyWorldOffset(InOffset, bWorldShift);

	if (GIsEditor)
	{
		for (auto& MeshPair : FoliageMeshes)
		{
			FFoliageMeshInfo& MeshInfo = *MeshPair.Value;

#if WITH_EDITORONLY_DATA
			MeshInfo.InstanceHash->Empty();
			for (int32 InstanceIdx = 0; InstanceIdx < MeshInfo.Instances.Num(); InstanceIdx++)
			{
				FFoliageInstance& Instance = MeshInfo.Instances[InstanceIdx];
				Instance.Location += InOffset;
				// Rehash instance location
				MeshInfo.InstanceHash->InsertInstance(Instance.Location, InstanceIdx);
			}

			for (auto It = MeshInfo.ComponentHash.CreateIterator(); It; ++It)
			{
				// We assume here that component we painted foliage on will be shifted by same value
				FFoliageComponentHashInfo& Info = It.Value();
				Info.CachedLocation += InOffset;
			}
#endif
		}
	}
}

#undef LOCTEXT_NAMESPACE

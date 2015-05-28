// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "RecastHelpers.h"
#include "AI/Navigation/NavAreas/NavArea_Null.h"
#include "AI/Navigation/NavAreas/NavArea_Default.h"
#include "AI/Navigation/NavAreas/NavArea_LowHeight.h"
#include "AI/Navigation/NavLinkCustomInterface.h"
#include "AI/Navigation/RecastNavMesh.h"
#include "AI/Navigation/RecastNavMeshDataChunk.h"
#include "VisualLogger/VisualLogger.h"

#if WITH_EDITOR
#include "ObjectEditorUtils.h"
#endif

#if WITH_RECAST
#include "DetourAlloc.h"
#endif // WITH_RECAST

#if WITH_EDITOR
#include "UnrealEd.h"
#endif

#include "AI/Navigation/NavMeshRenderingComponent.h"

#if WITH_RECAST
/// Helper for accessing navigation query from different threads
#define INITIALIZE_NAVQUERY(NavQueryVariable, NumNodes)	\
	dtNavMeshQuery NavQueryVariable##Private;	\
	dtNavMeshQuery& NavQueryVariable = IsInGameThread() ? RecastNavMeshImpl->SharedNavQuery : NavQueryVariable##Private; \
	NavQueryVariable.init(RecastNavMeshImpl->DetourNavMesh, NumNodes);

#define INITIALIZE_NAVQUERY_WLINKFILTER(NavQueryVariable, NumNodes, LinkFilter)	\
	dtNavMeshQuery NavQueryVariable##Private;	\
	dtNavMeshQuery& NavQueryVariable = IsInGameThread() ? RecastNavMeshImpl->SharedNavQuery : NavQueryVariable##Private; \
	NavQueryVariable.init(RecastNavMeshImpl->DetourNavMesh, NumNodes, &LinkFilter);

#endif // WITH_RECAST

static const int32 ArbitraryMaxVoxelTileSize = 1024;

FNavMeshTileData::FNavData::~FNavData()
{
#if WITH_RECAST
	dtFree(RawNavData);
#else
	FMemory::Free(RawNavData);
#endif // WITH_RECAST
}

FNavMeshTileData::FNavMeshTileData(uint8* RawData, int32 RawDataSize, int32 LayerIdx, FBox LayerBounds)
	: LayerIndex(LayerIdx)
	, LayerBBox(LayerBounds)
	, DataSize(RawDataSize)
{
	INC_MEMORY_STAT_BY(STAT_Navigation_TileCacheMemory, DataSize);
	NavData = MakeShareable(new FNavData(RawData));
}

FNavMeshTileData::~FNavMeshTileData()
{
	if (NavData.IsUnique() && NavData->RawNavData)
	{
		DEC_MEMORY_STAT_BY(STAT_Navigation_TileCacheMemory, DataSize);
	}
}

uint8* FNavMeshTileData::Release()
{
	uint8* RawData = nullptr;

	if (NavData.IsValid() && NavData->RawNavData) 
	{ 
		RawData = NavData->RawNavData;
		NavData->RawNavData = nullptr;
		DEC_MEMORY_STAT_BY(STAT_Navigation_TileCacheMemory, DataSize);
	} 
 
	DataSize = 0; 
	LayerIndex = 0; 
	return RawData;
}

void FNavMeshTileData::MakeUnique()
{
	if (DataSize > 0 && !NavData.IsUnique())
	{
		INC_MEMORY_STAT_BY(STAT_Navigation_TileCacheMemory, DataSize);
#if WITH_RECAST
		uint8* UniqueRawData = (uint8*)dtAlloc(sizeof(uint8)*DataSize, DT_ALLOC_PERM);
#else
		uint8* UniqueRawData = (uint8*)FMemory::Malloc(sizeof(uint8)*DataSize);
#endif //WITH_RECAST
		FMemory::Memcpy(UniqueRawData, NavData->RawNavData, DataSize);
		NavData = MakeShareable(new FNavData(UniqueRawData));
	}
}

float ARecastNavMesh::DrawDistanceSq = 0.0f;
#if !WITH_RECAST

ARecastNavMesh::ARecastNavMesh(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void ARecastNavMesh::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);

	uint32 NavMeshVersion;
	Ar << NavMeshVersion;

	//@todo: How to handle loading nav meshes saved w/ recast when recast isn't present????

	// when writing, write a zero here for now.  will come back and fill it in later.
	uint32 RecastNavMeshSizeBytes = 0;
	int32 RecastNavMeshSizePos = Ar.Tell();
	Ar << RecastNavMeshSizeBytes;

	if (Ar.IsLoading())
	{
		// incompatible, just skip over this data.  navmesh needs rebuilt.
		Ar.Seek( RecastNavMeshSizePos + RecastNavMeshSizeBytes );

		// Mark self for delete
		CleanUpAndMarkPendingKill();
	}
}

#else // WITH_RECAST

#include "PImplRecastNavMesh.h"
#include "RecastNavMeshGenerator.h"
#include "DetourNavMeshQuery.h"

#define DO_NAVMESH_DEBUG_DRAWING_PER_TILE 0

//----------------------------------------------------------------------//
// FRecastDebugGeometry
//----------------------------------------------------------------------//
uint32 FRecastDebugGeometry::GetAllocatedSize() const
{
	uint32 Size = sizeof(*this) + MeshVerts.GetAllocatedSize()
		+ BuiltMeshIndices.GetAllocatedSize()
		+ PolyEdges.GetAllocatedSize()
		+ NavMeshEdges.GetAllocatedSize()
		+ OffMeshLinks.GetAllocatedSize()
		+ OffMeshSegments.GetAllocatedSize()
		+ Clusters.GetAllocatedSize()
		+ ClusterLinks.GetAllocatedSize();

	for (int i = 0; i < RECAST_MAX_AREAS; ++i)
	{
		Size += AreaIndices[i].GetAllocatedSize();
	}

	for (int i = 0; i < Clusters.Num(); ++i)
	{
		Size += Clusters[i].MeshIndices.GetAllocatedSize();
	}

	return Size;
}

//----------------------------------------------------------------------//
// ARecastNavMesh
//----------------------------------------------------------------------//

namespace ERecastNamedFilter
{
	FRecastQueryFilter FilterOutNavLinksImpl;
	FRecastQueryFilter FilterOutAreasImpl;
	FRecastQueryFilter FilterOutNavLinksAndAreasImpl;
}

const FRecastQueryFilter* ARecastNavMesh::NamedFilters[] = {
	&ERecastNamedFilter::FilterOutNavLinksImpl
	, &ERecastNamedFilter::FilterOutAreasImpl
	, &ERecastNamedFilter::FilterOutNavLinksAndAreasImpl
};

namespace FNavMeshConfig
{
	ARecastNavMesh::FNavPolyFlags NavLinkFlag = ARecastNavMesh::FNavPolyFlags(0);

	FRecastNamedFiltersCreator::FRecastNamedFiltersCreator(bool bVirtualFilters)
	{
		// setting up the last bit available in dtPoly::flags
		NavLinkFlag = ARecastNavMesh::FNavPolyFlags(1 << (sizeof(((dtPoly*)0)->flags) * 8 - 1));

		ERecastNamedFilter::FilterOutNavLinksImpl.SetIsVirtual(bVirtualFilters);
		ERecastNamedFilter::FilterOutAreasImpl.SetIsVirtual(bVirtualFilters);
		ERecastNamedFilter::FilterOutNavLinksAndAreasImpl.SetIsVirtual(bVirtualFilters);

		ERecastNamedFilter::FilterOutNavLinksImpl.setExcludeFlags(NavLinkFlag);
		ERecastNamedFilter::FilterOutNavLinksAndAreasImpl.setExcludeFlags(NavLinkFlag);

		for (int32 AreaID = 0; AreaID < RECAST_MAX_AREAS; ++AreaID)
		{
			ERecastNamedFilter::FilterOutAreasImpl.setAreaCost(AreaID, RECAST_UNWALKABLE_POLY_COST);
			ERecastNamedFilter::FilterOutNavLinksAndAreasImpl.setAreaCost(AreaID, RECAST_UNWALKABLE_POLY_COST);
		}

		ERecastNamedFilter::FilterOutAreasImpl.setAreaCost(RECAST_DEFAULT_AREA, 1.f);
		ERecastNamedFilter::FilterOutNavLinksAndAreasImpl.setAreaCost(RECAST_DEFAULT_AREA, 1.f);
	}
}

ARecastNavMesh::FNavPolyFlags ARecastNavMesh::NavLinkFlag = ARecastNavMesh::FNavPolyFlags(0);

ARecastNavMesh::ARecastNavMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bDrawNavMeshEdges(true)
	, bDrawNavLinks(true)
	, bDistinctlyDrawTilesBeingBuilt(true)
	, bDrawNavMesh(true)
	, DrawOffset(10.f)
	, TilePoolSize(1024)
	, MaxSimplificationError(1.3f)	// from RecastDemo
	, DefaultMaxSearchNodes(RECAST_MAX_SEARCH_NODES)
	, DefaultMaxHierarchicalSearchNodes(RECAST_MAX_SEARCH_NODES)
	, bPerformVoxelFiltering(true)	
	, bMarkLowHeightAreas(false)
	, bUseVirtualFilters(true)
	, TileSetUpdateInterval(1.0f)
	, NavMeshVersion(NAVMESHVER_LATEST)	
	, RecastNavMeshImpl(NULL)
{
	HeuristicScale = 0.999f;
	RegionPartitioning = ERecastPartitioning::Watershed;
	LayerPartitioning = ERecastPartitioning::Watershed;
	RegionChunkSplits = 2;
	LayerChunkSplits = 2;
	MaxSimultaneousTileGenerationJobsCount = 1024;
	bDoFullyAsyncNavDataGathering = false;
	TileNumberHardLimit = 1 << 20;

#if RECAST_ASYNC_REBUILDING
	BatchQueryCounter = 0;
#endif // RECAST_ASYNC_REBUILDING


	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		INC_DWORD_STAT_BY( STAT_NavigationMemory, sizeof(*this) );

		FindPathImplementation = FindPath;
		FindHierarchicalPathImplementation = FindPath;

		TestPathImplementation = TestPath;
		TestHierarchicalPathImplementation = TestHierarchicalPath;

		RaycastImplementation = NavMeshRaycast;

		RecastNavMeshImpl = new FPImplRecastNavMesh(this);
	
		// add predefined areas up front
		SupportedAreas.Add(FSupportedAreaData(UNavArea_Null::StaticClass(), RECAST_NULL_AREA));
		SupportedAreas.Add(FSupportedAreaData(UNavArea_LowHeight::StaticClass(), RECAST_LOW_AREA));
		SupportedAreas.Add(FSupportedAreaData(UNavArea_Default::StaticClass(), RECAST_DEFAULT_AREA));
	}
}

ARecastNavMesh::~ARecastNavMesh()
{
	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		DEC_DWORD_STAT_BY( STAT_NavigationMemory, sizeof(*this) );
		DestroyRecastPImpl();
	}
}

void ARecastNavMesh::DestroyRecastPImpl()
{
	if (RecastNavMeshImpl != NULL)
	{
		delete RecastNavMeshImpl;
		RecastNavMeshImpl = NULL;
	}
}

ARecastNavMesh* ARecastNavMesh::SpawnInstance(UNavigationSystem* NavSys, const FNavDataConfig* AgentProps)
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.OverrideLevel = NavSys->GetWorld()->PersistentLevel;
	ARecastNavMesh* Instance = NavSys->GetWorld()->SpawnActor<ARecastNavMesh>( SpawnInfo );

	if (Instance != NULL && AgentProps != NULL)
	{
		Instance->SetConfig(*AgentProps);
		if (AgentProps->Name != NAME_None)
		{
			FString StrName = FString::Printf(TEXT("%s-%s"), *(Instance->GetFName().GetPlainNameString()), *(AgentProps->Name.ToString()));
			// temporary solution to make sure we don't try to change name while there's already
			// an object with this name
			UObject* ExistingObject = StaticFindObject(/*Class=*/ NULL, Instance->GetOuter(), *StrName, true);
			if (ExistingObject != NULL)
			{
				ExistingObject->Rename(NULL, NULL, REN_DontCreateRedirectors | REN_ForceGlobalUnique | REN_DoNotDirty | REN_NonTransactional);
			}

			// Set descriptive name
			Instance->Rename(*StrName);
#if WITH_EDITOR
			Instance->SetActorLabel(StrName);
#endif // WITH_EDITOR
		}
	}

	return Instance;
}

ANavigationData* ARecastNavMesh::CreateNavigationInstances(UNavigationSystem* NavSys)
{
	if (NavSys == NULL || NavSys->GetWorld() == NULL)
	{
		return NULL;
	}

	const TArray<FNavDataConfig>* SupportedAgents = &NavSys->GetSupportedAgents();
	const int SupportedAgentsCount = SupportedAgents->Num();

	if (SupportedAgentsCount > 0)
	{
		// Bit array might be a bit of an overkill here, but this function will be called very rarely
		TBitArray<> AlreadyInstantiated(false, SupportedAgentsCount);
		uint8 NumberFound = 0;

		// 1. check whether any of required navmeshes has already been instantiated
		for (TActorIterator<ARecastNavMesh> It(NavSys->GetWorld()); It && NumberFound < SupportedAgentsCount; ++It)
		{
			ARecastNavMesh* Nav = (*It);
			if (Nav != NULL && Nav->GetTypedOuter<UWorld>() == NavSys->GetWorld() && Nav->IsPendingKill() == false)
			{
				// find out which one it is
				const FNavDataConfig* AgentProps = SupportedAgents->GetData();
				for (int i = 0; i < SupportedAgentsCount; ++i, ++AgentProps)
				{
					if (AlreadyInstantiated[i] == true)
					{
						// already present, skip
						continue;
					}

					if (Nav->DoesSupportAgent(*AgentProps) == true)
					{
						AlreadyInstantiated[i] = true;
						++NumberFound;
						break;
					}
				}				
			}
		}

		// 2. for all those that are required and don't have their instances yet just create them
		if (NumberFound < SupportedAgentsCount)
		{
			for (int i = 0; i < SupportedAgentsCount; ++i)
			{
				if (AlreadyInstantiated[i] == false)
				{
					ARecastNavMesh* Instance = SpawnInstance(NavSys, &(*SupportedAgents)[i]);
					if (Instance != NULL)
					{
						NavSys->RequestRegistration(Instance);
					}
				}
			}
		}
	}
	else
	{
		ARecastNavMesh* Instance = NULL;
		for (TActorIterator<ARecastNavMesh> It(NavSys->GetWorld()); It; ++It)
		{
			ARecastNavMesh* Nav = (*It);
			if( Nav != NULL && Nav->GetTypedOuter<UWorld>() == NavSys->GetWorld())
			{
				Instance = Nav;
				break;
			}
		}

		if (Instance == NULL)
		{
			Instance = SpawnInstance(NavSys);
		}
		else
		{
			NavSys->RequestRegistration(Instance);
		}
	}

	return NULL;
}

UPrimitiveComponent* ARecastNavMesh::ConstructRenderingComponent() 
{
	return ConstructRenderingComponentImpl();
}

UPrimitiveComponent* ARecastNavMesh::ConstructRenderingComponentImpl() 
{
#if DO_NAVMESH_DEBUG_DRAWING_PER_TILE
	const bool bIsGameThread = IsInGameThread();

	if (RecastNavMeshImpl != NULL)
	{
		bool bComponentsRemoved = false;

		for (int i = 0; i < TileRenderingComponents.Num(); ++i)
		{
			if (TileRenderingComponents[i] != NULL)
			{
				TileRenderingComponents[i]->UnregisterComponent();
				Components.Remove(TileRenderingComponents[i]);
				bComponentsRemoved = true;
			}
		}

		TileRenderingComponents.Reset();
		RecastNavMeshImpl->GenerateTileRenderingComponents(TileRenderingComponents);

		//Components.Append(TileRenderingComponents);
		for (int i = 0; i < TileRenderingComponents.Num(); ++i)
		{
			if (TileRenderingComponents[i] != NULL)
			{
				TileRenderingComponents[i]->RegisterComponent();
			}
		}

		if (bComponentsRemoved || TileRenderingComponents.Num() > 0)
		{
			//RegisterAllComponents();
			//MarkComponentsRenderStateDirty();
		}
	}

	return NULL;
#else
	return NewObject<UNavMeshRenderingComponent>(this, TEXT("NavMeshRenderer"), RF_Transient);
#endif // DO_NAVMESH_DEBUG_DRAWING_PER_TILE
}

void ARecastNavMesh::UpdateNavMeshDrawing()
{
#if !UE_BUILD_SHIPPING
#if DO_NAVMESH_DEBUG_DRAWING_PER_TILE
	// @todo - we shouldn't be updating _every_ tile renderer, just the ones that have been changed
	for (int i = 0; i < TileRenderingComponents.Num(); ++i)
	{
		if (TileRenderingComponents[i] != NULL && TileRenderingComponents[i]->bVisible)
		{
			TileRenderingComponents[i]->MarkRenderStateDirty();
		}
	}
#else // DO_NAVMESH_DEBUG_DRAWING_PER_TILE
	if (RenderingComp != NULL && RenderingComp->bVisible && UNavMeshRenderingComponent::IsNavigationShowFlagSet(GetWorld()))
	{
		RenderingComp->MarkRenderStateDirty();
	}
#endif // DO_NAVMESH_DEBUG_DRAWING_PER_TILE
#endif // UE_BUILD_SHIPPING
}

void ARecastNavMesh::CleanUp()
{
	Super::CleanUp();
	if (NavDataGenerator.IsValid())
	{
		NavDataGenerator->CancelBuild();
		NavDataGenerator.Reset();
	}
	DestroyRecastPImpl();
}

void ARecastNavMesh::PostLoad()
{
	Super::PostLoad();
	// tilesize validation. This is temporary and should get removed by 4.9
	TileSizeUU = FMath::Clamp(TileSizeUU, CellSize, ArbitraryMaxVoxelTileSize * CellSize);
	UpdatePolyRefBitsPreview();
}

void ARecastNavMesh::PostInitProperties()
{
	if (HasAnyFlags(RF_ClassDefaultObject) == true)
	{
		SetDrawDistance(DefaultDrawDistance);

		static const FNavMeshConfig::FRecastNamedFiltersCreator RecastNamedFiltersCreator(bUseVirtualFilters);
		NavLinkFlag = FNavMeshConfig::NavLinkFlag;
	}
	else if(GetWorld() != NULL)
	{
		// get rid of instances saved within levels that are streamed-in
		if ((GEngine->IsSettingUpPlayWorld() == false) // this is a @HACK
			&&	(GetWorld()->GetOutermost() != GetOutermost())
			// If we are cooking, then let them all pass.
			// They will be handled at load-time when running.
			&&	(IsRunningCommandlet() == false))
		{
			// marking self for deletion 
			CleanUpAndMarkPendingKill();
		}
	}
	
	Super::PostInitProperties();

	TileSizeUU = FMath::Clamp(TileSizeUU, CellSize, ArbitraryMaxVoxelTileSize * CellSize);

	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		DefaultQueryFilter->SetFilterType<FRecastQueryFilter>();
		FRecastQueryFilter* DetourFilter = static_cast<FRecastQueryFilter*>(DefaultQueryFilter->GetImplementation());
		DetourFilter->SetIsVirtual(bUseVirtualFilters);
		DetourFilter->setHeuristicScale(HeuristicScale);
		DefaultQueryFilter->SetMaxSearchNodes(DefaultMaxSearchNodes);		
	}

	// voxel cache requires the same rasterization setup for all navmeshes, as it's stored in octree
	if (IsVoxelCacheEnabled() && !HasAnyFlags(RF_ClassDefaultObject))
	{
		ARecastNavMesh* DefOb = (ARecastNavMesh*)ARecastNavMesh::StaticClass()->GetDefaultObject();

		if (TileSizeUU != DefOb->TileSizeUU)
		{
			UE_LOG(LogNavigation, Warning, TEXT("%s param: TileSizeUU(%f) differs from config settings, forcing value %f so it can be used with voxel cache!"),
				*GetNameSafe(this), TileSizeUU, DefOb->TileSizeUU);
			
			TileSizeUU = DefOb->TileSizeUU;
		}

		if (CellSize != DefOb->CellSize)
		{
			UE_LOG(LogNavigation, Warning, TEXT("%s param: CellSize(%f) differs from config settings, forcing value %f so it can be used with voxel cache!"),
				*GetNameSafe(this), CellSize, DefOb->CellSize);

			CellSize = DefOb->CellSize;
		}

		if (CellHeight != DefOb->CellHeight)
		{
			UE_LOG(LogNavigation, Warning, TEXT("%s param: CellHeight(%f) differs from config settings, forcing value %f so it can be used with voxel cache!"),
				*GetNameSafe(this), CellHeight, DefOb->CellHeight);

			CellHeight = DefOb->CellHeight;
		}

		if (AgentMaxSlope != DefOb->AgentMaxSlope)
		{
			UE_LOG(LogNavigation, Warning, TEXT("%s param: AgentMaxSlope(%f) differs from config settings, forcing value %f so it can be used with voxel cache!"),
				*GetNameSafe(this), AgentMaxSlope, DefOb->AgentMaxSlope);

			AgentMaxSlope = DefOb->AgentMaxSlope;
		}

		if (AgentMaxStepHeight != DefOb->AgentMaxStepHeight)
		{
			UE_LOG(LogNavigation, Warning, TEXT("%s param: AgentMaxStepHeight(%f) differs from config settings, forcing value %f so it can be used with voxel cache!"),
				*GetNameSafe(this), AgentMaxStepHeight, DefOb->AgentMaxStepHeight);

			AgentMaxStepHeight = DefOb->AgentMaxStepHeight;
		}
	}

	UpdatePolyRefBitsPreview();
}

FVector ARecastNavMesh::GetModifiedQueryExtent(const FVector& QueryExtent) const
{
	return FVector(QueryExtent.X, QueryExtent.Y, QueryExtent.Z + FMath::Max(0.0f, VerticalDeviationFromGroundCompensation));
}

void ARecastNavMesh::UpdatePolyRefBitsPreview()
{
	static const int32 TotalBits = (sizeof(dtPolyRef) * 8);

	FRecastNavMeshGenerator::CalcPolyRefBits(this, PolyRefTileBits, PolyRefNavPolyBits);
	PolyRefSaltBits = TotalBits - PolyRefTileBits - PolyRefNavPolyBits;
}

void ARecastNavMesh::OnNavAreaAdded(const UClass* NavAreaClass, int32 AgentIndex)
{
	Super::OnNavAreaAdded(NavAreaClass, AgentIndex);

	// update navmesh query filter with area costs
	const int32 AreaID = GetAreaID(NavAreaClass);
	if (AreaID != INDEX_NONE)
	{
		UNavArea* DefArea = ((UClass*)NavAreaClass)->GetDefaultObject<UNavArea>();

		DefaultQueryFilter->SetAreaCost(AreaID, DefArea->DefaultCost);
		DefaultQueryFilter->SetFixedAreaEnteringCost(AreaID, DefArea->GetFixedAreaEnteringCost());
	}

	// update generator's cached data
	FRecastNavMeshGenerator* MyGenerator = static_cast<FRecastNavMeshGenerator*>(GetGenerator());
	if (MyGenerator)
	{
		MyGenerator->OnAreaAdded(NavAreaClass, AreaID);
	}
}

void ARecastNavMesh::OnNavAreaChanged()
{
	if (RecastNavMeshImpl)
	{
		RecastNavMeshImpl->OnAreaCostChanged();
	}
}

int32 ARecastNavMesh::GetNewAreaID(const UClass* AreaClass) const
{
	if (AreaClass == UNavigationSystem::GetDefaultWalkableArea())
	{
		return RECAST_DEFAULT_AREA;
	}

	if (AreaClass == UNavArea_Null::StaticClass())
	{
		return RECAST_NULL_AREA;
	}

	if (AreaClass == UNavArea_LowHeight::StaticClass())
	{
		return RECAST_LOW_AREA;
	}

	int32 FreeAreaID = Super::GetNewAreaID(AreaClass);
	while (FreeAreaID == RECAST_NULL_AREA || FreeAreaID == RECAST_DEFAULT_AREA || FreeAreaID == RECAST_LOW_AREA)
	{
		FreeAreaID++;
	}

	check(FreeAreaID < GetMaxSupportedAreas());
	return FreeAreaID;
}

FColor ARecastNavMesh::GetAreaIDColor(uint8 AreaID) const
{
	const UClass* AreaClass = GetAreaClass(AreaID);
	const UNavArea* DefArea = AreaClass ? ((UClass*)AreaClass)->GetDefaultObject<UNavArea>() : NULL;
	return DefArea ? DefArea->DrawColor : NavDataConfig.Color;
}

void ARecastNavMesh::SortAreasForGenerator(TArray<FRecastAreaNavModifierElement>& Modifiers) const
{
	// initialize costs for sorting
	float AreaCosts[RECAST_MAX_AREAS];
	float AreaFixedCosts[RECAST_MAX_AREAS];
	DefaultQueryFilter->GetAllAreaCosts(AreaCosts, AreaFixedCosts, RECAST_MAX_AREAS);

	for (auto& Element : Modifiers)
	{
		check(Element.Areas.Num() > 0);
		
		FAreaNavModifier& AreaMod = Element.Areas[0];
		const int32 AreaId = GetAreaID(AreaMod.GetAreaClass());
		if (AreaId >= 0 && AreaId < RECAST_MAX_AREAS)
		{
			AreaMod.Cost = AreaCosts[AreaId];
			AreaMod.FixedCost = AreaFixedCosts[AreaId];
		}
	}

	struct FNavAreaSortPredicate
	{
		FORCEINLINE bool operator()(const FRecastAreaNavModifierElement& ElA, const FRecastAreaNavModifierElement& ElB) const
		{
			check(ElA.Areas.Num() > 0);
			check(ElB.Areas.Num() > 0);
			// assuming composite modifiers has same area type
			const FAreaNavModifier& A = ElA.Areas[0];
			const FAreaNavModifier& B = ElB.Areas[0];
			
			const bool bIsAReplacing = (A.GetAreaClassToReplace() != NULL);
			const bool bIsBReplacing = (B.GetAreaClassToReplace() != NULL);
			if (bIsAReplacing != bIsBReplacing)
			{
				return bIsAReplacing;
			}

			return A.Cost != B.Cost ? A.Cost < B.Cost : A.FixedCost < B.FixedCost;
		}
	};

	Modifiers.Sort(FNavAreaSortPredicate());
}

TArray<FIntPoint>& ARecastNavMesh::GetActiveTiles()
{
	FRecastNavMeshGenerator* MyGenerator = static_cast<FRecastNavMeshGenerator*>(GetGenerator());
	check(MyGenerator);
	return MyGenerator->ActiveTiles;
}

void ARecastNavMesh::RestrictBuildingToActiveTiles(bool InRestrictBuildingToActiveTiles)
{
	FRecastNavMeshGenerator* MyGenerator = static_cast<FRecastNavMeshGenerator*>(GetGenerator());
	if (MyGenerator)
	{
		MyGenerator->RestrictBuildingToActiveTiles(InRestrictBuildingToActiveTiles);
	}
}

void ARecastNavMesh::SerializeRecastNavMesh(FArchive& Ar, FPImplRecastNavMesh*& NavMesh, int32 InNavMeshVersion)
{
	if (!Ar.IsLoading()	&& NavMesh == NULL)
	{
		return;
	}

	if (Ar.IsLoading())
	{
		// allocate if necessary
		if (RecastNavMeshImpl == NULL)
		{
			RecastNavMeshImpl = new FPImplRecastNavMesh(this);
		}
	}
	
	if (RecastNavMeshImpl)
	{
		RecastNavMeshImpl->Serialize(Ar, InNavMeshVersion);
	}	
}

void ARecastNavMesh::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);

	Ar << NavMeshVersion;

	//@todo: How to handle loading nav meshes saved w/ recast when recast isn't present????
	
	// when writing, write a zero here for now.  will come back and fill it in later.
	uint32 RecastNavMeshSizeBytes = 0;
	int32 RecastNavMeshSizePos = Ar.Tell();
	Ar << RecastNavMeshSizeBytes;

	if (Ar.IsLoading())
	{
		if (NavMeshVersion < NAVMESHVER_MIN_COMPATIBLE)
		{
			// incompatible, just skip over this data.  navmesh needs rebuilt.
			Ar.Seek( RecastNavMeshSizePos + RecastNavMeshSizeBytes );

			// Mark self for delete
			CleanUpAndMarkPendingKill();
		}
		else if (RecastNavMeshSizeBytes > 4)
		{
			SerializeRecastNavMesh(Ar, RecastNavMeshImpl, NavMeshVersion);
			bWantsUpdate = bForceRebuildOnLoad == true || HasValidNavmesh() == false;
#if !(UE_BUILD_SHIPPING)
			RequestDrawingUpdate();
#endif //!(UE_BUILD_SHIPPING)
		}
		else
		{
			// empty, just skip over this data
			Ar.Seek( RecastNavMeshSizePos + RecastNavMeshSizeBytes );
			// if it's not getting filled it's better to just remove it
			RecastNavMeshImpl->ReleaseDetourNavMesh();
		}
	}
	else
	{
		SerializeRecastNavMesh(Ar, RecastNavMeshImpl, NavMeshVersion);

		if (Ar.IsSaving())
		{
			int32 CurPos = Ar.Tell();
			RecastNavMeshSizeBytes = CurPos - RecastNavMeshSizePos;
			Ar.Seek(RecastNavMeshSizePos);
			Ar << RecastNavMeshSizeBytes;
			Ar.Seek(CurPos);
		}
	}
}

void ARecastNavMesh::SetConfig(const FNavDataConfig& Src) 
{ 
	NavDataConfig = Src; 
	AgentMaxHeight = AgentHeight = Src.AgentHeight;
	AgentRadius = Src.AgentRadius;

	if (Src.HasStepHeightOverride())
	{
		AgentMaxStepHeight = Src.AgentStepHeight;
	}
}

void ARecastNavMesh::FillConfig(FNavDataConfig& Dest)
{
	Dest = NavDataConfig;
	Dest.AgentHeight = AgentHeight;
	Dest.AgentRadius = AgentRadius;
	Dest.AgentStepHeight = AgentMaxStepHeight;
}

void ARecastNavMesh::BeginBatchQuery() const
{
#if RECAST_ASYNC_REBUILDING
	// lock critical section when no other batch queries are active
	if (BatchQueryCounter <= 0)
	{
		BatchQueryCounter = 0;
	}

	BatchQueryCounter++;
#endif // RECAST_ASYNC_REBUILDING
}

void ARecastNavMesh::FinishBatchQuery() const
{
#if RECAST_ASYNC_REBUILDING
	BatchQueryCounter--;
#endif // RECAST_ASYNC_REBUILDING
}

FBox ARecastNavMesh::GetNavMeshBounds() const
{
	FBox Bounds;
	if (RecastNavMeshImpl)
	{
		Bounds = RecastNavMeshImpl->GetNavMeshBounds();
	}

	return Bounds;
}

FBox ARecastNavMesh::GetNavMeshTileBounds(int32 TileIndex) const
{
	FBox Bounds;
	if (RecastNavMeshImpl)
	{
		Bounds = RecastNavMeshImpl->GetNavMeshTileBounds(TileIndex);
	}

	return Bounds;
}

bool ARecastNavMesh::GetNavMeshTileXY(int32 TileIndex, int32& OutX, int32& OutY, int32& OutLayer) const
{
	if (RecastNavMeshImpl)
	{
		return RecastNavMeshImpl->GetNavMeshTileXY(TileIndex, OutX, OutY, OutLayer);
	}

	return false;
}

bool ARecastNavMesh::GetNavMeshTileXY(const FVector& Point, int32& OutX, int32& OutY) const
{
	if (RecastNavMeshImpl)
	{
		return RecastNavMeshImpl->GetNavMeshTileXY(Point, OutX, OutY);
	}

	return false;
}

void ARecastNavMesh::GetNavMeshTilesAt(int32 TileX, int32 TileY, TArray<int32>& Indices) const
{
	if (RecastNavMeshImpl)
	{
		RecastNavMeshImpl->GetNavMeshTilesAt(TileX, TileY, Indices);
	}
}

bool ARecastNavMesh::GetPolysInTile(int32 TileIndex, TArray<FNavPoly>& Polys) const
{
	bool bSuccess = false;
	if (RecastNavMeshImpl)
	{
		bSuccess = RecastNavMeshImpl->GetPolysInTile(TileIndex, Polys);
	}

	return bSuccess;
}

int32 ARecastNavMesh::GetNavMeshTilesCount() const
{
	int32 NumTiles = 0;
	if (RecastNavMeshImpl)
	{
		NumTiles = RecastNavMeshImpl->GetNavMeshTilesCount();
	}

	return NumTiles;
}

void ARecastNavMesh::RemoveTileCacheLayers(int32 TileX, int32 TileY)
{
	if (RecastNavMeshImpl)
	{
		RecastNavMeshImpl->RemoveTileCacheLayers(TileX, TileY);
	}
}
	
void ARecastNavMesh::AddTileCacheLayers(int32 TileX, int32 TileY, const TArray<FNavMeshTileData>& Layers)
{
	if (RecastNavMeshImpl)
	{
		RecastNavMeshImpl->AddTileCacheLayers(TileX, TileY, Layers);
	}
}
	
TArray<FNavMeshTileData> ARecastNavMesh::GetTileCacheLayers(int32 TileX, int32 TileY) const
{
	if (RecastNavMeshImpl)
	{
		return RecastNavMeshImpl->GetTileCacheLayers(TileX, TileY);
	}
	
	return TArray<FNavMeshTileData>();
}

bool ARecastNavMesh::IsResizable() const
{
	return !bFixedTilePoolSize;
}

void ARecastNavMesh::GetEdgesForPathCorridor(const TArray<NavNodeRef>* PathCorridor, TArray<FNavigationPortalEdge>* PathCorridorEdges) const
{
	check(PathCorridor != NULL && PathCorridorEdges != NULL);

	if (RecastNavMeshImpl)
	{
		RecastNavMeshImpl->GetEdgesForPathCorridor(PathCorridor, PathCorridorEdges);
	}
}

FNavLocation ARecastNavMesh::GetRandomPoint(TSharedPtr<const FNavigationQueryFilter> Filter, const UObject* QueryOwner) const
{
	FNavLocation RandomPt;
	if (RecastNavMeshImpl)
	{
		RandomPt = RecastNavMeshImpl->GetRandomPoint(GetRightFilterRef(Filter), QueryOwner);
	}

	return RandomPt;
}

bool ARecastNavMesh::GetRandomReachablePointInRadius(const FVector& Origin, float Radius, FNavLocation& OutResult, TSharedPtr<const FNavigationQueryFilter> Filter, const UObject* QueryOwner) const
{
	if (RecastNavMeshImpl == nullptr || RecastNavMeshImpl->DetourNavMesh == nullptr || Radius <= 0.f)
	{
		return false;
	}

	const FNavigationQueryFilter& FilterInstance = GetRightFilterRef(Filter);

	FRecastSpeciaLinkFilter LinkFilter(UNavigationSystem::GetCurrent(GetWorld()), QueryOwner);
	INITIALIZE_NAVQUERY_WLINKFILTER(NavQuery, FilterInstance.GetMaxSearchNodes(), LinkFilter);

	// inits to "pass all"
	const dtQueryFilter* QueryFilter = (static_cast<const FRecastQueryFilter*>(FilterInstance.GetImplementation()))->GetAsDetourQueryFilter();
	ensure(QueryFilter);
	if (QueryFilter)
	{
		// find starting poly
		// convert start/end pos to Recast coords
		const float Extent[3] = { Radius, Radius, Radius };
		const FVector RecastOrigin = Unreal2RecastPoint(Origin);
		NavNodeRef OriginPolyID = INVALID_NAVNODEREF;
		NavQuery.findNearestPoly(&RecastOrigin.X, Extent, QueryFilter, &OriginPolyID, nullptr);

		dtPolyRef Poly;
		float RandPt[3];
		dtStatus Status = NavQuery.findRandomPointAroundCircle(OriginPolyID, &RecastOrigin.X, Radius
			, QueryFilter, FMath::FRand, &Poly, RandPt);

		if (dtStatusSucceed(Status))
		{
			OutResult = FNavLocation(Recast2UnrealPoint(RandPt), Poly);
			return true;
		}
		else
		{
			OutResult = FNavLocation(Origin, OriginPolyID);
		}
	}

	return false;
}

bool ARecastNavMesh::GetRandomPointInNavigableRadius(const FVector& Origin, float Radius, FNavLocation& OutResult, TSharedPtr<const FNavigationQueryFilter> Filter, const UObject* Querier) const
{
	const FVector ProjectionExtent(NavDataConfig.DefaultQueryExtent.X, NavDataConfig.DefaultQueryExtent.Y, BIG_NUMBER);
	OutResult = FNavLocation(FNavigationSystem::InvalidLocation);
	
	// this is super naive implementation for now. We give it 10 tries, and fail if it's not enough. 
	// The proper solution would involve processing nearby&in-radius tiles in batches until the whole radius of the query is exhausted
	static const int32 IterationsLimit = 10;
	int32 Interation = 0;
	do 
	{
		const float RandomAngle = 2.f * PI * FMath::FRand();
		const float U = FMath::FRand() + FMath::FRand();
		const float RandomRadius = Radius * (U > 1 ? 2.f - U : U);
		const FVector RandomOffset(FMath::Cos(RandomAngle) * RandomRadius, FMath::Sin(RandomAngle) * RandomRadius, 0);
		FVector RandomLocationInRadius = Origin + RandomOffset;

		// naive implementation 
		ProjectPoint(RandomLocationInRadius, OutResult, ProjectionExtent, Filter);
	} while (OutResult.HasNodeRef() == false && ++Interation < IterationsLimit);

	return OutResult.HasNodeRef() == true;
}

bool ARecastNavMesh::GetRandomPointInCluster(NavNodeRef ClusterRef, FNavLocation& OutLocation) const
{
	bool bSuccess = false;
	if (RecastNavMeshImpl)
	{
		bSuccess = RecastNavMeshImpl->GetRandomPointInCluster(ClusterRef, OutLocation);
	}

	return bSuccess;
}

NavNodeRef ARecastNavMesh::GetClusterRef(NavNodeRef PolyRef) const
{
	NavNodeRef ClusterRef = 0;
	if (RecastNavMeshImpl)
	{
		ClusterRef = RecastNavMeshImpl->GetClusterRefFromPolyRef(PolyRef);
	}

	return ClusterRef;
}

bool ARecastNavMesh::ProjectPoint(const FVector& Point, FNavLocation& OutLocation, const FVector& Extent, TSharedPtr<const FNavigationQueryFilter> Filter, const UObject* QueryOwner) const
{
	bool bSuccess = false;
	if (RecastNavMeshImpl)
	{
		bSuccess = RecastNavMeshImpl->ProjectPointToNavMesh(Point, OutLocation, Extent, GetRightFilterRef(Filter), QueryOwner);
	}

	return bSuccess;
}

void ARecastNavMesh::BatchProjectPoints(TArray<FNavigationProjectionWork>& Workload, const FVector& Extent, TSharedPtr<const FNavigationQueryFilter> Filter, const UObject* Querier) const 
{
	if (Workload.Num() == 0 || RecastNavMeshImpl == NULL || RecastNavMeshImpl->DetourNavMesh == NULL)
	{
		return;
	}
	
	const FNavigationQueryFilter& FilterToUse = GetRightFilterRef(Filter);

	FRecastSpeciaLinkFilter LinkFilter(UNavigationSystem::GetCurrent(GetWorld()), Querier);
	INITIALIZE_NAVQUERY_WLINKFILTER(NavQuery, FilterToUse.GetMaxSearchNodes(), LinkFilter);
	const dtQueryFilter* QueryFilter = ((const FRecastQueryFilter*)(FilterToUse.GetImplementation()))->GetAsDetourQueryFilter();
	
	ensure(QueryFilter);
	if (QueryFilter)
	{
		const FVector ModifiedExtent = GetModifiedQueryExtent(Extent);
		FVector RcExtent = Unreal2RecastPoint(ModifiedExtent).GetAbs();
		float ClosestPoint[3];
		dtPolyRef PolyRef;

		for (int32 Idx = 0; Idx < Workload.Num(); Idx++)
		{
			FVector RcPoint = Unreal2RecastPoint(Workload[Idx].Point);
			NavQuery.findNearestPoly(&RcPoint.X, &RcExtent.X, QueryFilter, &PolyRef, ClosestPoint);

			// one last step required due to recast's BVTree imprecision
			if (PolyRef > 0)
			{
				const FVector& UnrealClosestPoint = Recast2UnrealPoint(ClosestPoint);
				if (FVector::DistSquared(UnrealClosestPoint, Workload[Idx].Point) <= ModifiedExtent.SizeSquared())
				{
					Workload[Idx].OutLocation = FNavLocation(UnrealClosestPoint, PolyRef);
					Workload[Idx].bResult = true;
				}
			}
		}
	}
}

bool ARecastNavMesh::ProjectPointMulti(const FVector& Point, TArray<FNavLocation>& OutLocations, const FVector& Extent,
	float MinZ, float MaxZ, TSharedPtr<const FNavigationQueryFilter> Filter, const UObject* QueryOwner) const
{
	bool bSuccess = false;
	if (RecastNavMeshImpl)
	{
		bSuccess = RecastNavMeshImpl->ProjectPointMulti(Point, OutLocations, Extent, MinZ, MaxZ, GetRightFilterRef(Filter), QueryOwner);
	}

	return bSuccess;
}

ENavigationQueryResult::Type ARecastNavMesh::CalcPathCost(const FVector& PathStart, const FVector& PathEnd, float& OutPathCost, TSharedPtr<const FNavigationQueryFilter> QueryFilter, const UObject* QueryOwner) const
{
	float TmpPathLength = 0.f;
	ENavigationQueryResult::Type Result = CalcPathLengthAndCost(PathStart, PathEnd, TmpPathLength, OutPathCost, QueryFilter, QueryOwner);
	return Result;
}

ENavigationQueryResult::Type ARecastNavMesh::CalcPathLength(const FVector& PathStart, const FVector& PathEnd, float& OutPathLength, TSharedPtr<const FNavigationQueryFilter> QueryFilter, const UObject* QueryOwner) const
{
	float TmpPathCost = 0.f;
	ENavigationQueryResult::Type Result = CalcPathLengthAndCost(PathStart, PathEnd, OutPathLength, TmpPathCost, QueryFilter, QueryOwner);
	return Result;
}

ENavigationQueryResult::Type ARecastNavMesh::CalcPathLengthAndCost(const FVector& PathStart, const FVector& PathEnd, float& OutPathLength, float& OutPathCost, TSharedPtr<const FNavigationQueryFilter> QueryFilter, const UObject* QueryOwner) const
{
	ENavigationQueryResult::Type Result = ENavigationQueryResult::Invalid;

	if (RecastNavMeshImpl)
	{
		if ((PathStart - PathEnd).IsNearlyZero() == true)
		{
			OutPathLength = 0.f;
			Result = ENavigationQueryResult::Success;
		}
		else
		{
			TSharedRef<FNavMeshPath> Path = MakeShareable(new FNavMeshPath());
			Path->SetWantsStringPulling(false);
			Path->SetWantsPathCorridor(true);
			
			Result = RecastNavMeshImpl->FindPath(PathStart, PathEnd, Path.Get(), GetRightFilterRef(QueryFilter), QueryOwner);

			if (Result == ENavigationQueryResult::Success || (Result == ENavigationQueryResult::Fail && Path->IsPartial()))
			{
				OutPathLength = Path->GetTotalPathLength();
				OutPathCost = Path->GetCost();
			}
		}
	}

	return Result;
}

bool ARecastNavMesh::DoesNodeContainLocation(NavNodeRef NodeRef, const FVector& WorldSpaceLocation) const
{
	bool bResult = false;
	if (RecastNavMeshImpl != nullptr && RecastNavMeshImpl->GetRecastMesh() != nullptr)
	{
		dtNavMeshQuery NavQuery;
		NavQuery.init(RecastNavMeshImpl->GetRecastMesh(), 0);

		const FVector RcLocation = Unreal2RecastPoint(WorldSpaceLocation);
		if (dtStatusFailed(NavQuery.isPointInsidePoly(NodeRef, &RcLocation.X, bResult)))
		{
			bResult = false;
		}
	}

	return bResult; 
}

NavNodeRef ARecastNavMesh::FindNearestPoly(FVector const& Loc, FVector const& Extent, TSharedPtr<const FNavigationQueryFilter> Filter, const UObject* QueryOwner) const
{
	NavNodeRef PolyRef = 0;
	if (RecastNavMeshImpl)
	{
		PolyRef = RecastNavMeshImpl->FindNearestPoly(Loc, Extent, GetRightFilterRef(Filter), QueryOwner);
	}

	return PolyRef;
}

float ARecastNavMesh::FindDistanceToWall(const FVector& StartLoc, TSharedPtr<const FNavigationQueryFilter> Filter, float MaxDistance) const
{
	if (HasValidNavmesh() == false)
	{
		return 0.f;
	}

	const FNavigationQueryFilter& FilterToUse = GetRightFilterRef(Filter);

	INITIALIZE_NAVQUERY(NavQuery, FilterToUse.GetMaxSearchNodes());
	const dtQueryFilter* QueryFilter = ((const FRecastQueryFilter*)(FilterToUse.GetImplementation()))->GetAsDetourQueryFilter();

	if (QueryFilter == nullptr)
	{
		UE_VLOG(this, LogNavigation, Warning, TEXT("ARecastNavMesh::FindDistanceToWall failing due to QueryFilter == NULL"));
		return 0.f;
	}

	const FVector NavExtent = GetModifiedQueryExtent(GetDefaultQueryExtent());
	const float Extent[3] = { NavExtent.X, NavExtent.Z, NavExtent.Y };

	const FVector RecastStart = Unreal2RecastPoint(StartLoc);

	NavNodeRef StartNode = INVALID_NAVNODEREF;
	NavQuery.findNearestPoly(&RecastStart.X, Extent, QueryFilter, &StartNode, NULL);

	if (StartNode != INVALID_NAVNODEREF)
	{
		float TmpHitPos[3], TmpHitNormal[3];
		float DistanceToWall = 0.f;
		const dtStatus RaycastStatus = NavQuery.findDistanceToWall(StartNode, &RecastStart.X, MaxDistance, QueryFilter
			, &DistanceToWall, TmpHitPos, TmpHitNormal);

		if (dtStatusSucceed(RaycastStatus))
		{
			return DistanceToWall;
		}
	}

	return 0.f;
}

void ARecastNavMesh::UpdateCustomLink(const INavLinkCustomInterface* CustomLink)
{
	TSubclassOf<UNavArea> AreaClass = CustomLink->GetLinkAreaClass();
	const int32 UserId = CustomLink->GetLinkId();
	const int32 AreaId = GetAreaID(AreaClass);
	if (AreaId >= 0 && RecastNavMeshImpl)
	{
		UNavArea* DefArea = (UNavArea*)(AreaClass->GetDefaultObject());

		RecastNavMeshImpl->UpdateNavigationLinkArea(UserId, AreaId, DefArea->GetAreaFlags());
		RecastNavMeshImpl->UpdateSegmentLinkArea(UserId, AreaId, DefArea->GetAreaFlags());
	}
}

void ARecastNavMesh::UpdateNavigationLinkArea(int32 UserId, TSubclassOf<UNavArea> AreaClass) const
{
	int32 AreaId = GetAreaID(AreaClass);
	if (AreaId >= 0 && RecastNavMeshImpl)
	{
		UNavArea* DefArea = (UNavArea*)(AreaClass->GetDefaultObject());

		RecastNavMeshImpl->UpdateNavigationLinkArea(UserId, AreaId, DefArea->GetAreaFlags());
	}
}

void ARecastNavMesh::UpdateSegmentLinkArea(int32 UserId, TSubclassOf<UNavArea> AreaClass) const
{
	int32 AreaId = GetAreaID(AreaClass);
	if (AreaId >= 0 && RecastNavMeshImpl)
	{
		UNavArea* DefArea = (UNavArea*)(AreaClass->GetDefaultObject());

		RecastNavMeshImpl->UpdateSegmentLinkArea(UserId, AreaId, DefArea->GetAreaFlags());
	}
}

bool ARecastNavMesh::GetPolyCenter(NavNodeRef PolyID, FVector& OutCenter) const
{
	bool bSuccess = false;
	if (RecastNavMeshImpl)
	{
		bSuccess = RecastNavMeshImpl->GetPolyCenter(PolyID, OutCenter);
	}

	return bSuccess;
}

bool ARecastNavMesh::GetPolyVerts(NavNodeRef PolyID, TArray<FVector>& OutVerts) const
{
	bool bSuccess = false;
	if (RecastNavMeshImpl)
	{
		bSuccess = RecastNavMeshImpl->GetPolyVerts(PolyID, OutVerts);
	}

	return bSuccess;
}

uint32 ARecastNavMesh::GetPolyAreaID(NavNodeRef PolyID) const
{
	uint32 AreaID = RECAST_DEFAULT_AREA;
	if (RecastNavMeshImpl)
	{
		AreaID = RecastNavMeshImpl->GetPolyAreaID(PolyID);
	}

	return AreaID;
}

bool ARecastNavMesh::GetPolyFlags(NavNodeRef PolyID, uint16& PolyFlags, uint16& AreaFlags) const
{
	bool bFound = false;
	if (RecastNavMeshImpl)
	{
		uint8 AreaType = RECAST_DEFAULT_AREA;
		bFound = RecastNavMeshImpl->GetPolyData(PolyID, PolyFlags, AreaType);
		if (bFound)
		{
			const UClass* AreaClass = GetAreaClass(AreaType);
			const UNavArea* DefArea = AreaClass ? ((UClass*)AreaClass)->GetDefaultObject<UNavArea>() : NULL;
			AreaFlags = DefArea ? DefArea->GetAreaFlags() : 0;
		}
	}

	return bFound;
}

bool ARecastNavMesh::GetClosestPointOnPoly(NavNodeRef PolyID, const FVector& TestPt, FVector& PointOnPoly) const
{
	bool bFound = false;
	if (RecastNavMeshImpl)
	{
		bFound = RecastNavMeshImpl->GetClosestPointOnPoly(PolyID, TestPt, PointOnPoly);
	}

	return bFound;
}

bool ARecastNavMesh::GetPolyTileIndex(NavNodeRef PolyID, uint32& PolyIndex, uint32& TileIndex) const
{
	bool bFound = false;
	if (RecastNavMeshImpl)
	{
		bFound = RecastNavMeshImpl->GetPolyTileIndex(PolyID, PolyIndex, TileIndex);
	}

	return bFound;
}

bool ARecastNavMesh::GetLinkEndPoints(NavNodeRef LinkPolyID, FVector& PointA, FVector& PointB) const
{
	bool bSuccess = false;
	if (RecastNavMeshImpl)
	{
		bSuccess = RecastNavMeshImpl->GetLinkEndPoints(LinkPolyID, PointA, PointB);
	}

	return bSuccess;
}

bool ARecastNavMesh::IsCustomLink(NavNodeRef LinkPolyID) const
{
	bool bSuccess = false;
	if (RecastNavMeshImpl)
	{
		bSuccess = RecastNavMeshImpl->IsCustomLink(LinkPolyID);
	}

	return bSuccess;
}

bool ARecastNavMesh::GetClusterBounds(NavNodeRef ClusterRef, FBox& OutBounds) const
{
	bool bSuccess = false;
	if (RecastNavMeshImpl)
	{
		bSuccess = RecastNavMeshImpl->GetClusterBounds(ClusterRef, OutBounds);
	}

	return bSuccess;
}

bool ARecastNavMesh::GetPolysWithinPathingDistance(FVector const& StartLoc, const float PathingDistance, TArray<NavNodeRef>& FoundPolys,
	TSharedPtr<const FNavigationQueryFilter> Filter, const UObject* QueryOwner, FRecastDebugPathfindingData* DebugData) const
{
	bool bSuccess = false;
	if (RecastNavMeshImpl)
	{
		bSuccess = RecastNavMeshImpl->GetPolysWithinPathingDistance(StartLoc, PathingDistance, GetRightFilterRef(Filter), QueryOwner, FoundPolys, DebugData);
	}

	return bSuccess;
}

void ARecastNavMesh::GetDebugGeometry(FRecastDebugGeometry& OutGeometry, int32 TileIndex) const
{
	if (RecastNavMeshImpl)
	{
		RecastNavMeshImpl->GetDebugGeometry(OutGeometry, TileIndex);
	}
}

void ARecastNavMesh::RequestDrawingUpdate()
{
#if !UE_BUILD_SHIPPING
	if (UNavMeshRenderingComponent::IsNavigationShowFlagSet(GetWorld()))
	{
		DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.Requesting navmesh redraw"),
		STAT_FSimpleDelegateGraphTask_RequestingNavmeshRedraw,
			STATGROUP_TaskGraphTasks);

		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateUObject(this, &ARecastNavMesh::UpdateDrawing),
			GET_STATID(STAT_FSimpleDelegateGraphTask_RequestingNavmeshRedraw), NULL, ENamedThreads::GameThread);
	}
#endif // !UE_BUILD_SHIPPING
}

void ARecastNavMesh::UpdateDrawing()
{
#if DO_NAVMESH_DEBUG_DRAWING_PER_TILE
	ConstructRenderingComponentImpl();
#endif
	UpdateNavMeshDrawing();
}

void ARecastNavMesh::DrawDebugPathCorridor(NavNodeRef const* PathPolys, int32 NumPathPolys, bool bPersistent) const
{
	static const FColor PathLineColor(255, 128, 0);

	// draw poly outlines
	TArray<FVector> PolyVerts;
	for (int32 PolyIdx=0; PolyIdx < NumPathPolys; ++PolyIdx)
	{
		if ( GetPolyVerts(PathPolys[PolyIdx], PolyVerts) )
		{
			for (int32 VertIdx=0; VertIdx < PolyVerts.Num()-1; ++VertIdx)
			{
				DrawDebugLine(GetWorld(), PolyVerts[VertIdx], PolyVerts[VertIdx+1], PathLineColor, bPersistent);
			}
			DrawDebugLine(GetWorld(), PolyVerts[PolyVerts.Num()-1], PolyVerts[0], PathLineColor, bPersistent);
		}
	}

	// draw ordered poly links
	if (NumPathPolys > 0)
	{
		FVector PolyCenter;
		FVector NextPolyCenter;
		if ( GetPolyCenter(PathPolys[0], NextPolyCenter) )			// prime the pump
		{
			for (int32 PolyIdx=0; PolyIdx < NumPathPolys-1; ++PolyIdx)
			{
				PolyCenter = NextPolyCenter;
				if ( GetPolyCenter(PathPolys[PolyIdx+1], NextPolyCenter) )
				{
					DrawDebugLine(GetWorld(), PolyCenter, NextPolyCenter, PathLineColor, bPersistent);
					DrawDebugBox(GetWorld(), PolyCenter, FVector(5.f), PathLineColor, bPersistent);
				}
			}
		}
	}
}

void ARecastNavMesh::InvalidateAffectedPaths(const TArray<uint32>& ChangedTiles)
{
	const int32 PathsCount = ActivePaths.Num();
	const int32 ChangedTilesCount = ChangedTiles.Num();
	
	if (ChangedTilesCount == 0 || PathsCount == 0)
	{
		return;
	}
	
	FNavPathWeakPtr* WeakPathPtr = (ActivePaths.GetData() + PathsCount - 1);
	for (int32 PathIndex = PathsCount - 1; PathIndex >= 0; --PathIndex, --WeakPathPtr)
	{
		FNavPathSharedPtr SharedPath = WeakPathPtr->Pin();
		if (WeakPathPtr->IsValid() == false)
		{
			ActivePaths.RemoveAtSwap(PathIndex, 1, /*bAllowShrinking=*/false);
		}
		else 
		{
			// iterate through all tile refs in FreshTilesCopy and 
			const FNavMeshPath* Path = (const FNavMeshPath*)(SharedPath.Get());
			if (Path->IsReady() == false)
			{
				// path not filled yet anyway
				continue;
			}

			const int32 PathLenght = Path->PathCorridor.Num();
			const NavNodeRef* PathPoly = Path->PathCorridor.GetData();
			for (int32 NodeIndex = 0; NodeIndex < PathLenght; ++NodeIndex, ++PathPoly)
			{
				const uint32 NodeTileIdx = RecastNavMeshImpl->GetTileIndexFromPolyRef(*PathPoly);
				if (ChangedTiles.Contains(NodeTileIdx))
				{
					SharedPath->Invalidate();
					ActivePaths.RemoveAtSwap(PathIndex, 1, /*bAllowShrinking=*/false);
					break;
				}
			}
		}
	}
}

URecastNavMeshDataChunk* ARecastNavMesh::GetNavigationDataChunk(ULevel* InLevel) const
{
	FName ThisName = GetFName();
	int32 ChunkIndex = InLevel->NavDataChunks.IndexOfByPredicate([&](UNavigationDataChunk* Chunk) 
	{
		return Chunk->NavigationDataName == ThisName;
	});
	
	URecastNavMeshDataChunk* RcNavDataChunk = nullptr;
	if (ChunkIndex != INDEX_NONE)
	{
		RcNavDataChunk = Cast<URecastNavMeshDataChunk>(InLevel->NavDataChunks[ChunkIndex]);
	}
		
	return RcNavDataChunk;
}

void ARecastNavMesh::OnNavMeshGenerationFinished()
{
	UWorld* World = GetWorld();

	if (World != nullptr && World->IsPendingKill() == false)
	{
#if WITH_EDITOR	
		// For navmeshes that support streaming create navigation data holders in each streaming level
		// so parts of navmesh can be streamed in/out with those levels
		if (!World->IsGameWorld())
		{
			const auto& Levels = World->GetLevels();
			for (auto Level : Levels)
			{
				if (Level->IsPersistentLevel())
				{
					continue;
				}

				URecastNavMeshDataChunk* NavDataChunk = GetNavigationDataChunk(Level);

				if (SupportsStreaming())
				{
					// We use navigation volumes that belongs to this streaming level to find tiles we want to save
					TArray<int32> LevelTiles;
					TArray<FBox> LevelNavBounds = GetNavigableBoundsInLevel(Level);
					RecastNavMeshImpl->GetNavMeshTilesIn(LevelNavBounds, LevelTiles);

					if (LevelTiles.Num())
					{
						// Create new chunk only if we have something to save in it			
						if (NavDataChunk == nullptr)
						{
							NavDataChunk = NewObject<URecastNavMeshDataChunk>(Level);
							NavDataChunk->NavigationDataName = GetFName();
							Level->NavDataChunks.Add(NavDataChunk);
						}

						NavDataChunk->GatherTiles(RecastNavMeshImpl, LevelTiles);
						NavDataChunk->MarkPackageDirty();
						continue;
					}
				}

				// stale data that is left in the level
				if (NavDataChunk)
				{
					// clear it
					NavDataChunk->ReleaseTiles();
					NavDataChunk->MarkPackageDirty();
					Level->NavDataChunks.Remove(NavDataChunk);
				}
			}
		}
#endif// WITH_EDITOR

		if (World->GetNavigationSystem())
		{
			World->GetNavigationSystem()->OnNavigationGenerationFinished(*this);
		}
	}
}

#if !UE_BUILD_SHIPPING
uint32 ARecastNavMesh::LogMemUsed() const 
{
	const uint32 SuperMemUsed = Super::LogMemUsed();
	uint32 MemUsed = 0;

	UE_LOG(LogNavigation, Display, TEXT("%s: ARecastNavMesh: %u\n    self: %d"), *GetName(), MemUsed, sizeof(ARecastNavMesh));	

	return MemUsed + SuperMemUsed;
}

#endif // !UE_BUILD_SHIPPING

SIZE_T ARecastNavMesh::GetResourceSize(EResourceSizeMode::Type Mode)
{
	return 0;
}

uint16 ARecastNavMesh::GetDefaultForbiddenFlags() const
{
	return FPImplRecastNavMesh::GetFilterForbiddenFlags((const FRecastQueryFilter*)DefaultQueryFilter->GetImplementation());
}

void ARecastNavMesh::SetDefaultForbiddenFlags(uint16 ForbiddenAreaFlags)
{
	FPImplRecastNavMesh::SetFilterForbiddenFlags((FRecastQueryFilter*)DefaultQueryFilter->GetImplementation(), ForbiddenAreaFlags);
}

void ARecastNavMesh::SetMaxSimultaneousTileGenerationJobsCount(int32 NewJobsCountLimit) 
{
	const int32 NewCount = NewJobsCountLimit > 0 ? NewJobsCountLimit : 1;
	if (MaxSimultaneousTileGenerationJobsCount != NewCount)
	{
		MaxSimultaneousTileGenerationJobsCount = NewCount;
		if (GetGenerator() != nullptr)
		{
			FRecastNavMeshGenerator* MyGenerator = static_cast<FRecastNavMeshGenerator*>(GetGenerator());
			MyGenerator->SetMaxTileGeneratorTasks(NewCount);
		}
	}
}

bool ARecastNavMesh::FilterPolys(TArray<NavNodeRef>& PolyRefs, const FRecastQueryFilter* Filter, const UObject* QueryOwner) const
{
	bool bSuccess = false;
	if (RecastNavMeshImpl)
	{
		bSuccess = RecastNavMeshImpl->FilterPolys(PolyRefs, Filter, QueryOwner);
	}

	return bSuccess;
}

void ARecastNavMesh::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	if (RecastNavMeshImpl)
	{
		RecastNavMeshImpl->ApplyWorldOffset(InOffset, bWorldShift);
	}

	Super::ApplyWorldOffset(InOffset, bWorldShift);
	RequestDrawingUpdate();
}

void ARecastNavMesh::OnStreamingLevelAdded(ULevel* InLevel, UWorld* InWorld)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RecastNavMesh_OnStreamingLevelAdded);
	
	bWantsUpdate = true;

	if (SupportsStreaming() && RecastNavMeshImpl)
	{
		URecastNavMeshDataChunk* NavDataChunk = GetNavigationDataChunk(InLevel);
		if (NavDataChunk)
		{
			TArray<uint32> AttachedIndices = NavDataChunk->AttachTiles(*RecastNavMeshImpl);
			if (AttachedIndices.Num() > 0)
			{
				InvalidateAffectedPaths(AttachedIndices);
				RequestDrawingUpdate();
			}
		}
	}
}

void ARecastNavMesh::OnStreamingLevelRemoved(ULevel* InLevel, UWorld* InWorld)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RecastNavMesh_OnStreamingLevelRemoved);
	
	if (SupportsStreaming() && RecastNavMeshImpl)
	{
		URecastNavMeshDataChunk* NavDataChunk = GetNavigationDataChunk(InLevel);
		if (NavDataChunk)
		{
			TArray<uint32> DetachedIndices = NavDataChunk->DetachTiles(*RecastNavMeshImpl);
			if (DetachedIndices.Num() > 0)
			{
				InvalidateAffectedPaths(DetachedIndices);
				RequestDrawingUpdate();
			}
		}
	}
}

bool ARecastNavMesh::AdjustLocationWithFilter(const FVector& StartLoc, FVector& OutAdjustedLocation, const FNavigationQueryFilter& Filter, const UObject* QueryOwner) const
{
	INITIALIZE_NAVQUERY(NavQuery, Filter.GetMaxSearchNodes());

	const FVector NavExtent = GetModifiedQueryExtent(GetDefaultQueryExtent());
	const float Extent[3] = { NavExtent.X, NavExtent.Z, NavExtent.Y };

	const dtQueryFilter* QueryFilter = ((const FRecastQueryFilter*)(Filter.GetImplementation()))->GetAsDetourQueryFilter();
	ensure(QueryFilter);

	FVector RecastStart = Unreal2RecastPoint(StartLoc);
	FVector RecastAdjustedPoint = Unreal2RecastPoint(StartLoc);
	NavNodeRef StartPolyID = INVALID_NAVNODEREF;
	NavQuery.findNearestPoly(&RecastStart.X, Extent, QueryFilter, &StartPolyID, &RecastAdjustedPoint.X);

	if (FVector::DistSquared(RecastStart, RecastAdjustedPoint) < KINDA_SMALL_NUMBER)
	{
		OutAdjustedLocation = StartLoc;
		return false;
	}
	else
	{
		OutAdjustedLocation = Recast2UnrealPoint(RecastAdjustedPoint);
		// move it just a bit further - otherwise recast can still pick "wrong" poly when 
		// later projecting StartLoc (meaning a poly we want to filter out with 
		// QueryFilter here)
		OutAdjustedLocation += (OutAdjustedLocation - StartLoc).GetSafeNormal() * 0.1f;
		return true;
	}
}

FPathFindingResult ARecastNavMesh::FindPath(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query)
{
	const ANavigationData* Self = Query.NavData.Get();
	check(Cast<const ARecastNavMesh>(Self));

	const ARecastNavMesh* RecastNavMesh = (const ARecastNavMesh*)Self;
	if (Self == NULL || RecastNavMesh->RecastNavMeshImpl == NULL)
	{
		return ENavigationQueryResult::Error;
	}
		
	FPathFindingResult Result;
	Result.Path = Query.PathInstanceToFill.IsValid() ? Query.PathInstanceToFill : Self->CreatePathInstance<FNavMeshPath>(Query.Owner.Get());

	FNavMeshPath* NavMeshPath = (FNavMeshPath*)Result.Path.Get();
	NavMeshPath->SetFilter(Query.QueryFilter);
	NavMeshPath->ApplyFlags(Query.NavDataFlags);

	if ((Query.StartLocation - Query.EndLocation).IsNearlyZero() == true)
	{
		Result.Path->GetPathPoints().Reset();
		Result.Path->GetPathPoints().Add(FNavPathPoint(Query.EndLocation));
		Result.Result = ENavigationQueryResult::Success;
	}
	else
	{
		if(Query.QueryFilter.IsValid())
		{
			Result.Result = RecastNavMesh->RecastNavMeshImpl->FindPath(Query.StartLocation, Query.EndLocation, *NavMeshPath,
				*(Query.QueryFilter.Get()), Query.Owner.Get());

			const bool bPartialPath = Result.IsPartial();
			if (bPartialPath)
			{
				Result.Result = Query.bAllowPartialPaths ? ENavigationQueryResult::Success : ENavigationQueryResult::Fail;
			}
		}
		else
		{
			Result.Result = ENavigationQueryResult::Error;
		}
	}

	return Result;
}

bool ARecastNavMesh::TestPath(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query, int32* NumVisitedNodes)
{
	const ANavigationData* Self = Query.NavData.Get();
	check(Cast<const ARecastNavMesh>(Self));

	const ARecastNavMesh* RecastNavMesh = (const ARecastNavMesh*)Self;
	if (Self == NULL || RecastNavMesh->RecastNavMeshImpl == NULL)
	{
		return false;
	}

	bool bPathExists = true;
	if ((Query.StartLocation - Query.EndLocation).IsNearlyZero() == false)
	{
		ENavigationQueryResult::Type Result = RecastNavMesh->RecastNavMeshImpl->TestPath(Query.StartLocation, Query.EndLocation, *(Query.QueryFilter.Get()), Query.Owner.Get(), NumVisitedNodes);
		bPathExists = (Result == ENavigationQueryResult::Success);
	}

	return bPathExists;
}

bool ARecastNavMesh::TestHierarchicalPath(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query, int32* NumVisitedNodes)
{
	const ANavigationData* Self = Query.NavData.Get();
	check(Cast<const ARecastNavMesh>(Self));

	const ARecastNavMesh* RecastNavMesh = (const ARecastNavMesh*)Self;
	if (Self == NULL || RecastNavMesh->RecastNavMeshImpl == NULL)
	{
		return false;
	}

	const bool bCanUseHierachicalPath = (Query.QueryFilter == RecastNavMesh->GetDefaultQueryFilter());
	bool bPathExists = true;

	if ((Query.StartLocation - Query.EndLocation).IsNearlyZero() == false)
	{
		bool bUseFallbackSearch = false;
		if (bCanUseHierachicalPath)
		{
			ENavigationQueryResult::Type Result = RecastNavMesh->RecastNavMeshImpl->TestClusterPath(Query.StartLocation, Query.EndLocation, NumVisitedNodes);
			bPathExists = (Result == ENavigationQueryResult::Success);

			if (Result == ENavigationQueryResult::Error)
			{
				bUseFallbackSearch = true;
			}
		}
		else
		{
			UE_LOG(LogNavigation, Log, TEXT("Hierarchical path finding test failed: filter doesn't match!"));
			bUseFallbackSearch = true;
		}

		if (bUseFallbackSearch)
		{
			ENavigationQueryResult::Type Result = RecastNavMesh->RecastNavMeshImpl->TestPath(Query.StartLocation, Query.EndLocation, *(Query.QueryFilter.Get()), Query.Owner.Get(), NumVisitedNodes);
			bPathExists = (Result == ENavigationQueryResult::Success);
		}
	}

	return bPathExists;
}

bool ARecastNavMesh::NavMeshRaycast(const ANavigationData* Self, const FVector& RayStart, const FVector& RayEnd, FVector& HitLocation, TSharedPtr<const FNavigationQueryFilter> QueryFilter,const UObject* QueryOwner, FRaycastResult& Result)
{
	check(Cast<const ARecastNavMesh>(Self));

	const ARecastNavMesh* RecastNavMesh = (const ARecastNavMesh*)Self;
	if (Self == NULL || RecastNavMesh->RecastNavMeshImpl == NULL)
	{
		HitLocation = RayStart;
		return true;
	}

	RecastNavMesh->RecastNavMeshImpl->Raycast2D(RayStart, RayEnd, RecastNavMesh->GetRightFilterRef(QueryFilter), QueryOwner, Result);

	HitLocation = Result.HasHit() ? (RayStart + (RayEnd - RayStart) * Result.HitTime) : RayEnd;

	return Result.HasHit();
}

bool ARecastNavMesh::NavMeshRaycast(const ANavigationData* Self, NavNodeRef RayStartNode, const FVector& RayStart, const FVector& RayEnd, FVector& HitLocation, TSharedPtr<const FNavigationQueryFilter> QueryFilter, const UObject* QueryOwner)
{
	check(Cast<const ARecastNavMesh>(Self));

	const ARecastNavMesh* RecastNavMesh = (const ARecastNavMesh*)Self;
	if (Self == NULL || RecastNavMesh->RecastNavMeshImpl == NULL)
	{
		HitLocation = RayStart;
		return true;
	}

	FRaycastResult Result;
	RecastNavMesh->RecastNavMeshImpl->Raycast2D(RayStartNode, RayStart, RayEnd, RecastNavMesh->GetRightFilterRef(QueryFilter), QueryOwner, Result);

	HitLocation = Result.HasHit() ? (RayStart + (RayEnd - RayStart) * Result.HitTime) : RayEnd;

	return Result.HasHit();
}

void ARecastNavMesh::BatchRaycast(TArray<FNavigationRaycastWork>& Workload, TSharedPtr<const FNavigationQueryFilter> Filter, const UObject* Querier) const
{
	if (RecastNavMeshImpl == NULL || Workload.Num() == 0 || RecastNavMeshImpl->DetourNavMesh == NULL)
	{
		return;
	}

	const FNavigationQueryFilter& FilterToUse = GetRightFilterRef(Filter);

	FRecastSpeciaLinkFilter LinkFilter(UNavigationSystem::GetCurrent(GetWorld()), Querier);
	INITIALIZE_NAVQUERY_WLINKFILTER(NavQuery, FilterToUse.GetMaxSearchNodes(), LinkFilter);
	const dtQueryFilter* QueryFilter = ((const FRecastQueryFilter*)(FilterToUse.GetImplementation()))->GetAsDetourQueryFilter();
	
	if (QueryFilter == NULL)
	{
		UE_VLOG(this, LogNavigation, Warning, TEXT("FPImplRecastNavMesh::FindPath failing due to QueryFilter == NULL"));
		return;
	}
	
	const FVector NavExtent = GetModifiedQueryExtent(GetDefaultQueryExtent());
	const float Extent[3] = { NavExtent.X, NavExtent.Z, NavExtent.Y };

	for (auto& WorkItem : Workload)
	{
		ARecastNavMesh::FRaycastResult RaycastResult;

		const FVector RecastStart = Unreal2RecastPoint(WorkItem.RayStart);
		const FVector RecastEnd = Unreal2RecastPoint(WorkItem.RayEnd);

		NavNodeRef StartNode = INVALID_NAVNODEREF;
		NavQuery.findNearestPoly(&RecastStart.X, Extent, QueryFilter, &StartNode, NULL);

		if (StartNode != INVALID_NAVNODEREF)
		{
			const dtStatus RaycastStatus = NavQuery.raycast(StartNode, &RecastStart.X, &RecastEnd.X
				, QueryFilter, &RaycastResult.HitTime, &RaycastResult.HitNormal.X
				, RaycastResult.CorridorPolys, &RaycastResult.CorridorPolysCount, RaycastResult.GetMaxCorridorSize());

			if (dtStatusSucceed(RaycastStatus) && RaycastResult.HasHit())
			{
				WorkItem.bDidHit = true;
				WorkItem.HitLocation = FNavLocation(WorkItem.RayStart + (WorkItem.RayEnd - WorkItem.RayStart) * RaycastResult.HitTime, RaycastResult.GetLastNodeRef());
			}
		}
	}
}

bool ARecastNavMesh::IsSegmentOnNavmesh(const FVector& SegmentStart, const FVector& SegmentEnd, TSharedPtr<const FNavigationQueryFilter> Filter, const UObject* QueryOwner) const
{
	if (RecastNavMeshImpl == NULL)
	{
		return false;
	}
	
	FRaycastResult Result;
	RecastNavMeshImpl->Raycast2D(SegmentStart, SegmentEnd, GetRightFilterRef(Filter), QueryOwner, Result);

	return Result.HasHit() == false;
}

bool ARecastNavMesh::FindStraightPath(const FVector& StartLoc, const FVector& EndLoc, const TArray<NavNodeRef>& PathCorridor, TArray<FNavPathPoint>& PathPoints, TArray<uint32>* CustomLinks) const
{
	bool bResult = false;
	if (RecastNavMeshImpl)
	{
		bResult = RecastNavMeshImpl->FindStraightPath(StartLoc, EndLoc, PathCorridor, PathPoints, CustomLinks);
	}

	return bResult;
}

int32 ARecastNavMesh::DebugPathfinding(const FPathFindingQuery& Query, TArray<FRecastDebugPathfindingData>& Steps)
{
	int32 NumSteps = 0;

	const ANavigationData* Self = Query.NavData.Get();
	check(Cast<const ARecastNavMesh>(Self));

	const ARecastNavMesh* RecastNavMesh = (const ARecastNavMesh*)Self;
	if (Self == NULL || RecastNavMesh->RecastNavMeshImpl == NULL)
	{
		return false;
	}

	if ((Query.StartLocation - Query.EndLocation).IsNearlyZero() == false)
	{
		NumSteps = RecastNavMesh->RecastNavMeshImpl->DebugPathfinding(Query.StartLocation, Query.EndLocation, *(Query.QueryFilter.Get()), Query.Owner.Get(), Steps);
	}

	return NumSteps;
}

void ARecastNavMesh::UpdateNavVersion() 
{ 
	NavMeshVersion = NAVMESHVER_LATEST; 
}

#if WITH_EDITOR

void ARecastNavMesh::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName NAME_Generation = FName(TEXT("Generation"));
	static const FName NAME_Display = FName(TEXT("Display"));
	static const FName NAME_TileNumberHardLimit = GET_MEMBER_NAME_CHECKED(ARecastNavMesh, TileNumberHardLimit);

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property != NULL)
	{
		if (FObjectEditorUtils::GetCategoryFName(PropertyChangedEvent.Property) == NAME_Generation)
		{
			FName PropName = PropertyChangedEvent.Property->GetFName();
			
			if (PropName == GET_MEMBER_NAME_CHECKED(ARecastNavMesh,AgentRadius) ||
				PropName == GET_MEMBER_NAME_CHECKED(ARecastNavMesh,TileSizeUU) ||
				PropName == GET_MEMBER_NAME_CHECKED(ARecastNavMesh,CellSize))
			{
				// rule of thumb, dimension tile shouldn't be less than 16 * AgentRadius
				if (TileSizeUU < 16.f * AgentRadius)
				{
					TileSizeUU = FMath::Max(16.f * AgentRadius, RECAST_MIN_TILE_SIZE);
				}

				// tile's can't be too big, otherwise we'll crash while tryng to allocate
				// memory during navmesh generation
				TileSizeUU = FMath::Clamp(TileSizeUU, CellSize, ArbitraryMaxVoxelTileSize * CellSize);

				// update config
				FillConfig(NavDataConfig);
			}
			else if (PropName == NAME_TileNumberHardLimit)
			{
				TileNumberHardLimit = 1 << (FMath::CeilToInt(FMath::Log2(TileNumberHardLimit)));
				UpdatePolyRefBitsPreview();
			}

			if (HasAnyFlags(RF_ClassDefaultObject) == false)
			{
				RebuildAll();
			}
		}
		else if (FObjectEditorUtils::GetCategoryFName(PropertyChangedEvent.Property) == NAME_Display)
		{
			RequestDrawingUpdate();
		}
	}
}

#endif // WITH_EDITOR

bool ARecastNavMesh::NeedsRebuild() const
{
	bool bLooksLikeNeeded = !RecastNavMeshImpl || RecastNavMeshImpl->GetRecastMesh() == 0;
	if (NavDataGenerator.IsValid())
	{
		return bLooksLikeNeeded || NavDataGenerator->GetNumRemaningBuildTasks() > 0;
	}

	return bLooksLikeNeeded;
}

bool ARecastNavMesh::SupportsRuntimeGeneration() const
{
	// Generator should be disabled for Static navmesh
	return (RuntimeGeneration != ERuntimeGenerationType::Static);
}

bool ARecastNavMesh::SupportsStreaming() const
{
	// Actually nothing prevents us to support streaming with dynamic generation
	// Right now streaming in sub-level causes navmesh to build itself, so no point to stream tiles in
	return (RuntimeGeneration != ERuntimeGenerationType::Dynamic);
}

void ARecastNavMesh::ConditionalConstructGenerator()
{	
	if (NavDataGenerator.IsValid())
	{
		NavDataGenerator->CancelBuild();
		NavDataGenerator.Reset();
	}

	UWorld* World = GetWorld();
	check(World);
	const bool bRequiresGenerator = SupportsRuntimeGeneration() || !World->IsGameWorld();
	if (bRequiresGenerator)
	{
		NavDataGenerator = MakeShareable(new FRecastNavMeshGenerator(*this));

		if (World->GetNavigationSystem())
		{
			RestrictBuildingToActiveTiles(World->GetNavigationSystem()->IsActiveTilesGenerationEnabled());
		}
	}
}

bool ARecastNavMesh::IsVoxelCacheEnabled()
{
#if RECAST_ASYNC_REBUILDING
	// voxel cache is using static buffers to minimize memory impact
	// therefore it can run only with synchronous navmesh rebuilds
	return false;
#endif

	ARecastNavMesh* DefOb = (ARecastNavMesh*)ARecastNavMesh::StaticClass()->GetDefaultObject();
	return DefOb && DefOb->bUseVoxelCache;
}

const FRecastQueryFilter* ARecastNavMesh::GetNamedFilter(ERecastNamedFilter::Type FilterType)
{
	check(FilterType < ERecastNamedFilter::NamedFiltersCount); 
	return NamedFilters[FilterType];
}

#undef INITIALIZE_NAVQUERY

void ARecastNavMesh::UpdateNavObject()
{
	OnNavMeshUpdate.Broadcast();
}
#endif	//WITH_RECAST

bool ARecastNavMesh::HasValidNavmesh() const
{
#if WITH_RECAST
	return (RecastNavMeshImpl && RecastNavMeshImpl->DetourNavMesh && RecastNavMeshImpl->DetourNavMesh->isEmpty() == false);
#else
	return false;
#endif // WITH_RECAST
}

//----------------------------------------------------------------------//
// RecastNavMesh: Active Tiles 
//----------------------------------------------------------------------//
#if WITH_RECAST
void ARecastNavMesh::UpdateActiveTiles(const TArray<FNavigationInvokerRaw>& InvokerLocations)
{
	if (HasValidNavmesh() == false)
	{
		return;
	}

	FRecastNavMeshGenerator* MyGenerator = static_cast<FRecastNavMeshGenerator*>(GetGenerator());
	if (MyGenerator == nullptr)
	{
		return;
	}

	const dtNavMeshParams* NavParams = GetRecastNavMeshImpl()->DetourNavMesh->getParams();
	check(NavParams && MyGenerator);
	const FRecastBuildConfig& Config = MyGenerator->GetConfig();
	const FVector NavmeshOrigin = Recast2UnrealPoint(NavParams->orig);
	const float TileDim = Config.tileSize * Config.cs;
	const FVector TileCenterOffset(TileDim, TileDim, 0);

	TArray<FIntPoint>& ActiveTiles = GetActiveTiles();
	TArray<FIntPoint> OldActiveSet = ActiveTiles;
	TArray<FIntPoint> TilesInMinDistance;
	TArray<FIntPoint> TilesInMaxDistance;
	TilesInMinDistance.Reserve(ActiveTiles.Num());
	TilesInMaxDistance.Reserve(ActiveTiles.Num());
	ActiveTiles.Reset();

	//const int32 TileRadius = FMath::CeilToInt(Radius / TileDim);
	static const float SqareRootOf2 = FMath::Sqrt(2.f);

	for (const auto& Invoker : InvokerLocations)
	{
		const FVector InvokerRelativeLocation = (NavmeshOrigin - Invoker.Location);
		const float TileCenterDistanceToRemoveSq = FMath::Square(TileDim * SqareRootOf2 / 2 + Invoker.RadiusMax);
		const float TileCenterDistanceToAddSq = FMath::Square(TileDim * SqareRootOf2 / 2 + Invoker.RadiusMin);

		const int32 MinTileX = FMath::FloorToInt((InvokerRelativeLocation.X - Invoker.RadiusMax) / TileDim);
		const int32 MaxTileX = FMath::CeilToInt((InvokerRelativeLocation.X + Invoker.RadiusMax) / TileDim);
		const int32 MinTileY = FMath::FloorToInt((InvokerRelativeLocation.Y - Invoker.RadiusMax) / TileDim);
		const int32 MaxTileY = FMath::CeilToInt((InvokerRelativeLocation.Y + Invoker.RadiusMax) / TileDim);

		for (int32 X = MinTileX; X <= MaxTileX; ++X)
		{
			for (int32 Y = MinTileY; Y <= MaxTileY; ++Y)
			{
				const float DistanceSq = (InvokerRelativeLocation - FVector(X * TileDim + TileDim / 2, Y * TileDim + TileDim / 2, 0.f)).SizeSquared2D();
				if (DistanceSq < TileCenterDistanceToRemoveSq)
				{
					TilesInMaxDistance.AddUnique(FIntPoint(X, Y));

					if (DistanceSq < TileCenterDistanceToAddSq)
					{
						TilesInMinDistance.AddUnique(FIntPoint(X, Y));
					}
				}
			}
		}
	}

	ActiveTiles.Append(TilesInMinDistance);

	TArray<FIntPoint> TilesToRemove;
	TilesToRemove.Reserve(OldActiveSet.Num());
	for (int32 Index = OldActiveSet.Num() - 1; Index >= 0; --Index)
	{
		if (TilesInMaxDistance.Find(OldActiveSet[Index]) == INDEX_NONE)
		{
			TilesToRemove.Add(OldActiveSet[Index]);
			OldActiveSet.RemoveAtSwap(Index, 1, /*bAllowShrinking=*/false);
		}
		else
		{
			ActiveTiles.AddUnique(OldActiveSet[Index]);
		}
	}

	TArray<FIntPoint> TilesToUpdate;
	TilesToUpdate.Reserve(ActiveTiles.Num());
	for (int32 Index = TilesInMinDistance.Num() - 1; Index >= 0; --Index)
	{
		// check if it's a new tile
		if (OldActiveSet.Find(TilesInMinDistance[Index]) == INDEX_NONE)
		{
			TilesToUpdate.Add(TilesInMinDistance[Index]);
		}
	}

	RemoveTiles(TilesToRemove);
	RebuildTile(TilesToUpdate);

	if (TilesToRemove.Num() > 0 || TilesToUpdate.Num() > 0)
	{
		UpdateNavMeshDrawing();
	}
}

void ARecastNavMesh::RemoveTiles(const TArray<FIntPoint>& Tiles)
{
	if (Tiles.Num() > 0)
	{
		bWantsUpdate = true;
		FRecastNavMeshGenerator* MyGenerator = static_cast<FRecastNavMeshGenerator*>(GetGenerator());
		if (MyGenerator)
		{
			MyGenerator->RemoveTiles(Tiles);
		}
	}
}

void ARecastNavMesh::RebuildTile(const TArray<FIntPoint>& Tiles)
{
	if (Tiles.Num() > 0)
	{
		bWantsUpdate = true;
		FRecastNavMeshGenerator* MyGenerator = static_cast<FRecastNavMeshGenerator*>(GetGenerator());
		if (MyGenerator)
		{
			MyGenerator->ReAddTiles(Tiles);
		}
	}
}

//----------------------------------------------------------------------//
// FRecastNavMeshCachedData
//----------------------------------------------------------------------//

FRecastNavMeshCachedData FRecastNavMeshCachedData::Construct(const ARecastNavMesh* RecastNavMeshActor)
{
	check(RecastNavMeshActor);
	
	FRecastNavMeshCachedData CachedData;

	CachedData.ActorOwner = RecastNavMeshActor;
	// create copies from crucial ARecastNavMesh data
	CachedData.bUseSortFunction = RecastNavMeshActor->bSortNavigationAreasByCost;

	TArray<FSupportedAreaData> Areas;
	RecastNavMeshActor->GetSupportedAreas(Areas);
	FMemory::Memzero(CachedData.FlagsPerArea, sizeof(ARecastNavMesh::FNavPolyFlags) * RECAST_MAX_AREAS);

	for (int32 i = 0; i < Areas.Num(); i++)
	{
		const UClass* AreaClass = Areas[i].AreaClass;
		const UNavArea* DefArea = AreaClass ? ((UClass*)AreaClass)->GetDefaultObject<UNavArea>() : NULL;
		if (DefArea)
		{
			CachedData.AreaClassToIdMap.Add(AreaClass, Areas[i].AreaID);
			CachedData.FlagsPerArea[Areas[i].AreaID] = DefArea->GetAreaFlags();
		}
	}

	FMemory::Memcpy(CachedData.FlagsPerOffMeshLinkArea, CachedData.FlagsPerArea, sizeof(CachedData.FlagsPerArea));
	static const ARecastNavMesh::FNavPolyFlags NavLinkFlag = ARecastNavMesh::GetNavLinkFlag();
	if (NavLinkFlag != 0)
	{
		ARecastNavMesh::FNavPolyFlags* AreaFlag = CachedData.FlagsPerOffMeshLinkArea;
		for (int32 AreaIndex = 0; AreaIndex < RECAST_MAX_AREAS; ++AreaIndex, ++AreaFlag)
		{
			*AreaFlag |= NavLinkFlag;
		}
	}

	return CachedData;
}

void FRecastNavMeshCachedData::OnAreaAdded(const UClass* AreaClass, int32 AreaID)
{
	const UNavArea* DefArea = AreaClass ? ((UClass*)AreaClass)->GetDefaultObject<UNavArea>() : NULL;
	if (DefArea && AreaID >= 0)
	{
		AreaClassToIdMap.Add(AreaClass, AreaID);
		FlagsPerArea[AreaID] = DefArea->GetAreaFlags();

		static const ARecastNavMesh::FNavPolyFlags NavLinkFlag = ARecastNavMesh::GetNavLinkFlag();
		if (NavLinkFlag != 0)
		{
			FlagsPerOffMeshLinkArea[AreaID] = FlagsPerArea[AreaID] | NavLinkFlag;
		}
	}		
}

#endif// WITH_RECAST


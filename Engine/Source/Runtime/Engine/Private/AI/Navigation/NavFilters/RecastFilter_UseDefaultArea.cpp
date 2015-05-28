// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "AI/Navigation/NavFilters/RecastFilter_UseDefaultArea.h"
#include "AI/Navigation/PImplRecastNavMesh.h"

URecastFilter_UseDefaultArea::URecastFilter_UseDefaultArea(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void URecastFilter_UseDefaultArea::InitializeFilter(const ANavigationData& NavData, FNavigationQueryFilter& Filter) const
{
#if WITH_RECAST
	Filter.SetFilterImplementation(dynamic_cast<const INavigationQueryFilterInterface*>(ARecastNavMesh::GetNamedFilter(ERecastNamedFilter::FilterOutAreas)));
#endif // WITH_RECAST

	Super::InitializeFilter(NavData, Filter);
}

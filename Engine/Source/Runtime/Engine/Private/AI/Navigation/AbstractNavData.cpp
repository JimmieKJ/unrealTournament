// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "AI/Navigation/AbstractNavData.h"

const FNavPathType FAbstractNavigationPath::Type;

FAbstractNavigationPath::FAbstractNavigationPath()
{
	PathType = FAbstractNavigationPath::Type;
}

INavigationQueryFilterInterface* FAbstractQueryFilter::CreateCopy() const
{
	return new FAbstractQueryFilter();
}

AAbstractNavData::AAbstractNavData(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		FindPathImplementation = FindPathAbstract;
		FindHierarchicalPathImplementation = FindPathAbstract;

		TestPathImplementation = TestPathAbstract;
		TestHierarchicalPathImplementation = TestPathAbstract;

		RaycastImplementation = RaycastAbstract;

		DefaultQueryFilter->SetFilterType<FAbstractQueryFilter>();
	}
}

FPathFindingResult AAbstractNavData::FindPathAbstract(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query)
{
	const ANavigationData* Self = Query.NavData.Get();
	if (Self == NULL)
	{
		return ENavigationQueryResult::Error;
	}

	FPathFindingResult Result;
	Result.Path = Query.PathInstanceToFill.IsValid() ? Query.PathInstanceToFill : Self->CreatePathInstance<FAbstractNavigationPath>(Query.Owner.Get());

	Result.Path->GetPathPoints().Reset();
	Result.Path->GetPathPoints().Add(FNavPathPoint(Query.StartLocation));
	Result.Path->GetPathPoints().Add(FNavPathPoint(Query.EndLocation));
	Result.Path->MarkReady();
	Result.Result = ENavigationQueryResult::Success;

	return Result;
}

bool AAbstractNavData::TestPathAbstract(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query, int32* NumVisitedNodes)
{
	return false;
}

bool AAbstractNavData::RaycastAbstract(const ANavigationData* NavDataInstance, const FVector& RayStart, const FVector& RayEnd, FVector& HitLocation, TSharedPtr<const FNavigationQueryFilter> QueryFilter, const UObject* Querier)
{
	return false;
}

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FoliagePrivate.h"
#include "FoliageStatistics.h"

//////////////////////////////////////////////////////////////////////////
// UFoliageStatics

UFoliageStatistics::UFoliageStatistics(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UFoliageStatistics::FoliageOverlappingSphereCount(UObject* WorldContextObject, const UStaticMesh* StaticMesh, FVector CenterPosition, float Radius)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	const FSphere Sphere(CenterPosition, Radius);

	int32 Count = 0;

	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		AInstancedFoliageActor* IFA = *It;
		if (!IFA->IsPendingKill())
		{
			TArray<const UFoliageType*> FoliageTypes;
			IFA->GetAllFoliageTypesForMesh(StaticMesh, FoliageTypes);

			for (const auto Type : FoliageTypes)
			{
				Count += IFA->GetOverlappingSphereCount(Type, Sphere);
			}
		}
	}

	return Count;
}

int32 UFoliageStatistics::FoliageOverlappingBoxCount(UObject* WorldContextObject, const UStaticMesh* StaticMesh, FBox Box)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);

	int32 Count = 0;

	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		AInstancedFoliageActor* IFA = *It;
		if (!IFA->IsPendingKill())
		{
			TArray<const UFoliageType*> FoliageTypes;
			IFA->GetAllFoliageTypesForMesh(StaticMesh, FoliageTypes);

			for (const auto Type : FoliageTypes)
			{
				Count += IFA->GetOverlappingBoxCount(Type, Box);
			}
		}
	}


	return Count;
}

void UFoliageStatistics::FoliageOverlappingBoxTransforms(UObject* WorldContextObject, const UStaticMesh* StaticMesh, FBox Box, TArray<FTransform>& OutTransforms)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);

	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		AInstancedFoliageActor* IFA = *It;
		if (!IFA->IsPendingKill())
		{
			TArray<const UFoliageType*> FoliageTypes;
			IFA->GetAllFoliageTypesForMesh(StaticMesh, FoliageTypes);

			for (const auto Type : FoliageTypes)
			{
				IFA->GetOverlappingBoxTransforms(Type, Box, OutTransforms);
			}
		}
	}
}

void UFoliageStatistics::FoliageOverlappingMeshCounts_Debug(UObject* WorldContextObject, FVector CenterPosition, float Radius, TMap<UStaticMesh*, int32>& OutMeshCounts)
{
	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	const FSphere Sphere(CenterPosition, Radius);

	OutMeshCounts.Reset();
	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		AInstancedFoliageActor* IFA = *It;
		if (!IFA->IsPendingKill())
		{
			IFA->GetOverlappingMeshCounts(Sphere, OutMeshCounts);
		}
	}
}
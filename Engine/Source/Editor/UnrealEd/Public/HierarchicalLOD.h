// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/*=============================================================================
	HierarchicalLOD.h: Hierarchical LOD definition.
=============================================================================*/

/**
 *
 *	This is a LOD cluster struct that holds list of actors with relevant information
 *
 *	http://deim.urv.cat/~rivi/pub/3d/icra04b.pdf
 *
 *	This is used by Hierarchical LOD Builder to generates list of clusters 
 *	that are together in vincinity and build as one actor
 *
 **/

struct FLODCluster
{
	// constructors
	FLODCluster(const FLODCluster& Other);
	FLODCluster(class AActor* Actor1);
	FLODCluster(class AActor* Actor1, class AActor* Actor2);

	FLODCluster operator+( const FLODCluster& Other ) const;
	FLODCluster operator+=( const FLODCluster& Other );
	FLODCluster operator-(const FLODCluster& Other) const;
	FLODCluster operator-=(const FLODCluster& Other);
	FLODCluster& operator=(const FLODCluster & Other);

	// Invalidate current cluster
	void Invalidate() { bValid = false; }
	// return true if valid
	bool IsValid() const {	return bValid; }

	// return cost of the cluster, lower is better
	const float GetCost() const
	{
		return (FMath::Pow(Bound.W, 3) / FillingFactor);
	}

	// return true if this cluster contains ANY of actors of Other
	bool Contains(FLODCluster& Other) const;
	// return string of data
	FString ToString() const;
	
	// member variable
	// list of actors
	TArray<class AActor*>	Actors;
	// bound of this cluster
	FSphere					Bound;
	// filling factor - higher means filled more
	float					FillingFactor;

	// if criteria matches, build new LODActor and replace current Actors with that. We don't need 
	// this clears previous actors and sets to this new actor
	// this is required when new LOD is created from these actors, this will be replaced
	// to save memory and to reduce memory increase during this process, we discard previous actors and replace with this actor
	void BuildActor(class ULevel* InLevel, const int32 LODIdx);

private:
	// cluster operations
	void MergeClusters(const FLODCluster& Other);
	void SubtractCluster(const FLODCluster& Other);

	// add new actor, this doesn't recalculate filling factor
	// the filling factor has to be calculated outside
	FSphere AddActor(class AActor* NewActor);

	// whether it's valid or not
	bool bValid;
};

/**
 *
 *	This is Hierarchical LOD builder
 *
 * This builds list of clusters and make sure it's sorted in the order of lower cost to high and merge clusters
 **/

struct UNREALED_API FHierarchicalLODBuilder
{
	FHierarchicalLODBuilder(class UWorld* InWorld);

#if WITH_HOT_RELOAD_CTORS
	/** DO NOT USE. This constructor is for internal usage only for hot-reload purposes. */
	FHierarchicalLODBuilder();
#endif // WITH_HOT_RELOAD_CTORS

	// build hierarchical cluster
	void Build();

private:
	// data structure - this is only valid within scope since mem stack allocator
	TArray<FLODCluster, TMemStackAllocator<>> Clusters;

	// owner world
	class UWorld*		World;

	// for now it only builds per level, it turns to one actor at the end
	void BuildClusters(class ULevel* InLevel);

	// initialize Clusters variable - each Actor becomes Cluster
	void InitializeClusters(class ULevel* InLevel, const int32 LODIdx, float CullCost);

	// merge clusters
	void MergeClustersAndBuildActors(class ULevel* InLevel, const int32 LODIdx, float HighestCost, int32 MinNumActors);

	// find minmal spanning tree of clusters
	void FindMST();
};

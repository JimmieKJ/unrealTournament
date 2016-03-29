// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"

class FHierarchicalLODProxyProcessor;
class AActor;
class ALODActor;
class UStaticMeshComponent;
class UPackage;
struct FHierarchicalSimplification;
struct FMatrix;
class UWorld;
class AWorldSettings;
class AHierarchicalLODVolume;

/**
 * IHierarchicalLODUtilities module interface
 */
class HIERARCHICALLODUTILITIES_API FHierarchicalLODUtilities : public IModuleInterface
{
public:
	virtual void ShutdownModule() override;
	virtual void StartupModule() override;
	
	/**
	* Recursively retrieves StaticMeshComponents from a LODActor and its child LODActors
	*
	* @param Actor - LODActor instance
	* @param InOutComponents - Will hold the StaticMeshComponents
	*/
	static void ExtractStaticMeshComponentsFromLODActor(AActor* Actor, TArray<UStaticMeshComponent*>& InOutComponents);

	/**
	* Recursively retrieves Actors from a LODActor and its child LODActors
	*
	* @param Actor - LODActor instance
	* @param InOutComponents - Will hold the StaticMeshComponents
	*/
	static void ExtractSubActorsFromLODActor(AActor* Actor, TArray<AActor*>& InOutActors);

	/** Computes the Screensize of the given Sphere taking into account the ProjectionMatrix and distance */
	static float CalculateScreenSizeFromDrawDistance(const float SphereRadius, const FMatrix& ProjectionMatrix, const float Distance);

	static float CalculateDrawDistanceFromScreenSize(const float SphereRadius, const float ScreenSize, const FMatrix& ProjectionMatrix);

	/** Creates or retrieves the HLOD package that is created for the given level */
	static UPackage* CreateOrRetrieveLevelHLODPackage(ULevel* InLevel);

	/**
	* Builds a static mesh object for the given LODACtor
	*
	* @param LODActor - Actor to build the mesh for
	* @param Outer - Outer object to store the mesh in
	* @return UStaticMesh*
	*/
	static bool BuildStaticMeshForLODActor(ALODActor* LODActor, UPackage* AssetsOuter, const FHierarchicalSimplification& LODSetup, const uint32 LODIndex);
	
	/**
	* Returns whether or not the given actor is eligible for creating a HLOD cluster creation
	*
	* @param Actor - Actor to check for if it is eligible for cluster generation
	* @return bool
	*/
	static bool ShouldGenerateCluster(AActor* Actor);

	/** Returns the ALODActor parent for the given InActor, nullptr if none available */
	static ALODActor* GetParentLODActor(const AActor* InActor);

	/** Deletes the given cluster's data and instance in the world */
	static void DestroyCluster(ALODActor* InActor);

	/** Deletes the given cluster's assets */
	static void DestroyClusterData(ALODActor* InActor);

	/** Creates a new cluster actor in the given InWorld with InLODLevel as HLODLevel */
	static ALODActor* CreateNewClusterActor(UWorld* InWorld, const int32 InLODLevel, AWorldSettings* WorldSettings);

	/** Creates a new cluster in InWorld with InActors as sub actors*/
	static ALODActor* CreateNewClusterFromActors(UWorld* InWorld, AWorldSettings* WorldSettings, const TArray<AActor*>& InActors, const int32 InLODLevel = 0);

	/** Removes the given actor from it's parent cluster */
	static const bool RemoveActorFromCluster(AActor* InActor);

	/** Adds an actor to the given cluster*/
	static const bool AddActorToCluster(AActor* InActor, ALODActor* InParentActor);

	/** Merges two clusters together */
	static const bool MergeClusters(ALODActor* TargetCluster, ALODActor* SourceCluster);

	/** Checks if all actors have the same outer world */
	static const bool AreActorsInSamePersistingLevel(const TArray<AActor*>& InActors);

	/** Checks if all clusters are in the same HLOD level */
	static const bool AreClustersInSameHLODLevel(const TArray<ALODActor*>& InLODActors);

	/** Checks if all actors are in the same HLOD level */
	static const bool AreActorsInSameHLODLevel(const TArray<AActor*>& InActors);

	/** Checks if all actors are part of a cluster */
	static const bool AreActorsClustered(const TArray<AActor*>& InActors);

	/** Checks if an actor is clustered*/
	static const bool IsActorClustered(const AActor* InActor);

	/** Excludes an actor from the cluster generation process */
	static void ExcludeActorFromClusterGeneration(AActor* InActor);

	/**
	* Destroys an LODActor instance
	*
	* @param InActor - ALODActor to destroy
	*/
	static void DestroyLODActor(ALODActor* InActor);

	/**
	* Extracts all the Static Mesh Actors from the given LODActor's SubActors array
	*
	* @param LODActor - LODActors to check the SubActors array for
	* @param InOutActors - Array to fill with Static Mesh Actors
	*/
	static void ExtractStaticMeshActorsFromLODActor(ALODActor* LODActor, TArray<AActor*> &InOutActors);

	/** Deletes all the ALODActors with the given HLODLevelIndex inside off InWorld */
	static void DeleteLODActorsInHLODLevel(UWorld* InWorld, const int32 HLODLevelIndex);

	/** Computes which LOD level of a Mesh corresponds to the given Distance (calculates closest screensize with distance) */
	static int32 ComputeStaticMeshLODLevel(const TArray<FStaticMeshSourceModel>& SourceModels, const FStaticMeshRenderData* RenderData, const float ScreenAreaSize);

	/** Computes the LODLevel for a StaticMeshComponent taking into account the ScreenArea */
	static int32 GetLODLevelForScreenAreaSize(const UStaticMeshComponent* StaticMeshComponent, const float ScreenAreaSize);
	
	/**
	* Creates a HierarchicalLODVolume using the bounds of a given LODActor
	* @param InLODActor - LODActor to create the volume for
	* @param InWorld - World to spawn the volume in
	* @return AHierarchicalLODVolume*
	*/
	static AHierarchicalLODVolume* CreateVolumeForLODActor(ALODActor* InLODActor, UWorld* InWorld);
		
	/** Returns the Proxy processor instance from within this module */
	FHierarchicalLODProxyProcessor* GetProxyProcessor();	
private:
	FHierarchicalLODProxyProcessor* ProxyProcessor;
};

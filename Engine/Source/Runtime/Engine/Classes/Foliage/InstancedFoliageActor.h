// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "GameFramework/Actor.h"
#include "Templates/UniqueObj.h"
#include "Components/InstancedStaticMeshComponent.h"

#include "InstancedFoliageActor.generated.h"

// Forward declarations
class UFoliageType;
struct FFoliageInstancePlacementInfo;
struct FFoliageMeshInfo;

UCLASS(notplaceable, hidecategories = Object, MinimalAPI, NotBlueprintable)
class AInstancedFoliageActor : public AActor
{
	GENERATED_UCLASS_BODY()

	/** The static mesh type that will be used to show the widget */
	UPROPERTY(transient)
	UFoliageType* SelectedMesh;

public:
	TMap<UFoliageType*, TUniqueObj<FFoliageMeshInfo>> FoliageMeshes;

public:
	// Begin UObject interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// End UObject interface. 

	// Begin AActor interface.
	// we don't want to have our components automatically destroyed by the Blueprint code
	virtual void RerunConstructionScripts() override {}
	// Add world origin offset
	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) override;
	// End AActor interface.

#if WITH_EDITOR
	virtual void PostEditUndo() override;

	// Called in response to BSP rebuilds to migrate foliage from obsolete to new components.
	ENGINE_API void MapRebuild();

	// Moves instances based on the specified component to the current streaming level
	ENGINE_API void MoveInstancesForComponentToCurrentLevel(UActorComponent* InComponent);

	// Change all instances based on one component to a new component (possible in another level).
	// The instances keep the same world locations
	ENGINE_API void MoveInstancesToNewComponent(UPrimitiveComponent* InOldComponent, UPrimitiveComponent* InNewComponent);

	// Move instances based on a component that has just been moved.
	void MoveInstancesForMovedComponent(UActorComponent* InComponent);

	// Returns a map of Static Meshes and their placed instances attached to a component.
	ENGINE_API TMap<UFoliageType*, TArray<const FFoliageInstancePlacementInfo*>> GetInstancesForComponent(UActorComponent* InComponent);

	// Deletes the instances attached to a component
	ENGINE_API void DeleteInstancesForComponent(UActorComponent* InComponent);

	// Finds a mesh entry
	ENGINE_API FFoliageMeshInfo* FindMesh(UFoliageType* InType);

	// Finds a mesh entry or adds it if it doesn't already exist
	ENGINE_API FFoliageMeshInfo* FindOrAddMesh(UFoliageType* InType);

	// Add a new static mesh.
	ENGINE_API FFoliageMeshInfo* AddMesh(UStaticMesh* InMesh, UFoliageType** OutSettings = nullptr);
	ENGINE_API FFoliageMeshInfo* AddMesh(UFoliageType* InType);

	// Remove the static mesh from the mesh list, and all its instances.
	ENGINE_API void RemoveMesh(UFoliageType* InFoliageType);

	// Performs a reverse lookup from a mesh to its settings
	ENGINE_API UFoliageType* GetSettingsForMesh(UStaticMesh* InMesh, FFoliageMeshInfo** OutMeshInfo = nullptr);

	// Select an individual instance.
	ENGINE_API void SelectInstance(UInstancedStaticMeshComponent* InComponent, int32 InComponentInstanceIndex, bool bToggle);

	// Propagate the selected instances to the actual render components
	ENGINE_API void ApplySelectionToComponents(bool bApply);

	// Updates the SelectedMesh property of the actor based on the actual selected instances
	ENGINE_API void CheckSelection();

	// Returns the location for the widget
	ENGINE_API FVector GetSelectionLocation();

	// Transforms Editor specific data which is stored in world space
	ENGINE_API void ApplyLevelTransform(const FTransform& LevelTransform);

	/**
	* Get the instanced foliage actor for the current streaming level.
	*
	* @param InCreationWorldIfNone			World to create the foliage instance in
	* @param bCreateIfNone					Create if doesnt already exist
	* returns								pointer to foliage object instance
	*/
	static ENGINE_API AInstancedFoliageActor* GetInstancedFoliageActorForCurrentLevel(UWorld* InWorld, bool bCreateIfNone = true);


	// Get the instanced foliage actor for the specified streaming level. Never creates a new IFA.
	static ENGINE_API AInstancedFoliageActor* GetInstancedFoliageActorForLevel(ULevel* Level);
#endif	//WITH_EDITOR
};

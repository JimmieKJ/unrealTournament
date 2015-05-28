// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AI/Navigation/NavigationTypes.h"
#include "StaticMeshActor.generated.h"

/**
 * StaticMeshActor is an instance of a UStaticMesh in the world.
 * Static meshes are geometry that do not animate or otherwise deform, and are more efficient to render than other types of geometry.
 * Static meshes dragged into the level from the Content Browser are automatically converted to StaticMeshActors.
 *
 * @see https://docs.unrealengine.com/latest/INT/Engine/Actors/StaticMeshActor/
 * @see UStaticMesh
 */
UCLASS(hidecategories=(Input), showcategories=("Input|MouseInput", "Input|TouchInput"), ConversionRoot, ComponentWrapperClass, meta=(ChildCanTick))
class ENGINE_API AStaticMeshActor : public AActor
{
	GENERATED_UCLASS_BODY()

private_subobject:
	DEPRECATED_FORGAME(4.6, "StaticMeshComponent should not be accessed directly, please use GetStaticMeshComponent() function instead. StaticMeshComponent will soon be private and your code will not compile.")
	UPROPERTY(Category = StaticMeshActor, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Mesh,Rendering,Physics,Components|StaticMesh", AllowPrivateAccess = "true"))
	class UStaticMeshComponent* StaticMeshComponent;
public:
	
	virtual void BeginPlay() override;

	/** This static mesh should replicate movement. Automatically sets the RemoteRole and bReplicateMovement flags. Meant to be edited on placed actors (those other two proeprties are not) */
	UPROPERTY(Category=Actor, EditAnywhere, AdvancedDisplay)
	bool bStaticMeshReplicateMovement;

	UPROPERTY(EditAnywhere, Category = Navigation, AdvancedDisplay)
	ENavDataGatheringMode NavigationGeometryGatheringMode;

	/** Function to change mobility type */
	void SetMobility(EComponentMobility::Type InMobility);

#if WITH_EDITOR
	// Begin AActor Interface
	virtual void CheckForErrors() override;
	virtual bool GetReferencedContentObjects( TArray<UObject*>& Objects ) const override;
	// End AActor Interface
#endif // WITH_EDITOR	

	// INavRelevantInterface begin
	virtual ENavDataGatheringMode GetGeometryGatheringMode() const { return ENavDataGatheringMode::Default; }
	// INavRelevantInterface end

protected:
	// Begin UObject interface.
	virtual FString GetDetailedInfoInternal() const override;
#if WITH_EDITOR
	virtual void LoadedFromAnotherClass(const FName& OldClassName) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR	
	// End UObject interface.

public:
	/** Returns StaticMeshComponent subobject **/
	class UStaticMeshComponent* GetStaticMeshComponent() const;
};




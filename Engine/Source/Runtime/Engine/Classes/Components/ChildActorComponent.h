// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "SceneComponent.h"
#include "ChildActorComponent.generated.h"

class FChildActorComponentInstanceData;

/** A component that spawns an Actor when registered, and destroys it when unregistered.*/
UCLASS(ClassGroup=Utility, hidecategories=(Object,LOD,Physics,Lighting,TextureStreaming,Activation,"Components|Activation",Collision), meta=(BlueprintSpawnableComponent), MinimalAPI)
class UChildActorComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintCallable, Category=ChildActorComponent)
	ENGINE_API void SetChildActorClass(TSubclassOf<AActor> Class);

	TSubclassOf<AActor> GetChildActorClass() const { return ChildActorClass; }

	friend class FChildActorComponentDetails;

private:
	/** The class of Actor to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=ChildActorComponent, meta=(OnlyPlaceable, AllowPrivateAccess="true"))
	TSubclassOf<AActor>	ChildActorClass;

public:
	/** The actor that we spawned and own */
	UPROPERTY(BlueprintReadOnly, Category=ChildActorComponent, TextExportTransient, NonPIEDuplicateTransient)
	AActor*	ChildActor;

	/** We try to keep the child actor's name as best we can, so we store it off here when destroying */
	FName ChildActorName;

	/** Cached copy of the instance data when the ChildActor is destroyed to be available when needed */
	mutable FChildActorComponentInstanceData* CachedInstanceData;

	// Begin Object interface.
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif
	// End Object interface.

	// Begin ActorComponent interface.
	virtual void OnComponentCreated() override;
	virtual void OnComponentDestroyed() override;
	virtual void OnRegister() override;
	virtual FActorComponentInstanceData* GetComponentInstanceData() const override;
	virtual FName GetComponentInstanceDataType() const override;
	// End ActorComponent interface.

	/** Apply the component instance data to the child actor component */
	void ApplyComponentInstanceData(class FChildActorComponentInstanceData* ComponentInstanceData);

	/** Create the child actor */
	ENGINE_API void CreateChildActor();

	/** Kill any currently present child actor */
	void DestroyChildActor(const bool bRequiresRename = true);
};




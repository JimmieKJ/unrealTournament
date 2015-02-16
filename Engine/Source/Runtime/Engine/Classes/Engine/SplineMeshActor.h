// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SplineMeshActor.generated.h"

/**
 * SplineMeshActor is an actor with a SplineMeshComponent.
 *
 * @see USplineMeshComponent
 */
UCLASS(hideCategories = (Input), showCategories = ("Input|MouseInput", "Input|TouchInput"), ConversionRoot, ComponentWrapperClass, meta = (ChildCanTick))
class ENGINE_API ASplineMeshActor : public AActor
{
	GENERATED_UCLASS_BODY()

private_subobject:
	UPROPERTY(Category = SplineMeshActor, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Mesh,Rendering,Physics,Components|StaticMesh,Components|SplineMesh", AllowPrivateAccess = "true"))
	class USplineMeshComponent* SplineMeshComponent;

public:

	/** Function to change mobility type */
	void SetMobility(EComponentMobility::Type InMobility);

#if WITH_EDITOR
	// Begin AActor Interface
	virtual void CheckForErrors() override;
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
	// End AActor Interface
#endif // WITH_EDITOR	

protected:
	// Begin UObject interface.
	virtual FString GetDetailedInfoInternal() const override;
	// End UObject interface.

public:
	/** Returns SplineMeshComponent subobject **/
	class USplineMeshComponent* GetSplineMeshComponent() const;
};




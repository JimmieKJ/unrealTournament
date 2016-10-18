// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/**
 * Subdivision Surface Actor (Experimental, Early work in progress)
 */

#pragma once

#include "SubDSurfaceActor.generated.h"

UCLASS(hidecategories=(Collision, Attachment, Actor))
class ENGINE_API ASubDSurfaceActor : public AActor
{
	GENERATED_UCLASS_BODY()

	friend class UActorFactorySubDSurface;

private:
	/** Component to render the actor, used GetSubDSurface() to access */
	UPROPERTY(Category = SubDSurfaceActor, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Rendering|Components|SubDSurface", AllowPrivateAccess = "true"))
	class USubDSurfaceComponent* SubDSurface;

	// later this all will be done by USubDSurfaceComponent
	UPROPERTY(Category = SubDSurfaceActor, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Mesh,Rendering,Physics,Components|StaticMesh", AllowPrivateAccess = "true"))
	UStaticMeshComponent* DisplayMeshComponent;


#if WITH_EDITORONLY_DATA
	// Reference to the billboard component
	UPROPERTY()
	UBillboardComponent* SpriteComponent;

	/** Returns SpriteComponent subobject **/
	UBillboardComponent* GetSpriteComponent() const { return SpriteComponent; }
#endif

public:
	/** Returns SubDSurface subobject **/
	class USubDSurfaceComponent* GetSubDSurface() const { return SubDSurface; }
};




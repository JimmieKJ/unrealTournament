// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "GeometryCacheActor.generated.h"

class UGeometryCacheComponent;

/** GeometryCache actor, serves as a place-able actor for GeometryCache objects*/
UCLASS(hidedropdown, MinimalAPI, notplaceable, NotBlueprintable)
class AGeometryCacheActor : public AActor
{
	GENERATED_UCLASS_BODY()

	// Begin AActor overrides.
#if WITH_EDITOR
	 virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override; 
#endif // WITH_EDITOR
	// End AActor overrides.

	UPROPERTY(Category = GeometryCacheActor, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Mesh,Rendering,Physics,Components|GeometryCache"))
	UGeometryCacheComponent* GeometryCacheComponent;
public:
	/** Returns GeometryCacheComponent subobject **/
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	GEOMETRYCACHE_API UGeometryCacheComponent* GetGeometryCacheComponent() const;
};

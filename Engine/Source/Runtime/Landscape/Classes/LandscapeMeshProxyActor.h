// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneTypes.h"
#include "StaticLighting.h"
#include "Components/PrimitiveComponent.h"

#include "LandscapeMeshProxyActor.generated.h"

UCLASS(MinimalAPI)
class ALandscapeMeshProxyActor : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(Category = LandscapeMeshProxyActor, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Mesh,Rendering,Physics,Components|StaticMesh", AllowPrivateAccess = "true"))
	class ULandscapeMeshProxyComponent* LandscapeMeshProxyComponent;

public:
	/** Returns StaticMeshComponent subobject **/
	class ULandscapeMeshProxyComponent* GetLandscapeMeshProxyComponent() const { return LandscapeMeshProxyComponent; }
};


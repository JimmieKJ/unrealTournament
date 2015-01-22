// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Components/StaticMeshComponent.h"
#include "MaterialEditorMeshComponent.generated.h"

UCLASS()
class UNREALED_API UMaterialEditorMeshComponent : public UStaticMeshComponent
{
	GENERATED_UCLASS_BODY()

	// USceneComponent Interface
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
};


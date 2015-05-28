// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FoliageType.h"
#include "FoliageType_InstancedStaticMesh.generated.h"

UCLASS(hidecategories=Object, editinlinenew, MinimalAPI)
class UFoliageType_InstancedStaticMesh : public UFoliageType
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=Mesh, meta=(DisplayThumbnail="true"))
	UStaticMesh* Mesh;

	virtual UStaticMesh* GetStaticMesh() const override
	{
		return Mesh;
	}

	virtual void SetStaticMesh(UStaticMesh* InStaticMesh) override
	{
		Mesh = InStaticMesh;
	}
};

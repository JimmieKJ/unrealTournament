// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Materials/Material.h"

#include "PreviewMaterial.generated.h"

UCLASS()
class UNREALED_API UPreviewMaterial : public UMaterial
{
	GENERATED_UCLASS_BODY()


	// Begin UMaterial interface.
	virtual FMaterialResource* AllocateResource() override;
	virtual bool IsAsset()  const override  { return false; }
	// End UMaterial interface.
};


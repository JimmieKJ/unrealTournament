// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/DebugCameraHUD.h"
#include "LogVisualizerHUD.generated.h"

UCLASS()
class ALogVisualizerHUD : public ADebugCameraHUD
{
	GENERATED_UCLASS_BODY()

	FFontRenderInfo TextRenderInfo;

	virtual bool DisplayMaterials( float X, float& Y, float DY, UMeshComponent* MeshComp ) override;

	// Begin AActor Interface
	virtual void PostRender() override;
	// End AActor Interface
};




// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/HUD.h"
#include "DebugCameraHUD.generated.h"


/**
 * HUD that displays info for the DebugCameraController view.
 */
UCLASS(config=Game)
class ENGINE_API ADebugCameraHUD
	: public AHUD
{
	GENERATED_UCLASS_BODY()

	/** @todo document */
	virtual bool DisplayMaterials( float X, float& Y, float DY, class UMeshComponent* MeshComp );
	
	// Begin AActor Interface
	virtual void PostRender() override;
	// End AActor Interface

};

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PreviewScene.h"

class UStaticMeshComponent;
class UExponentialHeightFogComponent;

class FAnimationEditorPreviewScene : public FPreviewScene
{
public:
	FAnimationEditorPreviewScene(ConstructionValues CVS);

	/** Editor accessory components **/
	UStaticMeshComponent*				EditorFloorComp;
	UStaticMeshComponent*				EditorSkyComp;
	UExponentialHeightFogComponent*		EditorHeightFogComponent;
};
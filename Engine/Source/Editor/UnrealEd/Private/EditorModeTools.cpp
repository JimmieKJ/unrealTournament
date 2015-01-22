// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "SurfaceIterators.h"
#include "BSPOps.h"


FModeTool::FModeTool():
	ID( MT_None ),
	bUseWidget( 1 )
{}

FModeTool::~FModeTool()
{
}

void FModeTool::DrawHUD(FEditorViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas)
{
}

void FModeTool::Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI)
{
}



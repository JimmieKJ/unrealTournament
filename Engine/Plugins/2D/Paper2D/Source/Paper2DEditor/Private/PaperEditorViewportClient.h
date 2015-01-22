// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneViewport.h"

struct FViewportSelectionRectangle
{
	FVector2D TopLeft;
	FVector2D Dimensions;
	FLinearColor Color;
};

//////////////////////////////////////////////////////////////////////////
// FAssetEditorModeTools

class FAssetEditorModeTools : public FEditorModeTools
{
public:
	FAssetEditorModeTools();
	virtual ~FAssetEditorModeTools();

	// FEditorModeTools interface
	virtual class USelection* GetSelectedActors() const override;
	virtual class USelection* GetSelectedObjects() const override;
	// End of FEditorModeTools interface

protected:
	class USelection* ActorSet;
	class USelection* ObjectSet;
};

//////////////////////////////////////////////////////////////////////////
// FPaperEditorViewportClient

class FPaperEditorViewportClient : public FEditorViewportClient
{
public:
	/** Constructor */
	FPaperEditorViewportClient();
	~FPaperEditorViewportClient();

	// FViewportClient interface
	virtual void Draw(FViewport* Viewport, FCanvas* Canvas) override;
	// End of FViewportClient interface

	// FSerializableObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// End of FSerializableObject interface

	/** Modifies the checkerboard texture's data */
	void ModifyCheckerboardTextureColors();

	void SetZoomPos(FVector2D InNewPos, float InNewZoom)
	{
		ZoomPos = InNewPos;
		ZoomAmount = InNewZoom;
	}

	// List of selection rectangles to draw
	TArray<FViewportSelectionRectangle> SelectionRectangles;
private:
	/** Initialize the checkerboard texture for the texture preview, if necessary */
	void SetupCheckerboardTexture(const FColor& ColorOne, const FColor& ColorTwo, int32 CheckerSize);

	/** Destroy the checkerboard texture if one exists */
	void DestroyCheckerboardTexture();

protected:
	void DrawSelectionRectangles(FViewport* Viewport, FCanvas* Canvas);

protected:
	/** Checkerboard texture */
	UTexture2D* CheckerboardTexture;
	FVector2D ZoomPos;
	float ZoomAmount;
};

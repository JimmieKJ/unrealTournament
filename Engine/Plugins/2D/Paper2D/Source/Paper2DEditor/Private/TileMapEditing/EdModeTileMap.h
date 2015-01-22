// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "PaperTileSet.h"

namespace ETileMapEditorTool
{
	enum Type
	{
		Paintbrush,
		Eraser,
		PaintBucket
	};
}

namespace ETileMapLayerPaintingMode
{
	enum Type
	{
		VisualLayers,
		CollisionLayers
	};
}

//////////////////////////////////////////////////////////////////////////
// FEdModeTileMap

class FEdModeTileMap : public FEdMode
{
public:
	static const FEditorModeID EM_TileMap;

public:
	FEdModeTileMap();
	virtual ~FEdModeTileMap();

	// FSerializableObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// End of FSerializableObject

	// FEdMode interface
	virtual bool UsesToolkits() const override;
	virtual void Enter() override;
	virtual void Exit() override;
	//virtual bool BoxSelect(FBox& InBox, bool bInSelect) override;

	//	virtual void PostUndo() override;
	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) override;
	virtual bool CapturedMouseMove(FEditorViewportClient* InViewportClient, FViewport* InViewport, int32 InMouseX, int32 InMouseY) override;
	virtual bool StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	virtual bool EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	// 	virtual void Tick(FEditorViewportClient* ViewportClient,float DeltaTime) override;
	virtual bool InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent) override;
	virtual bool InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale) override;
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
	virtual void ActorSelectionChangeNotify() override;
	virtual bool AllowWidgetMove();
	virtual bool ShouldDrawWidget() const override;
	virtual bool UsesTransformWidget() const override;
	// 	virtual int32 GetWidgetAxisToDraw(FWidget::EWidgetMode InWidgetMode) const override;
	// 	virtual bool DisallowMouseDeltaTracking() const override;
	// End of FEdMode interface

	void SetActiveTool(ETileMapEditorTool::Type NewTool);
	ETileMapEditorTool::Type GetActiveTool() const;

	void SetActiveLayerPaintingMode(ETileMapLayerPaintingMode::Type NewMode);
	ETileMapLayerPaintingMode::Type GetActiveLayerPaintingMode() const;

	void SetActivePaint(UPaperTileSet* TileSet, FIntPoint TopLeft, FIntPoint Dimensions);

	void RefreshBrushSize();
protected:
	bool UseActiveToolAtLocation(const FViewportCursorLocation& Ray);

	bool PaintTiles(const FViewportCursorLocation& Ray);
	bool EraseTiles(const FViewportCursorLocation& Ray);
	bool FloodFillTiles(const FViewportCursorLocation& Ray);


	void UpdatePreviewCursor(const FViewportCursorLocation& Ray);

	// Returns the selected layer under the cursor, and the intersection tile coordinates
	// Note: The tile coordinates can be negative if the brush is off the top or left of the tile map, but still overlaps the map!!!
	UPaperTileLayer* GetSelectedLayerUnderCursor(const FViewportCursorLocation& Ray, int32& OutTileX, int32& OutTileY) const;

	// Compute a world space ray from the screen space mouse coordinates
	FViewportCursorLocation CalculateViewRay(FEditorViewportClient* InViewportClient, FViewport* InViewport);
	
	TSharedRef<FExtender> AddCreationModeExtender(const TSharedRef<FUICommandList> InCommandList);

	void EnableTileMapEditMode();
	bool IsTileMapEditModeActive() const;

public:
	UPaperTileMapComponent* FindSelectedComponent() const;

protected:
	bool bIsPainting;
	TWeakObjectPtr<UPaperTileSet> PaintSourceTileSet;

	FIntPoint PaintSourceTopLeft;
	FIntPoint PaintSourceDimensions;

	FTransform DrawPreviewSpace;
	FVector DrawPreviewLocation;
	FVector DrawPreviewDimensionsLS;

	int32 EraseBrushSize;

	int32 CursorWidth;
	int32 CursorHeight;
	int32 BrushWidth;
	int32 BrushHeight;

	ETileMapEditorTool::Type ActiveTool;
	ETileMapLayerPaintingMode::Type LayerPaintingMode;
	mutable FTransform ComponentToWorld;
};


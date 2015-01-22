// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Paper2DEditorPrivatePCH.h"
#include "TileMapEditor.h"
#include "SSingleObjectDetailsPanel.h"
#include "SceneViewport.h"
#include "PaperEditorViewportClient.h"
#include "PreviewScene.h"
#include "ScopedTransaction.h"

//////////////////////////////////////////////////////////////////////////
// FTileMapEditorViewportClient

class FTileMapEditorViewportClient : public FPaperEditorViewportClient
{
public:
	/** Constructor */
	FTileMapEditorViewportClient(TWeakPtr<FTileMapEditor> InTileMapEditor, TWeakPtr<class STileMapEditorViewport> InTileMapEditorViewportPtr);

	// FViewportClient interface
	virtual void Draw(FViewport* Viewport, FCanvas* Canvas) override;
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI);
	virtual void DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas) override;
	virtual void Tick(float DeltaSeconds) override;
	// End of FViewportClient interface

	// FEditorViewportClient interface
	// End of FEditorViewportClient interface

	void ToggleShowPivot() { bShowPivot = !bShowPivot; Invalidate(); }
	bool IsShowPivotChecked() const { return bShowPivot; }

	void ToggleShowMeshEdges();
	bool IsShowMeshEdgesChecked() const;

	//
	void FocusOnTileMap();

	// Invalidate any references to the tile map being edited; it has changed
	void NotifyTileMapBeingEditedHasChanged();

	// Note: Has to be delayed due to an unfortunate init ordering
	void ActivateEditMode();
private:
	// The preview scene
	FPreviewScene OwnedPreviewScene;

	// Tile map editor that owns this viewport
	TWeakPtr<FTileMapEditor> TileMapEditorPtr;

	// Render component for the tile map being edited
	UPaperTileMapComponent* RenderTileMapComponent;

	// Widget mode
	FWidget::EWidgetMode WidgetMode;

	// Are we currently manipulating something?
	bool bManipulating;

	// Did we dirty something during manipulation?
	bool bManipulationDirtiedSomething;

	// Pointer back to the tile map editor viewport control that owns us
	TWeakPtr<class STileMapEditorViewport> TileMapEditorViewportPtr;

	// The current transaction for undo/redo
	class FScopedTransaction* ScopedTransaction;

	// Should we show the sprite pivot?
	bool bShowPivot;

	// Should we zoom to the tile map next tick?
	bool bDeferZoomToTileMap;
private:
	UPaperTileMap* GetTileMapBeingEdited() const
	{
		return TileMapEditorPtr.Pin()->GetTileMapBeingEdited();
	}

	void DrawTriangleList(FViewport& InViewport, FSceneView& View, FCanvas& Canvas, const TArray<FVector2D>& Triangles);
	void DrawBoundsAsText(FViewport& InViewport, FSceneView& View, FCanvas& Canvas, int32& YPos);
	
	void BeginTransaction(const FText& SessionName);
	void EndTransaction();

	void ClearSelectionSet();
};

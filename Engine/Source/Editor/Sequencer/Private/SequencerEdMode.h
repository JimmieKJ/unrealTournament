// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EditorModeTools.h"
#include "MovieSceneHitProxy.h"

class FSequencer;

/**
 * FSequencerEdMode is the editor mode for additional drawing and handling sequencer hotkeys in the editor
 */
class FSequencerEdMode : public FEdMode
{
public:
	static const FEditorModeID EM_SequencerMode;

public:
	FSequencerEdMode();
	virtual ~FSequencerEdMode();

	/* FEdMode interface */
	virtual void Enter() override;
	virtual void Exit() override;
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override;
	virtual bool InputKey( FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event ) override;
	virtual void Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI) override;

	void SetSequencer(FSequencer* InSequencer) { Sequencer = InSequencer; }

	void OnKeySelected(FViewport* Viewport, HMovieSceneKeyProxy* KeyProxy);

protected:
	void DrawTracks3D(const FSceneView* View, FPrimitiveDrawInterface* PDI);

private:
	FSequencer* Sequencer;
};

/**
 * FSequencerEdMode is the editor mode tool for additional drawing and handling sequencer hotkeys in the editor
 */
class FSequencerEdModeTool : public FModeTool
{
public:
	FSequencerEdModeTool(FSequencerEdMode* InSequencerEdMode);
	virtual ~FSequencerEdModeTool();

	virtual FString GetName() const override { return TEXT("Sequencer Edit"); }

	/**
	 * @return		true if the key was handled by this editor mode tool.
	 */
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;

private:
	FSequencerEdMode* SequencerEdMode;
};

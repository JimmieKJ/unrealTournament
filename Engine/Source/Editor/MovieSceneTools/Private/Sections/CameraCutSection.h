// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ThumbnailSection.h"


class FTrackEditorThumbnail;
class FTrackEditorThumbnailPool;
class IMenu;
class ISectionLayoutBuilder;
class UMovieSceneCameraCutSection;


/**
 * CameraCut section, which paints and ticks the appropriate section.
 */
class FCameraCutSection
	: public FThumbnailSection
{
public:

	/** Create and initialize a new instance. */
	FCameraCutSection(TSharedPtr<ISequencer> InSequencer, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, UMovieSceneSection& InSection);

	/** Virtual destructor. */
	virtual ~FCameraCutSection();

public:

	// ISequencerSection interface
	virtual void Tick(const FGeometry& AllottedGeometry, const FGeometry& ClippedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding) override;
	virtual FText GetDisplayName() const override;
	virtual float GetSectionHeight() const override;
	virtual int32 OnPaintSection(FSequencerSectionPainter& InPainter) const override;
	virtual FMargin GetContentPadding() const override;
	// FThumbnail interface

	virtual void SetSingleTime(float GlobalTime) override;
	virtual FText HandleThumbnailTextBlockText() const override;

private:

	/** Get a representative camera for the given time */
	const AActor* GetCameraForFrame(float Time) const;

	/** Callback for executing a "Set Camera" menu entry in the context menu. */
	void HandleSetCameraMenuEntryExecute(AActor* InCamera);

};

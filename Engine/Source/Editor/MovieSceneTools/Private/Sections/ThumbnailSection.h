// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencerSection.h"
#include "TrackEditorThumbnail.h"


class FTrackEditorThumbnailPool;
class IMenu;
class ISectionLayoutBuilder;
class SWidget;
class UMovieSceneSection;


struct FThumbnailCameraSettings
{
	float AspectRatio;
};


/**
 * Thumbnail section, which paints and ticks the appropriate section.
 */
class FThumbnailSection
	: public ISequencerSection
	, public IThumbnailClient
	, public TSharedFromThis<FThumbnailSection>
{
public:

	/** Create and initialize a new instance. */
	FThumbnailSection(TSharedPtr<ISequencer> InSequencer, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, UMovieSceneSection& InSection);

	/** Virtual destructor. */
	~FThumbnailSection();

public:

	/** @return The sequencer widget owning the MovieScene section. */
	TSharedRef<SWidget> GetSequencerWidget()
	{
		return SequencerPtr.Pin()->GetSequencerWidget();
	}

	/** Get whether the text is renameable */
	virtual bool CanRename() const { return false; }

	/** Callback for getting the text of the track name text block. */
	virtual FText HandleThumbnailTextBlockText() const { return FText::GetEmpty(); }

	/** Callback for when the text of the track name text block has changed. */
	virtual void HandleThumbnailTextBlockTextCommitted(const FText& NewThumbnailName, ETextCommit::Type CommitType) { }

public:

	/** Set this thumbnail section to draw a single thumbnail at the specified time */
	virtual void SetSingleTime(float GlobalTime) = 0;

public:

	//~ ISequencerSection interface

	virtual bool AreSectionsConnected() const override;
	virtual void GenerateSectionLayout(ISectionLayoutBuilder& LayoutBuilder) const override { }
	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding) override;
	virtual TSharedRef<SWidget> GenerateSectionWidget() override;
	virtual float GetSectionGripSize() const override;
	virtual float GetSectionHeight() const override;
	virtual UMovieSceneSection* GetSectionObject() override;
	virtual FText GetSectionTitle() const override;
	virtual int32 OnPaintSection( FSequencerSectionPainter& InPainter ) const override;
	virtual void Tick( const FGeometry& AllottedGeometry, const FGeometry& ParentGeometry, const double InCurrentTime, const float InDeltaTime ) override;

public:

	//~ IThumbnailClient interface

	virtual void PreDraw(FTrackEditorThumbnail& Thumbnail, FLevelEditorViewportClient& ViewportClient, FSceneViewport& SceneViewport) override;
	virtual void PostDraw(FTrackEditorThumbnail& Thumbnail, FLevelEditorViewportClient& ViewportClient, FSceneViewport& SceneViewport) override;

protected:

	/** Called to force a redraw of this section's thumbnails */
	void RedrawThumbnails();

protected:

	/** The section we are visualizing. */
	UMovieSceneSection* Section;

	/** The parent sequencer we are a part of. */
	TWeakPtr<ISequencer> SequencerPtr;

	/** A list of all thumbnails this CameraCut section has. */
	FTrackEditorThumbnailCache ThumbnailCache;

	/** Saved playback status. Used for restoring state when rendering thumbnails */
	EMovieScenePlayerStatus::Type SavedPlaybackStatus;
	
	/** Fade brush. */
	const FSlateBrush* WhiteBrush;
};

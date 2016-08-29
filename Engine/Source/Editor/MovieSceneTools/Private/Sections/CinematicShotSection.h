// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ThumbnailSection.h"


class FThumbnailSection;
class FTrackEditorThumbnail;
class FTrackEditorThumbnailPool;
class FCinematicShotTrackEditor; 
class IMenu;
class ISectionLayoutBuilder;
class UMovieSceneCinematicShotSection;
class FMovieSceneSequenceInstance;


/**
 * CinematicShot section, which paints and ticks the appropriate section.
 */
class FCinematicShotSection
	: public FThumbnailSection
{
public:

	/** Create and initialize a new instance. */
	FCinematicShotSection(TSharedPtr<ISequencer> InSequencer, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, UMovieSceneSection& InSection, TSharedPtr<FCinematicShotTrackEditor> InCinematicShotTrackEditor);

	/** Virtual destructor. */
	virtual ~FCinematicShotSection();

public:

	// ISequencerSection interface

	virtual void Tick(const FGeometry& AllottedGeometry, const FGeometry& ClippedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaintSection( FSequencerSectionPainter& Painter ) const override;
	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding) override;
	virtual FText GetDisplayName() const override;
	virtual FReply OnSectionDoubleClicked(const FGeometry& SectionGeometry, const FPointerEvent& MouseEvent) override;
	virtual float GetSectionHeight() const override;
	virtual FMargin GetContentPadding() const override;
	virtual void BeginResizeSection() override;
	virtual void ResizeSection(ESequencerSectionResizeMode ResizeMode, float ResizeTime) override;

	// FThumbnail interface
	virtual void SetSingleTime(float GlobalTime) override;
	virtual bool CanRename() const override { return true; }
	virtual FText HandleThumbnailTextBlockText() const override;
	virtual void HandleThumbnailTextBlockTextCommitted(const FText& NewThumbnailName, ETextCommit::Type CommitType) override;

private:

	/** Add shot takes menu */
	void AddTakesMenu(FMenuBuilder& MenuBuilder);

private:

	/** The section we are visualizing */
	UMovieSceneCinematicShotSection& SectionObject;

	/** The instance that this section is part of */
	TWeakPtr<FMovieSceneSequenceInstance> SequenceInstance;

	/** Sequencer interface */
	TWeakPtr<ISequencer> Sequencer;

	/** The cinematic shot track editor that contains this section */
	TWeakPtr<FCinematicShotTrackEditor> CinematicShotTrackEditor;

	/** Cached start offset value valid only during resize */
	float InitialStartOffsetDuringResize;
	
	/** Cached start time valid only during resize */
	float InitialStartTimeDuringResize;

	struct FCinematicSectionCache
	{
		FCinematicSectionCache(UMovieSceneCinematicShotSection* Section = nullptr);

		bool operator!=(const FCinematicSectionCache& RHS) const
		{
			return !FMath::IsNearlyEqual(ActualStartTime, RHS.ActualStartTime, 0.001f) || TimeScale != RHS.TimeScale;
		}

		float ActualStartTime;
		float TimeScale;
	};

	/** Cached section thumbnail data */
	FCinematicSectionCache ThumbnailCacheData;
};

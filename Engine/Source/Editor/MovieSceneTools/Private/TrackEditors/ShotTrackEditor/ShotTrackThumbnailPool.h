// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class FShotTrackThumbnail;


/**
 * Shot Thumbnail pool, which keeps a list of thumbnails that
 * need to be drawn and draws them incrementally.
 */
class FShotTrackThumbnailPool
{
public:

	FShotTrackThumbnailPool(TSharedPtr<ISequencer> InSequencer, uint32 InMaxThumbnailsToDrawAtATime = 1);

public:

	/** Requests that the passed in thumbnails need to be drawn */
	void AddThumbnailsNeedingRedraw(const TArray<TSharedPtr<FShotTrackThumbnail>>& InThumbnails);

	/** Draws a small number of thumbnails that are enqueued for drawing */
	void DrawThumbnails();

	/** Informs the pool that the thumbnails passed in no longer need to be drawn */
	void RemoveThumbnailsNeedingRedraw(const TArray< TSharedPtr<FShotTrackThumbnail>>& InThumbnails);

private:

	/** Parent sequencer we're drawing thumbnails for */
	TWeakPtr<ISequencer> Sequencer;

	/** Thumbnails enqueued for drawing */
	TArray< TSharedPtr<FShotTrackThumbnail> > ThumbnailsNeedingDraw;

	/** How many thumbnails we are allowed to draw in a single DrawThumbnails call */
	uint32 MaxThumbnailsToDrawAtATime;
};

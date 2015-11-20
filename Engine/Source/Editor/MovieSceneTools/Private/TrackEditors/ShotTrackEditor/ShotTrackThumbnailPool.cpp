// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"


/* FShotSequencerSection structors
 *****************************************************************************/

FShotTrackThumbnailPool::FShotTrackThumbnailPool(TSharedPtr<ISequencer> InSequencer, uint32 InMaxThumbnailsToDrawAtATime)
	: Sequencer(InSequencer)
	, ThumbnailsNeedingDraw()
	, MaxThumbnailsToDrawAtATime(InMaxThumbnailsToDrawAtATime)
{ }


/* FShotSequencerSection interface
 *****************************************************************************/

void FShotTrackThumbnailPool::AddThumbnailsNeedingRedraw(const TArray<TSharedPtr<FShotTrackThumbnail>>& InThumbnails)
{
	ThumbnailsNeedingDraw.Append(InThumbnails);
}


void FShotTrackThumbnailPool::DrawThumbnails()
{
	if (ThumbnailsNeedingDraw.Num() == 0)
	{
		return;
	}

	// save the global time
	float SavedTime = Sequencer.Pin()->GetGlobalTime();
	uint32 ThumbnailsDrawn = 0;

	for (int32 ThumbnailsIndex = 0; ThumbnailsDrawn < MaxThumbnailsToDrawAtATime && ThumbnailsIndex < ThumbnailsNeedingDraw.Num(); ++ThumbnailsIndex)
	{
		TSharedPtr<FShotTrackThumbnail> Thumbnail = ThumbnailsNeedingDraw[ThumbnailsIndex];
			
		if (Thumbnail->IsVisible())
		{
			
			bool bIsEnabled = Sequencer.Pin()->IsPerspectiveViewportCameraCutEnabled();

			Sequencer.Pin()->SetPerspectiveViewportCameraCutEnabled(false);
			
			Thumbnail->DrawThumbnail();
			
			Sequencer.Pin()->SetPerspectiveViewportCameraCutEnabled(bIsEnabled);
			++ThumbnailsDrawn;

			ThumbnailsNeedingDraw.Remove(Thumbnail);
		}
		else if (!Thumbnail->IsValid())
		{
			ensure(0);

			ThumbnailsNeedingDraw.Remove(Thumbnail);
		}
	}
		
	if (ThumbnailsDrawn > 0)
	{
		// Ensure all buffers are updated
		FlushRenderingCommands();

		// restore the global time
		Sequencer.Pin()->SetGlobalTime(SavedTime);
	}
}


void FShotTrackThumbnailPool::RemoveThumbnailsNeedingRedraw(const TArray< TSharedPtr<FShotTrackThumbnail> >& InThumbnails)
{
	for (int32 i = 0; i < InThumbnails.Num(); ++i)
	{
		ThumbnailsNeedingDraw.Remove(InThumbnails[i]);
	}
}

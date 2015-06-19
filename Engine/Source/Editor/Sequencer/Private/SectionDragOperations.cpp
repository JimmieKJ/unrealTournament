// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SectionDragOperations.h"
#include "MovieSceneSection.h"
#include "ISequencerSection.h"
#include "IKeyArea.h"
#include "CommonMovieSceneTools.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieSceneDirectorTrack.h"
#include "Sequencer.h"

/** How many pixels near the mouse has to be before snapping occurs */
const float PixelSnapWidth = 10.f;

void FSequencerDragOperation::BeginTransaction( UMovieSceneSection& Section, const FText& TransactionDesc )
{
	// Begin an editor transaction and mark the section as transactional so it's state will be saved
	GEditor->BeginTransaction( TransactionDesc );
	Section.SetFlags( RF_Transactional );
	// Save the current state of the section
	Section.Modify();
}

void FSequencerDragOperation::EndTransaction()
{
	// End the transaction if one was active
	if( GEditor->IsTransactionActive() )
	{
		GEditor->EndTransaction();
	}
	Sequencer.UpdateRuntimeInstances();
}

TRange<float> FSequencerDragOperation::GetSectionBoundaries(UMovieSceneSection* Section, TSharedPtr<FTrackNode> SequencerNode) const
{
	// Find the borders of where you can drag to
	float LowerBound = -FLT_MAX, UpperBound = FLT_MAX;

	// Also get the closest borders on either side
	const TArray< TSharedRef<ISequencerSection> >& Sections = SequencerNode->GetSections();
	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		const UMovieSceneSection* InSection = Sections[SectionIndex]->GetSectionObject();
		if (Section != InSection && Section->GetRowIndex() == InSection->GetRowIndex())
		{
			if (InSection->GetEndTime() <= Section->GetStartTime() && InSection->GetEndTime() > LowerBound)
			{
				LowerBound = InSection->GetEndTime();
			}
			if (InSection->GetStartTime() >= Section->GetEndTime() && InSection->GetStartTime() < UpperBound)
			{
				UpperBound = InSection->GetStartTime();
			}
		}
	}

	return TRange<float>(LowerBound, UpperBound);
}

void FSequencerDragOperation::GetSectionSnapTimes(TArray<float>& OutSnapTimes, UMovieSceneSection* Section, TSharedPtr<FTrackNode> SequencerNode, bool bIgnoreOurSectionCustomSnaps)
{
	// @todo Sequencer handle dilation snapping better

	// Collect all the potential snap times from other section borders
	const TArray< TSharedRef<ISequencerSection> >& Sections = SequencerNode->GetSections();
	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		const UMovieSceneSection* InSection = Sections[SectionIndex]->GetSectionObject();
		bool bIsThisSection = Section == InSection;
		if (!bIgnoreOurSectionCustomSnaps || !bIsThisSection)
		{
			InSection->GetSnapTimes(OutSnapTimes, Section != InSection);
		}
	}

	// snap to director track if it exists, and we are not the director track
	UMovieSceneTrack* OuterTrack = Cast<UMovieSceneTrack>(Section->GetOuter());
	UMovieScene* MovieScene = Cast<UMovieScene>(OuterTrack->GetOuter());
	UMovieSceneTrack* DirectorTrack = MovieScene->FindMasterTrack(UMovieSceneDirectorTrack::StaticClass());
	if (DirectorTrack && OuterTrack != DirectorTrack)
	{
		TArray<UMovieSceneSection*> ShotSections = DirectorTrack->GetAllSections();
		for (int32 SectionIndex = 0; SectionIndex < ShotSections.Num(); ++SectionIndex)
		{
			auto Shot = ShotSections[SectionIndex];
			Shot->GetSnapTimes(OutSnapTimes, true);
		}
	}
}

bool FSequencerDragOperation::SnapToTimes(TArray<float> InitialTimes, const TArray<float>& SnapTimes, const FTimeToPixel& TimeToPixelConverter, float& OutInitialTime, float& OutSnapTime)
{
	bool bSuccess = false;
	float ClosestTimePixelDistance = PixelSnapWidth;
	
	for (int32 InitialTimeIndex = 0; InitialTimeIndex < InitialTimes.Num(); ++InitialTimeIndex)
	{
		float InitialTime = InitialTimes[InitialTimeIndex];
		float PixelXOfTime = TimeToPixelConverter.TimeToPixel(InitialTime);

		for (int32 SnapTimeIndex = 0; SnapTimeIndex < SnapTimes.Num(); ++SnapTimeIndex)
		{
			float SnapTime = SnapTimes[SnapTimeIndex];
			float PixelXOfSnapTime = TimeToPixelConverter.TimeToPixel(SnapTime);

			float PixelDistance = FMath::Abs(PixelXOfTime - PixelXOfSnapTime);
			if (PixelDistance < ClosestTimePixelDistance)
			{
				ClosestTimePixelDistance = PixelDistance;
				OutInitialTime = InitialTime;
				OutSnapTime = SnapTime;
				bSuccess = true;
			}
		}
	}

	return bSuccess;
}

TOptional<float> FSequencerDragOperation::SnapToTimes(float InitialTime, const TArray<float>& SnapTimes, const FTimeToPixel& TimeToPixelConverter)
{
	TArray<float> InitialTimes;
	InitialTimes.Add(InitialTime);

	float OutSnapTime = 0.f;
	float OutInitialTime = 0.f;
	bool bSuccess = SnapToTimes(InitialTimes, SnapTimes, TimeToPixelConverter, OutInitialTime, OutSnapTime);
	if (bSuccess)
	{
		return TOptional<float>(OutSnapTime);
	}
	return TOptional<float>();
}


FResizeSection::FResizeSection( FSequencer& Sequencer, UMovieSceneSection& InSection, bool bInDraggingByEnd )
	: FSequencerDragOperation( Sequencer )
	, Section( &InSection )
	, bDraggingByEnd( bInDraggingByEnd )
{
}

void FResizeSection::OnBeginDrag(const FVector2D& LocalMousePos, TSharedPtr<FTrackNode> SequencerNode)
{
	if( Section.IsValid() )
	{
		BeginTransaction( *Section, NSLOCTEXT("Sequencer", "DragSectionEdgeTransaction", "Resize section") );
	}
}

void FResizeSection::OnEndDrag(TSharedPtr<FTrackNode> SequencerNode)
{
	EndTransaction();
}

void FResizeSection::OnDrag( const FPointerEvent& MouseEvent, const FVector2D& LocalMousePos, const FTimeToPixel& TimeToPixelConverter, TSharedPtr<FTrackNode> SequencerNode )
{
	bool bIsDilating = MouseEvent.IsControlDown();

	if( Section.IsValid() )
	{
		// Convert the current mouse position to a time
		float NewTime = TimeToPixelConverter.PixelToTime( LocalMousePos.X );

		// Find the borders of where you can drag to
		TRange<float> SectionBoundaries = GetSectionBoundaries(Section.Get(), SequencerNode);
		
		// Snapping
		if ( Settings->GetIsSnapEnabled() )
		{
			bool bSnappedToSection = false;
			if ( Settings->GetSnapSectionTimesToSections() )
		{
			TArray<float> TimesToSnapTo;
			GetSectionSnapTimes(TimesToSnapTo, Section.Get(), SequencerNode, bIsDilating);

			TOptional<float> NewSnappedTime = SnapToTimes(NewTime, TimesToSnapTo, TimeToPixelConverter);

			if (NewSnappedTime.IsSet())
			{
				NewTime = NewSnappedTime.GetValue();
					bSnappedToSection = true;
				}
			}

			if ( bSnappedToSection == false && Settings->GetSnapSectionTimesToInterval() )
			{
				NewTime = Settings->SnapTimeToInterval(NewTime);
			}
		}

		if( bDraggingByEnd )
		{
			// Dragging the end of a section
			// Ensure we aren't shrinking past the start time
			NewTime = FMath::Clamp( NewTime, Section->GetStartTime(), SectionBoundaries.GetUpperBoundValue() );

			if (bIsDilating)
			{
				float NewSize = NewTime - Section->GetStartTime();
				float DilationFactor = NewSize / Section->GetTimeSize();
				Section->DilateSection(DilationFactor, Section->GetStartTime());
			}
			else
			{
				Section->SetEndTime( NewTime );
			}
		}
		else if( !bDraggingByEnd )
		{
			// Dragging the start of a section
			// Ensure we arent expanding past the end time
			NewTime = FMath::Clamp( NewTime, SectionBoundaries.GetLowerBoundValue(), Section->GetEndTime() );
			
			if (bIsDilating)
			{
				float NewSize = Section->GetEndTime() - NewTime;
				float DilationFactor = NewSize / Section->GetTimeSize();
				Section->DilateSection(DilationFactor, Section->GetEndTime());
			}
			else
			{
				Section->SetStartTime( NewTime );
			}
		}
	}
}

FMoveSection::FMoveSection( FSequencer& Sequencer, UMovieSceneSection& InSection )
	: FSequencerDragOperation( Sequencer )
	, Section( &InSection )
	, DragOffset(ForceInit)
{
}

void FMoveSection::OnBeginDrag(const FVector2D& LocalMousePos, TSharedPtr<FTrackNode> SequencerNode)
{
	if( Section.IsValid() )
	{
		BeginTransaction( *Section, NSLOCTEXT("Sequencer", "MoveSectionTransaction", "Move Section") );
		
		DragOffset = LocalMousePos;
	}
}

void FMoveSection::OnEndDrag(TSharedPtr<FTrackNode> SequencerNode)
{
	EndTransaction();

	if (Section.IsValid())
	{
		SequencerNode->FixRowIndices();
	}
}

void FMoveSection::OnDrag( const FPointerEvent& MouseEvent, const FVector2D& LocalMousePos, const FTimeToPixel& TimeToPixelConverter, TSharedPtr<FTrackNode> SequencerNode )
{
	if( Section.IsValid() )
	{
		auto Sections = SequencerNode->GetSections();
		TArray<UMovieSceneSection*> MovieSceneSections;
		for (int32 i = 0; i < Sections.Num(); ++i)
		{
			MovieSceneSections.Add(Sections[i]->GetSectionObject());
		}
		

		FVector2D TotalDelta = LocalMousePos - DragOffset;
		float DistanceMoved = TotalDelta.X / TimeToPixelConverter.GetPixelsPerInput();
		
		float DeltaTime = DistanceMoved;
		
		if ( Settings->GetIsSnapEnabled() )
		{
			bool bSnappedToSection = false;
			if ( Settings->GetSnapSectionTimesToSections() )
		{
			TArray<float> TimesToSnapTo;
			GetSectionSnapTimes(TimesToSnapTo, Section.Get(), SequencerNode, true);

			TArray<float> TimesToSnap;
			TimesToSnap.Add(DistanceMoved + Section->GetStartTime());
			TimesToSnap.Add(DistanceMoved + Section->GetEndTime());

			float OutSnappedTime = 0.f;
			float OutNewTime = 0.f;
				if ( SnapToTimes( TimesToSnap, TimesToSnapTo, TimeToPixelConverter, OutSnappedTime, OutNewTime ) )
			{
				DeltaTime = OutNewTime - (OutSnappedTime - DistanceMoved);
					bSnappedToSection = true;
			}
		}

			if ( bSnappedToSection == false && Settings->GetSnapSectionTimesToInterval() )
			{
				float NewStartTime = DistanceMoved + Section->GetStartTime();
				DeltaTime = Settings->SnapTimeToInterval( NewStartTime ) - Section->GetStartTime();
			}
		}

		int32 TargetRowIndex = Section->GetRowIndex();

		// vertical dragging - master tracks only
		if (SequencerNode->GetTrack()->SupportsMultipleRows() && Sections.Num() > 1)
		{
			float TrackHeight = Sections[0]->GetSectionHeight();

			if (LocalMousePos.Y < 0.f || LocalMousePos.Y > TrackHeight)
			{
				int32 MaxRowIndex = 0;
				for (int32 i = 0; i < Sections.Num(); ++i)
				{
					if (Sections[i]->GetSectionObject() != Section.Get())
					{
						MaxRowIndex = FMath::Max(MaxRowIndex, Sections[i]->GetSectionObject()->GetRowIndex());
					}
				}

				TargetRowIndex = FMath::Clamp(Section->GetRowIndex() + FMath::FloorToInt(LocalMousePos.Y / TrackHeight),
					0, MaxRowIndex + 1);
			}
		}
		
		bool bDeltaX = !FMath::IsNearlyZero(TotalDelta.X);
		bool bDeltaY = TargetRowIndex != Section->GetRowIndex();

		if (bDeltaX && bDeltaY &&
			!Section->OverlapsWithSections(MovieSceneSections, TargetRowIndex - Section->GetRowIndex(), DeltaTime))
		{
			Section->MoveSection(DeltaTime);
			Section->SetRowIndex(TargetRowIndex);
		}
		else
		{
			if (bDeltaY &&
				!Section->OverlapsWithSections(MovieSceneSections, TargetRowIndex - Section->GetRowIndex(), 0.f))
			{
				Section->SetRowIndex(TargetRowIndex);
			}

			if (bDeltaX)
			{
				if (!Section->OverlapsWithSections(MovieSceneSections, 0, DeltaTime))
				{
					Section->MoveSection(DeltaTime);
				}
				else
				{
					// Find the borders of where you can move to
					TRange<float> SectionBoundaries = GetSectionBoundaries(Section.Get(), SequencerNode);

					float LeftMovementMaximum = SectionBoundaries.GetLowerBoundValue() - Section->GetStartTime();
					float RightMovementMaximum = SectionBoundaries.GetUpperBoundValue() - Section->GetEndTime();

					// Tell the section to move itself and any data it has
					Section->MoveSection( FMath::Clamp(DeltaTime, LeftMovementMaximum, RightMovementMaximum) );
				}
			}
		}
	}
}

bool FMoveSection::IsSectionAtNewLocationValid(UMovieSceneSection* InSection, int32 RowIndex, float DeltaTime, TSharedPtr<FTrackNode> SequencerNode) const
{
	// Also get the closest borders on either side
	const TArray< TSharedRef<ISequencerSection> >& Sections = SequencerNode->GetSections();
	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		const UMovieSceneSection* MovieSection = Sections[SectionIndex]->GetSectionObject();
		if (InSection != MovieSection && MovieSection->GetRowIndex() == RowIndex)
		{
			TRange<float> OffsetSectionRange = TRange<float>(
				InSection->GetRange().GetLowerBoundValue() + DeltaTime,
				InSection->GetRange().GetUpperBoundValue() + DeltaTime);
			if (OffsetSectionRange.Overlaps(MovieSection->GetRange()))
			{
				return false;
			}
		}
	}
	return true;
}


void FMoveKeys::OnBeginDrag(const FVector2D& LocalMousePos, TSharedPtr<FTrackNode> SequencerNode)
{
	check( SelectedKeys->Num() > 0 )

	// Begin an editor transaction and mark the section as transactional so it's state will be saved
	GEditor->BeginTransaction( NSLOCTEXT("Sequencer", "MoveKeysTransaction", "Move Keys") );

	TSet<UMovieSceneSection*> ModifiedSections;
	for( FSelectedKey SelectedKey : *SelectedKeys )
	{
		UMovieSceneSection* OwningSection = SelectedKey.Section;

		// Only modify sections once
		if( !ModifiedSections.Contains( OwningSection ) )
		{
			OwningSection->SetFlags( RF_Transactional );

			// Save the current state of the section
			OwningSection->Modify();

			// Section has been modified
			ModifiedSections.Add( OwningSection );
		}
	}
}

void FMoveKeys::OnEndDrag(TSharedPtr<FTrackNode> SequencerNode)
{
	EndTransaction();
}

void FMoveKeys::OnDrag( const FPointerEvent& MouseEvent, const FVector2D& LocalMousePos, const FTimeToPixel& TimeToPixelConverter, TSharedPtr<FTrackNode> SequencerNode )
{
	// Convert the delta position to a delta time amount that was moved
	float MouseTime = TimeToPixelConverter.PixelToTime(LocalMousePos.X);
	float SelectedKeyTime = DraggedKey.KeyArea->GetKeyTime(DraggedKey.KeyHandle.GetValue());
	float DistanceMoved = MouseTime - SelectedKeyTime;

	if( DistanceMoved != 0.0f )
	{
		float TimeDelta = DistanceMoved;
		// Snapping
		if ( Settings->GetIsSnapEnabled() )
		{
			bool bSnappedToKeyTime = false;
			if ( Settings->GetSnapKeyTimesToKeys() )
			{
				TArray<float> OutSnapTimes;
				GetKeySnapTimes(OutSnapTimes, SequencerNode);

				TArray<float> InitialTimes;
				for ( FSelectedKey SelectedKey : *SelectedKeys )
				{
					InitialTimes.Add(SelectedKey.KeyArea->GetKeyTime(SelectedKey.KeyHandle.GetValue()) + DistanceMoved);
				}
				float OutInitialTime = 0.f;
				float OutSnapTime = 0.f;
				if ( SnapToTimes( InitialTimes, OutSnapTimes, TimeToPixelConverter, OutInitialTime, OutSnapTime ) )
				{
					bSnappedToKeyTime = true;
					TimeDelta = OutSnapTime - (OutInitialTime - DistanceMoved);
				}
			}

			if ( bSnappedToKeyTime == false && Settings->GetSnapKeyTimesToInterval() )
			{
				TimeDelta = Settings->SnapTimeToInterval( MouseTime ) - SelectedKeyTime;
			}
		}

		for( FSelectedKey SelectedKey : *SelectedKeys )
		{
			UMovieSceneSection* Section = SelectedKey.Section;

			TSharedPtr<IKeyArea>& KeyArea = SelectedKey.KeyArea;


			// Tell the key area to move the key.  We reset the key index as a result of the move because moving a key can change it's internal index 
			KeyArea->MoveKey( SelectedKey.KeyHandle.GetValue(), TimeDelta );

			// Update the key that moved
			float NewKeyTime = KeyArea->GetKeyTime( SelectedKey.KeyHandle.GetValue() );

			// If the key moves outside of the section resize the section to fit the key
			// @todo Sequencer - Doesn't account for hitting other sections 
			if( NewKeyTime > Section->GetEndTime() )
			{
				Section->SetEndTime( NewKeyTime );
			}
			else if( NewKeyTime < Section->GetStartTime() )
			{
				Section->SetStartTime( NewKeyTime );
			}
		}
	}
}

void FMoveKeys::GetKeySnapTimes(TArray<float>& OutSnapTimes, TSharedPtr<FTrackNode> SequencerNode)
{
	// snap to non-selected keys
	TArray< TSharedRef<FSectionKeyAreaNode> > KeyAreaNodes;
	SequencerNode->GetChildKeyAreaNodesRecursively(KeyAreaNodes);

	for (int32 NodeIndex = 0; NodeIndex < KeyAreaNodes.Num(); ++NodeIndex)
	{
		TArray< TSharedRef<IKeyArea> > KeyAreas = KeyAreaNodes[NodeIndex]->GetAllKeyAreas();
		for (int32 KeyAreaIndex = 0; KeyAreaIndex < KeyAreas.Num(); ++KeyAreaIndex)
		{
			auto KeyArea = KeyAreas[KeyAreaIndex];

			TArray<FKeyHandle> KeyHandles = KeyArea->GetUnsortedKeyHandles();
			for( int32 KeyIndex = 0; KeyIndex < KeyHandles.Num(); ++KeyIndex )
			{
				FKeyHandle KeyHandle = KeyHandles[KeyIndex];
				bool bKeyIsSnappable = true;
				for ( FSelectedKey SelectedKey : *SelectedKeys )
				{
					if (SelectedKey.KeyArea == KeyArea && SelectedKey.KeyHandle.GetValue() == KeyHandle)
					{
						bKeyIsSnappable = false;
						break;
					}
				}
				if (bKeyIsSnappable)
				{
					OutSnapTimes.Add(KeyArea->GetKeyTime(KeyHandle));
				}
			}
		}
	}
}

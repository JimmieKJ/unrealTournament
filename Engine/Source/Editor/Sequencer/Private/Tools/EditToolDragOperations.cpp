// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "EditToolDragOperations.h"
#include "Sequencer.h"
#include "MovieSceneSection.h"
#include "ISequencerSection.h"
#include "IKeyArea.h"
#include "CommonMovieSceneTools.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "VirtualTrackArea.h"

struct FDefaultKeySnappingCandidates : ISequencerSnapCandidate
{
	FDefaultKeySnappingCandidates(const TSet<FSequencerSelectedKey>& InKeysToExclude)
		: KeysToExclude(InKeysToExclude)
	{}

	virtual bool IsKeyApplicable(FKeyHandle KeyHandle, const TSharedPtr<IKeyArea>& KeyArea, UMovieSceneSection* Section) override
	{
		return !KeysToExclude.Contains(FSequencerSelectedKey(*Section, KeyArea, KeyHandle));
	}

	const TSet<FSequencerSelectedKey>& KeysToExclude;
};

struct FDefaultSectionSnappingCandidates : ISequencerSnapCandidate
{
	FDefaultSectionSnappingCandidates(const TArray<FSectionHandle>& InSectionsToIgnore)
	{
		for (auto& SectionHandle : InSectionsToIgnore)
		{
			SectionsToIgnore.Add(SectionHandle.GetSectionObject());
		}
	}

	virtual bool AreSectionBoundsApplicable(UMovieSceneSection* Section) override
	{
		return !SectionsToIgnore.Contains(Section);
	}

	TSet<UMovieSceneSection*> SectionsToIgnore;
};

TOptional<FSequencerSnapField::FSnapResult> SnapToInterval(const TArray<float>& InTimes, float Threshold, const USequencerSettings& Settings)
{
	TOptional<FSequencerSnapField::FSnapResult> Result;

	float SnapAmount = 0.f;
	for (float Time : InTimes)
	{
		float IntervalSnap = Settings.SnapTimeToInterval(Time);
		float ThisSnapAmount = IntervalSnap - Time;
		if (FMath::Abs(ThisSnapAmount) <= Threshold)
		{
			if (!Result.IsSet() || FMath::Abs(ThisSnapAmount) < SnapAmount)
			{
				Result = FSequencerSnapField::FSnapResult{Time, IntervalSnap};
				SnapAmount = ThisSnapAmount;
			}
		}
	}

	return Result;
}

/** How many pixels near the mouse has to be before snapping occurs */
const float PixelSnapWidth = 10.f;

TRange<float> GetSectionBoundaries(UMovieSceneSection* Section, TSharedPtr<FSequencerTrackNode> SequencerNode)
{
	// Find the borders of where you can drag to
	float LowerBound = -FLT_MAX, UpperBound = FLT_MAX;

	// Also get the closest borders on either side
	const TArray< TSharedRef<ISequencerSection> >& Sections = SequencerNode->GetSections();
	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		const UMovieSceneSection* TestSection = Sections[SectionIndex]->GetSectionObject();
		if (Section != TestSection && Section->GetRowIndex() == TestSection->GetRowIndex())
		{
			if (TestSection->GetEndTime() <= Section->GetStartTime() && TestSection->GetEndTime() > LowerBound)
			{
				LowerBound = TestSection->GetEndTime();
			}
			if (TestSection->GetStartTime() >= Section->GetEndTime() && TestSection->GetStartTime() < UpperBound)
			{
				UpperBound = TestSection->GetStartTime();
			}
		}
	}

	return TRange<float>(LowerBound, UpperBound);
}

FEditToolDragOperation::FEditToolDragOperation( FSequencer& InSequencer )
	: Sequencer(InSequencer)
{
	Settings = Sequencer.GetSettings();
}

FCursorReply FEditToolDragOperation::GetCursor() const
{
	return FCursorReply::Cursor( EMouseCursor::Default );
}

int32 FEditToolDragOperation::OnPaint(const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	return LayerId;
}

void FEditToolDragOperation::BeginTransaction( TArray< FSectionHandle >& Sections, const FText& TransactionDesc )
{
	// Begin an editor transaction and mark the section as transactional so it's state will be saved
	Transaction.Reset( new FScopedTransaction(TransactionDesc) );

	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); )
	{
		UMovieSceneSection* SectionObj = Sections[SectionIndex].GetSectionObject();

		SectionObj->SetFlags( RF_Transactional );
		// Save the current state of the section
		if (SectionObj->TryModify())
		{
			++SectionIndex;
		}
		else
		{
			Sections.RemoveAt(SectionIndex);
		}
	}
}

void FEditToolDragOperation::EndTransaction()
{
	Transaction.Reset();
	Sequencer.NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
}

FResizeSection::FResizeSection( FSequencer& InSequencer, TArray<FSectionHandle> InSections, bool bInDraggingByEnd )
	: FEditToolDragOperation( InSequencer )
	, Sections( MoveTemp(InSections) )
	, bDraggingByEnd(bInDraggingByEnd)
	, MouseDownTime(0.f)
{
}

void FResizeSection::OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	BeginTransaction( Sections, NSLOCTEXT("Sequencer", "DragSectionEdgeTransaction", "Resize section") );

	MouseDownTime = VirtualTrackArea.PixelToTime(LocalMousePos.X);

	// Construct a snap field of unselected sections
	FDefaultSectionSnappingCandidates SnapCandidates(Sections);
	SnapField = FSequencerSnapField(Sequencer, SnapCandidates, ESequencerEntity::Section);

	DraggedKeyHandles.Empty();
	SectionInitTimes.Empty();

	bool bIsDilating = MouseEvent.IsControlDown();

	for (auto& Handle : Sections)
	{
		UMovieSceneSection* Section = Handle.GetSectionObject();

		for (auto SequencerSection : Handle.TrackNode->GetSections())
		{
			if (SequencerSection->GetSectionObject() == Section)
			{
				if (bIsDilating)
				{
					SequencerSection->BeginDilateSection();
				}
				else
				{
					SequencerSection->BeginResizeSection();
				}
				break;
			}
		}

		Section->GetKeyHandles(DraggedKeyHandles, Section->GetRange());
		SectionInitTimes.Add(Section, bDraggingByEnd ? Section->GetEndTime() : Section->GetStartTime());
	}
}

void FResizeSection::OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	EndTransaction();

	DraggedKeyHandles.Empty();
}

void FResizeSection::OnDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	bool bIsDilating = MouseEvent.IsControlDown();

	// Convert the current mouse position to a time
	float DeltaTime = VirtualTrackArea.PixelToTime(LocalMousePos.X) - MouseDownTime;

	// Snapping
	if ( Settings->GetIsSnapEnabled() )
	{
		TArray<float> SectionTimes;
		for (auto& Handle : Sections)
		{
			UMovieSceneSection* Section = Handle.GetSectionObject();
			SectionTimes.Add(SectionInitTimes[Section] + DeltaTime);
		}

		float SnapThresold = VirtualTrackArea.PixelToTime(PixelSnapWidth) - VirtualTrackArea.PixelToTime(0.f);

		TOptional<FSequencerSnapField::FSnapResult> SnappedTime;

		if (Settings->GetSnapSectionTimesToSections())
		{
			SnappedTime = SnapField->Snap(SectionTimes, SnapThresold);
		}

		if (!SnappedTime.IsSet() && Settings->GetSnapSectionTimesToInterval())
		{
			SnappedTime = SnapToInterval(SectionTimes, Settings->GetTimeSnapInterval()/2.f, *Settings);
		}

		if (SnappedTime.IsSet())
		{
			// Add the snapped amopunt onto the delta
			DeltaTime += SnappedTime->Snapped - SnappedTime->Original;
		}
	}

	for (auto& Handle : Sections)
	{
		UMovieSceneSection* Section = Handle.GetSectionObject();

		// Find the corresponding sequencer section to this movie scene section
		for (auto SequencerSection : Handle.TrackNode->GetSections())
		{
			if (SequencerSection->GetSectionObject() == Section)
			{
				float NewTime = SectionInitTimes[Section] + DeltaTime;

				if( bDraggingByEnd )
				{
					// Dragging the end of a section
					// Ensure we aren't shrinking past the start time
					NewTime = FMath::Max( NewTime, Section->GetStartTime() );

					if (bIsDilating)
					{
						float NewSize = NewTime - Section->GetStartTime();
						float DilationFactor = NewSize / Section->GetTimeSize();
						SequencerSection->DilateSection(DilationFactor, Section->GetStartTime(), DraggedKeyHandles);
					}
					else
					{
						SequencerSection->ResizeSection( SSRM_TrailingEdge, NewTime );
					}
				}
				else
				{
					// Dragging the start of a section
					// Ensure we arent expanding past the end time
					NewTime = FMath::Min( NewTime, Section->GetEndTime() );

					if (bIsDilating)
					{
						float NewSize = Section->GetEndTime() - NewTime;
						float DilationFactor = NewSize / Section->GetTimeSize();
						SequencerSection->DilateSection(DilationFactor, Section->GetEndTime(), DraggedKeyHandles);
					}
					else
					{
						SequencerSection->ResizeSection( SSRM_LeadingEdge, NewTime );
					}
				}

				UMovieSceneTrack* OuterTrack = Section->GetTypedOuter<UMovieSceneTrack>();
				if (OuterTrack)
				{
					OuterTrack->Modify();
					OuterTrack->OnSectionMoved(*Section);
				}

				break;
			}
		}
	}

	Sequencer.NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
}

FMoveSection::FMoveSection( FSequencer& InSequencer, TArray<FSectionHandle> InSections )
	: FEditToolDragOperation( InSequencer )
{
	// Only allow sections that are not infinite to be movable.
	for (auto InSection : InSections)
	{
		if (!InSection.GetSectionObject()->IsInfinite())
		{
			Sections.Add(InSection);
		}
	}
}

void FMoveSection::OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	if (!Sections.Num())
	{
		return;
	}

	BeginTransaction( Sections, NSLOCTEXT("Sequencer", "MoveSectionTransaction", "Move Section") );

	// Construct a snap field of unselected sections
	FDefaultSectionSnappingCandidates SnapCandidates(Sections);
	SnapField = FSequencerSnapField(Sequencer, SnapCandidates, ESequencerEntity::Section);

	DraggedKeyHandles.Empty();

	const FVector2D InitialPosition = VirtualTrackArea.PhysicalToVirtual(LocalMousePos);

	RelativeOffsets.Reserve(Sections.Num());
	for (int32 Index = 0; Index < Sections.Num(); ++Index)
	{
		auto* Section = Sections[Index].GetSectionObject();

		Section->GetKeyHandles(DraggedKeyHandles, Section->GetRange());
		RelativeOffsets.Add(FRelativeOffset{ Section->GetStartTime() - InitialPosition.X, Section->GetEndTime() - InitialPosition.X });
	}
}

void FMoveSection::OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	if (!Sections.Num())
	{
		return;
	}

	DraggedKeyHandles.Empty();

	for (auto& Handle : Sections)
	{
		Handle.TrackNode->FixRowIndices();

		UMovieSceneSection* Section = Handle.GetSectionObject();
		UMovieSceneTrack* OuterTrack = Cast<UMovieSceneTrack>(Section->GetOuter());

		if (OuterTrack)
		{
			OuterTrack->Modify();
			OuterTrack->OnSectionMoved(*Section);
		}
	}

	EndTransaction();
}

void FMoveSection::OnDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	if (!Sections.Num())
	{
		return;
	}

	LocalMousePos.Y = FMath::Clamp(LocalMousePos.Y, 0.f, VirtualTrackArea.GetPhysicalSize().Y);

	// Convert the current mouse position to a time
	FVector2D VirtualMousePos = VirtualTrackArea.PhysicalToVirtual(LocalMousePos);

	// Snapping
	if ( Settings->GetIsSnapEnabled() )
	{
		TArray<float> SectionTimes;
		SectionTimes.Reserve(RelativeOffsets.Num());
		for (const auto& Offset : RelativeOffsets)
		{
			SectionTimes.Add(VirtualMousePos.X + Offset.StartTime);
			SectionTimes.Add(VirtualMousePos.X + Offset.EndTime);
		}

		float SnapThresold = VirtualTrackArea.PixelToTime(PixelSnapWidth) - VirtualTrackArea.PixelToTime(0.f);

		TOptional<FSequencerSnapField::FSnapResult> SnappedTime;

		if (Settings->GetSnapSectionTimesToSections())
		{
			SnappedTime = SnapField->Snap(SectionTimes, SnapThresold);
		}

		if (!SnappedTime.IsSet() && Settings->GetSnapSectionTimesToInterval())
		{
			SnappedTime = SnapToInterval(SectionTimes, Settings->GetTimeSnapInterval()/2.f, *Settings);
		}

		if (SnappedTime.IsSet())
		{
			// Add the snapped amopunt onto the delta
			VirtualMousePos.X += SnappedTime->Snapped - SnappedTime->Original;
		}
	}

	for (int32 Index = 0; Index < Sections.Num(); ++Index)
	{
		auto& Handle = Sections[Index];
		UMovieSceneSection* Section = Handle.GetSectionObject();

		// *2 because we store start/end times
		const float DeltaTime = VirtualMousePos.X + RelativeOffsets[Index].StartTime - Section->GetStartTime();

		auto SequencerNodeSections = Handle.TrackNode->GetSections();
		
		TArray<UMovieSceneSection*> MovieSceneSections;
		for (int32 i = 0; i < SequencerNodeSections.Num(); ++i)
		{
			MovieSceneSections.Add(SequencerNodeSections[i]->GetSectionObject());
		}

		int32 TargetRowIndex = Section->GetRowIndex();

		// vertical dragging - master tracks only
		if (Handle.TrackNode->GetTrack()->SupportsMultipleRows() && SequencerNodeSections.Num() > 1)
		{
			int32 NumRows = 0;
			for (auto* Sec : MovieSceneSections)
			{
				if (Sec != Section)
				{
					NumRows = FMath::Max(Sec->GetRowIndex() + 1, NumRows);
				}
			}
			
			// Compute the max row index whilst disregarding the one we're dragging
			const int32 MaxRowIndex = NumRows;

			// Now factor in the row index of the one we're dragging
			NumRows = FMath::Max(Section->GetRowIndex() + 1, NumRows);

			const float VirtualSectionHeight = Handle.TrackNode->GetVirtualBottom() - Handle.TrackNode->GetVirtualTop();
			const float VirtualRowHeight = VirtualSectionHeight / NumRows;
			const float MouseOffsetWithinRow = VirtualMousePos.Y - (Handle.TrackNode->GetVirtualTop() + VirtualRowHeight * TargetRowIndex);

			if (MouseOffsetWithinRow < 0 || MouseOffsetWithinRow > VirtualRowHeight)
			{
				const int32 NewIndex = FMath::FloorToInt((VirtualMousePos.Y - Handle.TrackNode->GetVirtualTop()) / VirtualRowHeight);

				if (NewIndex < 0)
				{
					// todo: Move everything else down?
				}

				TargetRowIndex = FMath::Clamp(NewIndex,	0, MaxRowIndex);
			}
		}

		bool bDeltaX = !FMath::IsNearlyZero(DeltaTime);
		bool bDeltaY = TargetRowIndex != Section->GetRowIndex();

		if (bDeltaX && bDeltaY &&
			!Section->OverlapsWithSections(MovieSceneSections, TargetRowIndex - Section->GetRowIndex(), DeltaTime))
		{
			Section->MoveSection(DeltaTime, DraggedKeyHandles);
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
					Section->MoveSection(DeltaTime, DraggedKeyHandles);
				}
				else
				{
					// Find the borders of where you can move to
					TRange<float> SectionBoundaries = GetSectionBoundaries(Section, Handle.TrackNode);

					float LeftMovementMaximum = SectionBoundaries.GetLowerBoundValue() - Section->GetStartTime();
					float RightMovementMaximum = SectionBoundaries.GetUpperBoundValue() - Section->GetEndTime();

					// Tell the section to move itself and any data it has
					Section->MoveSection( FMath::Clamp(DeltaTime, LeftMovementMaximum, RightMovementMaximum), DraggedKeyHandles );
				}
			}
		}
	}

	Sequencer.NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
}

void FMoveKeys::OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	check( SelectedKeys.Num() > 0 )

	FSequencerDisplayNode::DisableKeyGoupingRegeneration();

	FDefaultKeySnappingCandidates SnapCandidates(SelectedKeys);
	SnapField = FSequencerSnapField(Sequencer, SnapCandidates);

	// Begin an editor transaction and mark the section as transactional so it's state will be saved
	TArray<FSectionHandle> DummySections;
	BeginTransaction( DummySections, NSLOCTEXT("Sequencer", "MoveKeysTransaction", "Move Keys") );

	const float MouseTime = VirtualTrackArea.PixelToTime(LocalMousePos.X);

	for( FSequencerSelectedKey SelectedKey : SelectedKeys )
	{
		UMovieSceneSection* OwningSection = SelectedKey.Section;

		RelativeOffsets.Add(SelectedKey, SelectedKey.KeyArea->GetKeyTime(SelectedKey.KeyHandle.GetValue()) - MouseTime);

		// Only modify sections once
		if( !ModifiedSections.Contains( OwningSection ) )
		{
			OwningSection->SetFlags( RF_Transactional );

			// Save the current state of the section
			if (OwningSection->TryModify())
			{
				// Section has been modified
				ModifiedSections.Add( OwningSection );
			}
		}
	}
}

void FMoveKeys::OnDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	float MouseTime = VirtualTrackArea.PixelToTime(LocalMousePos.X);
	float DistanceMoved = MouseTime - VirtualTrackArea.PixelToTime(LocalMousePos.X - MouseEvent.GetCursorDelta().X);

	if( DistanceMoved == 0.0f )
	{
		return;
	}

	// Snapping
	if ( Settings->GetIsSnapEnabled() )
	{
		TArray<float> KeyTimes;
		for( FSequencerSelectedKey SelectedKey : SelectedKeys )
		{
			KeyTimes.Add(MouseTime + RelativeOffsets.FindRef(SelectedKey));
		}

		float SnapThresold = VirtualTrackArea.PixelToTime(PixelSnapWidth) - VirtualTrackArea.PixelToTime(0.f);

		TOptional<FSequencerSnapField::FSnapResult> SnappedTime;

		if (Settings->GetSnapKeyTimesToKeys())
		{
			SnappedTime = SnapField->Snap(KeyTimes, SnapThresold);
		}

		if (!SnappedTime.IsSet() && Settings->GetSnapKeyTimesToInterval())
		{
			SnappedTime = SnapToInterval(KeyTimes, Settings->GetTimeSnapInterval()/2.f, *Settings);
		}

		if (SnappedTime.IsSet())
		{
			MouseTime += SnappedTime->Snapped - SnappedTime->Original;
		}
	}

	float PrevNewKeyTime = FLT_MAX;

	for( FSequencerSelectedKey SelectedKey : SelectedKeys )
	{
		UMovieSceneSection* Section = SelectedKey.Section;

		if (!ModifiedSections.Contains(Section))
		{
			continue;
		}

		TSharedPtr<IKeyArea>& KeyArea = SelectedKey.KeyArea;

		float NewKeyTime = MouseTime + RelativeOffsets.FindRef(SelectedKey);
		float CurrentTime = KeyArea->GetKeyTime( SelectedKey.KeyHandle.GetValue() );

		// Tell the key area to move the key.  We reset the key index as a result of the move because moving a key can change it's internal index 
		KeyArea->MoveKey( SelectedKey.KeyHandle.GetValue(), NewKeyTime - CurrentTime );

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

		if (PrevNewKeyTime == FLT_MAX)
		{
			PrevNewKeyTime = NewKeyTime;
		}
		else if (!FMath::IsNearlyEqual(NewKeyTime, PrevNewKeyTime))
		{
			PrevNewKeyTime = -FLT_MAX;
		}
	}

	// Snap the play time to the new dragged key time if all the keyframes were dragged to the same time
	if (Settings->GetSnapPlayTimeToDraggedKey() && PrevNewKeyTime != FLT_MAX && PrevNewKeyTime != -FLT_MAX)
	{
		Sequencer.SetGlobalTime(PrevNewKeyTime);
	}

	Sequencer.NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
}

void FMoveKeys::OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	ModifiedSections.Empty();
	EndTransaction();
	FSequencerDisplayNode::EnableKeyGoupingRegeneration();
}

void FDuplicateKeys::OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	// Duplicate and select all the keys
	TSet<FSequencerSelectedKey> OldSelection = SelectedKeys;

	// Begin an editor transaction and mark the section as transactional so it's state will be saved
	TArray<FSectionHandle> DummySections;
	BeginTransaction( DummySections, NSLOCTEXT("Sequencer", "DuplicateKeysTransaction", "Duplicate Keys") );

	// Modify all the sections first
	for (const FSequencerSelectedKey& SelectedKey : SelectedKeys)
	{
		UMovieSceneSection* OwningSection = SelectedKey.Section;

		// Only modify sections once
		if (!ModifiedSections.Contains(OwningSection ))
		{
			OwningSection->SetFlags(RF_Transactional);
			// Save the current state of the section
			if (OwningSection->TryModify())
			{
				// Section has been modified
				ModifiedSections.Add( OwningSection );
			}
		}
	}

	// Then duplicate the keys

	// @todo sequencer: selection in transactions
	Sequencer.GetSelection().EmptySelectedKeys();
	for (const FSequencerSelectedKey& SelectedKey : OldSelection)
	{
		FSequencerSelectedKey NewKey = SelectedKey;
		NewKey.KeyHandle = SelectedKey.KeyArea->DuplicateKey(SelectedKey.KeyHandle.GetValue());
		Sequencer.GetSelection().AddToSelection(NewKey);
	}

	// Now start the move drag
	FMoveKeys::OnBeginDrag(MouseEvent, LocalMousePos, VirtualTrackArea);
}

void FDuplicateKeys::OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea)
{
	FMoveKeys::OnEndDrag(MouseEvent, LocalMousePos, VirtualTrackArea);

	EndTransaction();
}
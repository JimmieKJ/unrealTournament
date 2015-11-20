// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"

#include "SAnimSegmentsPanel.h"
#include "ScopedTransaction.h"
#include "DragAndDrop/AssetDragDropOp.h"

#define LOCTEXT_NAMESPACE "AnimSegmentPanel"

//////////////////////////////////////////////////////////////////////////
// SAnimSegmentPanel

void SAnimSegmentsPanel::Construct(const FArguments& InArgs)
{
	bDragging = false;
	const int32 NumTracks = 2;

	AnimTrack = InArgs._AnimTrack;
	ViewInputMin = InArgs._ViewInputMin;
	ViewInputMax = InArgs._ViewInputMax;

	OnAnimSegmentNodeClickedDelegate = InArgs._OnAnimSegmentNodeClicked;
	OnPreAnimUpdateDelegate			 = InArgs._OnPreAnimUpdate;
	OnPostAnimUpdateDelegate		 = InArgs._OnPostAnimUpdate;
	OnAnimSegmentRemovedDelegate	 = InArgs._OnAnimSegmentRemoved;

	// Register and bind ui commands
	FAnimSegmentsPanelCommands::Register();
	BindCommands();

	// Empty out current widget array
	TrackWidgets.Empty();

	// Animation Segment tracks
	TArray<TSharedPtr<STrackNode>> AnimNodes;

	FLinearColor SelectedColor = FLinearColor(1.0f,0.65,0.0f);

	TSharedPtr<SVerticalBox> AnimSegmentTracks;

	ChildSlot
	[
		SAssignNew(AnimSegmentTracks, SVerticalBox)
	];

	for (int32 TrackIdx=0; TrackIdx < NumTracks; TrackIdx++)
	{
		TSharedPtr<STrack> AnimSegmentTrack;

		AnimSegmentTracks->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding( FMargin(0.5f, 0.5f) )
			[
				SAssignNew(AnimSegmentTrack, STrack)							
				.TrackColor( InArgs._ColorTracker->GetNextColor() )
				.ViewInputMin(ViewInputMin)
				.ViewInputMax(ViewInputMax)
				.TrackMaxValue( InArgs._TrackMaxValue)
				//section stuff
				.OnBarDrag(InArgs._OnBarDrag)
				.OnBarDrop(InArgs._OnBarDrop)
				.OnBarClicked(InArgs._OnBarClicked)
				.DraggableBars(InArgs._DraggableBars)
				.DraggableBarSnapPositions(InArgs._DraggableBarSnapPositions)
				.TrackNumDiscreteValues(InArgs._TrackNumDiscreteValues)
				.OnTrackRightClickContextMenu( InArgs._OnTrackRightClickContextMenu )
				.ScrubPosition( InArgs._ScrubPosition )
				.OnTrackDragDrop( this, &SAnimSegmentsPanel::OnTrackDragDrop )
			];

		TrackWidgets.Add(AnimSegmentTrack);
	}

	// Generate Nodes and map them to tracks
	for ( int32 SegmentIdx=0; SegmentIdx < AnimTrack->AnimSegments.Num(); SegmentIdx++ )
	{
		TrackWidgets[ SegmentIdx % TrackWidgets.Num() ]->AddTrackNode(
			SNew(STrackNode)
			.ViewInputMax(this->ViewInputMax)
			.ViewInputMin(this->ViewInputMin)
			.NodeColor(InArgs._NodeColor)
			.SelectedNodeColor(SelectedColor)
			.DataLength(this, &SAnimSegmentsPanel::GetSegmentLength, SegmentIdx)
			.DataStartPos(this, &SAnimSegmentsPanel::GetSegmentStartPos, SegmentIdx)
			.NodeName(this, &SAnimSegmentsPanel::GetAnimSegmentName, SegmentIdx)
			.ToolTipText(this, &SAnimSegmentsPanel::GetAnimSegmentDetailedInfo, SegmentIdx)
			.OnTrackNodeDragged( this, &SAnimSegmentsPanel::SetSegmentStartPos, SegmentIdx )
			.OnTrackNodeDropped( this, &SAnimSegmentsPanel::OnSegmentDropped, SegmentIdx)
			.OnNodeRightClickContextMenu( this, &SAnimSegmentsPanel::SummonSegmentNodeContextMenu, SegmentIdx)
			.OnTrackNodeClicked( this, &SAnimSegmentsPanel::OnAnimSegmentNodeClicked, SegmentIdx )
			.NodeSelectionSet(InArgs._NodeSelectionSet)
			);
	}
}

bool SAnimSegmentsPanel::ValidIndex(int32 AnimSegmentIndex) const
{
	return (AnimTrack && AnimTrack->AnimSegments.IsValidIndex(AnimSegmentIndex));
}

float SAnimSegmentsPanel::GetSegmentLength(int32 AnimSegmentIndex) const
{
	if (ValidIndex(AnimSegmentIndex))
	{
		return AnimTrack->AnimSegments[AnimSegmentIndex].GetLength();
	}
	return 0.f;
}

float SAnimSegmentsPanel::GetSegmentStartPos(int32 AnimSegmentIndex) const
{
	if (ValidIndex(AnimSegmentIndex))
	{
		return AnimTrack->AnimSegments[AnimSegmentIndex].StartPos;
	}
	return 0.f;
}

FString	SAnimSegmentsPanel::GetAnimSegmentName(int32 AnimSegmentIndex) const
{
	if (ValidIndex(AnimSegmentIndex))
	{
		UAnimSequenceBase* AnimReference = AnimTrack->AnimSegments[AnimSegmentIndex].AnimReference;
		if(AnimReference)
		{
			return AnimReference->GetName();
		}
	}
	return FString();
}

FText SAnimSegmentsPanel::GetAnimSegmentDetailedInfo(int32 AnimSegmentIndex) const
{
	if (ValidIndex(AnimSegmentIndex))
	{
		FAnimSegment& AnimSegment = AnimTrack->AnimSegments[AnimSegmentIndex];
		UAnimSequenceBase * Anim = AnimSegment.AnimReference;
		if ( Anim != NULL )
		{
			static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
				.SetMinimumFractionalDigits(2)
				.SetMaximumFractionalDigits(2);
			return FText::Format(LOCTEXT("AnimSegmentPanel_GetAnimSegmentDetailedInfoFmt", "{0} {1}"), FText::FromString(Anim->GetName()), FText::AsNumber(AnimSegment.GetLength(), &FormatOptions) );
		}
	}
	return FText::GetEmpty();
}

void SAnimSegmentsPanel::SetSegmentStartPos(float NewStartPos, int32 AnimSegmentIndex)
{
	if (ValidIndex(AnimSegmentIndex))
	{
		if(!bDragging)
		{
			const FScopedTransaction Transaction( LOCTEXT("AnimSegmentPanel_SetSegmentStart", "Edit Segment Start Time") );
			OnPreAnimUpdateDelegate.Execute();
			bDragging = true;
		}

		AnimTrack->AnimSegments[AnimSegmentIndex].StartPos = NewStartPos;
		AnimTrack->CollapseAnimSegments();
	}
}

void SAnimSegmentsPanel::OnSegmentDropped(int32 AnimSegmentIndex)
{
	if(bDragging)
	{
		bDragging = false;
		OnPostAnimUpdateDelegate.Execute();
	}
}

void SAnimSegmentsPanel::SummonSegmentNodeContextMenu(FMenuBuilder& MenuBuilder, int32 AnimSegmentIndex)
{
	FUIAction UIAction;

	MenuBuilder.BeginSection("AnimSegmentsDelete", LOCTEXT("Anim Segment", "Anim Segment") );
	{
		UIAction.ExecuteAction.BindRaw(this, &SAnimSegmentsPanel::RemoveAnimSegment, AnimSegmentIndex);
		MenuBuilder.AddMenuEntry(LOCTEXT("DeleteSegment", "Delete Segment"), LOCTEXT("DeleteSegmentHint", "Delete Segment"), FSlateIcon(), UIAction);
	}
	MenuBuilder.EndSection();
}

void SAnimSegmentsPanel::AddAnimSegment( UAnimSequence *NewSequence, float NewStartPos )
{
	if(AnimTrack != NULL && NewSequence != NULL)
	{
		if(DoesAnimTypeMatchTrack(NewSequence))
		{
			const FScopedTransaction Transaction( LOCTEXT("AnimSegmentPanel_AddSegment", "Add Segment") );
			OnPreAnimUpdateDelegate.Execute();

			FAnimSegment NewSegment;
			NewSegment.AnimReference = NewSequence;
			NewSegment.AnimStartTime = 0.f;
			NewSegment.AnimEndTime = NewSequence->SequenceLength;
			NewSegment.AnimPlayRate = 1.f;
			NewSegment.LoopingCount = 1;
			NewSegment.StartPos = NewStartPos;

			AnimTrack->AnimSegments.Add(NewSegment);
			OnPostAnimUpdateDelegate.Execute();
		}
	}
}

bool SAnimSegmentsPanel::DoesAnimTypeMatchTrack(UAnimSequence* NewSequence)
{
	auto TrackType = AnimTrack->GetTrackAdditiveType();
	return (TrackType == -1) || (TrackType == NewSequence->AdditiveAnimType);
}

void SAnimSegmentsPanel::RemoveAnimSegment(int32 AnimSegmentIndex)
{
	if(ValidIndex(AnimSegmentIndex))
	{
		const FScopedTransaction Transaction( LOCTEXT("AnimSegmentseEditor", "Remove Segment") );
		OnPreAnimUpdateDelegate.Execute();

		AnimTrack->AnimSegments.RemoveAt(AnimSegmentIndex);

		OnAnimSegmentRemovedDelegate.ExecuteIfBound(AnimSegmentIndex);
		OnPostAnimUpdateDelegate.Execute();
	}
}

void SAnimSegmentsPanel::OnTrackDragDrop( TSharedPtr<FDragDropOperation> DragDropOp, float DataPos )
{
	if (DragDropOp.IsValid() && DragDropOp->IsOfType<FAssetDragDropOp>())
	{
		TSharedPtr<FAssetDragDropOp> AssetOp = StaticCastSharedPtr<FAssetDragDropOp>(DragDropOp);
		UAnimSequence *DroppedSequence = FAssetData::GetFirstAsset<UAnimSequence>(AssetOp->AssetData);
		if (DroppedSequence != NULL)
		{
			AddAnimSegment(DroppedSequence, DataPos);
		}
	}
}

void SAnimSegmentsPanel::OnAnimSegmentNodeClicked(int32 SegmentIdx)
{
	OnAnimSegmentNodeClickedDelegate.ExecuteIfBound(SegmentIdx);
}

void SAnimSegmentsPanel::RemoveSelectedAnimSegments()
{
	TArray<int32> SelectedNodeIndices;
	for(int i = 0 ; i < TrackWidgets.Num() ; ++i)
	{
		TSharedPtr<STrack> Track = TrackWidgets[i];
		Track->GetSelectedNodeIndices(SelectedNodeIndices);

		// Reverse order to preserve indices
		for(int32 j = SelectedNodeIndices.Num() - 1 ; j >= 0 ; --j)
		{
			// Segments are placed on one of two tracks with the first segment always residing
			// in track 0 - need to modify the index from track and index to data index.
			int32 ModifiedIndex = i + 2 * SelectedNodeIndices[j];
			RemoveAnimSegment(ModifiedIndex);
		}
	}
}

void SAnimSegmentsPanel::BindCommands()
{
	check(!UICommandList.IsValid());

	UICommandList = MakeShareable(new FUICommandList);
	const FAnimSegmentsPanelCommands& Commands = FAnimSegmentsPanelCommands::Get();

	UICommandList->MapAction(
		Commands.DeleteSegment,
		FExecuteAction::CreateSP(this, &SAnimSegmentsPanel::RemoveSelectedAnimSegments));
}

FReply SAnimSegmentsPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if(UICommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void FAnimSegmentsPanelCommands::RegisterCommands()
{
	UI_COMMAND(DeleteSegment, "Delete", "Deletes the selected segment", EUserInterfaceActionType::Button, FInputChord(EKeys::Platform_Delete));
}

#undef LOCTEXT_NAMESPACE

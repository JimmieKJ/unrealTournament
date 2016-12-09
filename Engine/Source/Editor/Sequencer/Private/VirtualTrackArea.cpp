// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "VirtualTrackArea.h"
#include "SSequencerTreeView.h"
#include "GroupedKeyArea.h"

FVirtualTrackArea::FVirtualTrackArea(const FSequencer& InSequencer, SSequencerTreeView& InTreeView, const FGeometry& InTrackAreaGeometry)
	: FTimeToPixel(InTrackAreaGeometry, InSequencer.GetViewRange())
	, TreeView(InTreeView)
	, TrackAreaGeometry(InTrackAreaGeometry)
{
}

float FVirtualTrackArea::PixelToVerticalOffset(float InPixel) const
{
	return TreeView.PhysicalToVirtual(InPixel);
}

float FVirtualTrackArea::VerticalOffsetToPixel(float InOffset) const
{
	return TreeView.VirtualToPhysical(InOffset);
}

FVector2D FVirtualTrackArea::PhysicalToVirtual(FVector2D InPosition) const
{
	InPosition.Y = PixelToVerticalOffset(InPosition.Y);
	InPosition.X = PixelToTime(InPosition.X);

	return InPosition;
}

FVector2D FVirtualTrackArea::VirtualToPhysical(FVector2D InPosition) const
{
	InPosition.Y = VerticalOffsetToPixel(InPosition.Y);
	InPosition.X = TimeToPixel(InPosition.X);

	return InPosition;
}

FVector2D FVirtualTrackArea::GetPhysicalSize() const
{
	return TrackAreaGeometry.Size;
}

TSharedPtr<FSequencerDisplayNode> FVirtualTrackArea::HitTestNode(float InPhysicalPosition) const
{
	return TreeView.HitTestNode(InPhysicalPosition);
}

TSharedPtr<FSequencerTrackNode> GetParentTrackNode(FSequencerDisplayNode& In)
{
	FSequencerDisplayNode* Current = &In;
	while(Current && Current->GetType() != ESequencerNode::Object)
	{
		if (Current->GetType() == ESequencerNode::Track)
		{
			return StaticCastSharedRef<FSequencerTrackNode>(Current->AsShared());
		}
		Current = Current->GetParent().Get();
	}

	return nullptr;
}

TOptional<FSectionHandle> FVirtualTrackArea::HitTestSection(FVector2D InPhysicalPosition) const
{
	TSharedPtr<FSequencerDisplayNode> Node = HitTestNode(InPhysicalPosition.Y);

	if (Node.IsValid())
	{
		TSharedPtr<FSequencerTrackNode> TrackNode = GetParentTrackNode(*Node);

		if (TrackNode.IsValid())
		{
			float Time = PixelToTime(InPhysicalPosition.X);

			const auto& Sections = TrackNode->GetSections();
			for (int32 Index = 0; Index < Sections.Num(); ++Index)
			{
				UMovieSceneSection* Section = Sections[Index]->GetSectionObject();
				if (Section->IsTimeWithinSection(Time))
				{
					return FSectionHandle(TrackNode.ToSharedRef(), Index);
				}
			}
		}
	}

	return TOptional<FSectionHandle>();
}

FSequencerSelectedKey FVirtualTrackArea::HitTestKey(FVector2D InPhysicalPosition) const
{
	TSharedPtr<FSequencerDisplayNode> Node = HitTestNode(InPhysicalPosition.Y);

	if (!Node.IsValid())
	{
		return FSequencerSelectedKey();
	}

	const float KeyLeft  = PixelToTime(InPhysicalPosition.X - SequencerSectionConstants::KeySize.X/2);
	const float KeyRight = PixelToTime(InPhysicalPosition.X + SequencerSectionConstants::KeySize.X/2);

	TArray<TSharedRef<IKeyArea>> KeyAreas;

	// First check for a key area node on the hit-tested node
	TSharedPtr<FSequencerSectionKeyAreaNode> KeyAreaNode;
	switch (Node->GetType())
	{
		case ESequencerNode::KeyArea: 	KeyAreaNode = StaticCastSharedPtr<FSequencerSectionKeyAreaNode>(Node); break;
		case ESequencerNode::Track:		KeyAreaNode = StaticCastSharedPtr<FSequencerTrackNode>(Node)->GetTopLevelKeyNode(); break;
	}

	if (KeyAreaNode.IsValid())
	{
		for (auto& KeyArea : KeyAreaNode->GetAllKeyAreas())
		{
			UMovieSceneSection* Section = KeyArea->GetOwningSection();
			if (Section->GetStartTime() <= KeyRight && Section->GetEndTime() >= KeyLeft)
			{
				KeyAreas.Add(KeyArea);
			}
		}
	}
	// Failing that, and the node is collapsed, we check for key groupings
	else if (!Node->IsExpanded())
	{
		TSharedPtr<FSequencerTrackNode> TrackNode = GetParentTrackNode(*Node);
		if (TrackNode.IsValid())
		{
			const TArray< TSharedRef<ISequencerSection> >& Sections = TrackNode->GetSections();

			for ( int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex )
			{
				UMovieSceneSection* Section = Sections[SectionIndex]->GetSectionObject();
				if (Section->GetStartTime() <= KeyRight && Section->GetEndTime() >= KeyLeft)
				{
					KeyAreas.Add(Node->GetKeyGrouping(SectionIndex));
				}
			}
		}
	}

	// Search for any key that matches the position
	// todo: investigate if this would be faster as a sort + binary search, rather than linear search
	for (auto& KeyArea : KeyAreas)
	{
		for (FKeyHandle Key : KeyArea->GetUnsortedKeyHandles())
		{
			const float Time = KeyArea->GetKeyTime(Key);
			if (Time >= KeyLeft && Time <= KeyRight)
			{
				return FSequencerSelectedKey(*KeyArea->GetOwningSection(), KeyArea, Key);
			}
		}
	}

	return FSequencerSelectedKey();
}

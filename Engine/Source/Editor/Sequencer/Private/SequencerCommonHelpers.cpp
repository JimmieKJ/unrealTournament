// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SequencerCommonHelpers.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"

void SequencerHelpers::GetAllKeyAreas(TSharedPtr<FSequencerDisplayNode> DisplayNode, TSet<TSharedPtr<IKeyArea>>& KeyAreas)
{
	TArray<TSharedPtr<FSequencerDisplayNode>> NodesToCheck;
	NodesToCheck.Add(DisplayNode);
	while (NodesToCheck.Num() > 0)
	{
		TSharedPtr<FSequencerDisplayNode> NodeToCheck = NodesToCheck[0];
		NodesToCheck.RemoveAt(0);

		if (NodeToCheck->GetType() == ESequencerNode::Track)
		{
			TSharedPtr<FSequencerTrackNode> TrackNode = StaticCastSharedPtr<FSequencerTrackNode>(NodeToCheck);
			TArray<TSharedRef<FSequencerSectionKeyAreaNode>> KeyAreaNodes;
			TrackNode->GetChildKeyAreaNodesRecursively(KeyAreaNodes);
			for (TSharedRef<FSequencerSectionKeyAreaNode> KeyAreaNode : KeyAreaNodes)
			{
				for (TSharedPtr<IKeyArea> KeyArea : KeyAreaNode->GetAllKeyAreas())
				{
					KeyAreas.Add(KeyArea);
				}
			}
		}
		else
		{
			if (NodeToCheck->GetType() == ESequencerNode::KeyArea)
			{
				TSharedPtr<FSequencerSectionKeyAreaNode> KeyAreaNode = StaticCastSharedPtr<FSequencerSectionKeyAreaNode>(NodeToCheck);
				for (TSharedPtr<IKeyArea> KeyArea : KeyAreaNode->GetAllKeyAreas())
				{
					KeyAreas.Add(KeyArea);
				}
			}
			for (TSharedRef<FSequencerDisplayNode> ChildNode : NodeToCheck->GetChildNodes())
			{
				NodesToCheck.Add(ChildNode);
			}
		}
	}
}

void SequencerHelpers::GetDescendantNodes(TSharedRef<FSequencerDisplayNode> DisplayNode, TSet<TSharedRef<FSequencerDisplayNode>>& Nodes)
{
	for (auto ChildNode : DisplayNode.Get().GetChildNodes())
	{
		Nodes.Add(ChildNode);

		GetDescendantNodes(ChildNode, Nodes);
	}
}

void SequencerHelpers::GetAllSections(TSharedRef<FSequencerDisplayNode> DisplayNode, TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections)
{
	TSet<TSharedRef<FSequencerDisplayNode> > AllNodes;
	AllNodes.Add(DisplayNode);
	GetDescendantNodes(DisplayNode, AllNodes);

	for (auto NodeToCheck : AllNodes)
	{
		TSet<TSharedPtr<IKeyArea> > KeyAreas;
		GetAllKeyAreas(NodeToCheck, KeyAreas);
		
		for (auto KeyArea : KeyAreas)
		{
			UMovieSceneSection* OwningSection = KeyArea->GetOwningSection();
			if (OwningSection != nullptr)
			{
				Sections.Add(OwningSection);	
			}
		}

		if (NodeToCheck->GetType() == ESequencerNode::Track)
		{
			TSharedRef<const FSequencerTrackNode> TrackNode = StaticCastSharedRef<const FSequencerTrackNode>( NodeToCheck );
			UMovieSceneTrack* Track = TrackNode->GetTrack();
			if (Track != nullptr)
			{
				for (auto Section : Track->GetAllSections())
				{
					Sections.Add(Section);
				}
			}
		}
	}
}

int32 SequencerHelpers::TimeToFrame(float Time, float FrameRate)
{
	float Frame = Time * FrameRate;
	return FMath::RoundToInt(Frame);
}

float SequencerHelpers::FrameToTime(int32 Frame, float FrameRate)
{
	return Frame / FrameRate;
}


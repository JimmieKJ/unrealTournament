// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "IKeyArea.h"
#include "ISequencerSection.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"
#include "MovieSceneNameableTrack.h"
#include "Sequencer.h"
#include "ISequencerTrackEditor.h"


namespace SequencerNodeConstants
{
	extern const float CommonPadding;
}


/* FTrackNode structors
 *****************************************************************************/

FSequencerTrackNode::FSequencerTrackNode(UMovieSceneTrack& InAssociatedTrack, ISequencerTrackEditor& InAssociatedEditor, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree)
	: FSequencerDisplayNode(InAssociatedTrack.GetTrackName(), InParentNode, InParentTree)
	, AssociatedEditor(InAssociatedEditor)
	, AssociatedTrack(&InAssociatedTrack)
{ }


/* FTrackNode interface
 *****************************************************************************/

void FSequencerTrackNode::SetSectionAsKeyArea(TSharedRef<IKeyArea>& KeyArea)
{
	if( !TopLevelKeyNode.IsValid() )
	{
		bool bTopLevel = true;
		TopLevelKeyNode = MakeShareable( new FSequencerSectionKeyAreaNode( GetNodeName(), FText::GetEmpty(), nullptr, ParentTree, bTopLevel ) );
	}

	TopLevelKeyNode->AddKeyArea( KeyArea );
}


int32 FSequencerTrackNode::GetMaxRowIndex() const
{
	int32 MaxRowIndex = 0;

	for (int32 i = 0; i < Sections.Num(); ++i)
	{
		MaxRowIndex = FMath::Max(MaxRowIndex, Sections[i]->GetSectionObject()->GetRowIndex());
	}

	return MaxRowIndex;
}


void FSequencerTrackNode::FixRowIndices()
{
	if (AssociatedTrack->SupportsMultipleRows())
	{
		// remove any empty track rows by waterfalling down sections to be as compact as possible
		TArray< TArray< TSharedRef<ISequencerSection> > > TrackIndices;
		TrackIndices.AddZeroed(GetMaxRowIndex() + 1);
		for (int32 i = 0; i < Sections.Num(); ++i)
		{
			TrackIndices[Sections[i]->GetSectionObject()->GetRowIndex()].Add(Sections[i]);
		}

		int32 NewIndex = 0;

		for (int32 i = 0; i < TrackIndices.Num(); ++i)
		{
			const TArray< TSharedRef<ISequencerSection> >& SectionsForThisIndex = TrackIndices[i];
			if (SectionsForThisIndex.Num() > 0)
			{
				for (int32 j = 0; j < SectionsForThisIndex.Num(); ++j)
				{
					SectionsForThisIndex[j]->GetSectionObject()->SetRowIndex(NewIndex);
				}

				++NewIndex;
			}
		}
	}
	else
	{
		// non master tracks can only have a single row
		for (int32 i = 0; i < Sections.Num(); ++i)
		{
			Sections[i]->GetSectionObject()->SetRowIndex(0);
		}
	}
}


/* FSequencerDisplayNode interface
 *****************************************************************************/

void FSequencerTrackNode::BuildContextMenu(FMenuBuilder& MenuBuilder)
{
	AssociatedEditor.BuildTrackContextMenu(MenuBuilder, AssociatedTrack.Get());
	FSequencerDisplayNode::BuildContextMenu(MenuBuilder );
}


bool FSequencerTrackNode::CanRenameNode() const
{
	UMovieSceneTrack* Track = AssociatedTrack.Get();
	return (Track != nullptr) && Track->IsA(UMovieSceneNameableTrack::StaticClass());
}


TSharedRef<SWidget> FSequencerTrackNode::GenerateEditWidgetForOutliner()
{
	TSharedPtr<FSequencerSectionKeyAreaNode> KeyAreaNode = GetTopLevelKeyNode();

	if (KeyAreaNode.IsValid())
	{
		// @todo - Sequencer - Support multiple sections/key areas?
		TArray<TSharedRef<IKeyArea>> KeyAreas = KeyAreaNode->GetAllKeyAreas();

		if (KeyAreas.Num() > 0)
		{
			if (KeyAreas[0]->CanCreateKeyEditor())
			{
				return KeyAreas[0]->CreateKeyEditor(&GetSequencer());
			}
		}
	}
	else
	{
		FGuid ObjectBinding;
		TSharedPtr<FSequencerDisplayNode> ParentNode = GetParent();

		if (ParentNode.IsValid() && (ParentNode->GetType() == ESequencerNode::Object))
		{
			ObjectBinding = StaticCastSharedPtr<FSequencerObjectBindingNode>(ParentNode)->GetObjectBinding();
		}

		TSharedPtr<SWidget> EditWidget = AssociatedEditor.BuildOutlinerEditWidget(ObjectBinding, AssociatedTrack.Get());
		if (EditWidget.IsValid())
		{
			return EditWidget.ToSharedRef();
		}
	}

	return FSequencerDisplayNode::GenerateEditWidgetForOutliner();
}


void FSequencerTrackNode::GetChildKeyAreaNodesRecursively(TArray<TSharedRef<FSequencerSectionKeyAreaNode>>& OutNodes) const
{
	FSequencerDisplayNode::GetChildKeyAreaNodesRecursively(OutNodes);

	if (TopLevelKeyNode.IsValid())
	{
		OutNodes.Add(TopLevelKeyNode.ToSharedRef());
	}
}


FText FSequencerTrackNode::GetDisplayName() const
{
	return AssociatedTrack->GetDisplayName();
}


float FSequencerTrackNode::GetNodeHeight() const
{
	return (Sections.Num() > 0)
		? Sections[0]->GetSectionHeight() * (GetMaxRowIndex() + 1)
		: SequencerLayoutConstants::SectionAreaDefaultHeight;
}


FNodePadding FSequencerTrackNode::GetNodePadding() const
{
	return FNodePadding(SequencerNodeConstants::CommonPadding);
}


ESequencerNode::Type FSequencerTrackNode::GetType() const
{
	return ESequencerNode::Track;
}


void FSequencerTrackNode::SetDisplayName(const FText& DisplayName)
{
	auto NameableTrack = Cast<UMovieSceneNameableTrack>(AssociatedTrack.Get());

	if (NameableTrack != nullptr)
	{
		NameableTrack->SetDisplayName(DisplayName);
	}
}

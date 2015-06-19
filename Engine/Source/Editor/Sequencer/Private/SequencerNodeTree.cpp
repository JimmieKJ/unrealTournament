// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "MovieSceneSection.h"
#include "SequencerNodeTree.h"
#include "Sequencer.h"
#include "ScopedTransaction.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieSceneTrackEditor.h"
#include "SectionLayoutBuilder.h"
#include "ISequencerSection.h"

void FSequencerNodeTree::Empty()
{
	RootNodes.Empty();
	ObjectBindingMap.Empty();
	Sequencer.GetSelection()->EmptySelectedOutlinerNodes();
	EditorMap.Empty();
	FilteredNodes.Empty();
}

void FSequencerNodeTree::Update()
{
	// @todo Sequencer - This update pass is too aggressive.  Some nodes may still be valid
	Empty();

	UMovieScene* MovieScene = Sequencer.GetFocusedMovieScene();

	// Get the master tracks  so we can get sections from them
	const TArray<UMovieSceneTrack*>& MasterTracks = MovieScene->GetMasterTracks();

	for( int32 TrackIndex = 0; TrackIndex < MasterTracks.Num(); ++TrackIndex )
	{
		UMovieSceneTrack& Track = *MasterTracks[TrackIndex];

		TSharedRef<FTrackNode> SectionNode = MakeShareable( new FTrackNode( Track.GetTrackName(), Track, NULL, *this ) );
		RootNodes.Add( SectionNode );
	
		MakeSectionInterfaces( Track, SectionNode );

		SectionNode->PinNode();
	}


	const TArray<FMovieSceneObjectBinding>& ObjectBindings = MovieScene->GetObjectBindings();

	// Make nodes for all object bindings
	for( int32 BindingIndex = 0; BindingIndex < ObjectBindings.Num(); ++BindingIndex )
	{
		TSharedRef<FObjectBindingNode> ObjectBindingNode = AddObjectBinding( ObjectBindings[BindingIndex].GetName(), ObjectBindings[BindingIndex].GetObjectGuid() );

		const TArray<UMovieSceneTrack*>& Tracks = ObjectBindings[BindingIndex].GetTracks();

		for( int32 TrackIndex = 0; TrackIndex < Tracks.Num(); ++TrackIndex )
		{
			UMovieSceneTrack& Track = *Tracks[TrackIndex];

			FName SectionName = Track.GetTrackName();
			check( SectionName != NAME_None );

			TSharedRef<FTrackNode> SectionAreaNode = ObjectBindingNode->AddSectionAreaNode( SectionName, Track );
			MakeSectionInterfaces( Track, SectionAreaNode );
		}

	}

	struct FRootNodeSorter
	{
		bool operator()( const TSharedRef<FSequencerDisplayNode>& A, const TSharedRef<FSequencerDisplayNode>& B ) const
		{
			return A->GetType() == ESequencerNode::Object ? false : true;
		}
	};

	// Sort so that master tracks appear before object tracks
	RootNodes.Sort( FRootNodeSorter() );

	// Re-filter the tree after updating 
	// @todo Sequencer - Newly added sections may need to be visible even when there is a filter
	FilterNodes( FilterString );
}

TSharedRef<FMovieSceneTrackEditor> FSequencerNodeTree::FindOrAddTypeEditor( UMovieSceneTrack& InTrack )
{
	TSharedPtr<FMovieSceneTrackEditor> Editor = EditorMap.FindRef( &InTrack );

	if( !Editor.IsValid() )
	{
		const TArray< TSharedPtr<FMovieSceneTrackEditor> >& TrackEditors = Sequencer.GetTrackEditors();

		// Get a tool for each track
		// @todo Sequencer - Should probably only need to get this once and it shouldnt be done here. It depends on when movie scene tool modules are loaded
		TSharedPtr<FMovieSceneTrackEditor> SupportedTool;
		for( int32 EditorIndex = 0; EditorIndex < TrackEditors.Num(); ++EditorIndex )
		{
			if( TrackEditors[EditorIndex]->SupportsType( InTrack.GetClass() ) )
			{
				Editor = TrackEditors[EditorIndex];
				EditorMap.Add( &InTrack, Editor );
				break;
			}
		}
	}

	return Editor.ToSharedRef();
}

void FSequencerNodeTree::MakeSectionInterfaces( UMovieSceneTrack& Track, TSharedRef<FTrackNode>& SectionAreaNode )
{
	const TArray<UMovieSceneSection*>& MovieSceneSections = Track.GetAllSections();

	TSharedRef<FMovieSceneTrackEditor> Editor = FindOrAddTypeEditor( Track );

	for (int32 SectionIndex = 0; SectionIndex < MovieSceneSections.Num(); ++SectionIndex )
	{
		UMovieSceneSection* SectionObject = MovieSceneSections[SectionIndex];
		TSharedRef<ISequencerSection> Section = Editor->MakeSectionInterface( *SectionObject, &Track );

		// Ask the section to generate it's inner layout
		FSectionLayoutBuilder Builder( SectionAreaNode );
		Section->GenerateSectionLayout( Builder );

		SectionAreaNode->AddSection( Section );
	}

	SectionAreaNode->FixRowIndices();
}

const TArray< TSharedRef<FSequencerDisplayNode> >& FSequencerNodeTree::GetRootNodes() const
{
	return RootNodes;
}


TSharedRef<FObjectBindingNode> FSequencerNodeTree::AddObjectBinding( const FString& ObjectName, const FGuid& ObjectBinding )
{
	// The node name is the object guid
	FName ObjectNodeName = *ObjectBinding.ToString();
	TSharedPtr<FSequencerDisplayNode> ParentNode = NULL;

	TSharedRef< FObjectBindingNode > ObjectNode = MakeShareable( new FObjectBindingNode( ObjectNodeName, ObjectName, ObjectBinding, ParentNode, *this ) );

	// Object binding nodes are always root nodes
	RootNodes.Add( ObjectNode );

	// Map the guid to the object binding node for fast lookup later
	ObjectBindingMap.Add( ObjectBinding, ObjectNode );

	return ObjectNode;
}

void FSequencerNodeTree::SaveExpansionState( const FSequencerDisplayNode& Node, bool bExpanded )
{	
	UMovieScene* MovieScene = Sequencer.GetFocusedMovieScene();

	FMovieSceneEditorData& EditorData = MovieScene->GetEditorData();

	if( bExpanded )
	{
		EditorData.CollapsedSequencerNodes.Remove( Node.GetPathName() );
	}
	else
	{
		// Collapsed nodes are stored instead of expanded so that new nodes are expanded by default
		EditorData.CollapsedSequencerNodes.AddUnique( Node.GetPathName() ) ;
	}

}

bool FSequencerNodeTree::GetSavedExpansionState( const FSequencerDisplayNode& Node ) const
{
	UMovieScene* MovieScene = Sequencer.GetFocusedMovieScene();

	FMovieSceneEditorData& EditorData = MovieScene->GetEditorData();

	// Collapsed nodes are stored instead of expanded so that new nodes are expanded by default
	bool bCollapsed = EditorData.CollapsedSequencerNodes.Contains( Node.GetPathName() );

	return !bCollapsed;
}

bool FSequencerNodeTree::IsNodeFiltered( const TSharedRef<const FSequencerDisplayNode> Node ) const
{
	return FilteredNodes.Contains( Node );
}

/**
 * Recursively filters nodes
 *
 * @param StartNode			The node to start from
 * @param FilterStrings		The filter strings which need to be matched
 * @param OutFilteredNodes	The list of all filtered nodes
 */
static bool FilterNodesRecursive( const TSharedRef<FSequencerDisplayNode>& StartNode, const TArray<FString>& FilterStrings, TSet< TSharedRef< const FSequencerDisplayNode> >& OutFilteredNodes )
{
	// Assume the filter is acceptable
	bool bFilterAcceptable = true;

	// Check each string in the filter strings list against 
	for (int32 TestNameIndex = 0; TestNameIndex < FilterStrings.Num(); ++TestNameIndex)
	{
		const FString& TestName = FilterStrings[TestNameIndex];

		if ( !StartNode->GetDisplayName().ToString().Contains( TestName ) ) 
		{
			bFilterAcceptable = false;
			break;
		}
	}

	// Whether or the start node is in the filter
	bool bInFilter = false;

	if( bFilterAcceptable )
	{
		// This node is now filtered
		OutFilteredNodes.Add( StartNode );
		bInFilter = true;
	}

	// Check each child node to determine if it is filtered
	const TArray< TSharedRef<FSequencerDisplayNode> >& ChildNodes = StartNode->GetChildNodes();
	for( int32 ChildIndex = 0; ChildIndex < ChildNodes.Num(); ++ChildIndex )
	{
		// Mark the parent as filtered if any child node was filtered
		bFilterAcceptable |= FilterNodesRecursive( ChildNodes[ChildIndex], FilterStrings, OutFilteredNodes );
		if( bFilterAcceptable && !bInFilter )
		{
			OutFilteredNodes.Add( StartNode );
			bInFilter = true;
		}
	}

	return bFilterAcceptable;
}

void FSequencerNodeTree::FilterNodes( const FString& InFilter )
{
	FilteredNodes.Empty();

	if( InFilter.IsEmpty() )
	{
		// No filter
		FilterString.Empty();
	}
	else
	{
		// Build a list of strings that must be matched
		TArray<FString> FilterStrings;

		FilterString = InFilter;
		// Remove whitespace from the front and back of the string
		FilterString.Trim();
		FilterString.TrimTrailing();
		const bool bCullEmpty = true;
		FilterString.ParseIntoArray( FilterStrings, TEXT(" "), bCullEmpty );

		for( auto It = ObjectBindingMap.CreateIterator(); It; ++It )
		{
			// Recursively filter all nodes, matching them against the list of filter strings.  All filter strings must be matched
			FilterNodesRecursive( It.Value().ToSharedRef(), FilterStrings, FilteredNodes );
		}
	}
}

void FSequencerNodeTree::UpdateCachedVisibilityBasedOnShotFiltersChanged()
{
	for (int32 i = 0; i < RootNodes.Num(); ++i)
	{
		RootNodes[i]->UpdateCachedShotFilteredVisibility();
	}
}

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"
#include "SequencerNodeTree.h"
#include "Sequencer.h"
#include "ScopedTransaction.h"
#include "MovieScene.h"
#include "MovieSceneTrackEditor.h"
#include "SequencerSectionLayoutBuilder.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"


void FSequencerNodeTree::Empty()
{
	RootNodes.Empty();
	ObjectBindingMap.Empty();
	Sequencer.GetSelection().EmptySelectedOutlinerNodes();
	EditorMap.Empty();
	FilteredNodes.Empty();
}


void FSequencerNodeTree::Update()
{
	// @todo Sequencer - This update pass is too aggressive.  Some nodes may still be valid
	Empty();

	UMovieScene* MovieScene = Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene();
	TArray<TSharedRef<FSequencerDisplayNode>> NewRootNodes;

	// Get the master tracks  so we can get sections from them
	const TArray<UMovieSceneTrack*>& MasterTracks = MovieScene->GetMasterTracks();

	for (UMovieSceneTrack* Track : MasterTracks)
	{
		UMovieSceneTrack& TrackRef = *Track;

		TSharedRef<FSequencerTrackNode> SectionNode = MakeShareable(new FSequencerTrackNode(TrackRef, *FindOrAddTypeEditor(TrackRef), nullptr, *this));
		NewRootNodes.Add(SectionNode);
	
		MakeSectionInterfaces(TrackRef, SectionNode);
	}

	const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
	TMap<FGuid, const FMovieSceneBinding*> GuidToBindingMap;

	for (const FMovieSceneBinding& Binding : Bindings)
	{
		GuidToBindingMap.Add(Binding.GetObjectGuid(), &Binding);
	}

	// Make nodes for all object bindings
	TArray<TSharedRef<FSequencerDisplayNode>> NewObjectNodes;
	for( const FMovieSceneBinding& Binding : Bindings )
	{
		TSharedRef<FSequencerObjectBindingNode> ObjectBindingNode = AddObjectBinding( Binding.GetName(), Binding.GetObjectGuid(), GuidToBindingMap, NewObjectNodes );

		const TArray<UMovieSceneTrack*>& Tracks = Binding.GetTracks();

		for( UMovieSceneTrack* Track : Tracks )
		{
			UMovieSceneTrack& TrackRef = *Track;
			TSharedRef<FSequencerTrackNode> SectionAreaNode = ObjectBindingNode->AddSectionAreaNode(TrackRef, *FindOrAddTypeEditor(TrackRef));
			MakeSectionInterfaces( TrackRef, SectionAreaNode );
		}
	}


	struct FObjectNodeSorter
	{
		bool operator()( const TSharedRef<FSequencerDisplayNode>& A, const TSharedRef<FSequencerDisplayNode>& B ) const
		{
			if (A->GetType() == ESequencerNode::Object && B->GetType() != ESequencerNode::Object)
			{
				return true;
			}

			if (A->GetType() != ESequencerNode::Object && B->GetType() == ESequencerNode::Object)
			{
				return false;
			}

			if ( A->GetType() == ESequencerNode::Object && B->GetType() == ESequencerNode::Object )
			{
				return A->GetDisplayName().ToString() < B->GetDisplayName().ToString();
			}

			return 0;
		}
	};


	NewObjectNodes.Sort( FObjectNodeSorter() );
	for (TSharedRef<FSequencerDisplayNode> NewObjectNode : NewObjectNodes)
	{
		NewObjectNode->SortChildNodes(FObjectNodeSorter());
	}

	NewRootNodes.Append(NewObjectNodes);

	// Look for a shot track.  It will always come first if it exists
	UMovieSceneTrack* ShotTrack = MovieScene->GetShotTrack();
	
	if(ShotTrack)
	{
		TSharedRef<FSequencerTrackNode> SectionNode = MakeShareable(new FSequencerTrackNode(*ShotTrack, *FindOrAddTypeEditor(*ShotTrack), nullptr, *this));

		// Shot track always comes first
		RootNodes.Add(SectionNode);
		MakeSectionInterfaces(*ShotTrack, SectionNode);
	}

	// Add all other nodes after the shot track
	RootNodes.Append(NewRootNodes);

	// Set up virtual offsets, and expansion states
	float VerticalOffset = 0.f;

	for (auto& Node : RootNodes)
	{
		Node->Traverse_ParentFirst([&](FSequencerDisplayNode& InNode) {
			float VerticalTop = VerticalOffset;
			VerticalOffset += InNode.GetNodeHeight() + InNode.GetNodePadding().Combined();
			InNode.Initialize(VerticalTop, VerticalOffset);
			return true;
		});

	}

	// Re-filter the tree after updating 
	// @todo sequencer: Newly added sections may need to be visible even when there is a filter
	FilterNodes( FilterString );
}


TSharedRef<ISequencerTrackEditor> FSequencerNodeTree::FindOrAddTypeEditor( UMovieSceneTrack& InTrack )
{
	TSharedPtr<ISequencerTrackEditor> Editor = EditorMap.FindRef( &InTrack );

	if( !Editor.IsValid() )
	{
		const TArray<TSharedPtr<ISequencerTrackEditor>>& TrackEditors = Sequencer.GetTrackEditors();

		// Get a tool for each track
		// @todo sequencer: Should probably only need to get this once and it shouldn't be done here. It depends on when movie scene tool modules are loaded
		TSharedPtr<ISequencerTrackEditor> SupportedTool;

		for (const auto& TrackEditor : TrackEditors)
		{
			if (TrackEditor->SupportsType(InTrack.GetClass()))
			{
				EditorMap.Add(&InTrack, TrackEditor);
				Editor = TrackEditor;

				break;
			}
		}
	}

	return Editor.ToSharedRef();
}


void FSequencerNodeTree::MakeSectionInterfaces( UMovieSceneTrack& Track, TSharedRef<FSequencerTrackNode>& SectionAreaNode )
{
	const TArray<UMovieSceneSection*>& MovieSceneSections = Track.GetAllSections();

	TSharedRef<ISequencerTrackEditor> Editor = FindOrAddTypeEditor( Track );

	for (int32 SectionIndex = 0; SectionIndex < MovieSceneSections.Num(); ++SectionIndex )
	{
		UMovieSceneSection* SectionObject = MovieSceneSections[SectionIndex];
		TSharedRef<ISequencerSection> Section = Editor->MakeSectionInterface( *SectionObject, Track );

		// Ask the section to generate it's inner layout
		FSequencerSectionLayoutBuilder Builder( SectionAreaNode );
		Section->GenerateSectionLayout( Builder );
		SectionAreaNode->AddSection( Section );
	}

	SectionAreaNode->FixRowIndices();
}


const TArray<TSharedRef<FSequencerDisplayNode>>& FSequencerNodeTree::GetRootNodes() const
{
	return RootNodes;
}


TSharedRef<FSequencerObjectBindingNode> FSequencerNodeTree::AddObjectBinding(const FString& ObjectName, const FGuid& ObjectBinding, TMap<FGuid, const FMovieSceneBinding*>& GuidToBindingMap, TArray<TSharedRef<FSequencerDisplayNode>>& OutNodeList)
{
	TSharedPtr<FSequencerObjectBindingNode> ObjectNode;
	TSharedPtr<FSequencerObjectBindingNode>* FoundObjectNode = ObjectBindingMap.Find(ObjectBinding);
	if (FoundObjectNode != nullptr)
	{
		ObjectNode = *FoundObjectNode;
	}
	else
	{
		// The node name is the object guid
		FName ObjectNodeName = *ObjectBinding.ToString();

		// Try to get the parent object node if there is one.
		TSharedPtr<FSequencerObjectBindingNode> ParentNode;

		UMovieSceneSequence* Sequence = Sequencer.GetFocusedMovieSceneSequence();
		TSharedRef<FMovieSceneSequenceInstance> SequenceInstance = Sequencer.GetFocusedMovieSceneSequenceInstance();

		// Prefer to use the parent spawnable if possible, rather than relying on runtime object presence
		FMovieScenePossessable* Possessable = Sequence->GetMovieScene()->FindPossessable(ObjectBinding);
		if (Possessable && Possessable->GetParent().IsValid())
		{
			const FMovieSceneBinding* ParentBinding = GuidToBindingMap.FindRef(Possessable->GetParent());
			if (ParentBinding)
			{
				ParentNode = AddObjectBinding( ParentBinding->GetName(), Possessable->GetParent(), GuidToBindingMap, OutNodeList );
			}
		}
		
		UObject* RuntimeObject = SequenceInstance->FindObject(ObjectBinding, Sequencer);

		// fallback to using the parent runtime object
		if (!ParentNode.IsValid() && RuntimeObject)
		{
			UObject* ParentObject = Sequence->GetParentObject(RuntimeObject);

			if (ParentObject != nullptr)
			{
				FGuid ParentBinding = SequenceInstance->FindObjectId(*ParentObject);
				TSharedPtr<FSequencerObjectBindingNode>* FoundParentNode = ObjectBindingMap.Find( ParentBinding );
				if ( FoundParentNode != nullptr )
				{
					ParentNode = *FoundParentNode;
				}
				else
				{
					const FMovieSceneBinding** FoundParentMovieSceneBinding = GuidToBindingMap.Find( ParentBinding );
					if ( FoundParentMovieSceneBinding != nullptr )
					{
						ParentNode = AddObjectBinding( (*FoundParentMovieSceneBinding)->GetName(), ParentBinding, GuidToBindingMap, OutNodeList );
					}
				}
			}
		}

		// get human readable name of the object
		AActor* RuntimeActor = Cast<AActor>(RuntimeObject);
		const FString& DisplayString = (RuntimeActor != nullptr)
			? RuntimeActor->GetActorLabel()
			: ObjectName;

		// Create the node.
		ObjectNode = MakeShareable(new FSequencerObjectBindingNode(ObjectNodeName, FText::FromString(DisplayString), ObjectBinding, ParentNode, *this));

		if (ParentNode.IsValid())
		{
			ParentNode->AddObjectBindingNode(ObjectNode.ToSharedRef());
		}
		else
		{
			OutNodeList.Add( ObjectNode.ToSharedRef() );
		}

		// Map the guid to the object binding node for fast lookup later
		ObjectBindingMap.Add( ObjectBinding, ObjectNode );
	}

	return ObjectNode.ToSharedRef();
}


void FSequencerNodeTree::SaveExpansionState(const FSequencerDisplayNode& Node, bool bExpanded)
{	
	// @todo Sequencer - This should be moved to the sequence level
	UMovieScene* MovieScene = Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene();
	FMovieSceneEditorData& EditorData = MovieScene->GetEditorData();

	EditorData.ExpansionStates.Add(Node.GetPathName(), FMovieSceneExpansionState(bExpanded));
}


bool FSequencerNodeTree::GetSavedExpansionState(const FSequencerDisplayNode& Node) const
{
	// @todo Sequencer - This should be moved to the sequence level
	UMovieScene* MovieScene = Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene();
	FMovieSceneEditorData& EditorData = MovieScene->GetEditorData();
	FMovieSceneExpansionState* ExpansionState = EditorData.ExpansionStates.Find( Node.GetPathName() );

	return ExpansionState ? ExpansionState->bExpanded : GetDefaultExpansionState(Node);
}


bool FSequencerNodeTree::GetDefaultExpansionState( const FSequencerDisplayNode& Node ) const
{
	// For now, only object nodes are expanded by default
	return Node.GetType() == ESequencerNode::Object;
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
static bool FilterNodesRecursive( FSequencer& Sequencer, const TSharedRef<FSequencerDisplayNode>& StartNode, const TArray<FString>& FilterStrings, TSet<TSharedRef<const FSequencerDisplayNode>>& OutFilteredNodes )
{
	// assume the filter is acceptable
	bool bFilterAcceptable = true;

	// check each string in the filter strings list against 
	UMovieScene* MovieScene = Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene();

	for (const FString& String : FilterStrings)
	{
		if (String.StartsWith(TEXT("label:")))
		{
			if (StartNode->GetType() == ESequencerNode::Object)
			{
				auto ObjectBindingNode = StaticCastSharedRef<FSequencerObjectBindingNode>(StartNode);
				auto Labels = MovieScene->GetObjectLabels(ObjectBindingNode->GetObjectBinding());

				if (!Labels.Strings.Contains(String.RightChop(6)))
				{
					bFilterAcceptable = false;
				}
			}
			else
			{
				bFilterAcceptable = false;
			}
		}
		else if (!StartNode->GetDisplayName().ToString().Contains(String)) 
		{
			bFilterAcceptable = false;
		}

		if (!bFilterAcceptable)
		{
			break;
		}
	}

	// whether or the start node is in the filter
	bool bInFilter = false;

	if (bFilterAcceptable)
	{
		// This node is now filtered
		OutFilteredNodes.Add(StartNode);
		bInFilter = true;
	}

	// check each child node to determine if it is filtered
	const TArray<TSharedRef<FSequencerDisplayNode>>& ChildNodes = StartNode->GetChildNodes();

	for (const auto& Node : ChildNodes)
	{
		// Mark the parent as filtered if any child node was filtered
		bFilterAcceptable |= FilterNodesRecursive(Sequencer, Node, FilterStrings, OutFilteredNodes);

		if (bFilterAcceptable && !bInFilter)
		{
			OutFilteredNodes.Add(StartNode);
			bInFilter = true;
		}
	}

	return bFilterAcceptable;
}


void FSequencerNodeTree::FilterNodes(const FString& InFilter)
{
	FilteredNodes.Empty();

	if (InFilter.IsEmpty())
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
		FilterString.ParseIntoArray(FilterStrings, TEXT(" "), true /*bCullEmpty*/);

		for (auto It = ObjectBindingMap.CreateIterator(); It; ++It)
		{
			// Recursively filter all nodes, matching them against the list of filter strings.  All filter strings must be matched
			FilterNodesRecursive(Sequencer, It.Value().ToSharedRef(), FilterStrings, FilteredNodes);
		}
	}
}

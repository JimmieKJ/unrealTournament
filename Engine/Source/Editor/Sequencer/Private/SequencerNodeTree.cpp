// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"
#include "MovieSceneCinematicShotTrack.h"
#include "SequencerNodeTree.h"
#include "Sequencer.h"
#include "ScopedTransaction.h"
#include "MovieScene.h"
#include "MovieSceneFolder.h"
#include "MovieSceneTrackEditor.h"
#include "SequencerSectionLayoutBuilder.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "SequencerSpacerNode.h"

void FSequencerNodeTree::Empty()
{
	RootNodes.Empty();
	ObjectBindingMap.Empty();
	Sequencer.GetSelection().EmptySelectedOutlinerNodes();
	EditorMap.Empty();
	FilteredNodes.Empty();
}


int32 NodeTypeToFolderSortId(ESequencerNode::Type NodeType)
{
	switch ( NodeType )
	{
	case ESequencerNode::Folder:
		return 0;
	case ESequencerNode::Track:
		return 1;
	case ESequencerNode::Object:
		return 2;
	default:
		return 3;
	}
}


int32 NodeTypeToObjectSortId( ESequencerNode::Type NodeType )
{
	switch ( NodeType )
	{
	case ESequencerNode::Object:
		return 0;
	case ESequencerNode::Track:
		return 1;
	default:
		return 2;
	}
}


struct FDisplayNodeSorter
{
	bool operator()( const TSharedRef<FSequencerDisplayNode>& A, const TSharedRef<FSequencerDisplayNode>& B ) const
	{
		TSharedPtr<FSequencerDisplayNode> ParentNode = A->GetParent();
		
		// If the nodes are root nodes, or in folders and they are the same type, sort by name.
		if ( (ParentNode.IsValid() == false || ParentNode->GetType() == ESequencerNode::Folder) && A->GetType() == B->GetType() )
		{
			return A->GetDisplayName().ToString() < B->GetDisplayName().ToString();
		}

		int32 SortIdA;
		int32 SortIdB;

		// Otherwise if they are root nodes or in folders use the folder sort id.
		if ( ParentNode.IsValid() == false || ParentNode->GetType() == ESequencerNode::Folder )
		{
			SortIdA = NodeTypeToFolderSortId( A->GetType() );
			SortIdB = NodeTypeToFolderSortId( B->GetType() );
		}
		// Otherwise if they are in an object node use the object node sort id.
		else if ( ParentNode->GetType() == ESequencerNode::Object )
		{
			SortIdA = NodeTypeToObjectSortId( A->GetType() );
			SortIdB = NodeTypeToObjectSortId( B->GetType() );
		}
		// Otherwise they are equal, and in a stable sort shouldn't change position.
		else
		{
			SortIdA = 0;
			SortIdB = 0;
		}

		return SortIdA < SortIdB;
	}
};


void FSequencerNodeTree::Update()
{
	HoveredNode = nullptr;

	// @todo Sequencer - This update pass is too aggressive.  Some nodes may still be valid
	Empty();

	UMovieScene* MovieScene = Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene();
	UMovieSceneCinematicShotTrack* CinematicShotTrack = MovieScene->FindMasterTrack<UMovieSceneCinematicShotTrack>();

	// Get the master tracks  so we can get sections from them
	const TArray<UMovieSceneTrack*>& MasterTracks = MovieScene->GetMasterTracks();
	TArray<TSharedRef<FSequencerTrackNode>> MasterTrackNodes;

	for (UMovieSceneTrack* Track : MasterTracks)
	{
		if (Track != CinematicShotTrack)
		{
			UMovieSceneTrack& TrackRef = *Track;

			TSharedRef<FSequencerTrackNode> SectionNode = MakeShareable(new FSequencerTrackNode(TrackRef, *FindOrAddTypeEditor(TrackRef), true, nullptr, *this));
			MasterTrackNodes.Add(SectionNode);
	
			MakeSectionInterfaces(TrackRef, SectionNode);
		}
	}

	const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
	TMap<FGuid, const FMovieSceneBinding*> GuidToBindingMap;

	for (const FMovieSceneBinding& Binding : Bindings)
	{
		GuidToBindingMap.Add(Binding.GetObjectGuid(), &Binding);
	}

	// Make nodes for all object bindings
	TArray<TSharedRef<FSequencerObjectBindingNode>> ObjectNodes;
	for( const FMovieSceneBinding& Binding : Bindings )
	{
		TSharedRef<FSequencerObjectBindingNode> ObjectBindingNode = AddObjectBinding( Binding.GetName(), Binding.GetObjectGuid(), GuidToBindingMap, ObjectNodes );

		const TArray<UMovieSceneTrack*>& Tracks = Binding.GetTracks();

		for( UMovieSceneTrack* Track : Tracks )
		{
			UMovieSceneTrack& TrackRef = *Track;
			TSharedRef<FSequencerTrackNode> SectionAreaNode = ObjectBindingNode->AddSectionAreaNode(TrackRef, *FindOrAddTypeEditor(TrackRef));
			MakeSectionInterfaces( TrackRef, SectionAreaNode );
		}
	}


	// Cinematic shot track always comes first
	if (CinematicShotTrack)
	{
		TSharedRef<FSequencerTrackNode> SectionNode = MakeShareable(new FSequencerTrackNode(*CinematicShotTrack, *FindOrAddTypeEditor(*CinematicShotTrack), false, nullptr, *this));

		RootNodes.Add(SectionNode);
		MakeSectionInterfaces(*CinematicShotTrack, SectionNode);
	}

	// Then comes the camera cut track
	UMovieSceneTrack* CameraCutTrack = MovieScene->GetCameraCutTrack();
	
	if (CameraCutTrack)
	{
		TSharedRef<FSequencerTrackNode> SectionNode = MakeShareable(new FSequencerTrackNode(*CameraCutTrack, *FindOrAddTypeEditor(*CameraCutTrack), false, nullptr, *this));

		RootNodes.Add(SectionNode);
		MakeSectionInterfaces(*CameraCutTrack, SectionNode);
	}

	// Add all other nodes after the camera cut track
	TArray<TSharedRef<FSequencerDisplayNode>> FolderMasterTrackAndObjectNodes;
	CreateAndPopulateFolderNodes( MasterTrackNodes, ObjectNodes, MovieScene->GetRootFolders(), FolderMasterTrackAndObjectNodes );
	
	// Sort the created nodes.
	FolderMasterTrackAndObjectNodes.Sort(FDisplayNodeSorter());
	for ( TSharedRef<FSequencerDisplayNode> Node : FolderMasterTrackAndObjectNodes )
	{
		Node->SortChildNodes(FDisplayNodeSorter());
	}

	RootNodes.Append( FolderMasterTrackAndObjectNodes );

	RootNodes.Reserve(RootNodes.Num()*2);
	for (int32 Index = 0; Index < RootNodes.Num(); Index += 2)
	{
		RootNodes.Insert(MakeShareable(new FSequencerSpacerNode(1.f, nullptr, *this)), Index);
	}
	RootNodes.Add(MakeShareable(new FSequencerSpacerNode(1.f, nullptr, *this)));

	// Set up virtual offsets, expansion states, and tints
	float VerticalOffset = 0.f;

	for (TSharedRef<FSequencerDisplayNode>& Node : RootNodes)
	{
		Node->Traverse_ParentFirst([&](FSequencerDisplayNode& InNode) {

			// Set up the virtual node position
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


TSharedRef<FSequencerObjectBindingNode> FSequencerNodeTree::AddObjectBinding(const FString& ObjectName, const FGuid& ObjectBinding, TMap<FGuid, const FMovieSceneBinding*>& GuidToBindingMap, TArray<TSharedRef<FSequencerObjectBindingNode>>& OutNodeList)
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


TSharedRef<FSequencerDisplayNode> CreateFolderNode(
	UMovieSceneFolder& MovieSceneFolder, FSequencerNodeTree& NodeTree, 
	TMap<UMovieSceneTrack*, TSharedRef<FSequencerTrackNode>>& MasterTrackToDisplayNodeMap,
	TMap<FGuid, TSharedRef<FSequencerObjectBindingNode>>& ObjectGuidToDisplayNodeMap )
{
	TSharedRef<FSequencerFolderNode> FolderNode( new FSequencerFolderNode( MovieSceneFolder, TSharedPtr<FSequencerDisplayNode>(), NodeTree ) );

	for ( UMovieSceneFolder* ChildFolder : MovieSceneFolder.GetChildFolders() )
	{
		FolderNode->AddChildNode( CreateFolderNode( *ChildFolder, NodeTree, MasterTrackToDisplayNodeMap, ObjectGuidToDisplayNodeMap ) );
	}

	for ( UMovieSceneTrack* MasterTrack : MovieSceneFolder.GetChildMasterTracks() )
	{
		TSharedRef<FSequencerTrackNode>* TrackNodePtr = MasterTrackToDisplayNodeMap.Find( MasterTrack );
		if ( TrackNodePtr != nullptr)
		{
			// TODO: Log this.
			FolderNode->AddChildNode( *TrackNodePtr );
			MasterTrackToDisplayNodeMap.Remove( MasterTrack );
		}
	}

	for (const FGuid& ObjectGuid : MovieSceneFolder.GetChildObjectBindings() )
	{
		TSharedRef<FSequencerObjectBindingNode>* ObjectNodePtr = ObjectGuidToDisplayNodeMap.Find( ObjectGuid );
		if ( ObjectNodePtr != nullptr )
		{
			// TODO: Log this.
			FolderNode->AddChildNode( *ObjectNodePtr );
			ObjectGuidToDisplayNodeMap.Remove( ObjectGuid );
		}
	}

	return FolderNode;
}


void FSequencerNodeTree::CreateAndPopulateFolderNodes( 
	TArray<TSharedRef<FSequencerTrackNode>>& MasterTrackNodes, TArray<TSharedRef<FSequencerObjectBindingNode>>& ObjectNodes,
	TArray<UMovieSceneFolder*>& MovieSceneFolders, TArray<TSharedRef<FSequencerDisplayNode>>& GroupedNodes )
{
	TMap<UMovieSceneTrack*, TSharedRef<FSequencerTrackNode>> MasterTrackToDisplayNodeMap;
	for ( TSharedRef<FSequencerTrackNode> MasterTrackNode : MasterTrackNodes )
	{
		MasterTrackToDisplayNodeMap.Add( MasterTrackNode->GetTrack(), MasterTrackNode );
	}

	TMap<FGuid, TSharedRef<FSequencerObjectBindingNode>> ObjectGuidToDisplayNodeMap;
	for ( TSharedRef<FSequencerObjectBindingNode> ObjectBindingNode : ObjectNodes )
	{
		ObjectGuidToDisplayNodeMap.Add( ObjectBindingNode->GetObjectBinding(), ObjectBindingNode );
	}

	for ( UMovieSceneFolder* MovieSceneFolder : MovieSceneFolders )
	{
		GroupedNodes.Add( CreateFolderNode( *MovieSceneFolder, *this, MasterTrackToDisplayNodeMap, ObjectGuidToDisplayNodeMap ) );	
	}

	TArray<TSharedRef<FSequencerTrackNode>> NonFolderTrackNodes;
	MasterTrackToDisplayNodeMap.GenerateValueArray( NonFolderTrackNodes );
	for ( TSharedRef<FSequencerTrackNode> NonFolderTrackNode : NonFolderTrackNodes )
	{
		GroupedNodes.Add( NonFolderTrackNode );
	}

	TArray<TSharedRef<FSequencerObjectBindingNode>> NonFolderObjectNodes;
	ObjectGuidToDisplayNodeMap.GenerateValueArray( NonFolderObjectNodes );
	for ( TSharedRef<FSequencerObjectBindingNode> NonFolderObjectNode : NonFolderObjectNodes )
	{
		GroupedNodes.Add( NonFolderObjectNode );
	}
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

void FSequencerNodeTree::SetHoveredNode(const TSharedPtr<FSequencerDisplayNode>& InHoveredNode)
{
	if (InHoveredNode != HoveredNode)
	{
		HoveredNode = InHoveredNode;
	}
}

const TSharedPtr<FSequencerDisplayNode>& FSequencerNodeTree::GetHoveredNode() const
{
	return HoveredNode;
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
	bool PassedTextFilter = true;

	// check each string in the filter strings list against 
	for (const FString& String : FilterStrings)
	{
		if (String.StartsWith(TEXT("label:")))
		{
			if (!StartNode->GetParent().IsValid() && (String.Len() > 6))
			{
				if (StartNode->GetType() == ESequencerNode::Object)
				{
					auto ObjectBindingNode = StaticCastSharedRef<FSequencerObjectBindingNode>(StartNode);
					auto Labels = Sequencer.GetLabelManager().GetObjectLabels(ObjectBindingNode->GetObjectBinding());

					if ((Labels == nullptr) || !Labels->Strings.Contains(String.RightChop(6)))
					{
						return false;
					}
				}
				else
				{
					return false;
				}
			}
		}
		else if (!StartNode->GetDisplayName().ToString().Contains(String)) 
		{
			PassedTextFilter = false;
		}

		if (!PassedTextFilter)
		{
			break;
		}
	}

	// whether or the start node is in the filter
	bool bInFilter = false;

	if (PassedTextFilter)
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
		PassedTextFilter |= FilterNodesRecursive(Sequencer, Node, FilterStrings, OutFilteredNodes);

		if (PassedTextFilter && !bInFilter)
		{
			OutFilteredNodes.Add(StartNode);
			bInFilter = true;
		}
	}

	return PassedTextFilter;
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

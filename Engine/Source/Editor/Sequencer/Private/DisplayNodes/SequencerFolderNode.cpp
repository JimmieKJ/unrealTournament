// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "MovieSceneFolder.h"
#include "SequencerUtilities.h"
#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "SequencerDisplayNodeDragDropOp.h"
#include "ScopedTransaction.h"

FSequencerFolderNode::FSequencerFolderNode( UMovieSceneFolder& InMovieSceneFolder, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree )
	: FSequencerDisplayNode( InMovieSceneFolder.GetFolderName(), InParentNode, InParentTree )
	, MovieSceneFolder(InMovieSceneFolder)
{
	FolderOpenBrush = FEditorStyle::GetBrush( "ContentBrowser.AssetTreeFolderOpen" );
	FolderClosedBrush = FEditorStyle::GetBrush( "ContentBrowser.AssetTreeFolderClosed" );
}


ESequencerNode::Type FSequencerFolderNode::GetType() const
{
	return ESequencerNode::Folder;
}


float FSequencerFolderNode::GetNodeHeight() const
{
	// TODO: Add another constant.
	return SequencerLayoutConstants::FolderNodeHeight;
}


FNodePadding FSequencerFolderNode::GetNodePadding() const
{
	return FNodePadding(4, 4);
}


bool FSequencerFolderNode::CanRenameNode() const
{
	return true;
}


FText FSequencerFolderNode::GetDisplayName() const
{
	return FText::FromName( MovieSceneFolder.GetFolderName() );
}


void FSequencerFolderNode::SetDisplayName( const FText& NewDisplayName )
{
	if ( MovieSceneFolder.GetFolderName() != FName( *NewDisplayName.ToString() ) )
	{
		TArray<FName> SiblingNames;
		TSharedPtr<FSequencerDisplayNode> ParentNode = GetParent();
		if ( ParentNode.IsValid() )
		{
			for ( TSharedRef<FSequencerDisplayNode> SiblingNode : ParentNode->GetChildNodes() )
			{
				if ( SiblingNode != AsShared() )
				{
					SiblingNames.Add( SiblingNode->GetNodeName() );
				}
			}
		}

		FName UniqueName = FSequencerUtilities::GetUniqueName( FName( *NewDisplayName.ToString() ), SiblingNames );

		const FScopedTransaction Transaction( NSLOCTEXT( "SequencerFolderNode", "RenameFolder", "Rename folder." ) );
		MovieSceneFolder.Modify();
		MovieSceneFolder.SetFolderName( UniqueName );
	}
}


const FSlateBrush* FSequencerFolderNode::GetIconBrush() const
{
	return IsExpanded()
		? FolderOpenBrush
		: FolderClosedBrush;
}


bool FSequencerFolderNode::CanDrag() const
{
	return true;
}


TOptional<EItemDropZone> FSequencerFolderNode::CanDrop( FSequencerDisplayNodeDragDropOp& DragDropOp, EItemDropZone ItemDropZone ) const
{
	DragDropOp.ResetToDefaultToolTip();

	if ( ItemDropZone == EItemDropZone::AboveItem )
	{
		if ( GetParent().IsValid() )
		{
			// When dropping above, only allow it for root level nodes.
			return TOptional<EItemDropZone>();
		}
		else
		{
			// Make sure there are no folder name collisions with the root folders
			UMovieScene* FocusedMovieScene = GetParentTree().GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();
			TSet<FName> RootFolderNames;
			for ( UMovieSceneFolder* RootFolder : FocusedMovieScene->GetRootFolders() )
			{
				RootFolderNames.Add( RootFolder->GetFolderName() );
			}

			for ( TSharedRef<FSequencerDisplayNode> DraggedNode : DragDropOp.GetDraggedNodes() )
			{
				if ( DraggedNode->GetType() == ESequencerNode::Folder )
				{
					TSharedRef<FSequencerFolderNode> DraggedFolder = StaticCastSharedRef<FSequencerFolderNode>( DraggedNode );
					if ( RootFolderNames.Contains( DraggedFolder->GetFolder().GetFolderName() ) )
					{
						DragDropOp.CurrentHoverText = FText::Format(
							NSLOCTEXT( "SeqeuencerFolderNode", "DuplicateRootFolderDragErrorFormat", "Root folder with name '{0}' already exists." ),
							FText::FromName( DraggedFolder->GetFolder().GetFolderName() ) );
						return TOptional<EItemDropZone>();
					}
				}
			}
		}
		return TOptional<EItemDropZone>( EItemDropZone::AboveItem );
	}
	else
	{
		// When dropping onto, don't allow dropping into the same folder, don't allow dropping
		// parents into children, and don't allow duplicate folder names.
		TSet<FName> ChildFolderNames;
		for ( UMovieSceneFolder* ChildFolder : GetFolder().GetChildFolders() )
		{
			ChildFolderNames.Add( ChildFolder->GetFolderName() );
		}

		for ( TSharedRef<FSequencerDisplayNode> DraggedNode : DragDropOp.GetDraggedNodes() )
		{
			TSharedPtr<FSequencerDisplayNode> ParentNode = DraggedNode->GetParent();
			if ( ParentNode.IsValid() && ParentNode.Get() == this )
			{
				DragDropOp.CurrentHoverText = NSLOCTEXT( "SeqeuencerFolderNode", "SameParentDragError", "Can't drag a node onto the same parent." );
				return TOptional<EItemDropZone>();
			}

			if ( DraggedNode->GetType() == ESequencerNode::Folder )
			{
				TSharedRef<FSequencerFolderNode> DraggedFolder = StaticCastSharedRef<FSequencerFolderNode>( DraggedNode );
				if ( ChildFolderNames.Contains( DraggedFolder->GetFolder().GetFolderName() ) )
				{
					DragDropOp.CurrentHoverText = FText::Format(
						NSLOCTEXT( "SeqeuencerFolderNode", "DuplicateChildFolderDragErrorFormat", "Folder with name '{0}' already exists." ),
						FText::FromName( DraggedFolder->GetFolder().GetFolderName() ) );
					return TOptional<EItemDropZone>();
				}
			}
		}
		TSharedPtr<FSequencerDisplayNode> CurrentNode = SharedThis( ( FSequencerDisplayNode* )this );
		while ( CurrentNode.IsValid() )
		{
			if ( DragDropOp.GetDraggedNodes().Contains( CurrentNode ) )
			{
				DragDropOp.CurrentHoverText = NSLOCTEXT( "SeqeuencerFolderNode", "ParentIntoChildDragError", "Can't drag a parent node into one of it's children." );
				return TOptional<EItemDropZone>();
			}
			CurrentNode = CurrentNode->GetParent();
		}
		return TOptional<EItemDropZone>( EItemDropZone::OntoItem );
	}
}


void FSequencerFolderNode::Drop( const TArray<TSharedRef<FSequencerDisplayNode>>& DraggedNodes, EItemDropZone ItemDropZone )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "SequencerFolderNode", "MoveIntoFolder", "Move items into folder." ) );
	GetFolder().SetFlags(RF_Transactional);
	GetFolder().Modify();
	for ( TSharedRef<FSequencerDisplayNode> DraggedNode : DraggedNodes )
	{
		TSharedPtr<FSequencerDisplayNode> ParentNode = DraggedNode->GetParent();
		switch ( DraggedNode->GetType() )
		{
			case ESequencerNode::Folder:
			{
				TSharedRef<FSequencerFolderNode> DraggedFolderNode = StaticCastSharedRef<FSequencerFolderNode>( DraggedNode );
				UMovieScene* FocusedMovieScene = GetParentTree().GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();

				if ( ItemDropZone == EItemDropZone::OntoItem )
				{
					GetFolder().AddChildFolder( &DraggedFolderNode->GetFolder() );
				}
				else
				{
					FocusedMovieScene->Modify();
					FocusedMovieScene->GetRootFolders().Add( &DraggedFolderNode->GetFolder() );
				}
				
				if ( ParentNode.IsValid() )
				{
					checkf( ParentNode->GetType() == ESequencerNode::Folder, TEXT( "Can not remove from unsupported parent node." ) );
					TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>( ParentNode );
					ParentFolder->GetFolder().Modify();
					ParentFolder->GetFolder().RemoveChildFolder( &DraggedFolderNode->GetFolder() );
				}
				else
				{
					FocusedMovieScene->Modify();
					FocusedMovieScene->GetRootFolders().Remove( &DraggedFolderNode->GetFolder() );
				}

				break;
			}
			case ESequencerNode::Track:
			{
				TSharedRef<FSequencerTrackNode> DraggedTrackNode = StaticCastSharedRef<FSequencerTrackNode>( DraggedNode );
				
				if( ItemDropZone == EItemDropZone::OntoItem )
				{
					GetFolder().AddChildMasterTrack( DraggedTrackNode->GetTrack() );
				}
				
				if ( ParentNode.IsValid() )
				{
					checkf( ParentNode->GetType() == ESequencerNode::Folder, TEXT( "Can not remove from unsupported parent node." ) );
					TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>( ParentNode );
					ParentFolder->GetFolder().Modify();
					ParentFolder->GetFolder().RemoveChildMasterTrack( DraggedTrackNode->GetTrack() );
				}

				break;
			}
			case ESequencerNode::Object:
			{
				TSharedRef<FSequencerObjectBindingNode> DraggedObjectBindingNode = StaticCastSharedRef<FSequencerObjectBindingNode>( DraggedNode );
				
				if ( ItemDropZone == EItemDropZone::OntoItem )
				{
					GetFolder().AddChildObjectBinding( DraggedObjectBindingNode->GetObjectBinding() );
				}

				if ( ParentNode.IsValid() )
				{
					checkf( ParentNode->GetType() == ESequencerNode::Folder, TEXT( "Can not remove from unsupported parent node." ) );
					TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>( ParentNode );
					ParentFolder->GetFolder().Modify();
					ParentFolder->GetFolder().RemoveChildObjectBinding( DraggedObjectBindingNode->GetObjectBinding() );
				}

				break;
			}
		}
	}
	SetExpansionState( true );
	ParentTree.GetSequencer().NotifyMovieSceneDataChanged();
}


void FSequencerFolderNode::AddChildNode( TSharedRef<FSequencerDisplayNode> ChildNode )
{
	AddChildAndSetParent( ChildNode );
}


UMovieSceneFolder& FSequencerFolderNode::GetFolder() const
{
	return MovieSceneFolder;
}


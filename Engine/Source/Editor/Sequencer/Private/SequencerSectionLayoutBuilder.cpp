// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequencerSectionLayoutBuilder.h"
#include "DisplayNodes/SequencerSectionCategoryNode.h"


FSequencerSectionLayoutBuilder::FSequencerSectionLayoutBuilder( TSharedRef<FSequencerTrackNode> InRootNode )
	: RootNode( InRootNode )
	, CurrentNode( InRootNode )
{}

void FSequencerSectionLayoutBuilder::PushCategory( FName CategoryName, const FText& DisplayLabel )
{
	CurrentNode = CurrentNode->AddCategoryNode( CategoryName, DisplayLabel );
}

void FSequencerSectionLayoutBuilder::PopCategory()
{
	// Pop a category if the current node is a category
	if( CurrentNode->GetParent().IsValid() && CurrentNode->GetType() == ESequencerNode::Category )
	{
		CurrentNode = CurrentNode->GetParent().ToSharedRef();
	}
}

void FSequencerSectionLayoutBuilder::SetSectionAsKeyArea( TSharedRef<IKeyArea> KeyArea )
{
	KeyArea->SetName(RootNode->GetNodeName());
	RootNode->SetSectionAsKeyArea( KeyArea ); 
}

void FSequencerSectionLayoutBuilder::AddKeyArea( FName KeyAreaName, const FText& DisplayName, TSharedRef<IKeyArea> KeyArea )
{
	KeyArea->SetName(KeyAreaName);
	CurrentNode->AddKeyAreaNode( KeyAreaName, DisplayName, KeyArea );
}


// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SectionLayoutBuilder.h"


FSectionLayoutBuilder::FSectionLayoutBuilder( TSharedRef<FTrackNode> InRootNode )
	: RootNode( InRootNode )
	, CurrentNode( InRootNode )
{}

void FSectionLayoutBuilder::PushCategory( FName CategoryName, const FText& DisplayLabel )
{
	CurrentNode = CurrentNode->AddCategoryNode( CategoryName, DisplayLabel );
}

void FSectionLayoutBuilder::PopCategory()
{
	// Pop a category if the current node is a category
	if( CurrentNode->GetParent().IsValid() && CurrentNode->GetType() == ESequencerNode::Category )
	{
		CurrentNode = CurrentNode->GetParent().ToSharedRef();
	}
}

void FSectionLayoutBuilder::SetSectionAsKeyArea( TSharedRef<IKeyArea> KeyArea )
{
	RootNode->SetSectionAsKeyArea( KeyArea ); 
}

void FSectionLayoutBuilder::AddKeyArea( FName KeyAreaName, const FText& DisplayName, TSharedRef<IKeyArea> KeyArea )
{
	CurrentNode->AddKeyAreaNode( KeyAreaName, DisplayName, KeyArea );
}


// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "KismetNodes/SGraphNodeK2Default.h"
#include "K2Node.h"


void SGraphNodeK2Default::Construct( const FArguments& InArgs, UK2Node* InNode )
{
	this->GraphNode = InNode;

	this->SetCursor( EMouseCursor::CardinalCross );

	this->UpdateGraphNode();
}

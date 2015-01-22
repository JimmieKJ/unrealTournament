// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#ifndef __SGraphNodeDefault_h__
#define __SGraphNodeDefault_h__

class SGraphNodeDefault : public SGraphNode
{
public:

	SLATE_BEGIN_ARGS( SGraphNodeDefault )
		: _GraphNodeObj( static_cast<UEdGraphNode*>(NULL) )
		{}

		SLATE_ARGUMENT( UEdGraphNode*, GraphNodeObj )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );
};

#endif // __SGraphNodeDefault_h__
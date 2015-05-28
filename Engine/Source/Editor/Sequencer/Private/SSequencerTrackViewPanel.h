// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Base panel for sequencer track views
 */
template<typename WidgetType>
class TSequencerTrackViewPanel : public SPanel
{
public:
	SLATE_BEGIN_ARGS( TSequencerTrackViewPanel<WidgetType> ) {}
	SLATE_END_ARGS()

	TSequencerTrackViewPanel()
	: Children()
	{
	}

	/** Construct this widget.  Called by the SNew() Slate macro. */
	void Construct( const FArguments& InArgs, TSharedRef<FSequencerDisplayNode> InRootNode )
	{
		RootNode = InRootNode;
	}

	/** SWidget interface */
	virtual FVector2D ComputeDesiredSize(float) const override
	{
		float NodeHeight = RootNode->GetNodeHeight();
	
		// Object nodes do not account for their children since they are viewed separatley
		if( RootNode->GetType() != ESequencerNode::Object )
		{
			NodeHeight += ComputeHeightRecursive( RootNode->GetChildNodes() );
		}

		// Note: X Size is not used
		return FVector2D( 100.0f, NodeHeight );
	}

	virtual FChildren* GetChildren() override
	{ 
		return &Children; 
	}
protected:
	/**
	 * Recursively compute the height starting at the provided nodes
	 *
	 * @param Nodes	The nodes to compute height from
	 */
	float ComputeHeightRecursive( const TArray< TSharedRef<FSequencerDisplayNode> >& Nodes ) const
	{
		const float Padding = SequencerLayoutConstants::NodePadding;

		float Height = 0;
		for( int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex )
		{
			const TSharedRef<FSequencerDisplayNode>& Node = Nodes[NodeIndex];
			if( Node->IsVisible() )
			{
				// If the node is visible compute the height of the node plus any of its children
				Height += ( Padding + Node->GetNodeHeight() + ComputeHeightRecursive( Node->GetChildNodes() ) );
			}
		}

		return Height;
	}

protected:
	/** Root node of this track view panel */
	TSharedPtr<FSequencerDisplayNode> RootNode;
	/** All the widgets in the panel */
	TSlotlessChildren<WidgetType> Children;
};

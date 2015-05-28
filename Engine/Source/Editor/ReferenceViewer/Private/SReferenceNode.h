// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * 
 */
class SReferenceNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS( SReferenceNode ){}

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs, UEdGraphNode_Reference* InNode );

	// SGraphNode implementation
	virtual void UpdateGraphNode() override;
	virtual bool IsNodeEditable() const override { return false; }
	// End SGraphNode implementation

private:
	TSharedPtr<class FAssetThumbnail> AssetThumbnail;
	bool bUsesThumbnail;
};

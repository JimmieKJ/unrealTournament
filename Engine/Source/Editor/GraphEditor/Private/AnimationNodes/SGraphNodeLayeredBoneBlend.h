// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class UAnimGraphNode_LayeredBoneBlend;

class SGraphNodeLayeredBoneBlend : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeLayeredBoneBlend){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UAnimGraphNode_LayeredBoneBlend* InNode);

protected:
	// SGraphNode interface
	virtual void CreateInputSideAddButton(TSharedPtr<SVerticalBox> InputBox) override;
	virtual FReply OnAddPin() override;
	// End of SGraphNode interface

private:

	// The node that we represent
	UAnimGraphNode_LayeredBoneBlend* Node;

};

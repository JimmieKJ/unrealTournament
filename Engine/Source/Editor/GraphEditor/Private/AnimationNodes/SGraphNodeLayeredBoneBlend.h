// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SAnimationGraphNode.h"

class UAnimGraphNode_LayeredBoneBlend;

class SGraphNodeLayeredBoneBlend : public SAnimationGraphNode
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

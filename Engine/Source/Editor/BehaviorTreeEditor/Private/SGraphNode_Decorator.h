// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SGraphNode_Decorator : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNode_Decorator){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UBehaviorTreeDecoratorGraphNode_Decorator* InNode);

	virtual FString GetNodeComment() const override;
};

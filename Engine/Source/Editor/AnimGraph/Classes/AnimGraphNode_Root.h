// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AnimGraphNode_Base.h"
#include "Animation/AnimNode_Root.h"
#include "AnimGraphNode_Root.generated.h"

UCLASS(MinimalAPI)
class UAnimGraphNode_Root : public UAnimGraphNode_Base
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=Settings)
	FAnimNode_Root Node;

	// Begin UEdGraphNode interface.
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool CanUserDeleteNode() const override { return false; }
	virtual bool CanDuplicateNode() const override { return false; }
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	// End UEdGraphNode interface.

	// UAnimGraphNode_Base interface
	virtual bool IsSinkNode() const override;

	// Get the link to the documentation
	virtual FString GetDocumentationLink() const override;

	// End of UAnimGraphNode_Base interface
};

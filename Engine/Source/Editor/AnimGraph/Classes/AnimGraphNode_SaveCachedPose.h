// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AnimGraphNode_Base.h"
#include "Animation/AnimNode_SaveCachedPose.h"
#include "EdGraph/EdGraphNodeUtils.h" // for FNodeTextCache
#include "AnimGraphNode_SaveCachedPose.generated.h"

UCLASS(MinimalAPI)
class UAnimGraphNode_SaveCachedPose : public UAnimGraphNode_Base
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=Settings)
	FAnimNode_SaveCachedPose Node;

	UPROPERTY(EditAnywhere, Category=CachedPose)
	FString CacheName;

	// UEdGraphNode interface
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void OnRenameNode(const FString& NewName) override;
	virtual TSharedPtr<class INameValidatorInterface> MakeNameValidator() const override;
	virtual bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	// End of UEdGraphNode interface

	// UK2Node interface.
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	// End of UK2Node interface

	// UAnimGraphNode_Base interface
	virtual FString GetNodeCategory() const override;
	virtual bool IsSinkNode() const override { return true; }
	// End of UAnimGraphNode_Base interface

private:
	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTextCache CachedNodeTitle;
};

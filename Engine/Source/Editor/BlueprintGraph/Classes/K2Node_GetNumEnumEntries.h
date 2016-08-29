// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NodeDependingOnEnumInterface.h"
#include "EdGraph/EdGraphNodeUtils.h" // for FNodeTextCache
#include "K2Node_GetNumEnumEntries.generated.h"

UCLASS(MinimalAPI)
class UK2Node_GetNumEnumEntries : public UK2Node, public INodeDependingOnEnumInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	UEnum* Enum;

	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	//~ End UEdGraphNode Interface

	//~ Begin UK2Node Interface
	virtual bool IsNodePure() const override { return true; }
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	//~ End UK2Node Interface

	// INodeDependingOnEnumInterface
	virtual class UEnum* GetEnum() const override { return Enum; }
	virtual bool ShouldBeReconstructedAfterEnumChanged() const override {return false;}
	// End of INodeDependingOnEnumInterface

private:
	/** Constructing FText strings can be costly, so we cache the node's title/tooltip */
	FNodeTextCache CachedTooltip;
	FNodeTextCache CachedNodeTitle;
};


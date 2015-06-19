// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "K2Node_Composite.h"
#include "EdGraph/EdGraphNodeUtils.h" // for FNodeTextCache
#include "K2Node_MathExpression.generated.h"

/**
* This node type acts like a collapsed node, a single node that represents
* a larger sub-network of nodes (contained within a sub-graph). This node will
* take the math expression it was named with, and attempt to convert it into a
* series of math nodes. If it is unsuccessful, then it generates a series of
* actionable errors.
*/
UCLASS()
class BLUEPRINTGRAPH_API UK2Node_MathExpression : public UK2Node_Composite
{
	GENERATED_UCLASS_BODY()

public:

	// The math expression to evaluate
	UPROPERTY(EditAnywhere, Category = Expression)
	FString Expression;

public:
	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface

	// UEdGraphNode interface
	virtual TSharedPtr<class INameValidatorInterface> MakeNameValidator() const override;
	virtual void OnRenameNode(const FString& NewName) override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void PostPlacedNewNode() override;
	virtual void ReconstructNode() override;
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	// End of UK2Node interface

	// Begin UK2Node_EditablePinBase interface
	virtual bool CanCreateUserDefinedPin(const FEdGraphPinType& InPinType, EEdGraphPinDirection InDesiredDirection, FText& OutErrorMessage) override { return false; }
	// End UK2Node_EditablePinBase interface

private:
	/**
	* Clears this node's sub-graph, and then takes the supplied string and
	* parses, and converts it into a series of new graph nodes.
	*
	* @param  NewExpression	The new string we wish to replace the current "Expression".
	*/
	void RebuildExpression(FString NewExpression);

	/**
	* Clears the cached expression string, deletes all generated node, clears
	* input pins, and resets the parser and graph generator (priming this node
	* for a new expression).
	*/
	void ClearExpression();

	/** Sanitizes an expression for display, removing outermost parentheses */
	FString SanitizeDisplayExpression(FString InExpression) const;

	/** Helper function to build the node's full title */
	FText GetFullTitle(FText InExpression) const;
private:
	/** Cached so we don't have to regenerate it when the graph is recompiled */
	TSharedPtr<class FCompilerResultsLog> CachedMessageLog;

	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTextCache CachedNodeTitle;

	/** Constructing the display string for a Math Expression is costly, so we cache it */
	FNodeTextCache CachedDisplayExpression;
};



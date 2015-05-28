// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node_Tunnel.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNodeUtils.h" // for FNodeTextCache
#include "K2Node_MacroInstance.generated.h"

UCLASS(MinimalAPI)
class UK2Node_MacroInstance : public UK2Node_Tunnel
{
	GENERATED_UCLASS_BODY()

private:
	/** A macro is like a composite node, except that the associated graph lives
	  * in another blueprint, and can be instanced multiple times. */
	UPROPERTY()
	class UEdGraph* MacroGraph_DEPRECATED;

	UPROPERTY()
	FGraphReference MacroGraphReference;

public:
	/** Stored type info for what type the wildcard pins in this macro should become. */
	UPROPERTY()
	struct FEdGraphPinType ResolvedWildcardType;

	/** Whether we need to reconstruct the node after the pins have changed */
	bool bReconstructNode;

	// Begin UObject interface
	virtual void Serialize(FArchive& Ar) override;
	// End UObject interface

	// Begin UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetTooltipText() const override;
	virtual FText GetKeywords() const override;
	virtual void PostPasteNode() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool CanUserDeleteNode() const override { return true; }
	void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void NodeConnectionListChanged() override;
	virtual FString GetDocumentationLink() const override;
	virtual FString GetDocumentationExcerptName() const override;
	virtual FName GetPaletteIcon(FLinearColor& OutColor) const override;
	virtual bool CanPasteHere(const UEdGraph* TargetGraph) const override;
	// End UEdGraphNode interface

	// Begin UK2Node interface
	virtual bool DrawNodeAsExit() const override { return false; }
	virtual bool DrawNodeAsEntry() const override { return false; }
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void PostReconstructNode() override;
	virtual FText GetActiveBreakpointToolTipText() const override;
	virtual bool HasExternalBlueprintDependencies(TArray<class UStruct*>* OptionalOutput) const override;
	virtual void GetNodeAttributes( TArray<TKeyValuePair<FString, FString>>& OutNodeAttributes ) const override;
	virtual FText GetMenuCategory() const override;
	virtual int32 GetNodeRefreshPriority() const override { return EBaseNodeRefreshPriority::Low_UsesDependentWildcard; }
	virtual FBlueprintNodeSignature GetSignature() const override;
	virtual bool IsActionFilteredOut(class FBlueprintActionFilter const& Filter) override;
	virtual FText GetCompactNodeTitle() const override;
	virtual bool ShouldDrawCompact() const override;
	// End UK2Node interface

	// Begin UK2Node_EditablePinBase interface
	virtual bool CanCreateUserDefinedPin(const FEdGraphPinType& InPinType, EEdGraphPinDirection InDesiredDirection, FText& OutErrorMessage) override { return false; }
	// End UK2Node_EditablePinBase interface

	void SetMacroGraph(UEdGraph* Graph) { MacroGraphReference.SetGraph(Graph); }
	UEdGraph* GetMacroGraph() const { return MacroGraphReference.GetGraph(); }
	UBlueprint* GetSourceBlueprint() const { return MacroGraphReference.GetBlueprint(); }

	// Finds the associated metadata for the macro instance if there is any; this function is not particularly fast.
	BLUEPRINTGRAPH_API static FKismetUserDeclaredFunctionMetadata* GetAssociatedGraphMetadata(const UEdGraph* AssociatedMacroGraph);
	static void FindInContentBrowser(TWeakObjectPtr<UK2Node_MacroInstance> MacroInstance);

private:
	/** Constructing FText strings can be costly, so we cache the node's tooltip */
	FNodeTextCache CachedTooltip;
};


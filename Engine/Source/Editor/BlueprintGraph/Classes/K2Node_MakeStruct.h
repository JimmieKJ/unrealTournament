// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "K2Node_StructMemberSet.h"
#include "EdGraph/EdGraphNodeUtils.h" // for FNodeTextCache
#include "K2Node_MakeStruct.generated.h"

// Pure kismet node that creates a struct with specified values for each member
UCLASS(MinimalAPI)
class UK2Node_MakeStruct : public UK2Node_StructMemberSet
{
	GENERATED_UCLASS_BODY()

	/**
	* Returns false if:
	*   1. The Struct has a 'native make' method
	* Returns true if:
	*   1. The Struct is tagged as BlueprintType
	*   or
	*   2. The Struct has any property that is tagged as CPF_BlueprintVisible and is not CPF_BlueprintReadOnly
	*   or
	*   3. The Struct has any property that is tagged as CPF_Edit and is not CPF_BlueprintReadOnly and bIncludeEditAnywhere is true 
	*
	* When constructing the context menu we do not allow the presence of the EditAnywhere flag to result in
	* a creation of a break node, although we could do so in the future. There are legacy break nodes that
	* rely on expansion of structs that neither have BlueprintVisible properties nor are tagged as BlueprintType
	*/
	BLUEPRINTGRAPH_API static bool CanBeMade(const UScriptStruct* Struct, bool bIncludeEditAnywhere = true);
	
	// Begin UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual FName GetPaletteIcon(FLinearColor& OutColor) const override{ return TEXT("GraphEditor.MakeStruct_16x"); }
	// End  UEdGraphNode interface

	// Begin K2Node interface
	virtual bool NodeCausesStructuralBlueprintChange() const override { return false; }
	virtual bool IsNodePure() const override { return true; }
	virtual bool DrawNodeAsVariable() const override { return false; }
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual ERedirectType DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex)  const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	// End K2Node interface

protected:
	struct FMakeStructPinManager : public FStructOperationOptionalPinManager
	{
		const uint8* const SampleStructMemory;
	public:
		FMakeStructPinManager(const uint8* InSampleStructMemory);
	protected:
		virtual void CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex, UProperty* Property) const override;
		virtual bool CanTreatPropertyAsOptional(UProperty* TestProperty) const override;
	};

private:
	/** Constructing FText strings can be costly, so we cache the node's title/tooltip */
	FNodeTextCache CachedTooltip;
	FNodeTextCache CachedNodeTitle;
};

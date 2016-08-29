// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "K2Node_StructMemberSet.h"
#include "EdGraph/EdGraphNodeUtils.h" // for FNodeTextCache
#include "K2Node_MakeStruct.generated.h"

// Pure kismet node that creates a struct with specified values for each member
UCLASS(MinimalAPI)
class UK2Node_MakeStruct : public UK2Node_StructMemberSet
{
	GENERATED_UCLASS_BODY()

	/** Helper property to handle upgrades from an old system of displaying pins for
	 *	the override values that properties referenced as a conditional of being set in a struct */
	UPROPERTY()
	bool bMadeAfterOverridePinRemoval;

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
	BLUEPRINTGRAPH_API static bool CanBeMade(const UScriptStruct* Struct, bool bIncludeEditAnywhere = true, bool bMustHaveValidProperties = false);
	
	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	// End of UObject interface

	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void PostPlacedNewNode() override;
	//~ End  UEdGraphNode Interface

	//~ Begin K2Node Interface
	virtual bool NodeCausesStructuralBlueprintChange() const override { return false; }
	virtual bool IsNodePure() const override { return true; }
	virtual bool DrawNodeAsVariable() const override { return false; }
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual ERedirectType DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex)  const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	//~ End K2Node Interface

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

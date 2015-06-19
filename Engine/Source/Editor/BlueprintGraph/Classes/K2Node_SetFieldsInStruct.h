// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "K2Node_MakeStruct.h"
#include "EdGraph/EdGraphNodeUtils.h" // for FNodeTextCache
#include "K2Node_SetFieldsInStruct.generated.h"

// Pure kismet node that creates a struct with specified values for each member
UCLASS(MinimalAPI)
class UK2Node_SetFieldsInStruct : public UK2Node_MakeStruct
{
	GENERATED_UCLASS_BODY()
	// Begin UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FName GetPaletteIcon(FLinearColor& OutColor) const override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	// End  UEdGraphNode interface

	// Begin K2Node interface
	virtual bool IsNodePure() const override { return false; }
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	// End K2Node interface

	BLUEPRINTGRAPH_API static bool ShowCustomPinActions(const UEdGraphPin* InGraphPin, bool bIgnorePinsNum);

	enum EPinsToRemove
	{
		GivenPin,
		AllOtherPins
	};

	BLUEPRINTGRAPH_API void RemoveFieldPins(const UEdGraphPin* InGraphPin, EPinsToRemove Selection);
	BLUEPRINTGRAPH_API bool AllPinsAreShown() const;
	BLUEPRINTGRAPH_API void RestoreAllPins();

protected:
	struct FSetFieldsInStructPinManager : public FMakeStructPinManager
	{
	public:
		FSetFieldsInStructPinManager(const uint8* InSampleStructMemory) : FMakeStructPinManager(InSampleStructMemory) 
		{}

		virtual void GetRecordDefaults(UProperty* TestProperty, FOptionalPinFromProperty& Record) const override;
	};

private:
	/** Constructing FText strings can be costly, so we cache the node's title/tooltip */
	FNodeTextCache CachedTooltip;
	FNodeTextCache CachedNodeTitle;
};

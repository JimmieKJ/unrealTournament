// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node_DynamicCast.h"
#include "K2Node_ClassDynamicCast.generated.h"

UCLASS(MinimalAPI)
class UK2Node_ClassDynamicCast : public UK2Node_DynamicCast
{
	GENERATED_UCLASS_BODY()

	// Begin UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	// End UEdGraphNode interface

	// UK2Node interface
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override { return UK2Node::IsConnectionDisallowed(MyPin, OtherPin, OutReason); }
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override { UK2Node::NotifyPinConnectionListChanged(Pin); }
	// End of UK2Node interface

	// UK2Node_DynamicCast interface
	virtual UEdGraphPin* GetCastSourcePin() const override;
	virtual UEdGraphPin* GetBoolSuccessPin() const override;
	// End of UK2Node_DynamicCast interface
};


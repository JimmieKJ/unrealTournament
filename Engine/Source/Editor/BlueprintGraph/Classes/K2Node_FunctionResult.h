// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node_FunctionTerminator.h"
#include "K2Node_FunctionResult.generated.h"

UCLASS(MinimalAPI)
class UK2Node_FunctionResult : public UK2Node_FunctionTerminator
{
	GENERATED_UCLASS_BODY()

	// Begin UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool CanUserDeleteNode() const override { return false; }
	// End UEdGraphNode interface

	// Begin UK2Node interface
	virtual bool DrawNodeAsExit() const override { return true; }
	virtual bool ShouldShowNodeProperties() const override { return true; }
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	// End UK2Node interface

	// Begin K2Node_FunctionTerminator interface
	virtual bool CanCreateUserDefinedPin(const FEdGraphPinType& InPinType, EEdGraphPinDirection InDesiredDirection, FText& OutErrorMessage) override;
	virtual UEdGraphPin* CreatePinFromUserDefinition(const TSharedPtr<FUserPinInfo> NewPinInfo) override;
	// End K2Node_FunctionTerminator interface
};


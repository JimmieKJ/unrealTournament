// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node_CallFunction.h"
#include "K2Node_CallFunctionOnMember.generated.h"


UCLASS(MinimalAPI)
class UK2Node_CallFunctionOnMember : public UK2Node_CallFunction
{
	GENERATED_UCLASS_BODY()

	/** Reference to member variable to call function on */
	UPROPERTY()
	FMemberReference				MemberVariableToCallOn;

	virtual bool HasExternalBlueprintDependencies(TArray<class UStruct*>* OptionalOutput) const override;

	// Begin UK2Node_CallFunction interface
	virtual UEdGraphPin* CreateSelfPin(const UFunction* Function) override;
	virtual FText GetFunctionContextString() const override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	// End UK2Node_CallFunction interface

};


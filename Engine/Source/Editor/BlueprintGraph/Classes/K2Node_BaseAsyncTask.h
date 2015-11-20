// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "EdGraphSchema_K2_Actions.h"
#include "K2Node_BaseAsyncTask.generated.h"

class UK2Node_CustomEvent;
class UEdGraphPin;
class UK2Node_TemporaryVariable;

/** !!! The proxy object should have RF_StrongRefOnFrame flag. !!! */

UCLASS(Abstract)
class BLUEPRINTGRAPH_API UK2Node_BaseAsyncTask : public UK2Node
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual bool HasExternalDependencies(TArray<class UStruct*>* OptionalOutput) const override;
	virtual FName GetCornerIcon() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	// End of UK2Node interface

protected:
	// Returns the factory function (checked)
	UFunction* GetFactoryFunction() const;

protected:
	// The name of the function to call to create a proxy object
	UPROPERTY()
	FName ProxyFactoryFunctionName;

	// The class containing the proxy object functions
	UPROPERTY()
	UClass* ProxyFactoryClass;

	// The type of proxy object that will be created
	UPROPERTY()
	UClass* ProxyClass;

	// The name of the 'go' function on the proxy object that will be called after delegates are in place, can be NAME_None
	UPROPERTY()
	FName ProxyActivateFunctionName;

	struct BLUEPRINTGRAPH_API FBaseAsyncTaskHelper
	{
		struct FOutputPinAndLocalVariable
		{
			UEdGraphPin* OutputPin;
			UK2Node_TemporaryVariable* TempVar;

			FOutputPinAndLocalVariable(UEdGraphPin* Pin, UK2Node_TemporaryVariable* Var) : OutputPin(Pin), TempVar(Var) {}
		};

		static bool ValidDataPin(const UEdGraphPin* Pin, EEdGraphPinDirection Direction, const UEdGraphSchema_K2* Schema);
		static bool CreateDelegateForNewFunction(UEdGraphPin* DelegateInputPin, FName FunctionName, UK2Node* CurrentNode, UEdGraph* SourceGraph, FKismetCompilerContext& CompilerContext);
		static bool CopyEventSignature(UK2Node_CustomEvent* CENode, UFunction* Function, const UEdGraphSchema_K2* Schema);
		static bool HandleDelegateImplementation(
			UMulticastDelegateProperty* CurrentProperty, const TArray<FBaseAsyncTaskHelper::FOutputPinAndLocalVariable>& VariableOutputs,
			UEdGraphPin* ProxyObjectPin, UEdGraphPin*& InOutLastThenPin,
			UK2Node* CurrentNode, UEdGraph* SourceGraph, FKismetCompilerContext& CompilerContext);

		static const FString& GetAsyncTaskProxyName();
	};
};

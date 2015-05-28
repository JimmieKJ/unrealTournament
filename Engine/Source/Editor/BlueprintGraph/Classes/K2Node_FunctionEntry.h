// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "K2Node_FunctionTerminator.h"
#include "K2Node_FunctionEntry.generated.h"

UCLASS(MinimalAPI)
class UK2Node_FunctionEntry : public UK2Node_FunctionTerminator
{
	GENERATED_UCLASS_BODY()

	/** If specified, the function that is created for this entry point will have this name.  Otherwise, it will have the function signature's name */
	UPROPERTY()
	FName CustomGeneratedFunctionName;

	/** Any extra flags that the function may need */
	UPROPERTY()
	int32 ExtraFlags;

	/** Function metadata */
	UPROPERTY()
	struct FKismetUserDeclaredFunctionMetadata MetaData;

	/** Array of local variables to be added to generated function */
	UPROPERTY()
	TArray<struct FBPVariableDescription> LocalVariables;

	/** Whether or not to enforce const-correctness for const function overrides */
	UPROPERTY()
	bool bEnforceConstCorrectness;

	// Begin UObject interface
	virtual void Serialize(FArchive& Ar) override;
	// End of UObject interface

	// Begin UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool CanUserDeleteNode() const override { return false; }
	virtual bool IsDeprecated() const override;
	virtual FString GetDeprecationMessage() const override;
	// End UEdGraphNode interface

	// Begin UK2Node interface
	virtual bool DrawNodeAsEntry() const override { return true; }
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void GetRedirectPinNames(const UEdGraphPin& Pin, TArray<FString>& RedirectPinNames) const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	// End UK2Node interface

	// Begin UK2Node_EditablePinBase interface
	virtual bool CanUseRefParams() const override { return true; }
	// End UK2Node_EditablePinBase interface

	// Begin K2Node_FunctionTerminator interface
	virtual bool CanCreateUserDefinedPin(const FEdGraphPinType& InPinType, EEdGraphPinDirection InDesiredDirection, FText& OutErrorMessage) override;
	virtual UEdGraphPin* CreatePinFromUserDefinition(const TSharedPtr<FUserPinInfo> NewPinInfo) override;
	// End K2Node_FunctionTerminator interface

	// Removes an output pin from the node
	BLUEPRINTGRAPH_API void RemoveOutputPin(UEdGraphPin* PinToRemove);

	// Returns pin for the automatically added WorldContext parameter (used only by BlueprintFunctionLibrary).
	BLUEPRINTGRAPH_API UEdGraphPin* GetAutoWorldContextPin() const;

	BLUEPRINTGRAPH_API void RemoveUnnecessaryAutoWorldContext();
};


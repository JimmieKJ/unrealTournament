// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node_EditablePinBase.h"
#include "K2Node_FunctionTerminator.generated.h"

UCLASS(abstract, MinimalAPI)
class UK2Node_FunctionTerminator : public UK2Node_EditablePinBase
{
	GENERATED_UCLASS_BODY()

	/** 
	 * The source class that defines the signature, if it is getting that from elsewhere (e.g. interface, base class etc). 
	 * If NULL, this is a newly created function.
	 */
	UPROPERTY()
	TSubclassOf<class UObject>  SignatureClass;

	/** The name of the signature function. */
	UPROPERTY()
	FName SignatureName;


	// Begin UEdGraphNode interface
	virtual bool CanDuplicateNode() const override { return false; }
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FString CreateUniquePinName(FString SourcePinName) const override;
	// End UEdGraphNode interface

	// Begin UK2Node interface
	virtual bool NodeCausesStructuralBlueprintChange() const override { return true; }
	// End UK2Node interface

	// Begin UK2Node_EditablePinBase interface
	virtual bool CanCreateUserDefinedPin(const FEdGraphPinType& InPinType, EEdGraphPinDirection InDesiredDirection, FText& OutErrorMessage) override;
	// End UK2Node_EditablePinBase interface
};


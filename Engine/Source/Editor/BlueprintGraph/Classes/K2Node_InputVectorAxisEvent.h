// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "K2Node_InputAxisKeyEvent.h"
#include "K2Node_InputVectorAxisEvent.generated.h"

UCLASS(MinimalAPI)
class UK2Node_InputVectorAxisEvent : public UK2Node_InputAxisKeyEvent
{
	GENERATED_UCLASS_BODY()

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	// End of UObject interface

	// Begin UK2Node interface
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual UClass* GetDynamicBindingClass() const override;
	virtual void RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	// End UK2Node interface
};
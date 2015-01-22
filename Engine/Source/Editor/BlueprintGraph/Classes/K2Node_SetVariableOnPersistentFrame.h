// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node_SetVariableOnPersistentFrame.generated.h"

/*
 *	FOR INTERNAL USAGE ONLY!
 */

UCLASS(MinimalAPI)
class UK2Node_SetVariableOnPersistentFrame : public UK2Node
{
	GENERATED_BODY()

public:
	// Begin UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	// End UEdGraphNode interface

	// Begin K2Node interface
	virtual bool IsNodePure() const override { return false; }
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	// End K2Node interface
};


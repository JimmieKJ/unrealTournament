// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SoundClassGraphNode.generated.h"

UCLASS(MinimalAPI)
class USoundClassGraphNode : public UEdGraphNode
{
	GENERATED_UCLASS_BODY()

	/** The SoundNode this represents */
	UPROPERTY(VisibleAnywhere, instanced, Category=Sound)
	USoundClass*		SoundClass;

	/** Get the Pin that connects to all children */
	UEdGraphPin* GetChildPin() const { return ChildPin; }
	/** Get the Pin that connects to its parent */
	UEdGraphPin* GetParentPin() const { return ParentPin; }
	/** Check whether the children of this node match the SoundClass it is representing */
	bool CheckRepresentsSoundClass();

	// Begin UEdGraphNode interface.
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual void AllocateDefaultPins() override;
	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool CanUserDeleteNode() const override;
	// End UEdGraphNode interface.

private:
	/** Pin that connects to all children */
	UEdGraphPin* ChildPin;
	/** Pin that connects to its parent */
	UEdGraphPin* ParentPin;
};

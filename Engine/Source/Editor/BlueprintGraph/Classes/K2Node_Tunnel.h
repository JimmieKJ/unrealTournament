// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "K2Node_EditablePinBase.h"
#include "K2Node_Tunnel.generated.h"

UCLASS(MinimalAPI)
class UK2Node_Tunnel : public UK2Node_EditablePinBase
{
	GENERATED_UCLASS_BODY()

	// A tunnel node either has output pins that came from another tunnel's input pins, or vis versa
	// Note: OutputSourceNode might be equal to InputSinkNode
	
	// The output pins of this tunnel node came from the input pins of OutputSourceNode
	UPROPERTY()
	class UK2Node_Tunnel* OutputSourceNode;

	// The input pins of this tunnel go to the output pins of InputSinkNode
	UPROPERTY()
	class UK2Node_Tunnel* InputSinkNode;

	// Whether this node is allowed to have inputs
	UPROPERTY()
	uint32 bCanHaveInputs:1;

	// Whether this node is allowed to have outputs
	UPROPERTY()
	uint32 bCanHaveOutputs:1;

	// The metadata for the function/subgraph associated with this tunnel node; it's only editable and used
	// on the tunnel entry node inside the subgraph or macro.  This structure is ignored on any other tunnel nodes.
	UPROPERTY()
	struct FKismetUserDeclaredFunctionMetadata MetaData;

	// Begin UEdGraphNode interface.
	virtual void DestroyNode() override;
	virtual void PostPasteNode() override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool CanUserDeleteNode() const override;
	BLUEPRINTGRAPH_API virtual bool CanDuplicateNode() const override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	virtual FString CreateUniquePinName(FString SourcePinName) const override;
	// End UEdGraphNode interface

	// Begin UK2Node interface.
	BLUEPRINTGRAPH_API virtual bool IsNodeSafeToIgnore() const override;
	virtual bool DrawNodeAsEntry() const override;
	virtual bool DrawNodeAsExit() const override;
	virtual bool NodeCausesStructuralBlueprintChange() const override { return true; }
	// End UK2Node interface

	// Begin UK2Node_EditablePinBase interface.
	BLUEPRINTGRAPH_API virtual UEdGraphPin* CreatePinFromUserDefinition(const TSharedPtr<FUserPinInfo> NewPinInfo) override;
	BLUEPRINTGRAPH_API virtual bool CanModifyExecutionWires() override;
	BLUEPRINTGRAPH_API virtual ERenamePinResult RenameUserDefinedPin(const FString& OldName, const FString& NewName, bool bTest = false) override;
	virtual bool CanUseRefParams() const override { return true; }
	virtual bool CanCreateUserDefinedPin(const FEdGraphPinType& InPinType, EEdGraphPinDirection InDesiredDirection, FText& OutErrorMessage) override;
	// End UK2Node_EditablePinBase interface
public:
	// The input pins of this tunnel go to the output pins of InputSinkNode (can be NULL).
	BLUEPRINTGRAPH_API virtual UK2Node_Tunnel* GetInputSink() const;

	// The output pins of this tunnel node came from the input pins of OutputSourceNode (can be NULL).
	BLUEPRINTGRAPH_API virtual UK2Node_Tunnel* GetOutputSource() const;
};




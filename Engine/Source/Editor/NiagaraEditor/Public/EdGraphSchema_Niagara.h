// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EdGraph/EdGraphSchema.h"
#include "INiagaraCompiler.h"
#include "EdGraphSchema_Niagara.generated.h"

/** Action to add a node to the graph */
USTRUCT()
struct NIAGARAEDITOR_API FNiagaraSchemaAction_NewNode : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	/** Template of node we want to create */
	UPROPERTY()
	class UNiagaraNode* NodeTemplate;


	FNiagaraSchemaAction_NewNode() 
		: FEdGraphSchemaAction()
		, NodeTemplate(NULL)
	{}

	FNiagaraSchemaAction_NewNode(const FText& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGrouping) 
		, NodeTemplate(NULL)
	{}

	//~ Begin FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	//~ End FEdGraphSchemaAction Interface

	template <typename NodeType>
	static NodeType* SpawnNodeFromTemplate(class UEdGraph* ParentGraph, NodeType* InTemplateNode, const FVector2D Location, bool bSelectNewNode = true)
	{
		FNiagaraSchemaAction_NewNode Action;
		Action.NodeTemplate = InTemplateNode;

		return Cast<NodeType>(Action.PerformAction(ParentGraph, NULL, Location, bSelectNewNode));
	}
};

UCLASS()
class NIAGARAEDITOR_API UEdGraphSchema_Niagara : public UEdGraphSchema
{
	GENERATED_UCLASS_BODY()

	// Allowable PinType.PinCategory values
	UPROPERTY()
	FString PC_Float;
	UPROPERTY()
	FString PC_Vector;
	UPROPERTY()
	FString PC_Matrix;

	//~ Begin EdGraphSchema Interface
	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
	virtual void GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, class FMenuBuilder* MenuBuilder, bool bIsDebugging) const override;
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;
	virtual bool ShouldHidePinDefaultValue(UEdGraphPin* Pin) const override;
	virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const override;

	virtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) override;
	//~ End EdGraphSchema Interface

	ENiagaraDataType GetPinDataType(UEdGraphPin* Pin)const;
	void GetPinDefaultValue(UEdGraphPin* Pin, float& OutDefault)const;
	void GetPinDefaultValue(UEdGraphPin* Pin, FVector4& OutDefault)const;
	void GetPinDefaultValue(UEdGraphPin* Pin, FMatrix& OutDefault)const;
		
	bool IsSystemConstant(const FNiagaraVariableInfo& Variable)const;

	virtual FString GetFloatName()const { return PC_Float; }
	virtual FString GetVectorName()const { return PC_Vector; }
	virtual FString GetMatrixName()const { return PC_Matrix; }

	static const FLinearColor NodeTitleColor_Attribute;
	static const FLinearColor NodeTitleColor_Constant;
	static const FLinearColor NodeTitleColor_SystemConstant;
	static const FLinearColor NodeTitleColor_FunctionCall;
	static const FLinearColor NodeTitleColor_Event;

private:
	void GetBreakLinkToSubMenuActions(class FMenuBuilder& MenuBuilder, UEdGraphPin* InGraphPin);
};


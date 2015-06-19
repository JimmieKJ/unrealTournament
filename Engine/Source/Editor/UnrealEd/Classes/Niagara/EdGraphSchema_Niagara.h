// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EdGraph/EdGraphSchema.h"
#include "INiagaraCompiler.h"
#include "EdGraphSchema_Niagara.generated.h"

/** Action to add a node to the graph */
USTRUCT()
struct UNREALED_API FNiagaraSchemaAction_NewNode : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	/** Template of node we want to create */
	UPROPERTY()
	class UNiagaraNode* NodeTemplate;


	FNiagaraSchemaAction_NewNode() 
		: FEdGraphSchemaAction()
		, NodeTemplate(NULL)
	{}

	FNiagaraSchemaAction_NewNode(const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGrouping) 
		, NodeTemplate(NULL)
	{}

	// FEdGraphSchemaAction interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	// End of FEdGraphSchemaAction interface

	template <typename NodeType>
	static NodeType* SpawnNodeFromTemplate(class UEdGraph* ParentGraph, NodeType* InTemplateNode, const FVector2D Location, bool bSelectNewNode = true)
	{
		FNiagaraSchemaAction_NewNode Action;
		Action.NodeTemplate = InTemplateNode;

		return Cast<NodeType>(Action.PerformAction(ParentGraph, NULL, Location, bSelectNewNode));
	}
};

UCLASS(MinimalAPI)
class UEdGraphSchema_Niagara : public UEdGraphSchema
{
	GENERATED_UCLASS_BODY()

	// Allowable PinType.PinCategory values
	UPROPERTY()
	FString PC_Float;
	UPROPERTY()
	FString PC_Vector;
	UPROPERTY()
	FString PC_Matrix;
	UPROPERTY()
	FString PC_Curve;

	// Begin EdGraphSchema interface
	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;
	virtual bool ShouldHidePinDefaultValue(UEdGraphPin* Pin) const override;
	// End EdGraphSchema interface

	UNREALED_API ENiagaraDataType GetPinType(UEdGraphPin* Pin)const;
	UNREALED_API void GetPinDefaultValue(UEdGraphPin* Pin, float& OutDefault)const;
	UNREALED_API void GetPinDefaultValue(UEdGraphPin* Pin, FVector4& OutDefault)const;
	UNREALED_API void GetPinDefaultValue(UEdGraphPin* Pin, FMatrix& OutDefault)const;
	
	bool IsSystemConstant(const FNiagaraVariableInfo& Variable)const;

	static const FLinearColor NodeTitleColor_Attribute;
	static const FLinearColor NodeTitleColor_Constant;
	static const FLinearColor NodeTitleColor_SystemConstant;
	static const FLinearColor NodeTitleColor_FunctionCall;
};


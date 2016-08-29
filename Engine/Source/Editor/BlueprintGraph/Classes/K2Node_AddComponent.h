// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node_CallFunction.h"
#include "EdGraph/EdGraphNodeUtils.h" // for FNodeTextCache
#include "K2Node_AddComponent.generated.h"

UCLASS(MinimalAPI)
class UK2Node_AddComponent : public UK2Node_CallFunction
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	uint32 bHasExposedVariable:1;

	/** The blueprint name we came from, so we can lookup the template after a paste */
	UPROPERTY()
	FString TemplateBlueprint;

	//~ Begin UObject Interface
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	//~ End UObject Interface

	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual void DestroyNode() override;
	virtual void PrepareForCopying() override;
	virtual void PostPasteNode() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FString GetDocumentationLink() const override;
	virtual FString GetDocumentationExcerptName() const override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	//~ End UEdGraphNode Interface

	//~ Begin K2Node Interface
	virtual UActorComponent* GetTemplateFromNode() const override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	//~ End UK2Node Interface

	BLUEPRINTGRAPH_API void AllocateDefaultPinsWithoutExposedVariables();
	BLUEPRINTGRAPH_API void AllocatePinsForExposedVariables();

	UEdGraphPin* GetTemplateNamePinChecked() const
	{
		UEdGraphPin* FoundPin = GetTemplateNamePin();
		check(FoundPin != NULL);
		return FoundPin;
	}

	UEdGraphPin* GetRelativeTransformPin() const
	{
		return FindPinChecked(TEXT("RelativeTransform"));
	}

	UEdGraphPin* GetManualAttachmentPin() const
	{
		return FindPinChecked(TEXT("bManualAttachment"));
	}

	/** Helper method used to instantiate a new component template after duplication. */
	BLUEPRINTGRAPH_API void MakeNewComponentTemplate();

	/** Static name of function to call */
	static BLUEPRINTGRAPH_API FName GetAddComponentFunctionName();

private: 
	UEdGraphPin* GetTemplateNamePin() const
	{
		return FindPin(TEXT("TemplateName"));
	}

	const UClass* GetSpawnedType() const;
};




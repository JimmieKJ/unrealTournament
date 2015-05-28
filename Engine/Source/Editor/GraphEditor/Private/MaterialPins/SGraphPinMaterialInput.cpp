// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "GraphEditorCommon.h"
#include "SGraphPinText.h"
#include "SGraphPinMaterialInput.h"

void SGraphPinMaterialInput::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments().UsePinColorForText(true), InGraphPinObj);
}

FSlateColor SGraphPinMaterialInput::GetPinColor() const
{
	check(GraphPinObj);
	UMaterialGraph* MaterialGraph = CastChecked<UMaterialGraph>(GraphPinObj->GetOwningNode()->GetGraph());
	const UMaterialGraphSchema* Schema = CastChecked<UMaterialGraphSchema>(MaterialGraph->GetSchema());

	if (MaterialGraph->IsInputActive(GraphPinObj))
	{
		return Schema->ActivePinColor;
	}
	else
	{
		return Schema->InactivePinColor;
	}
}

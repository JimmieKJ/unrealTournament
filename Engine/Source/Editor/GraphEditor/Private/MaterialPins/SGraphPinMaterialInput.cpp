// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "GraphEditorCommon.h"
#include "SGraphPinText.h"
#include "SGraphPinMaterialInput.h"

void SGraphPinMaterialInput::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	UMaterialGraph* MaterialGraph = CastChecked<UMaterialGraph>(InGraphPinObj->GetOwningNode()->GetGraph());
	const UMaterialGraphSchema* Schema = CastChecked<UMaterialGraphSchema>(MaterialGraph->GetSchema());

	if (MaterialGraph->IsInputActive(InGraphPinObj))
	{
		CachedPinColor = Schema->ActivePinColor;
	}
	else
	{
		CachedPinColor = Schema->InactivePinColor;
	}

	SGraphPin::Construct(SGraphPin::FArguments().UsePinColorForText(true), InGraphPinObj);
}

FSlateColor SGraphPinMaterialInput::GetPinColor() const
{
	return CachedPinColor;
}

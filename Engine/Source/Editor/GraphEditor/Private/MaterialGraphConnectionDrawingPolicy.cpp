// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GraphEditorCommon.h"
#include "MaterialGraphConnectionDrawingPolicy.h"

/////////////////////////////////////////////////////
// FMaterialGraphConnectionDrawingPolicy

FMaterialGraphConnectionDrawingPolicy::FMaterialGraphConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
	: FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements)
	, MaterialGraph(CastChecked<UMaterialGraph>(InGraphObj))
	, MaterialGraphSchema(CastChecked<UMaterialGraphSchema>(InGraphObj->GetSchema()))
{
	// Don't want to draw ending arrowheads
	ArrowImage = nullptr;
	ArrowRadius = FVector2D::ZeroVector;

	// Still need to be able to perceive the graph while dragging connectors, esp over comment boxes
	HoverDeemphasisDarkFraction = 0.4f;
}

void FMaterialGraphConnectionDrawingPolicy::DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params)
{
	Params.AssociatedPin1 = OutputPin;
	Params.AssociatedPin2 = InputPin;
	Params.WireColor = MaterialGraphSchema->ActivePinColor;

	// Have to consider both pins as the input will be an 'output' when previewing a connection
	if (OutputPin)
	{
		if (!MaterialGraph->IsInputActive(OutputPin))
		{
			Params.WireColor = MaterialGraphSchema->InactivePinColor;
		}
	}
	if (InputPin)
	{
		if (!MaterialGraph->IsInputActive(InputPin))
		{
			Params.WireColor = MaterialGraphSchema->InactivePinColor;
		}
	}

	const bool bDeemphasizeUnhoveredPins = HoveredPins.Num() > 0;
	if (bDeemphasizeUnhoveredPins)
	{
		ApplyHoverDeemphasis(OutputPin, InputPin, /*inout*/ Params.WireThickness, /*inout*/ Params.WireColor);
	}
}

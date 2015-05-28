// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConnectionDrawingPolicy.h"

/////////////////////////////////////////////////////
// FMaterialGraphConnectionDrawingPolicy

// This class draws the connections for an UEdGraph using a MaterialGraph schema
class FMaterialGraphConnectionDrawingPolicy : public FConnectionDrawingPolicy
{
protected:
	UMaterialGraph* MaterialGraph;
	const UMaterialGraphSchema* MaterialGraphSchema;

public:
	FMaterialGraphConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj);

	// FConnectionDrawingPolicy interface
	virtual void DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params) override;
	// End of FConnectionDrawingPolicy interface
};

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AIGraphConnectionDrawingPolicy.h"

// This class draws the connections for an UEdGraph with a behavior tree schema
class BEHAVIORTREEEDITOR_API FBehaviorTreeConnectionDrawingPolicy : public FAIGraphConnectionDrawingPolicy
{
public:
	//
	FBehaviorTreeConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj);

	// FConnectionDrawingPolicy interface 
	virtual void DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params) override;
	// End of FConnectionDrawingPolicy interface
};

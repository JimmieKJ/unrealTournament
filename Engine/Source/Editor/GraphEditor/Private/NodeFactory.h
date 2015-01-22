// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Class that decides which widget type to create for a given data object */
class GRAPHEDITOR_API FNodeFactory
{
public:
	/** Create a widget for the supplied node */
	static TSharedPtr<SGraphNode> CreateNodeWidget(UEdGraphNode* InNode);

	/** Create a widget for the supplied pin */
	static TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* InPin);
};

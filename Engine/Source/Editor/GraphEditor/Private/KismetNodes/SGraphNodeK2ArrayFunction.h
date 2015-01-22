// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SGraphNodeK2ArrayFunction : public SGraphNodeK2Base
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeK2ArrayFunction){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UK2Node_CallArrayFunction* InNode);

	virtual const FSlateBrush* GetShadowBrush(bool bSelected) const;

	/** Returns the color for the main type of the node, based on the target pin */
	FSlateColor GetTypeIconColor() const;

	/** Returns the size the background array image should be */
	FOptionalSize GetBackgroundImageSize() const;

	/**
	 * Update this GraphNode to match the data that it is observing
	 */
	virtual void UpdateGraphNode();

private:
	/** The main body of the node */
	TWeakPtr< SWidget > MainNodeContent;
};

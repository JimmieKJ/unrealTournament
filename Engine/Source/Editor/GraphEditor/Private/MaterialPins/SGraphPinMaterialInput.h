// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SGraphPinMaterialInput : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinMaterialInput) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
	// Begin SGraphPin interface
	virtual FSlateColor GetPinColor() const override;
	// End SGraphPin interface

};

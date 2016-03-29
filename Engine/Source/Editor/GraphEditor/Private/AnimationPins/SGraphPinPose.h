// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class SGraphPinPose : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinPose)	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

protected:
	//~ Begin SGraphPin Interface
	virtual const FSlateBrush* GetPinIcon() const override;
	//~ End SGraphPin Interface
};

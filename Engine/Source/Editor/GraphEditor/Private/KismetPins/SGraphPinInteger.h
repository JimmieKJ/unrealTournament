// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SGraphPinInteger : public SGraphPinNum
{
public:
	SLATE_BEGIN_ARGS(SGraphPinNum) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
	// Begin SGraphPinString interface
	virtual void SetTypeInValue(const FText& NewTypeInValue, ETextCommit::Type CommitInfo) override;
	// End of SGraphPinString interface
};

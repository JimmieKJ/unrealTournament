// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphPinObject.h"

/////////////////////////////////////////////////////
// SGraphPinClass

class SGraphPinClass : public SGraphPinObject
{
public:
	SLATE_BEGIN_ARGS(SGraphPinClass) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);
protected:

	// Called when a new class was picked via the asset picker
	void OnPickedNewClass(UClass* ChosenClass);

	// Begin SGraphPinObject interface
	virtual FReply OnClickUse() override;
	virtual bool AllowSelfPinWidget() const override { return false; }
	virtual TSharedRef<SWidget> GenerateAssetPicker() override;
	virtual FText GetDefaultComboText() const override;
	virtual FOnClicked GetOnUseButtonDelegate() override;
	// End SGraphPinObject interface

};

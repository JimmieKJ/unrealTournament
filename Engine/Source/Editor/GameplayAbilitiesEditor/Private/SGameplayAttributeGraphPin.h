// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"
#include "SGraphPin.h"

class SGameplayAttributeGraphPin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGameplayAttributeGraphPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

	//~ Begin SGraphPin Interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	//~ End SGraphPin Interface

	void OnAttributeChanged(UProperty* SelectedAttribute);

	UProperty* LastSelectedProperty;

private:

};

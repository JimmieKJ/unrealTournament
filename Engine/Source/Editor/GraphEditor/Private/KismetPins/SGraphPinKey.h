// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SGraphPinKey : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinKey) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:

	/**
	 *	Function to create class specific widget.
	 *
	 *	@return Reference to the newly created widget object
	 */
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;

private:

	/** Gets the current Key being edited. */
	FKey GetCurrentKey() const;

	/** Updates the pin default when a new key is selected. */
	void OnKeyChanged(TSharedPtr<FKey> SelectedKey);

	FKey SelectedKey;
};

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class SNameComboBox;


/**
 * Customizes a CollisionProfileName graph pin to use a dropdown
 */
class SGraphPinCollisionProfile : public SGraphPin
{
public:

	SLATE_BEGIN_ARGS(SGraphPinCollisionProfile) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:

	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;

	void OnSelectionChanged(TSharedPtr<FName> NameItem, ESelectInfo::Type SelectInfo);
	void OnComboBoxOpening();

	TSharedPtr<FName> GetSelectedName() const;

	void SetPropertyWithName(const FName& Name);
	void GetPropertyAsName(FName& OutName) const;

protected:

	TArray<TSharedPtr<FName>> NameList;
	TSharedPtr<SNameComboBox> NameComboBox;
};

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "PropertyEditing.h"
#include "PropertyCustomizationHelpers.h"

class FEnvQueryParamSetupCustomization : public IPropertyTypeCustomization
{
public:

	// Begin IPropertyTypeCustomization interface
	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	// End IPropertyTypeCustomization interface

	static TSharedRef<IPropertyTypeCustomization> MakeInstance( );

protected:

	TSharedPtr<IPropertyHandle> PropertyHandle;
	TSharedPtr<IPropertyHandle> ValueProp;
	TSharedPtr<IPropertyHandle> NameProp;
	TSharedPtr<SEditableTextBox> TextBox;

	FName LastPropName;
	bool bIsNamed;
	EEnvQueryParam::Type ParamType;

	void CacheMyValues();

	TOptional<float> GetParamNumValue() const;
	void OnParamNumValueChanged(float FloatValue) const;
	EVisibility GetParamNumValueVisibility() const;

	ECheckBoxState GetParamBoolValue() const;
	void OnParamBoolValueChanged(ECheckBoxState BoolValue) const;
	EVisibility GetParamBoolValueVisibility() const;

	void OnParamNameCommitted(const FText& ParamName, ETextCommit::Type CommitInfo);
	EVisibility GetParamNameVisibility() const;

	FReply ToggleMode();
	void OnModeChanged();
	FString GetComboText() const;
	FString GetComboTooltip() const;
};

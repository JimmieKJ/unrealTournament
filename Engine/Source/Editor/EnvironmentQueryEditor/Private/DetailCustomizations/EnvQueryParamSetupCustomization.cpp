// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQueryEditorPrivatePCH.h"
#include "EnvQueryParamSetupCustomization.h"
#include "SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "FEnvQueryCustomization"

TSharedRef<IPropertyTypeCustomization> FEnvQueryParamSetupCustomization::MakeInstance( )
{
	return MakeShareable(new FEnvQueryParamSetupCustomization);
}

void FEnvQueryParamSetupCustomization::CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	PropertyHandle = StructPropertyHandle;
	CacheMyValues();

	// create struct header
	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding(0.0f, 2.0f, 5.0f, 2.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.OnClicked( this, &FEnvQueryParamSetupCustomization::ToggleMode )
			.ContentPadding(2.0f)
			.VAlign(VAlign_Center)
			.Content()
			[
				SNew(STextBlock) 
				.Text(this, &FEnvQueryParamSetupCustomization::GetComboText)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ToolTipText(this, &FEnvQueryParamSetupCustomization::GetComboTooltip)
			]
		]
		+SHorizontalBox::Slot()
		.Padding(0.0f, 2.0f, 5.0f, 2.0f)
		[
			SAssignNew(TextBox, SEditableTextBox)
			.Visibility(this, &FEnvQueryParamSetupCustomization::GetParamNameVisibility)
			.OnTextCommitted(this, &FEnvQueryParamSetupCustomization::OnParamNameCommitted)
		]
		+SHorizontalBox::Slot()
		.Padding(0.0f, 2.0f, 5.0f, 2.0f)
		[
			SNew(SNumericEntryBox<float>)
			.AllowSpin(false)
			.Visibility(this, &FEnvQueryParamSetupCustomization::GetParamNumValueVisibility)
			.Value(this, &FEnvQueryParamSetupCustomization::GetParamNumValue)
			.OnValueChanged(this, &FEnvQueryParamSetupCustomization::OnParamNumValueChanged)
		]
		+SHorizontalBox::Slot()
		.Padding(0.0f, 2.0f, 5.0f, 2.0f)
		[
			SNew(SCheckBox)
			.Visibility(this, &FEnvQueryParamSetupCustomization::GetParamBoolValueVisibility)
			.IsChecked(this, &FEnvQueryParamSetupCustomization::GetParamBoolValue )
			.OnCheckStateChanged(this, &FEnvQueryParamSetupCustomization::OnParamBoolValueChanged )
		]
	];

	OnModeChanged();
}

void FEnvQueryParamSetupCustomization::CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	// do nothing
}

#define GET_STRUCT_NAME_CHECKED(StructName) \
	((void)sizeof(StructName), TEXT(#StructName))

void FEnvQueryParamSetupCustomization::CacheMyValues()
{
	ValueProp = PropertyHandle->GetChildHandle(TEXT("Value"));
	NameProp = PropertyHandle->GetChildHandle(TEXT("ParamName"));

	FString TypeText, ExtendedTypeText;
	TypeText = PropertyHandle->GetProperty()->GetCPPType(&ExtendedTypeText, CPPF_None);
	if (TypeText.Contains(GET_STRUCT_NAME_CHECKED(FEnvIntParam)))
	{
		ParamType = EEnvQueryParam::Int;
	}
	else if (TypeText.Contains(GET_STRUCT_NAME_CHECKED(FEnvBoolParam)))
	{
		ParamType = EEnvQueryParam::Bool;
	}
	else
	{
		ParamType = EEnvQueryParam::Float;
	}

	FName NamedProperty;
	FPropertyAccess::Result PropResult = NameProp->GetValue(NamedProperty);

	bIsNamed = (PropResult == FPropertyAccess::Success && NamedProperty != NAME_None);
	LastPropName = bIsNamed ? NamedProperty : FName(TEXT("unknown"));
}

#undef GET_STRUCT_NAME_CHECKED

TOptional<float> FEnvQueryParamSetupCustomization::GetParamNumValue() const
{
	// Gets the actual aspect ratio property value
	if (ParamType == EEnvQueryParam::Float)
	{
		float FloatValue = 0.0f;
		FPropertyAccess::Result PropResult = ValueProp->GetValue(FloatValue);
		if (PropResult == FPropertyAccess::Success)
		{
			return FloatValue;
		}
	}
	else if (ParamType == EEnvQueryParam::Int)
	{
		int32 IntValue = 0;
		FPropertyAccess::Result PropResult = ValueProp->GetValue(IntValue);
		if (PropResult == FPropertyAccess::Success)
		{
			float FloatValue = IntValue;
			return FloatValue;
		}
	}

	return TOptional<float>();
}

void FEnvQueryParamSetupCustomization::OnParamNumValueChanged(float FloatValue) const
{
	if (ParamType == EEnvQueryParam::Float)
	{
		ValueProp->SetValue(FloatValue);
	}
	else if (ParamType == EEnvQueryParam::Int)
	{
		const int32 IntValue = FMath::TruncToInt(FloatValue);
		ValueProp->SetValue(IntValue);
	}
}

void FEnvQueryParamSetupCustomization::OnParamNameCommitted(const FText& ParamName, ETextCommit::Type CommitInfo)
{
	FName NameValue = *ParamName.ToString();
	NameProp->SetValue(NameValue);

	LastPropName = (NameValue != NAME_None) ? NameValue : FName(TEXT("unknown"));
}

ECheckBoxState FEnvQueryParamSetupCustomization::GetParamBoolValue() const
{
	if (ParamType == EEnvQueryParam::Bool)
	{
		bool BoolValue = false;
		FPropertyAccess::Result PropResult = ValueProp->GetValue(BoolValue);
		if (PropResult == FPropertyAccess::Success)
		{
			return BoolValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
	}
	
	return ECheckBoxState::Undetermined;
}

void FEnvQueryParamSetupCustomization::OnParamBoolValueChanged(ECheckBoxState BoolValue) const
{
	if (ParamType == EEnvQueryParam::Bool)
	{
		const bool StoreValue = (BoolValue == ECheckBoxState::Checked);
		ValueProp->SetValue(StoreValue);
	}
}

FString FEnvQueryParamSetupCustomization::GetComboText() const
{
	return bIsNamed ? FString("NAMED") : FString("VALUE");
}

FString FEnvQueryParamSetupCustomization::GetComboTooltip() const
{
	return bIsNamed ?
		LOCTEXT("NamedTooltip","Value will be provided by named parameter, press to switch into direct mode.").ToString() :
		LOCTEXT("ValueTooltip","Direct value of property, press to switch into parameter mode.").ToString();
}

FReply FEnvQueryParamSetupCustomization::ToggleMode()
{
	bIsNamed = !bIsNamed;
	OnModeChanged();

	return FReply::Handled();
}

EVisibility FEnvQueryParamSetupCustomization::GetParamNameVisibility() const
{
	return bIsNamed ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FEnvQueryParamSetupCustomization::GetParamNumValueVisibility() const
{
	return (bIsNamed || ParamType == EEnvQueryParam::Bool) ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility FEnvQueryParamSetupCustomization::GetParamBoolValueVisibility() const
{
	return (bIsNamed || ParamType != EEnvQueryParam::Bool) ? EVisibility::Collapsed : EVisibility::Visible;
}

void FEnvQueryParamSetupCustomization::OnModeChanged()
{
	if (bIsNamed)
	{
		NameProp->SetValue(LastPropName);

		if (TextBox.IsValid())
		{
			TextBox->SetText(FText::FromName(LastPropName));
		}
	}
	else
	{
		FName NameValue = NAME_None;
		NameProp->SetValue(NameValue);
	}
}

#undef LOCTEXT_NAMESPACE

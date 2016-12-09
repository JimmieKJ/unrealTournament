// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SkeletalControlNodeDetails.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "DetailCategoryBuilder.h"
#include "PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE "SkeletalControlNodeDetails"

/////////////////////////////////////////////////////
// FSkeletalControlNodeDetails 

TSharedRef<IDetailCustomization> FSkeletalControlNodeDetails::MakeInstance()
{
	return MakeShareable(new FSkeletalControlNodeDetails());
}

void FSkeletalControlNodeDetails::CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder)
{
	DetailCategory = &DetailBuilder.EditCategory("PinOptions");
	TSharedRef<IPropertyHandle> AvailablePins = DetailBuilder.GetProperty("ShowPinForProperties");
	TSharedPtr<IPropertyHandleArray> ArrayProperty = AvailablePins->AsArray();

	TSet<FName> UniqueCategoryNames;
	{
		int32 NumElements = 0;
		{
			uint32 UnNumElements = 0;
			if (ArrayProperty.IsValid() && (FPropertyAccess::Success == ArrayProperty->GetNumElements(UnNumElements)))
			{
				NumElements = static_cast<int32>(UnNumElements);
			}
		}
		for (int32 Index = 0; Index < NumElements; ++Index)
		{
			auto StructPropHandle = ArrayProperty->GetElement(Index);
			auto CategoryPropeHandle = StructPropHandle->GetChildHandle("CategoryName");
			check(CategoryPropeHandle.IsValid());
			FName CategoryNameValue;
			const auto Result = CategoryPropeHandle->GetValue(CategoryNameValue);
			if (ensure(FPropertyAccess::Success == Result))
			{
				UniqueCategoryNames.Add(CategoryNameValue);
			}
		}
	}

	//@TODO: Shouldn't show this if the available pins array is empty!
	const bool bGenerateHeader = true;
	const bool bDisplayResetToDefault = false;
	const bool bDisplayElementNum = false;
	const bool bForAdvanced = false;
	for (auto CategoryName : UniqueCategoryNames)
	{
		TSharedRef<FDetailArrayBuilder> AvailablePinsBuilder = MakeShareable(new FDetailArrayBuilder(AvailablePins, bGenerateHeader, bDisplayResetToDefault, bDisplayElementNum));
		AvailablePinsBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FSkeletalControlNodeDetails::OnGenerateElementForPropertyPin, CategoryName));
		AvailablePinsBuilder->SetDisplayName((CategoryName == NAME_None) ? LOCTEXT("DefaultCategory", "Default Category") : FText::FromName(CategoryName));
		DetailCategory->AddCustomBuilder(AvailablePinsBuilder, bForAdvanced);
	}
}

ECheckBoxState FSkeletalControlNodeDetails::GetShowPinValueForProperty(TSharedRef<IPropertyHandle> InElementProperty) const
{
	FString Value;
	InElementProperty->GetChildHandle("bShowPin")->GetValueAsFormattedString(Value);

	if (Value == TEXT("true"))
	{
		return ECheckBoxState::Checked;
	}
	else if(Value == TEXT("false"))
	{
		return ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Undetermined;
}

void FSkeletalControlNodeDetails::OnShowPinChanged(ECheckBoxState InNewState, TSharedRef<IPropertyHandle> InElementProperty)
{
	InElementProperty->GetChildHandle("bShowPin")->SetValueFromFormattedString(InNewState == ECheckBoxState::Checked? TEXT("true") : TEXT("false"));
}

void FSkeletalControlNodeDetails::OnGenerateElementForPropertyPin(TSharedRef<IPropertyHandle> ElementProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder, FName CategoryName)
{
	{
		auto CategoryPropeHandle = ElementProperty->GetChildHandle("CategoryName");
		check(CategoryPropeHandle.IsValid());
		FName CategoryNameValue;
		const auto Result = CategoryPropeHandle->GetValue(CategoryNameValue);
		const bool bProperCategory = ensure(FPropertyAccess::Success == Result) && (CategoryNameValue == CategoryName);
		if (!bProperCategory)
		{
			return;
		}
	}

	TSharedPtr<IPropertyHandle> PropertyNameHandle = ElementProperty->GetChildHandle("PropertyFriendlyName");
	FText PropertyFriendlyName(LOCTEXT("Invalid", "Invalid"));

	if (PropertyNameHandle.IsValid())
	{
		FString DisplayFriendlyName;
		switch (PropertyNameHandle->GetValue(/*out*/ DisplayFriendlyName))
		{
		case FPropertyAccess::Success:
		{
			PropertyFriendlyName = FText::FromString(DisplayFriendlyName);

			//DetailBuilder.EditCategory
			//DisplayNameAsName
		}
			break;
		case FPropertyAccess::MultipleValues:
			ChildrenBuilder.AddChildContent(FText::GetEmpty())
				[
					SNew(STextBlock).Text(LOCTEXT("OnlyWorksInSingleSelectMode", "Multiple types selected"))
				];
			return;
		case FPropertyAccess::Fail:
		default:
			check(false);
			break;
		}
	}
	
	TSharedPtr<IPropertyHandle> HasOverrideValueHandle = ElementProperty->GetChildHandle("bHasOverridePin");
	bool bHasOverrideValue;
	HasOverrideValueHandle->GetValue(/*out*/ bHasOverrideValue);
	FText Tooltip;

	// Setup a tooltip based on whether the property has an override value or not.
	if (bHasOverrideValue)
	{
		Tooltip = LOCTEXT("HasOverridePin", "Enabling this pin will make it visible for setting on the node and automatically enable the value for override when using the struct. Any updates to the resulting struct will require the value be set again or the override will be automatically disabled.");
	}
	else
	{
		Tooltip = LOCTEXT("HasNoOverridePin", "Enabling this pin will make it visible for setting on the node.");
	}

	TSharedRef<SWidget> PropertyNameWidget = ElementProperty->CreatePropertyNameWidget(PropertyFriendlyName);
	PropertyNameWidget->SetToolTipText(Tooltip);

	FString Value;
	ElementProperty->GetChildHandle("bShowPin")->GetValueAsFormattedString(Value);

	ChildrenBuilder.AddChildContent( PropertyFriendlyName )
	[
		SNew( SHorizontalBox )
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		.Padding( 3.0f, 0.0f )
		.FillWidth(1.0f)
		[
			PropertyNameWidget
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.FillWidth(1.0f)
		[
			SNew(SCheckBox)
				.IsChecked(this, &FSkeletalControlNodeDetails::GetShowPinValueForProperty, ElementProperty)
				.OnCheckStateChanged(this, &FSkeletalControlNodeDetails::OnShowPinChanged, ElementProperty)
				.ToolTipText(Tooltip)
		]
	];
}


/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE


// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "IntervalStructCustomization.h"
#include "SNumericEntryBox.h"


#define LOCTEXT_NAMESPACE "IntervalStructCustomization"


/* Helper traits for getting a metadata property based on the template parameter type
 *****************************************************************************/

namespace IntervalMetadata
{
	template <typename NumericType>
	struct FGetHelper
	{ };

	template <>
	struct FGetHelper<float>
	{
		static float GetMetaData(const UProperty* Property, const TCHAR* Key)
		{
			return Property->GetFLOATMetaData(Key);
		}
	};

	template <>
	struct FGetHelper<int32>
	{
		static int32 GetMetaData(const UProperty* Property, const TCHAR* Key)
		{
			return Property->GetINTMetaData(Key);
		}
	};
}


/* FIntervalStructCustomization static interface
 *****************************************************************************/

template <typename NumericType>
TSharedRef<IPropertyTypeCustomization> FIntervalStructCustomization<NumericType>::MakeInstance()
{
	return MakeShareable(new FIntervalStructCustomization<NumericType>);
}


/* IPropertyTypeCustomization interface
 *****************************************************************************/

template <typename NumericType>
void FIntervalStructCustomization<NumericType>::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// Get handles to the properties we're interested in
	MinValueHandle = StructPropertyHandle->GetChildHandle(TEXT("Min"));
	MaxValueHandle = StructPropertyHandle->GetChildHandle(TEXT("Max"));
	check(MinValueHandle.IsValid());
	check(MaxValueHandle.IsValid());

	// Get min/max metadata values if defined
	auto Property = StructPropertyHandle->GetProperty();
	if (Property != nullptr)
	{
		if (Property->HasMetaData(TEXT("UIMin")))
		{
			MinAllowedValue = TOptional<NumericType>(IntervalMetadata::FGetHelper<NumericType>::GetMetaData(Property, TEXT("UIMin")));
		}

		if (Property->HasMetaData(TEXT("UIMax")))
		{
			MaxAllowedValue = TOptional<NumericType>(IntervalMetadata::FGetHelper<NumericType>::GetMetaData(Property, TEXT("UIMax")));
		}
	}

	// Make weak pointers to be passed as payloads to the widgets
	TWeakPtr<IPropertyHandle> MinValueWeakPtr = MinValueHandle;
	TWeakPtr<IPropertyHandle> MaxValueWeakPtr = MaxValueHandle;

	// Build the widgets
	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(200.0f)
	//.MaxDesiredWidth(200.0f)
	[
		SNew(SBox)
		.Padding(FMargin(0.0f, 3.0f, 0.0f, 2.0f))
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.Padding(FMargin(0.0f, 0.0f, 5.0f, 0.0f))
			.VAlign(VAlign_Center)
			[
				SNew(SNumericEntryBox<NumericType>)
				.Value(this, &FIntervalStructCustomization<NumericType>::OnGetValue, MinValueWeakPtr)
				.MinValue(MinAllowedValue)
				.MinSliderValue(MinAllowedValue)
				.MaxValue(this, &FIntervalStructCustomization<NumericType>::OnGetValue, MaxValueWeakPtr)
				.MaxSliderValue(this, &FIntervalStructCustomization<NumericType>::OnGetValue, MaxValueWeakPtr)
				.OnValueCommitted(this, &FIntervalStructCustomization<NumericType>::OnValueCommitted, MinValueWeakPtr)
				.OnValueChanged(this, &FIntervalStructCustomization<NumericType>::OnValueChanged, MinValueWeakPtr)
				.OnBeginSliderMovement(this, &FIntervalStructCustomization<NumericType>::OnBeginSliderMovement)
				.OnEndSliderMovement(this, &FIntervalStructCustomization<NumericType>::OnEndSliderMovement)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.AllowSpin(true)
				.LabelVAlign(VAlign_Center)
				.Label()
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("MinLabel", "Min"))
				]
			]

			+SHorizontalBox::Slot()
			.Padding(FMargin(0.0f, 0.0f, 5.0f, 0.0f))
			.VAlign(VAlign_Center)
			[
				SNew(SNumericEntryBox<NumericType>)
				.Value(this, &FIntervalStructCustomization<NumericType>::OnGetValue, MaxValueWeakPtr)
				.MinValue(this, &FIntervalStructCustomization<NumericType>::OnGetValue, MinValueWeakPtr)
				.MinSliderValue(this, &FIntervalStructCustomization<NumericType>::OnGetValue, MinValueWeakPtr)
				.MaxValue(MaxAllowedValue)
				.MaxSliderValue(MaxAllowedValue)
				.OnValueCommitted(this, &FIntervalStructCustomization<NumericType>::OnValueCommitted, MaxValueWeakPtr)
				.OnValueChanged(this, &FIntervalStructCustomization<NumericType>::OnValueChanged, MaxValueWeakPtr)
				.OnBeginSliderMovement(this, &FIntervalStructCustomization<NumericType>::OnBeginSliderMovement)
				.OnEndSliderMovement(this, &FIntervalStructCustomization<NumericType>::OnEndSliderMovement)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.AllowSpin(true)
				.LabelVAlign(VAlign_Center)
				.Label()
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("MaxLabel", "Max"))
				]
			]
		]
	];
}


template <typename NumericType>
void FIntervalStructCustomization<NumericType>::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// Don't display children, as editing them directly can break the constraints
}


/* FIntervalStructCustomization callbacks
 *****************************************************************************/

template <typename NumericType>
TOptional<NumericType> FIntervalStructCustomization<NumericType>::OnGetValue(TWeakPtr<IPropertyHandle> ValueWeakPtr) const
{
	auto ValueSharedPtr = ValueWeakPtr.Pin();

	NumericType Value;
	if (ValueSharedPtr->GetValue(Value) == FPropertyAccess::Success)
	{
		return TOptional<NumericType>(Value);
	}

	// Value couldn't be accessed. Return an unset value
	return TOptional<NumericType>();
}

template <typename NumericType>
void FIntervalStructCustomization<NumericType>::OnValueCommitted(NumericType NewValue, ETextCommit::Type CommitType, TWeakPtr<IPropertyHandle> HandleWeakPtr)
{
	auto HandleSharedPtr = HandleWeakPtr.Pin();
	if (HandleSharedPtr.IsValid() && (!bIsUsingSlider || (bIsUsingSlider && ShouldAllowSpin())))
	{
		ensure(HandleSharedPtr->SetValue(NewValue) == FPropertyAccess::Success);
	}
}

template <typename NumericType>
void FIntervalStructCustomization<NumericType>::OnValueChanged(NumericType NewValue, TWeakPtr<IPropertyHandle> HandleWeakPtr)
{
	if (bIsUsingSlider && ShouldAllowSpin())
	{
		auto HandleSharedPtr = HandleWeakPtr.Pin();
		if (HandleSharedPtr.IsValid())
		{
			ensure(HandleSharedPtr->SetValue(NewValue, EPropertyValueSetFlags::InteractiveChange) == FPropertyAccess::Success);
		}
	}
}


template <typename NumericType>
void FIntervalStructCustomization<NumericType>::OnBeginSliderMovement()
{
	bIsUsingSlider = true;

	if (ShouldAllowSpin())
	{
		GEditor->BeginTransaction(LOCTEXT("SetIntervalProperty", "Set Interval Property"));
	}
}


template <typename NumericType>
void FIntervalStructCustomization<NumericType>::OnEndSliderMovement(NumericType /*NewValue*/)
{
	bIsUsingSlider = false;

	if (ShouldAllowSpin())
	{
		GEditor->EndTransaction();
	}
}

template <typename NumericType>
bool FIntervalStructCustomization<NumericType>::ShouldAllowSpin() const
{
	return true;
}

/* Only explicitly instantiate the types which are supported
 *****************************************************************************/

template class FIntervalStructCustomization<float>;
template class FIntervalStructCustomization<int32>;


#undef LOCTEXT_NAMESPACE

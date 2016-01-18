// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "PropertyEditorConstants.h"
#include "SNumericEntryBox.h"

#include "UnitConversion.h"
#include "NumericUnitTypeInterface.inl"

#define LOCTEXT_NAMESPACE "PropertyEditor"

template <typename NumericType>
class SPropertyEditorNumeric : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SPropertyEditorNumeric<NumericType> )
		: _Font( FEditorStyle::GetFontStyle( PropertyEditorConstants::PropertyFontStyle ) ) 
		{}
		SLATE_ATTRIBUTE( FSlateFontInfo, Font )
	SLATE_END_ARGS()
	
	void Construct( const FArguments& InArgs, const TSharedRef<FPropertyEditor>& InPropertyEditor )
	{
		bIsUsingSlider = false;

		PropertyEditor = InPropertyEditor;

		const TSharedRef< FPropertyNode > PropertyNode = InPropertyEditor->GetPropertyNode();
		const UProperty* Property = InPropertyEditor->GetProperty();

		// Instance metadata overrides per-class metadata.
		auto GetMetaDataFromKey = [&PropertyNode, &Property](const FName& Key) -> const FString&
		{
			const FString* InstanceValue = PropertyNode->GetInstanceMetaData(Key);
			return (InstanceValue != nullptr) ? *InstanceValue : Property->GetMetaData(Key);
		};

		const FString& MetaUIMinString = GetMetaDataFromKey("UIMin");
		const FString& MetaUIMaxString = GetMetaDataFromKey("UIMax");
		const FString& SliderExponentString = GetMetaDataFromKey("SliderExponent");
		const FString& DeltaString = GetMetaDataFromKey("Delta");
		const FString& ClampMinString = GetMetaDataFromKey("ClampMin");
		const FString& ClampMaxString = GetMetaDataFromKey("ClampMax");

		// If no UIMin/Max was specified then use the clamp string
		const FString& UIMinString = MetaUIMinString.Len() ? MetaUIMinString : ClampMinString;
		const FString& UIMaxString = MetaUIMaxString.Len() ? MetaUIMaxString : ClampMaxString;

		NumericType ClampMin = TNumericLimits<NumericType>::Lowest();
		NumericType ClampMax = TNumericLimits<NumericType>::Max();

		if (!ClampMinString.IsEmpty())
		{
			TTypeFromString<NumericType>::FromString(ClampMin, *ClampMinString);
		}

		if (!ClampMaxString.IsEmpty())
		{
			TTypeFromString<NumericType>::FromString(ClampMax, *ClampMaxString);
		}

		NumericType UIMin = TNumericLimits<NumericType>::Lowest();
		NumericType UIMax = TNumericLimits<NumericType>::Max();
		TTypeFromString<NumericType>::FromString(UIMin, *UIMinString);
		TTypeFromString<NumericType>::FromString(UIMax, *UIMaxString);

		NumericType SliderExponent = NumericType(1);
		if (SliderExponentString.Len())
		{
			TTypeFromString<NumericType>::FromString(SliderExponent, *SliderExponentString);
		}

		NumericType Delta = NumericType(0);
		if (DeltaString.Len())
		{
			TTypeFromString<NumericType>::FromString(Delta, *DeltaString);
		}

		if (ClampMin >= ClampMax && (ClampMinString.Len() || ClampMaxString.Len()))
		{
			UE_LOG(LogPropertyNode, Warning, TEXT("Clamp Min (%s) >= Clamp Max (%s) for Ranged Numeric property %s"), *ClampMinString, *ClampMaxString, *Property->GetPathName());
		}

		const NumericType ActualUIMin = FMath::Max(UIMin, ClampMin);
		const NumericType ActualUIMax = FMath::Min(UIMax, ClampMax);

		TOptional<NumericType> MinValue = ClampMinString.Len() ? ClampMin : TOptional<NumericType>();
		TOptional<NumericType> MaxValue = ClampMaxString.Len() ? ClampMax : TOptional<NumericType>();
		TOptional<NumericType> SliderMinValue = (UIMinString.Len()) ? ActualUIMin : TOptional<NumericType>();
		TOptional<NumericType> SliderMaxValue = (UIMaxString.Len()) ? ActualUIMax : TOptional<NumericType>();

		if ((ActualUIMin >= ActualUIMax) && (SliderMinValue.IsSet() && SliderMaxValue.IsSet()))
		{
			UE_LOG(LogPropertyNode, Warning, TEXT("UI Min (%s) >= UI Max (%s) for Ranged Numeric property %s"), *UIMinString, *UIMaxString, *Property->GetPathName());
		}

		FObjectPropertyNode* ObjectPropertyNode = PropertyNode->FindObjectItemParent();
		const bool bAllowSpin = (!ObjectPropertyNode || (1 == ObjectPropertyNode->GetNumObjects()))
			&& !PropertyNode->GetProperty()->GetBoolMetaData("NoSpinbox");

		// Set up the correct type interface if we want to display units on the property editor
		if (FUnitConversion::Settings().ShouldDisplayUnits())
		{
			const FString& Units = InPropertyEditor->GetProperty()->GetMetaData(TEXT("Units"));
			auto PropertyUnits = FUnitConversion::UnitFromString(*Units);

			if (PropertyUnits.IsSet())
			{
				// Create the type interface and set up the default input units if they are compatible
				TypeInterface = MakeShareable(new TNumericUnitTypeInterface<NumericType>(PropertyUnits.GetValue()));

				auto Value = OnGetValue();
				if (Value.IsSet())
				{
					TypeInterface->SetupFixedDisplay(Value.GetValue());
				}
			}
		}

		ChildSlot
			[
				SAssignNew(PrimaryWidget, SNumericEntryBox<NumericType>)
				// Only allow spinning if we have a single value
				.AllowSpin(bAllowSpin)
				.Value(this, &SPropertyEditorNumeric<NumericType>::OnGetValue)
				.Font(InArgs._Font)
				.MinValue(MinValue)
				.MaxValue(MaxValue)
				.MinSliderValue(SliderMinValue)
				.MaxSliderValue(SliderMaxValue)
				.SliderExponent(SliderExponent)
				.Delta(Delta)
				.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
				.OnValueChanged(this, &SPropertyEditorNumeric<NumericType>::OnValueChanged)
				.OnValueCommitted(this, &SPropertyEditorNumeric<NumericType>::OnValueCommitted)
				.OnBeginSliderMovement(this, &SPropertyEditorNumeric<NumericType>::OnBeginSliderMovement)
				.OnEndSliderMovement(this, &SPropertyEditorNumeric<NumericType>::OnEndSliderMovement)
				.TypeInterface(TypeInterface)
			];

		SetEnabled(TAttribute<bool>(this, &SPropertyEditorNumeric<NumericType>::CanEdit));
	}

	virtual bool SupportsKeyboardFocus() const override
	{
		return PrimaryWidget.IsValid() && PrimaryWidget->SupportsKeyboardFocus();
	}

	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override
	{
		// Forward keyboard focus to our editable text widget
		return FReply::Handled().SetUserFocus(PrimaryWidget.ToSharedRef(), InFocusEvent.GetCause());
	}
	
	void GetDesiredWidth( float& OutMinDesiredWidth, float& OutMaxDesiredWidth )
	{
		const UProperty* Property = PropertyEditor->GetProperty();
		const bool bIsNonEnumByte = (Property->IsA(UByteProperty::StaticClass()) && Cast<const UByteProperty>(Property)->Enum == NULL);

		if( bIsNonEnumByte )
		{
			OutMinDesiredWidth = 75.0f;
			OutMaxDesiredWidth = 75.0f;
		}
		else
		{
			OutMinDesiredWidth = 125.0f;
			OutMaxDesiredWidth = 125.0f;
		}
	}

	static bool Supports( const TSharedRef< FPropertyEditor >& PropertyEditor ) 
	{ 
		const TSharedRef< FPropertyNode > PropertyNode = PropertyEditor->GetPropertyNode();
		
		if (!PropertyNode->HasNodeFlags(EPropertyNodeFlags::EditInline))
		{
			return TTypeToProperty<NumericType>::Match(PropertyEditor->GetProperty());
		}
		
		return false;
	}

private:
	/**
	 *	Helper to provide mapping for a given UProperty object to a property editor type
	 */ 
	template<typename T, typename U = void> 
	struct TTypeToProperty 
	{ 
		static bool Match(const UProperty* InProperty) { return false; } 
	};
	
	template<typename U> 
	struct TTypeToProperty<float, U> 
	{	
		static bool Match(const UProperty* InProperty) { return InProperty->IsA(UFloatProperty::StaticClass()); } 
	};

	template<typename U>
	struct TTypeToProperty<int8, U>
	{
		static bool Match(const UProperty* InProperty) { return InProperty->IsA(UInt8Property::StaticClass()); }
	};

	template<typename U>
	struct TTypeToProperty<int16, U>
	{
		static bool Match(const UProperty* InProperty) { return InProperty->IsA(UInt16Property::StaticClass()); }
	};

	template<typename U>
	struct TTypeToProperty<int32, U>
	{
		static bool Match(const UProperty* InProperty) { return InProperty->IsA(UIntProperty::StaticClass()); }
	};

	template<typename U>
	struct TTypeToProperty<int64, U>
	{
		static bool Match(const UProperty* InProperty) { return InProperty->IsA(UInt64Property::StaticClass()); }
	};

	template<typename U>
	struct TTypeToProperty<uint8, U>
	{
		static bool Match(const UProperty* InProperty) { return (InProperty->IsA(UByteProperty::StaticClass()) && Cast<const UByteProperty>(InProperty)->Enum == NULL); }
	};

	template<typename U>
	struct TTypeToProperty<uint16, U>
	{
		static bool Match(const UProperty* InProperty) { return InProperty->IsA(UUInt16Property::StaticClass()); }
	};

	template<typename U>
	struct TTypeToProperty<uint32, U>
	{
		static bool Match(const UProperty* InProperty) { return InProperty->IsA(UUInt32Property::StaticClass()); }
	};

	template<typename U>
	struct TTypeToProperty<uint64, U>
	{
		static bool Match(const UProperty* InProperty) { return InProperty->IsA(UUInt64Property::StaticClass()); }
	};
		
	
	/** @return The value or unset if properties with multiple values are viewed */
	TOptional<NumericType> OnGetValue() const
	{
		NumericType NumericVal;
		const TSharedRef< IPropertyHandle > PropertyHandle = PropertyEditor->GetPropertyHandle();

		if (PropertyHandle->GetValue( NumericVal ) == FPropertyAccess::Success )
		{
			return NumericVal;
		}

		// Return an unset value so it displays the "multiple values" indicator instead
		return TOptional<NumericType>();
	}

	void OnValueChanged( NumericType NewValue )
	{
		if( bIsUsingSlider )
		{
			const TSharedRef< IPropertyHandle > PropertyHandle = PropertyEditor->GetPropertyHandle();

			NumericType OrgValue(0);
			if (PropertyHandle->GetValue(OrgValue) != FPropertyAccess::Fail)
			{
				// Value hasn't changed, so lets return now
				if (OrgValue == NewValue)
				{ 
					return;
				}
			}

			// We don't create a transaction for each property change when using the slider.  Only once when the slider first is moved
			EPropertyValueSetFlags::Type Flags = (EPropertyValueSetFlags::InteractiveChange | EPropertyValueSetFlags::NotTransactable);
			PropertyHandle->SetValue( NewValue, Flags );

			if (TypeInterface.IsValid() && !TypeInterface->FixedDisplayUnits.IsSet())
			{
				TypeInterface->SetupFixedDisplay(NewValue);
			}
		}
	}

	void OnValueCommitted( NumericType NewValue, ETextCommit::Type CommitInfo )
	{
		const TSharedRef< IPropertyHandle > PropertyHandle = PropertyEditor->GetPropertyHandle();
		PropertyHandle->SetValue( NewValue );

		if (TypeInterface.IsValid() && !TypeInterface->FixedDisplayUnits.IsSet())
		{
			TypeInterface->SetupFixedDisplay(NewValue);
		}
	}
	
	/**
	 * Called when the slider begins to move.  We create a transaction here to undo the property
	 */
	void OnBeginSliderMovement()
	{
		bIsUsingSlider = true;

		GEditor->BeginTransaction( TEXT("PropertyEditor"), FText::Format( NSLOCTEXT("PropertyEditor", "SetNumericPropertyTransaction", "Edit {0}"), PropertyEditor->GetDisplayName() ), PropertyEditor->GetPropertyHandle()->GetProperty() );
	}


	/**
	 * Called when the slider stops moving.  We end the previously created transaction
	 */
	void OnEndSliderMovement( NumericType NewValue )
	{
		bIsUsingSlider = false;

		GEditor->EndTransaction();
	}

	/** @return True if the property can be edited */
	bool CanEdit() const
	{
		return PropertyEditor.IsValid() ? !PropertyEditor->IsEditConst() : true;
	}

private:

	TSharedPtr<TNumericUnitTypeInterface<NumericType>> TypeInterface;	

	TSharedPtr< class FPropertyEditor > PropertyEditor;

	TSharedPtr< class SWidget > PrimaryWidget;

	/** True if the slider is being used to change the value of the property */
	bool bIsUsingSlider;
};

#undef LOCTEXT_NAMESPACE
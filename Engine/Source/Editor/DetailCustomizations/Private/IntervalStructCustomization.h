// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
* Implements a details panel customization for numeric TInterval structures.
*/
template <typename NumericType>
class FIntervalStructCustomization : public IPropertyTypeCustomization
{
public:

	FIntervalStructCustomization()
		: bIsUsingSlider(false) {}

public:

	// IPropertyTypeCustomization interface

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

public:

	/**
	* Creates a new instance.
	*
	* @return A new struct customization for Guids.
	*/
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

protected:

	/**
	 * Gets the value for the provided property handle
	 *
	 * @param	ValueWeakPtr	Handle to the property to get the value from
	 * @return	The value or unset if it could not be accessed
	 */
	TOptional<NumericType> OnGetValue(TWeakPtr<IPropertyHandle> ValueWeakPtr) const;

	/**
	 * Called when the value is committed from the property editor
	 *
	 * @param	NewValue		The new value of the property
	 * @param	CommitType		How the value was committed (unused)
	 * @param	HandleWeakPtr	Handle to the property that the new value is for
	 */
	void OnValueCommitted(NumericType NewValue, ETextCommit::Type CommitType, TWeakPtr<IPropertyHandle> HandleWeakPtr);

	/**
	 * Called when the value is changed in the property editor
	 *
	 * @param	NewValue		The new value of the property
	 * @param	HandleWeakPtr	Handle to the property that the new value is for
	 */
	void OnValueChanged(NumericType NewValue, TWeakPtr<IPropertyHandle> HandleWeakPtr);

	/**
	 * Called when a value starts to be changed by a slider
	 */
	void OnBeginSliderMovement();

	/**
	 * Called when a value stops being changed by a slider
	 *
	 * @param	NewValue		The new value of the property
	 */
	void OnEndSliderMovement(NumericType NewValue);

	/**
	 * Determines if the spin box is enabled on a numeric value widget
	 *
	 * @return Whether the spin box should be enabled.
	 */
	bool ShouldAllowSpin() const;

	// Cached shared pointers to properties that we are managing
	TSharedPtr<IPropertyHandle> MinValueHandle;
	TSharedPtr<IPropertyHandle> MaxValueHandle;

	// Min and max allowed values from the metadata
	TOptional<NumericType> MinAllowedValue;
	TOptional<NumericType> MaxAllowedValue;
	
	// Flags whether the slider is being moved at the moment on any of our widgets
	bool bIsUsingSlider;
};

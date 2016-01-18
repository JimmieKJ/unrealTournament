// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Base class for math struct customization (e.g, vector, rotator, color)                                                              
 */
class FMathStructCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	FMathStructCustomization()
		: bIsUsingSlider(false)
		, bPreserveScaleRatio(false)
	{ }

	virtual ~FMathStructCustomization() {}

	/** IPropertyTypeCustomization instance */
	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	
protected:
	/**
	 * Makes the header row for the customization
	 *
	 * @param StructPropertyHandle	Handle to the struct property
	 * @param Row	The header row to add widgets to
	 */
	virtual void MakeHeaderRow( TSharedRef<class IPropertyHandle>& StructPropertyHandle, FDetailWidgetRow& Row );

	/**
	 * Gets the sorted children for the struct
	 *
	 * @param StructPropertyHandle	The handle to the struct property
	 * @param OutChildren			The child array that should be populated in the order that children should be displayed
	 */
	virtual void GetSortedChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, TArray< TSharedRef<IPropertyHandle> >& OutChildren );

	/**
	 * Constructs a widget for the property handle 
	 *
	 * @param StructurePropertyHandle	handle of the struct property
	 * @param PropertyHandle	Child handle of the struct property
	 */
	virtual TSharedRef<SWidget> MakeChildWidget( 
		TSharedRef<IPropertyHandle>& StructurePropertyHandle,
		TSharedRef<IPropertyHandle>& PropertyHandle );

	/**
	 * Gets the value as a float for the provided property handle
	 *
	 * @param WeakHandlePtr	Handle to the property to get the value from
	 * @return The value or unset if it could not be accessed
	 */
	template<typename NumericType>
	TOptional<NumericType> OnGetValue( TWeakPtr<IPropertyHandle> WeakHandlePtr ) const;

	/**
	 * Called when the value is committed from the property editor
	 *
	 * @param NewValue		The new value of the property as a float
	 * @param CommitType	How the value was committed (unused)
	 * @param WeakHandlePtr	Handle to the property that the new value is for
	 */
	template<typename NumericType>
	void OnValueCommitted( NumericType NewValue, ETextCommit::Type CommitType, TWeakPtr<IPropertyHandle> WeakHandlePtr );
	
	/**
	 * Called to see if the value is enabled for editing
	 *
	 * @param WeakHandlePtr	Handle to the property that the new value is for
	 */
	bool IsValueEnabled( TWeakPtr<IPropertyHandle> WeakHandlePtr ) const;

	/**
	 * Called when the value is changed in the property editor
	 *
	 * @param NewValue		The new value of the property as a float
	 * @param WeakHandlePtr	Handle to the property that the new value is for
	 */
	template<typename NumericType>
	void OnValueChanged( NumericType NewValue, TWeakPtr<IPropertyHandle> WeakHandlePtr );

	/**
	 * Called to set the value of the property handle.
	 *
	 * @param NewValue		The new value of the property as a float
	 * @param Flags         The flags to pass when setting the value on the property handle.
	 * @param WeakHandlePtr	Handle to the property that the new value is for
	 */
	template<typename NumericType>
	void SetValue(NumericType NewValue, EPropertyValueSetFlags::Type Flags, TWeakPtr<IPropertyHandle> WeakHandlePtr);

private:
	/** Gets the brush to use for the lock icon. */
	const FSlateBrush* GetPreserveScaleRatioImage() const;

	/** Gets the checked value of the preserve scale ratio option */
	ECheckBoxState IsPreserveScaleRatioChecked() const;

	/** Called when the user toggles preserve ratio. */
	void OnPreserveScaleRatioToggled(ECheckBoxState NewState, TWeakPtr<IPropertyHandle> PropertyHandle);

private:
	/** Called when a value starts to be changed by a slider */
	void OnBeginSliderMovement();

	/** Called when a value stops being changed by a slider */
	template<typename NumericType>
	void OnEndSliderMovement( NumericType NewValue );

	template<typename NumericType>
	TSharedRef<SWidget> MakeNumericWidget(TSharedRef<IPropertyHandle>& StructurePropertyHandle, TSharedRef<IPropertyHandle>& PropertyHandle);

protected:
	/** All the sorted children of the struct that should be displayed */
	TArray< TSharedRef<IPropertyHandle> > SortedChildHandles;

	/** True if a value is being changed by dragging a slider */
	bool bIsUsingSlider;

	/** True if the ratio is locked when scaling occurs (uniform scaling) */
	bool bPreserveScaleRatio;
};

/**
 * Customizes FRotator structs                                                              
 */
class FRotatorStructCustomization : public FMathStructCustomization
{
public:
	/** @return A new instance of this class */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
private:
	/** FMathStructCustomization interface */
	virtual void GetSortedChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, TArray< TSharedRef<IPropertyHandle> >& OutChildren ) override;
};

/**
 * Base class for color struct customization (FColor,FLinearColor)                                                             
 */
class FColorStructCustomization : public FMathStructCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	
protected:
	FColorStructCustomization()
		: bIgnoreAlpha( false )
		, bIsInlineColorPickerVisible(false)
		, bIsInteractive(false)
		, bDontUpdateWhileEditing(false)
	{}

	/** FMathStructCustomization interface */
	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void MakeHeaderRow( TSharedRef<class IPropertyHandle>& InStructPropertyHandle, FDetailWidgetRow& Row ) override;
	virtual void GetSortedChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, TArray< TSharedRef<IPropertyHandle> >& OutChildren ) override;

	/** Creates the color widget that when clicked spawns the color picker window. */
	TSharedRef<SWidget> CreateColorWidget();

	/**
	 * Get the color used by this struct as a linear color value
	 * @param InColor To be filled with the color value used by this struct, or white if this struct is being used to edit multiple values  
	 * @return The result of trying to get the color value
	 */
	FPropertyAccess::Result GetColorAsLinear(FLinearColor& InColor) const;

	/**
	 * Does this struct have multiple values?
	 * @return EVisibility::Visible if it does, EVisibility::Collapsed otherwise
	 */
	EVisibility GetMultipleValuesTextVisibility() const;

	/**
	 * Creates a new color picker for interactively selecting the color
	 *
	 * @param bUseAlpha			If true alpha will be displayed, otherwise it is ignored
	 * @param bOnlyRefreshOnOk	If true the value of the property will only be refreshed when the user clicks OK in the color picker
	 */
	void CreateColorPicker( bool bUseAlpha, bool bOnlyRefreshOnOk );
	
	/** Creates a new color picker for interactively selecting color */
	TSharedRef<class SColorPicker> CreateInlineColorPicker();

	/**
	 * Called when the property is set from the color picker 
	 * 
	 * @param NewColor	The new color value
	 */
	void OnSetColorFromColorPicker( FLinearColor NewColor );
	
	/**
	 * Called when the user clicks cancel in the color picker
	 * The values are reset to their original state when this happens
	 *
	 * @param OriginalColor Original color of the property
	 */
	void OnColorPickerCancelled( FLinearColor OriginalColor );

	/**
	 * Called when the user enters an interactive color change (dragging something in the picker)
	 */
	void OnColorPickerInteractiveBegin();

	/**
	 * Called when the user completes an interactive color change (dragging something in the picker)
	 */
	void OnColorPickerInteractiveEnd();

	/**
	 * @return The color that should be displayed in the color block                                                              
	 */
	FLinearColor OnGetColorForColorBlock() const;
	
	/**
	 * Called when the user clicks in the color block (opens inline color picker)
	 */
	FReply OnMouseButtonDownColorBlock(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	
	/** Gets the visibility of the inline color picker */
	EVisibility GetInlineColorPickerVisibility() const;

	/** Called when the user clicks on the the button to get the full color picker */
	FReply OnOpenFullColorPickerClicked();

protected:
	/** Saved per struct colors in case the user clicks cancel in the color picker */
	TArray<FLinearColor> SavedPreColorPickerColors;
	/** Color struct handle */
	TSharedPtr<IPropertyHandle> StructPropertyHandle;
	/** True if the property is a linear color property */
	bool bIsLinearColor;
	/** True if the property wants to ignore the alpha component */
	bool bIgnoreAlpha;
	/** True if the inline color picker is visible */
	bool bIsInlineColorPickerVisible;
	/** True if the user is performing an interactive color change */
	bool bIsInteractive;
	/** Cached widget for the color picker to use as a parent */
	TSharedPtr<SWidget> ColorPickerParentWidget;
	/** The value won;t be updated while editing */
	bool bDontUpdateWhileEditing;
	/** Overrides the default state of the sRGB checkbox */
	TOptional<bool> sRGBOverride;
};


// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CheckBoxWidgetStyle.h"

#include "CheckBox.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam( FOnCheckBoxComponentStateChanged, bool, bIsChecked );

/**
 * The checkbox widget allows you to display a toggled state of 'unchecked', 'checked' and 
 * 'indeterminable.  You can use the checkbox for a classic checkbox, or as a toggle button,
 * or as radio buttons.
 * 
 * ● Single Child
 * ● Toggle
 */
UCLASS()
class UMG_API UCheckBox : public UContentWidget
{
	GENERATED_UCLASS_BODY()

public:
	/** Whether the check box is currently in a checked state */
	UPROPERTY(EditAnywhere, Category=Appearance)
	ECheckBoxState CheckedState;

	/** A bindable delegate for the IsChecked. */
	UPROPERTY()
	FGetCheckBoxState CheckedStateDelegate;

public:
	/** The checkbox bar style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style", meta=( DisplayName="Style" ))
	FCheckBoxStyle WidgetStyle;

	/** Style of the check box */
	UPROPERTY()
	USlateWidgetStyleAsset* Style_DEPRECATED;

	/** Image to use when the checkbox is unchecked */
	UPROPERTY()
	USlateBrushAsset* UncheckedImage_DEPRECATED;
	
	/** Image to use when the checkbox is unchecked and hovered */
	UPROPERTY()
	USlateBrushAsset* UncheckedHoveredImage_DEPRECATED;
	
	/** Image to use when the checkbox is unchecked and pressed */
	UPROPERTY()
	USlateBrushAsset* UncheckedPressedImage_DEPRECATED;
	
	/** Image to use when the checkbox is checked */
	UPROPERTY()
	USlateBrushAsset* CheckedImage_DEPRECATED;
	
	/** Image to use when the checkbox is checked and hovered */
	UPROPERTY()
	USlateBrushAsset* CheckedHoveredImage_DEPRECATED;
	
	/** Image to use when the checkbox is checked and pressed */
	UPROPERTY()
	USlateBrushAsset* CheckedPressedImage_DEPRECATED;
	
	/** Image to use when the checkbox is in an ambiguous state and hovered */
	UPROPERTY()
	USlateBrushAsset* UndeterminedImage_DEPRECATED;
	
	/** Image to use when the checkbox is checked and hovered */
	UPROPERTY()
	USlateBrushAsset* UndeterminedHoveredImage_DEPRECATED;
	
	/** Image to use when the checkbox is in an ambiguous state and pressed */
	UPROPERTY()
	USlateBrushAsset* UndeterminedPressedImage_DEPRECATED;

	/** How the content of the toggle button should align within the given space */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	/** Spacing between the check box image and its content */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FMargin Padding;

	/** The color of the background border */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FSlateColor BorderBackgroundColor;

public:

	/** Called when the checked state has changed */
	UPROPERTY(BlueprintAssignable, Category="CheckBox|Event")
	FOnCheckBoxComponentStateChanged OnCheckStateChanged;

public:

	/** Returns true if this button is currently pressed */
	UFUNCTION(BlueprintCallable, Category="Widget")
	bool IsPressed() const;
	
	/** Returns true if the checkbox is currently checked */
	UFUNCTION(BlueprintCallable, Category="Widget")
	bool IsChecked() const;

	/** @return the full current checked state. */
	UFUNCTION(BlueprintCallable, Category="Widget")
	ECheckBoxState GetCheckedState() const;

	/** Sets the checked state. */
	UFUNCTION(BlueprintCallable, Category="Widget")
	void SetIsChecked(bool InIsChecked);

	/** Sets the checked state. */
	UFUNCTION(BlueprintCallable, Category="Widget")
	void SetCheckedState(ECheckBoxState InCheckedState);

public:
	
	// UWidget interface
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

	// UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End of UVisual interface

	// Begin UObject interface
	virtual void PostLoad() override;
	// End of UObject interface

#if WITH_EDITOR
	virtual const FSlateBrush* GetEditorIcon() override;
	virtual const FText GetPaletteCategory() override;
#endif

protected:

	// UPanelWidget
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End UPanelWidget

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

	void SlateOnCheckStateChangedCallback(ECheckBoxState NewState);
	
protected:
	TSharedPtr<SCheckBox> MyCheckbox;
};

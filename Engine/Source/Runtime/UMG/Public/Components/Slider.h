// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slider.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMouseCaptureBeginEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMouseCaptureEndEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFloatValueChangedEvent, float, Value);

/**
 * A simple widget that shows a sliding bar with a handle that allows you to control the value between 0..1.
 *
 * ● No Children
 */
UCLASS()
class UMG_API USlider : public UWidget
{
	GENERATED_UCLASS_BODY()

public:
	/** The volume value to display. */
	UPROPERTY(EditAnywhere, Category=Appearance)
	float Value;

	/** A bindable delegate to allow logic to drive the value of the widget */
	UPROPERTY()
	FGetFloat ValueDelegate;

public:
	
	/** The progress bar style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style", meta=( DisplayName="Style" ))
	FSliderStyle WidgetStyle;

	/** The slider's orientation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	TEnumAsByte<EOrientation> Orientation;

	/** The color to draw the slider bar in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FLinearColor SliderBarColor;

	/** The color to draw the slider handle in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FLinearColor SliderHandleColor;

	/** Whether the slidable area should be indented to fit the handle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, AdvancedDisplay)
	bool IndentHandle;

	/** Whether the handle is interactive or fixed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, AdvancedDisplay)
	bool Locked;

public:

	/** Invoked when the mouse is pressed and a capture begins. */
	UPROPERTY(BlueprintAssignable, Category="Widget Event")
	FOnMouseCaptureBeginEvent OnMouseCaptureBegin;

	/** Invoked when the mouse is released and a capture ends. */
	UPROPERTY(BlueprintAssignable, Category="Widget Event")
	FOnMouseCaptureEndEvent OnMouseCaptureEnd;

	/** Called when the value is changed by slider or typing. */
	UPROPERTY(BlueprintAssignable, Category="Widget Event")
	FOnFloatValueChangedEvent OnValueChanged;

	/** Gets the current value of the slider. */
	UFUNCTION(BlueprintCallable, Category="Behavior")
	float GetValue() const;

	/** Sets the current value of the slider. */
	UFUNCTION(BlueprintCallable, Category="Behavior")
	void SetValue(float InValue);
	
	// UWidget interface
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

#if WITH_EDITOR
	virtual const FSlateBrush* GetEditorIcon() override;
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	/** Native Slate Widget */
	TSharedPtr<SSlider> MySlider;

	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

	void HandleOnValueChanged(float InValue);
	void HandleOnMouseCaptureBegin();
	void HandleOnMouseCaptureEnd();
};

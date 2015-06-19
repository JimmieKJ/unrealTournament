// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditableTextWidgetStyle.h"

#include "EditableText.generated.h"

/**
 * Editable text box widget
 */
UCLASS()
class UMG_API UEditableText : public UWidget
{
	GENERATED_UCLASS_BODY()

public:

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEditableTextChangedEvent, const FText&, Text);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEditableTextCommittedEvent, const FText&, Text, ETextCommit::Type, CommitMethod);

public:
	/** The text content for this editable text box widget */
	UPROPERTY(EditAnywhere, Category=Content)
	FText Text;

	/** A bindable delegate to allow logic to drive the text of the widget */
	UPROPERTY()
	FGetText TextDelegate;

public:

	/** Hint text that appears when there is no text in the text box */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Content)
	FText HintText;

	/** A bindable delegate to allow logic to drive the hint text of the widget */
	UPROPERTY()
	FGetText HintTextDelegate;

	/** The style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style", meta=( DisplayName="Style" ))
	FEditableTextStyle WidgetStyle;

	/** Text style */
	UPROPERTY()
	USlateWidgetStyleAsset* Style_DEPRECATED;

	/** Background image for the selected text (overrides Style) */
	UPROPERTY()
	USlateBrushAsset* BackgroundImageSelected_DEPRECATED;

	/** Background image for the selection targeting effect (overrides Style) */
	UPROPERTY()
	USlateBrushAsset* BackgroundImageSelectionTarget_DEPRECATED;

	/** Background image for the composing text (overrides Style) */
	UPROPERTY()
	USlateBrushAsset* BackgroundImageComposing_DEPRECATED;

	/** Image brush used for the caret (overrides Style) */
	UPROPERTY()
	USlateBrushAsset* CaretImage_DEPRECATED;

	/** Font color and opacity (overrides Style) */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FSlateFontInfo Font;

	/** Text color and opacity (overrides Style) */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FSlateColor ColorAndOpacity;

	/** Sets whether this text box can actually be modified interactively by the user */
	UPROPERTY(EditAnywhere, Category=Appearance)
	bool IsReadOnly;

	/** Sets whether this text box is for storing a password */
	UPROPERTY(EditAnywhere, Category=Appearance)
	bool IsPassword;

	/** Minimum width that a text block should be */
	UPROPERTY(EditAnywhere, Category=Appearance)
	float MinimumDesiredWidth;

	/** Workaround as we lose focus when the auto completion closes. */
	UPROPERTY(EditAnywhere, Category=Behavior, AdvancedDisplay)
	bool IsCaretMovedWhenGainFocus;

	/** Whether to select all text when the user clicks to give focus on the widget */
	UPROPERTY(EditAnywhere, Category=Behavior, AdvancedDisplay)
	bool SelectAllTextWhenFocused;

	/** Whether to allow the user to back out of changes when they press the escape key */
	UPROPERTY(EditAnywhere, Category=Behavior, AdvancedDisplay)
	bool RevertTextOnEscape;

	/** Whether to clear keyboard focus when pressing enter to commit changes */
	UPROPERTY(EditAnywhere, Category=Behavior, AdvancedDisplay)
	bool ClearKeyboardFocusOnCommit;

	/** Whether to select all text when pressing enter to commit changes */
	UPROPERTY(EditAnywhere, Category=Behavior, AdvancedDisplay)
	bool SelectAllTextOnCommit;

public:

	/** Called whenever the text is changed interactively by the user */
	UPROPERTY(BlueprintAssignable, Category="Widget Event", meta=(DisplayName="OnTextChanged (Editable Text)"))
	FOnEditableTextChangedEvent OnTextChanged;

	/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
	UPROPERTY(BlueprintAssignable, Category="Widget Event", meta=(DisplayName="OnTextCommitted (Editable Text)"))
	FOnEditableTextCommittedEvent OnTextCommitted;

	/**
	 * Gets the widget text
	 * @return The widget text
	 */
	UFUNCTION(BlueprintCallable, Category = "Widget", meta=(DisplayName="GetText (Editable Text)"))
	FText GetText() const;

	/**
	 * Directly sets the widget text.
	 * Warning: This will wipe any binding created for the Text property!
	 * @param InText The text to assign to the widget
	 */
	UFUNCTION(BlueprintCallable, Category = "Widget", meta=(DisplayName="SetText (Editable Text)"))
	void SetText(FText InText);

	UFUNCTION(BlueprintCallable, Category = "Widget")
	void SetIsPassword(bool InbIsPassword);

	UFUNCTION(BlueprintCallable, Category = "Widget")
	void SetHintText(FText InHintText);

	UFUNCTION(BlueprintCallable, Category = "Widget")
	void SetIsReadOnly(bool InbIsReadyOnly);

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
	// End of UWidget interface

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget

	void HandleOnTextChanged(const FText& Text);
	void HandleOnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

protected:
	TSharedPtr<SEditableText> MyEditableText;
};

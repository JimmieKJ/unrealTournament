// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditableTextBox.generated.h"

/**
 * Allows the user to type in custom text.  Only permits a single line of text to be entered.
 * 
 * ● No Children
 * ● Text Entry
 */
UCLASS(meta=(DisplayName="Text Box"))
class UMG_API UEditableTextBox : public UWidget
{
	GENERATED_UCLASS_BODY()

public:

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEditableTextBoxChangedEvent, const FText&, Text);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEditableTextBoxCommittedEvent, const FText&, Text, ETextCommit::Type, CommitMethod);

public:
	/** The text content for this editable text box widget */
	UPROPERTY(EditAnywhere, Category=Content)
	FText Text;

	/** A bindable delegate to allow logic to drive the text of the widget */
	UPROPERTY()
	FGetText TextDelegate;

public:

	/** The style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style", meta=( DisplayName="Style" ))
	FEditableTextBoxStyle WidgetStyle;

	/** Style used for the text box */
	UPROPERTY()
	USlateWidgetStyleAsset* Style_DEPRECATED;

	/** Hint text that appears when there is no text in the text box */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Content)
	FText HintText;

	/** A bindable delegate to allow logic to drive the hint text of the widget */
	UPROPERTY()
	FGetText HintTextDelegate;

	/** Font color and opacity (overrides Style) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FSlateFontInfo Font;

	/** Text color and opacity (overrides Style) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FLinearColor ForegroundColor;

	/** The color of the background/border around the editable text (overrides Style) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FLinearColor BackgroundColor;

	/** Text color and opacity when read-only (overrides Style) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FLinearColor ReadOnlyForegroundColor;

	/** Sets whether this text box can actually be modified interactively by the user */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	bool IsReadOnly;

	/** Sets whether this text box is for storing a password */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	bool IsPassword;

	/** Minimum width that a text block should be */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	float MinimumDesiredWidth;

	/** Padding between the box/border and the text widget inside (overrides Style) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FMargin Padding;

	/** Workaround as we lose focus when the auto completion closes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Behavior, AdvancedDisplay)
	bool IsCaretMovedWhenGainFocus;

	/** Whether to select all text when the user clicks to give focus on the widget */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Behavior, AdvancedDisplay)
	bool SelectAllTextWhenFocused;

	/** Whether to allow the user to back out of changes when they press the escape key */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Behavior, AdvancedDisplay)
	bool RevertTextOnEscape;

	/** Whether to clear keyboard focus when pressing enter to commit changes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Behavior, AdvancedDisplay)
	bool ClearKeyboardFocusOnCommit;

	/** Whether to select all text when pressing enter to commit changes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Behavior, AdvancedDisplay)
	bool SelectAllTextOnCommit;

public:

	/** Called whenever the text is changed interactively by the user */
	UPROPERTY(BlueprintAssignable, Category="TextBox|Event")
	FOnEditableTextBoxChangedEvent OnTextChanged;

	/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
	UPROPERTY(BlueprintAssignable, Category="TextBox|Event")
	FOnEditableTextBoxCommittedEvent OnTextCommitted;

	/** Provide a alternative mechanism for error reporting. */
	//SLATE_ARGUMENT(TSharedPtr<class IErrorReportingWidget>, ErrorReporting)

public:

	/**  */
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="GetText (Text Box)"))
	FText GetText() const;

	/**  */
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="SetText (Text Box)"))
	void SetText(FText InText);

	UFUNCTION(BlueprintCallable, Category="Widget",  meta=(DisplayName="SetError (Text Box)"))
	void SetError(FText InError);

	UFUNCTION(BlueprintCallable, Category="Widget")
	void ClearError();

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
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget

	void HandleOnTextChanged(const FText& Text);
	void HandleOnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

protected:
	TSharedPtr<SEditableTextBox> MyEditableTextBlock;
};

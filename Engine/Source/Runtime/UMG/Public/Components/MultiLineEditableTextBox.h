// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MultiLineEditableTextBox.generated.h"

/**
 * Allows a user to enter multiple lines of text
 */
UCLASS(meta=(DisplayName="Text Box (Multi-Line)"))
class UMG_API UMultiLineEditableTextBox : public UWidget
{
	GENERATED_UCLASS_BODY()

public:

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMultiLineEditableTextBoxChangedEvent, const FText&, Text);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMultiLineEditableTextBoxCommittedEvent, const FText&, Text, ETextCommit::Type, CommitMethod);

public:
	
	/** The text content for this editable text box widget */
	UPROPERTY(EditAnywhere, Category=Content, meta=(MultiLine="true"))
	FText Text;

	/** Hint text that appears when there is no text in the text box */
	UPROPERTY(EditAnywhere, Category=Content, meta=(MultiLine="true"))
	FText HintText;

	/** A bindable delegate to allow logic to drive the hint text of the widget */
	UPROPERTY()
	FGetText HintTextDelegate;
public:

	/** The style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style", meta=( DisplayName="Style" ))
	FEditableTextBoxStyle WidgetStyle;

	/** The text style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style", meta=( DisplayName="Text Style" ))
	FTextBlockStyle TextStyle;

	UPROPERTY()
	USlateWidgetStyleAsset* Style_DEPRECATED;

	/** The justification of the text in the multilinebox */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Content)
	TEnumAsByte<ETextJustify::Type> Justification;

	/** Whether to wrap text automatically based on the widget's computed horizontal space.*/
	UPROPERTY(EditAnywhere, Category=Content)
	bool bAutoWrapText;

	/** Whether text wraps onto a new line when it's length exceeds this width; if this value is zero or negative, no wrapping occurs. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Content)
	float WrapTextAt;

	/** Font color and opacity (overrides Style) */
	UPROPERTY()
	FSlateFontInfo Font_DEPRECATED;

	/** Text color and opacity (overrides Style) */
	UPROPERTY()
	FLinearColor ForegroundColor_DEPRECATED;

	/** The color of the background/border around the editable text (overrides Style) */
	UPROPERTY()
	FLinearColor BackgroundColor_DEPRECATED;

	/** Text color and opacity when read-only (overrides Style) */
	UPROPERTY()
	FLinearColor ReadOnlyForegroundColor_DEPRECATED;

	/** Called whenever the text is changed interactively by the user */
	UPROPERTY(BlueprintAssignable, Category="Widget Event", meta=(DisplayName="OnTextChanged (Multi-Line Text Box)"))
	FOnMultiLineEditableTextBoxChangedEvent OnTextChanged;

	/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
	UPROPERTY(BlueprintAssignable, Category="Widget Event", meta=(DisplayName="OnTextCommitted (Multi-Line Text Box)"))
	FOnMultiLineEditableTextBoxCommittedEvent OnTextCommitted;

	/** Provide a alternative mechanism for error reporting. */
	//SLATE_ARGUMENT(TSharedPtr<class IErrorReportingWidget>, ErrorReporting)

public:

	/**  */
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="GetText (Multi-Line Text Box)"))
	FText GetText() const;

	/**  */
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="SetText (Multi-Line Text Box)"))
	void SetText(FText InText);

	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="SetError (Multi-Line Text Box)"))
	void SetError(FText InError);

	//TODO UMG Add Set ReadOnlyForegroundColor
	//TODO UMG Add Set BackgroundColor
	//TODO UMG Add Set ForegroundColor
	//TODO UMG Add Set Font
	//TODO UMG Add Set Justification

public:

	//~ Begin UWidget Interface
	virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

#if WITH_EDITOR
	virtual const FSlateBrush* GetEditorIcon() override;
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget

	void HandleOnTextChanged(const FText& Text);
	void HandleOnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

protected:
	TSharedPtr<SMultiLineEditableTextBox> MyEditableTextBlock;
};

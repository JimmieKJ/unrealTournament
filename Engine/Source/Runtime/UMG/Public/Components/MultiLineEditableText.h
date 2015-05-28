// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MultiLineEditableText.generated.h"

/**
 * Editable text box widget
 */
UCLASS(meta=( DisplayName="Editable Text (Multi-Line)" ))
class UMG_API UMultiLineEditableText : public UWidget
{
	GENERATED_UCLASS_BODY()

public:

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMultiLineEditableTextChangedEvent, const FText&, Text);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMultiLineEditableTextCommittedEvent, const FText&, Text, ETextCommit::Type, CommitMethod);

public:
	/** The text content for this editable text box widget */
	UPROPERTY(EditAnywhere, Category=Content, meta=(MultiLine="true"))
	FText Text;

public:

	/** The style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style", meta=( DisplayName="Style" ))
	FTextBlockStyle WidgetStyle;

	/** The justification of the text in the multilinebox */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Content)
	TEnumAsByte<ETextJustify::Type> Justification;

	/** Whether to wrap text automatically based on the widget's computed horizontal space.*/
	UPROPERTY(EditAnywhere, Category=Content)
	bool bAutoWrapText;

	/** Whether text wraps onto a new line when it's length exceeds this width; if this value is zero or negative, no wrapping occurs. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Content)
	float WrapTextAt;

	/** Font color and opacity (overrides Style) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	FSlateFontInfo Font;

	/** Called whenever the text is changed interactively by the user */
	UPROPERTY(BlueprintAssignable, Category="Widget Event", meta=(DisplayName="OnTextChanged (Multi-Line Editable Text)"))
	FOnMultiLineEditableTextChangedEvent OnTextChanged;

	/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
	UPROPERTY(BlueprintAssignable, Category="Widget Event", meta=(DisplayName="OnTextCommitted (Multi-Line Editable Text)"))
	FOnMultiLineEditableTextCommittedEvent OnTextCommitted;

public:

	/**  */
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="GetText (Multi-Line Editable Text)"))
	FText GetText() const;

	/**  */
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="SetText (Multi-Line Editable Text)"))
	void SetText(FText InText);
	
	// UWidget interface
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

	// UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End of UVisual interface

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
	TSharedPtr<SMultiLineEditableText> MyMultiLineEditableText;
};

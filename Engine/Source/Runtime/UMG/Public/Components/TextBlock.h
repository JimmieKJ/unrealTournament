// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TextBlock.generated.h"

/**
 * A simple static text widget.
 *
 * ● No Children
 * ● Text
 */
UCLASS(meta=(DisplayName="Text"))
class UMG_API UTextBlock : public UWidget
{
	GENERATED_UCLASS_BODY()

public:
	/**  
	 * Sets the color and opacity of the text in this text block
	 *
	 * @param InColorAndOpacity		The new text color and opacity
	 */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetColorAndOpacity(FSlateColor InColorAndOpacity);

	/**  
	 * Sets the color and opacity of the text drop shadow
	 * Note: if opacity is zero no shadow will be drawn
	 *
	 * @param InShadowColorAndOpacity		The new drop shadow color and opacity
	 */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetShadowColorAndOpacity(FLinearColor InShadowColorAndOpacity);

	/**  
	 * Sets the offset that the text drop shadow should be drawn at
	 *
	 * @param InShadowOffset		The new offset
	 */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetShadowOffset(FVector2D InShadowOffset);

	/**
	 *  Set the text justification for this text block
	 *
	 *  @param Justification new justification
	 */
	UFUNCTION( BlueprintCallable, Category = "Appearance" )
	void SetJustification( ETextJustify::Type InJustification );

public:
	UPROPERTY()
	USlateWidgetStyleAsset* Style_DEPRECATED;

	/** The text to display */
	UPROPERTY(EditAnywhere, Category=Content, meta=( MultiLine="true" ))
	FText Text;
	
	/** A bindable delegate to allow logic to drive the text of the widget */
	UPROPERTY()
	FGetText TextDelegate;

	/** The color of the text */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FSlateColor ColorAndOpacity;

	/** A bindable delegate for the ColorAndOpacity. */
	UPROPERTY()
	FGetSlateColor ColorAndOpacityDelegate;
	
	/** The font to render the text with */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FSlateFontInfo Font;

	/** The direction the shadow is cast */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FVector2D ShadowOffset;

	/** The color of the shadow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, meta=( DisplayName="Shadow Color" ))
	FLinearColor ShadowColorAndOpacity;

	/** A bindable delegate for the ShadowColorAndOpacity. */
	UPROPERTY()
	FGetLinearColor ShadowColorAndOpacityDelegate;

	/** How the text should be aligned with the margin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	TEnumAsByte<ETextJustify::Type> Justification;

	/** True if we're wrapping text automatically based on the computed horizontal space for this widget */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	bool AutoWrapText;

	/** Whether text wraps onto a new line when it's length exceeds this width; if this value is zero or negative, no wrapping occurs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, AdvancedDisplay)
	float WrapTextAt;

	/** The minimum desired size for the text */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, AdvancedDisplay)
	float MinDesiredWidth;

	/** The amount of blank space left around the edges of text area. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, AdvancedDisplay)
	FMargin Margin;

	/** The amount to scale each lines height by. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, AdvancedDisplay)
	float LineHeightPercentage;

	///** Called when this text is double clicked */
	//SLATE_EVENT(FOnClicked, OnDoubleClicked)

	/** 
	 * Gets the widget text
	 * @return The widget text
	 */
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="GetText (Text)"))
	FText GetText() const;

	/**
	 * Directly sets the widget text.
	 * Warning: This will wipe any binding created for the Text property!
	 * @param InText The text to assign to the widget
	 */
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="SetText (Text)"))
	void SetText(FText InText);

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
	// UWidget interface
	virtual const FSlateBrush* GetEditorIcon() override;
	virtual const FText GetPaletteCategory() override;
	virtual void OnCreationFromPalette() override;
	// End UWidget interface

	virtual FString GetLabelMetadata() const override;

	void HandleTextCommitted(const FText& InText, ETextCommit::Type CommitteType);
#endif

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void OnBindingChanged(const FName& Property) override;
	// End of UWidget interface

protected:

	TSharedPtr<STextBlock> MyTextBlock;
};

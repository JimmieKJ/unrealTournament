// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScrollBox.generated.h"

/**
 * An arbitrary scrollable collection of widgets.  Great for presenting 10-100 widgets in a list.  Doesn't support virtualization.
 */
UCLASS()
class UMG_API UScrollBox : public UPanelWidget
{
	GENERATED_UCLASS_BODY()

public:

	/** The style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style", meta=( DisplayName="Style" ))
	FScrollBoxStyle WidgetStyle;

	/** The bar style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style", meta=( DisplayName="Bar Style" ))
	FScrollBarStyle WidgetBarStyle;

	UPROPERTY()
	USlateWidgetStyleAsset* Style_DEPRECATED;

	UPROPERTY()
	USlateWidgetStyleAsset* BarStyle_DEPRECATED;

	/** The orientation of the scrolling and stacking in the box. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scroll")
	TEnumAsByte<EOrientation> Orientation;

	/** Visibility */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scroll")
	TEnumAsByte<ESlateVisibility> ScrollBarVisibility;

	/**  Enable to always consume mouse wheel event, even when scrolling is not possible */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scroll")
	TEnumAsByte<EConsumeMouseWheel> ConsumeMouseWheel;

	/**  */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scroll")
	FVector2D ScrollbarThickness;

	/**  */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scroll")
	bool AlwaysShowScrollbar;
	
	//TODO UMG Add SetOrientation
	//TODO UMG Add SetScrollBarVisibility
	//TODO UMG Add SetScrollbarThickness
	//TODO UMG Add SetAlwaysShowScrollbar

public:

	/**
	 * Updates the scroll offset of the scrollbox.
	 * @param NewScrollOffset is in Slate Units.
	 */
	UFUNCTION(BlueprintCallable, Category="Widget")
	void SetScrollOffset(float NewScrollOffset);
	
	/**
	 * Gets the scroll offset of the scrollbox in Slate Units.
	 */
	UFUNCTION(BlueprintCallable, Category="Widget")
	float GetScrollOffset() const;

	/** Scrolls the ScrollBox to the top instantly */
	UFUNCTION(BlueprintCallable, Category="Widget")
	void ScrollToStart();

	/** Scrolls the ScrollBox to the bottom instantly during the next layout pass. */
	UFUNCTION(BlueprintCallable, Category="Widget")
	void ScrollToEnd();

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
	// End UWidget interface
#endif

protected:

	// UPanelWidget
	virtual UClass* GetSlotClass() const override;
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End UPanelWidget

protected:
	/** The desired scroll offset for the underlying scrollbox.  This is a cache so that it can be set before the widget is constructed. */
	float DesiredScrollOffset;

	TSharedPtr<class SScrollBox> MyScrollBox;

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface
};

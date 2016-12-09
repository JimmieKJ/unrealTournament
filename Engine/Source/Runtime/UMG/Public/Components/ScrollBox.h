// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Components/PanelWidget.h"
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
	ESlateVisibility ScrollBarVisibility;

	/**  Enable to always consume mouse wheel event, even when scrolling is not possible */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scroll")
	EConsumeMouseWheel ConsumeMouseWheel;

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

	/** Scrolls the ScrollBox to the widget during the next layout pass. */
	UFUNCTION(BlueprintCallable, Category="Widget")
	void ScrollWidgetIntoView(UWidget* WidgetToFind, bool AnimateScroll = true);

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
	//~ Begin UWidget Interface
	virtual const FText GetPaletteCategory() override;
	virtual void OnDescendantSelected( UWidget* DescendantWidget ) override;
	virtual void OnDescendantDeselected( UWidget* DescendantWidget ) override;
	//~ End UWidget Interface
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
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget Interface

#if WITH_EDITOR
	FDelegateHandle TickHandle;
#endif //WITH_EDITOR
};

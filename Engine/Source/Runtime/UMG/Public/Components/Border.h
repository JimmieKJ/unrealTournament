// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Border.generated.h"

class USlateBrushAsset;

/**
 * A border is a container widget that can contain one child widget, providing an opportunity 
 * to surround it with a background image and adjustable padding.
 *
 * ● Single Child
 * ● Image
 */
UCLASS()
class UMG_API UBorder : public UContentWidget
{
	GENERATED_UCLASS_BODY()

public:

	/** Color and opacity multiplier of content in the border */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Content")
	FLinearColor ContentColorAndOpacity;

	/** A bindable delegate for the ContentColorAndOpacity. */
	UPROPERTY()
	FGetLinearColor ContentColorAndOpacityDelegate;

	/** The padding area between the slot and the content it contains. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Content")
	FMargin Padding;

	/** The alignment of the content horizontally. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Content")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	/** The alignment of the content vertically. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Content")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment;

	/** Brush to drag as the background */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, meta=( DisplayName="Brush" ))
	FSlateBrush Background;

	/** A bindable delegate for the Brush. */
	UPROPERTY()
	FGetSlateBrush BackgroundDelegate;

	/** Color and opacity of the actual border image */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FLinearColor BrushColor;

	/** A bindable delegate for the BrushColor. */
	UPROPERTY()
	FGetLinearColor BrushColorDelegate;

	/** Whether or not to show the disabled effect when this border is disabled */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, AdvancedDisplay)
	bool bShowEffectWhenDisabled;

public:

	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnPointerEvent OnMouseButtonDownEvent;

	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnPointerEvent OnMouseButtonUpEvent;

	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnPointerEvent OnMouseMoveEvent;

	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnPointerEvent OnMouseDoubleClickEvent;

public:

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetContentColorAndOpacity(FLinearColor InContentColorAndOpacity);

	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetPadding(FMargin InPadding);

	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetBrushColor(FLinearColor InBrushColor);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetBrush(const FSlateBrush& InBrush);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetBrushFromAsset(USlateBrushAsset* Asset);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetBrushFromTexture(UTexture2D* Texture);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetBrushFromMaterial(UMaterialInterface* Material);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	UMaterialInstanceDynamic* GetDynamicMaterial();

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
	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface

	virtual const FSlateBrush* GetEditorIcon() override;
	virtual const FText GetPaletteCategory() override;
#endif

protected:

	// UPanelWidget
	virtual UClass* GetSlotClass() const override;
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End UPanelWidget

protected:
	TSharedPtr<SBorder> MyBorder;

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

	FReply HandleMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent);
	FReply HandleMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& MouseEvent);
	FReply HandleMouseMove(const FGeometry& Geometry, const FPointerEvent& MouseEvent);
	FReply HandleMouseDoubleClick(const FGeometry& Geometry, const FPointerEvent& MouseEvent);

	/** Translates the bound brush data and assigns it to the cached brush used by this widget. */
	const FSlateBrush* ConvertImage(TAttribute<FSlateBrush> InImageAsset) const;

protected:
	/** Image to use for the border */
	UPROPERTY()
	USlateBrushAsset* Brush_DEPRECATED;
};

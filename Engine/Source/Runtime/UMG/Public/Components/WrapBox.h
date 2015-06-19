// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WrapBox.generated.h"

/**
 * Arranges widgets left-to-right.  When the widgets exceed the Width it will place widgets on the next line.
 * 
 * ● Many Children
 * ● Flows
 * ● Wraps
 */
UCLASS()
class UMG_API UWrapBox : public UPanelWidget
{
	GENERATED_UCLASS_BODY()

public:
	/** The inner slot padding goes between slots sharing borders */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Content Layout")
	FVector2D InnerSlotPadding;

	/** Sets the inner slot padding goes between slots sharing borders */
	UFUNCTION(BlueprintCallable, Category="Content Layout")
	void SetInnerSlotPadding(FVector2D InPadding);

public:

	/**  */
	UFUNCTION(BlueprintCallable, Category="Panel")
	UWrapBoxSlot* AddChildWrapBox(UWidget* Content);

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

	// UWidget interface
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

	// UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End of UVisual interface

protected:

	TSharedPtr<class SWrapBox> MyWrapBox;

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface
};

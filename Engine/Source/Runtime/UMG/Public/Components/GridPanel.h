// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GridPanel.generated.h"

/**
 * A panel that evenly divides up available space between all of its children.
 * 
 * ● Many Children
 */
UCLASS()
class UMG_API UGridPanel : public UPanelWidget
{
	GENERATED_UCLASS_BODY()

public:

	/** The column fill rules */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fill Rules")
	TArray<float> ColumnFill;

	/** The row fill rules */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fill Rules")
	TArray<float> RowFill;

public:

	/**  */
	UFUNCTION(BlueprintCallable, Category="Widget")
	UGridSlot* AddChildToGrid(UWidget* Content);

public:

	// UWidget interface
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
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

	TSharedPtr<SGridPanel> MyGridPanel;

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface
};

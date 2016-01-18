// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RetainerBox.generated.h"

/**
 * Invalidate
 * * Single Child
 * * Caching / Performance
 */
UCLASS()
class UMG_API URetainerBox : public UContentWidget
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Phasing", meta=( UIMin=0, ClampMin=0 ))
	int32 Phase;

	UPROPERTY(EditAnywhere, Category="Phasing", meta=( UIMin=1, ClampMin=1 ))
	int32 PhaseCount;

public:

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FSlateBrush* GetEditorIcon() override;
	virtual const FText GetPaletteCategory() override;
#endif

protected:

	// UPanelWidget
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End UPanelWidget

	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

protected:

protected:
	TSharedPtr<class SRetainerWidget> MyRetainerWidget;
};

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ContentWidget.h"
#include "NamedSlot.generated.h"

/**
 * Allows you to expose an external slot for your user widget.  When others reuse your user control, 
 * they can put whatever they want in this named slot.
 */
UCLASS()
class UMG_API UNamedSlot : public UContentWidget
{
	GENERATED_UCLASS_BODY()

public:

	// UPanelWidget interface
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End of UPanelWidget interface

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
	// End of UWidget interface

protected:
	TSharedPtr<SBox> MyBox;
};

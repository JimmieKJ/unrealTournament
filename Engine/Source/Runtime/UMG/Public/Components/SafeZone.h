// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SafeZone.generated.h"

UCLASS()
class UMG_API USafeZone : public UContentWidget
{
	GENERATED_BODY()
public:
	USafeZone();

#if WITH_EDITOR
	virtual const FSlateBrush* GetEditorIcon() override;
	virtual const FText GetPaletteCategory() override;
#endif

	virtual void OnSlotAdded( UPanelSlot* Slot ) override;
	virtual void OnSlotRemoved( UPanelSlot* Slot ) override;
	virtual UClass* GetSlotClass() const override;

	void UpdateWidgetProperties();

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	TSharedPtr< class SSafeZone > MySafeZone;
};
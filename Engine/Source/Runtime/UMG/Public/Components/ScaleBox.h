// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScaleBox.generated.h"

/**
 * Allows you to place content with a desired size and have it scale to meet the constraints placed on this box's alloted area.  If
 * you needed to have a background image scale to fill an area but not become distorted with different aspect ratios, or if you need
 * to auto fit some text to an area, this is the control for you.
 *
 * * Single Child
 * * Aspect Ratio
 */
UCLASS()
class UMG_API UScaleBox : public UContentWidget
{
	GENERATED_UCLASS_BODY()

public:

	/** The stretching rule to apply when content is stretched */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stretching")
	TEnumAsByte<EStretch::Type> Stretch;

	/** Controls in what direction content can be scaled */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stretching")
	TEnumAsByte<EStretchDirection::Type> StretchDirection;

	/** Optional scale that can be specified by the User. Used only for UserSpecified stretching. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stretching")
	float UserSpecifiedScale;

	/** Optional bool to ignore the inherited scale. Applies inverse scaling to counteract parents before applying the local scale operation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stretching")
	bool IgnoreInheritedScale;

public:
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetStretch(EStretch::Type InStretch);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetStretchDirection(EStretchDirection::Type InStretchDirection);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetUserSpecifiedScale(float InUserSpecifiedScale);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetIgnoreInheritedScale(bool bInIgnoreInheritedScale);
public:

	// UWidget interface
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
	virtual bool CanEditChange(const UProperty* InProperty) const override;
#endif

protected:

	// UPanelWidget
	virtual UClass* GetSlotClass() const override;
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End UPanelWidget

protected:
	TSharedPtr<SScaleBox> MyScaleBox;

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface
};

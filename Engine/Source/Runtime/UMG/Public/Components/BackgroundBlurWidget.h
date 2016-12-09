// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ContentWidget.h"
#include "BackgroundBlurWidget.generated.h"

UCLASS()
class UMG_API UBackgroundBlurWidget : public UContentWidget
{
	GENERATED_UCLASS_BODY()

public:
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetBlurRadius(int32 InBlurRadius);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetBlurStrength(float InStrength);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetLowQualityFallbackBrush(const FSlateBrush& InBrush);

protected:
	/** UWidget interface */
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;

	/** UPanelWidget interface */
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;

public:
	/**
	* This is the strength of the blur.  It is heavily dependent on the BlurRadius value.  The ultimate strength of the blur is determined by this value and the BlurRadius value
	* A small blur radius and a large strength will quickly darken the entire blurred area and result in less overall blurriness that can be achieved.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, meta=(ClampMin=0,ClampMax=100))
	float BlurStrength;

	/**
	 * Whether or not the radius should be computed automatically or if it should use the radius  
	 */
	UPROPERTY()
	bool bOverrideAutoRadiusCalculation;

	/**
	 * This is the number of pixels which will be weighted in each direction from any given pixel when computing the blur
	 * A larger value is more costly but allows for stronger blurs.  
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Appearance, meta=(ClampMin=0, ClampMax=255, EditCondition="bOverrideAutoRadiusCalculation"))
	int32 BlurRadius;

	/**
	 * An image to draw instead of applying a blur when low quality override mode is enabled. 
	 * You can enable low quality mode for background blurs by setting the cvar Slate.ForceBackgroundBlurLowQualityOverride to 1. 
	 * This is usually done in the project's scalability settings
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Appearance)
	FSlateBrush LowQualityFallbackBrush;

protected:
	TSharedPtr<class SBackgroundBlurWidget> MyWidget;
};

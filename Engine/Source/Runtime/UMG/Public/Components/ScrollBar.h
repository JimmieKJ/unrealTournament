// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScrollBar.generated.h"

/** */
UCLASS(Experimental)
class UMG_API UScrollBar : public UWidget
{
	GENERATED_UCLASS_BODY()

public:

	/** Style of the scrollbar */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style", meta=( DisplayName="Style" ))
	FScrollBarStyle WidgetStyle;

	UPROPERTY()
	USlateWidgetStyleAsset* Style_DEPRECATED;

	/**  */
	UPROPERTY(EditAnywhere, Category="Behavior")
	bool bAlwaysShowScrollbar;

	/**  */
	UPROPERTY(EditAnywhere, Category="Behavior")
	TEnumAsByte<EOrientation> Orientation;

	/** The thickness of the scrollbar thumb */
	UPROPERTY(EditAnywhere, Category="Behavior")
	FVector2D Thickness;

public:

	/**
	* Set the offset and size of the track's thumb.
	* Note that the maximum offset is 1.0-ThumbSizeFraction.
	* If the user can view 1/3 of the items in a single page, the maximum offset will be ~0.667f
	*
	* @param InOffsetFraction     Offset of the thumbnail from the top as a fraction of the total available scroll space.
	* @param InThumbSizeFraction  Size of thumbnail as a fraction of the total available scroll space.
	*/
	UFUNCTION(BlueprintCallable, Category="Scrolling")
	void SetState(float InOffsetFraction, float InThumbSizeFraction);

	///** @return true if scrolling is possible; false if the view is big enough to fit all the content */
	//bool IsNeeded() const;

	///** @return normalized distance from top */
	//float DistanceFromTop() const;

	///** @return normalized distance from bottom */
	//float DistanceFromBottom() const;

	///** @return the scrollbar's visibility as a product of internal rules and user-specified visibility */
	//EVisibility ShouldBeVisible() const;

	///** @return True if the user is scrolling by dragging the scroll bar thumb. */
	//bool IsScrolling() const;

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

	TSharedPtr<class SScrollBar> MyScrollBar;

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface
};

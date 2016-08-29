// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SScaleBox.generated.h"

UENUM(BlueprintType)
namespace EStretchDirection
{
	enum Type
	{
		/** Will scale the content up or down. */
		Both,
		/** Will only make the content smaller, will never scale it larger than the content's desired size. */
		DownOnly,
		/** Will only make the content larger, will never scale it smaller than the content's desired size. */
		UpOnly
	};
}

UENUM(BlueprintType)
namespace EStretch
{
	enum Type
	{
		/** Does not scale the content. */
		None,
		/** Scales the content non-uniformly filling the entire space of the area. */
		Fill,
		/**
		 * Scales the content uniformly (preserving aspect ratio) 
		 * until it can no longer scale the content without clipping it.
		 */
		ScaleToFit,
		/**
		 * Scales the content uniformly (preserving aspect ratio) 
		 * until it can no longer scale the content without clipping it along the x-axis, 
		 * the y-axis can/will be clipped.
		 */
		ScaleToFitX,
		/**
		 * Scales the content uniformly (preserving aspect ratio) 
		 * until it can no longer scale the content without clipping it along the y-axis, 
		 * the x-axis can/will be clipped.
		 */
		ScaleToFitY,
		/**
		 * Scales the content uniformly (preserving aspect ratio), until all sides meet 
		 * or exceed the size of the area.  Will result in clipping the longer side.
		 */
		ScaleToFill,
		/** Scales the content according to the size of the safe zone currently applied to the viewport. */
		ScaleBySafeZone,
		/** Scales the content by the scale specified by the user. */
		UserSpecified
	};
}

/**
 * Allows you to place content with a desired size and have it scale to meet the constraints placed on this box's alloted area.  If
 * you needed to have a background image scale to fill an area but not become distorted with different aspect ratios, or if you need
 * to auto fit some text to an area, this is the control for you.
 */
class SLATE_API SScaleBox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SScaleBox)
	: _Content()
	, _HAlign(HAlign_Center)
	, _VAlign(VAlign_Center)
	, _StretchDirection(EStretchDirection::Both)
	, _Stretch(EStretch::None)
	, _UserSpecifiedScale(1.0f)
	, _IgnoreInheritedScale(false)
	{}
		/** Slot for this designers content (optional) */
		SLATE_DEFAULT_SLOT(FArguments, Content)

		/** The horizontal alignment of the content */
		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)

		/** The vertical alignment of the content */
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		
		/** Controls in what direction content can be scaled */
		SLATE_ATTRIBUTE(EStretchDirection::Type, StretchDirection)
		
		/** The stretching rule to apply when content is stretched */
		SLATE_ATTRIBUTE(EStretch::Type, Stretch)

		/** Optional scale that can be specified by the User */
		SLATE_ATTRIBUTE(float, UserSpecifiedScale)

		/** Undo any inherited scale factor before applying this scale box's scale */
		SLATE_ATTRIBUTE(bool, IgnoreInheritedScale)

	SLATE_END_ARGS()

	/** Constructor */
	SScaleBox()
	{
		bCanTick = false;
		bCanSupportFocus = false;
	}

	void Construct(const FArguments& InArgs);
	
	// SWidget interface
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual FVector2D ComputeDesiredSize(float InScale) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	// End SWidget of interface

	/** See Content slot */
	void SetContent(TSharedRef<SWidget> InContent);

	/** See HAlign argument */
	void SetHAlign(EHorizontalAlignment HAlign);

	/** See VAlign argument */
	void SetVAlign(EVerticalAlignment VAlign);

	/** See StretchDirection argument */
	void SetStretchDirection(EStretchDirection::Type InStretchDirection);

	/** See Stretch argument */
	void SetStretch(EStretch::Type InStretch);

	/** See UserSpecifiedScale argument */
	void SetUserSpecifiedScale(float InUserSpecifiedScale);

	/** Set IgnoreInheritedScale argument */
	void SetIgnoreInheritedScale(bool InIgnoreInheritedScale);
	
protected:
	virtual float GetRelativeLayoutScale(const FSlotBase& Child) const override;

	float GetLayoutScale() const;
	void RefreshSafeZoneScale();
private:
	/** The allowed direction of stretching of the content */
	TAttribute<EStretchDirection::Type> StretchDirection;

	/** The method of scaling that is applied to the content. */
	TAttribute<EStretch::Type> Stretch;

	/** Optional scale that can be specified by the User */
	TAttribute<float> UserSpecifiedScale;

	/** Optional bool to ignore the inherited scale */
	TAttribute<bool> IgnoreInheritedScale;

	/** Computed scale when scaled by safe zone padding */
	float SafeZoneScale;
};

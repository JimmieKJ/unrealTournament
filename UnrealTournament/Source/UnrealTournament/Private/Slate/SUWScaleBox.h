// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate.h"
#if !UE_SERVER
class SUWScaleBox : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUWScaleBox)
	: _bMaintainAspectRatio(true)
	{
	}

	/** Slot for this designers content (optional) */
	SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_ARGUMENT(bool, bMaintainAspectRatio)									
	SLATE_END_ARGS()

	bool bMaintainAspectRatio;

	/** needed for every widget */
	void Construct(const FArguments& InArgs);

	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	/** See Content slot */
	void SetContent(TSharedRef<SWidget> InContent);

};
#endif
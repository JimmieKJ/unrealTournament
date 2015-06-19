// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements a widget that holds a weak pointer to one child widget.
 *
 * Weak widgets encapsulate a piece of content without owning it.
 * e.g. A tooltip is owned by the hovered widget but displayed
 *      by a floating window. That window cannot own the tooltip
 *      and must therefore use an SWeakWidget.
 */
class SLATE_API SWeakWidget : public SWidget
{
public:

	SLATE_BEGIN_ARGS(SWeakWidget)
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
		}
		SLATE_ARGUMENT( TSharedPtr<SWidget>, PossiblyNullContent )
	SLATE_END_ARGS()

public:

	SWeakWidget();

	void Construct(const FArguments& InArgs);

	void SetContent(const TSharedRef<SWidget>& InWidget);

	bool ChildWidgetIsValid() const;

	TWeakPtr<SWidget> GetChildWidget() const;

public:

	// SWidget interface

	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual FChildren* GetChildren() override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;

private:

	TWeakChild<SWidget> WeakChild;
};

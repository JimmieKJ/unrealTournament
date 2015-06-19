// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class SLATE_API SSpacer : public SLeafWidget
{
public:

	SLATE_BEGIN_ARGS( SSpacer )
		: _Size(FVector2D::ZeroVector)
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
		}

		SLATE_ATTRIBUTE( FVector2D, Size )
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	// SWidget interface
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	// End of SWidget interface

	FVector2D GetSize() const
	{
		return SpacerSize.Get();
	}

	void SetSize( TAttribute<FVector2D> InSpacerSize )
	{
		SpacerSize = InSpacerSize;
	}


private:
	TAttribute<FVector2D> SpacerSize;
};

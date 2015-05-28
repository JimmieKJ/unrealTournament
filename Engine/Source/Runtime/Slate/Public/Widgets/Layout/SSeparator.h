// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class SLATE_API SSeparator : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SSeparator)
		: _SeparatorImage( FCoreStyle::Get().GetBrush("Separator") )
		, _Orientation(Orient_Horizontal)
		, _Thickness(3.0f)
		{}
		
		SLATE_ARGUMENT(const FSlateBrush*, SeparatorImage)
		/** A horizontal separator is used in a vertical list (orientation is direction of the line drawn) */
		SLATE_ARGUMENT( EOrientation, Orientation)

		SLATE_ARGUMENT(float, Thickness)
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	// SWidget interface
	virtual FVector2D ComputeDesiredSize(float) const override;
	// End of SWidget interface
private:
	EOrientation Orientation;
	float Thickness;
};

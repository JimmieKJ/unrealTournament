// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
	Here's how you could make use of TitleSafe and ActionSafe areas:

		SNew(SOverlay)
		+SOverlay::Slot()
		[
			// ActioSafe
			SNew(SSafeZone) .IsTitleSafe( false )
		]
		+SOverlay::Slot()
		[
			// TitleSafe
			SNew(SSafeZone) .IsTitleSafe( true )
		] 
*/

class SLATE_API SSafeZone : public SBox
{
	SLATE_BEGIN_ARGS(SSafeZone)
		: _HAlign( HAlign_Fill )
		, _VAlign( VAlign_Fill )
		, _Padding( 0.0f )
		, _Content()
		, _IsTitleSafe( false )
		, _SafeAreaScale(1,1,1,1)
		{}

		/** Horizontal alignment of content in the area allotted to the SBox by its parent */
		SLATE_ARGUMENT( EHorizontalAlignment, HAlign )

		/** Vertical alignment of content in the area allotted to the SBox by its parent */
		SLATE_ARGUMENT( EVerticalAlignment, VAlign )

		/** Padding between the SBox and the content that it presents. Padding affects desired size. */
		SLATE_ATTRIBUTE( FMargin, Padding )

		/** The widget content presented by the SBox */
		SLATE_DEFAULT_SLOT( FArguments, Content )
	
		/** True if the zone is TitleSafe, otherwise it's ActionSafe */
		SLATE_ARGUMENT( bool, IsTitleSafe )

		/**
		 * The scalar to apply to each side we want to have safe padding for.  This is a handy way 
		 * to ignore the padding along certain areas by setting it to 0, if you only care about asymmetric safe areas.
		 */
		SLATE_ARGUMENT(FMargin, SafeAreaScale)

	SLATE_END_ARGS()

public:

	void Construct( const FArguments& InArgs );

	void SetTitleSafe( bool bIsTitleSafe );
	void SetSafeAreaScale(FMargin InSafeAreaScale);

	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScale) const override;

private:
	/** Cached values from the args */
	TAttribute<FMargin> Padding;
	FMargin SafeAreaScale;
	FMargin SafeMargin;
};

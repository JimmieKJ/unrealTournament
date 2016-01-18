// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

	FMargin GetSafeZonePadding() const;

	void SetTitleSafe( bool bIsTitleSafe );

private:
	/** Cached values from the args */
	TAttribute<FMargin> Padding;
	FMargin SafeMargin;
};

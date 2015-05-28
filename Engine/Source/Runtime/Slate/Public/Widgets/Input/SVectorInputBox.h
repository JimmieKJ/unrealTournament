// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#undef _X //@todo vogel: needs to be removed once Slate no longer uses _##Name

/**
 * Vector Slate control
 */
class SLATE_API SVectorInputBox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SVectorInputBox )
		: _Font( FCoreStyle::Get().GetFontStyle("NormalFont") ),
		  _bColorAxisLabels( false )
		{}
		
		/** X Component of the vector */
		SLATE_ATTRIBUTE( TOptional<float>, X )

		/** Y Component of the vector */
		SLATE_ATTRIBUTE( TOptional<float>, Y )

		/** Z Component of the vector */
		SLATE_ATTRIBUTE( TOptional<float>, Z )

		/** Font to use for the text in this box */
		SLATE_ATTRIBUTE( FSlateFontInfo, Font )

		/** Should the axis labels be colored */
		SLATE_ARGUMENT( bool, bColorAxisLabels )		

		/** Called when the x value of the vector is changed */
		SLATE_EVENT( FOnFloatValueChanged, OnXChanged )

		/** Called when the y value of the vector is changed */
		SLATE_EVENT( FOnFloatValueChanged, OnYChanged )

		/** Called when the z value of the vector is changed */
		SLATE_EVENT( FOnFloatValueChanged, OnZChanged )

		/** Called when the x value of the vector is committed */
		SLATE_EVENT( FOnFloatValueCommitted, OnXCommitted )

		/** Called when the y value of the vector is committed */
		SLATE_EVENT( FOnFloatValueCommitted, OnYCommitted )

		/** Called when the z value of the vector is committed */
		SLATE_EVENT( FOnFloatValueCommitted, OnZCommitted )

		/** Menu extender delegate for the X value */
		SLATE_EVENT( FMenuExtensionDelegate, ContextMenuExtenderX )

		/** Menu extender delegate for the Y value */
		SLATE_EVENT( FMenuExtensionDelegate, ContextMenuExtenderY )

		/** Menu extender delegate for the Z value */
		SLATE_EVENT( FMenuExtensionDelegate, ContextMenuExtenderZ )

		/** Provide custom type functionality for the vector */
		SLATE_ARGUMENT( TSharedPtr< INumericTypeInterface<float> >, TypeInterface )

	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );
private:

	/**
	 * Construct widgets for the X Value
	 */
	void ConstructX( const FArguments& InArgs, TSharedRef<SHorizontalBox> HorizontalBox );

	/**
	 * Construct widgets for the Y Value
	 */
	void ConstructY( const FArguments& InArgs, TSharedRef<SHorizontalBox> HorizontalBox );

	/**
	 * Construct widgets for the Z Value
	 */
	void ConstructZ( const FArguments& InArgs, TSharedRef<SHorizontalBox> HorizontalBox );
};

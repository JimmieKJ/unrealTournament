// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Represents an expandable area of content            
 */
class SLATE_API SExpandableArea : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SExpandableArea )
		: _Style( &FCoreStyle::Get().GetWidgetStyle<FExpandableAreaStyle>("ExpandableArea") )
		, _BorderBackgroundColor( FLinearColor::White )
		, _BorderImage( FCoreStyle::Get().GetBrush("ExpandableArea.Border") )
		, _AreaTitle( )
		, _InitiallyCollapsed( false )
		, _MaxHeight( 0.0f )
		, _Padding( 1.0f )
		, _AreaTitleFont( FCoreStyle::Get().GetFontStyle("ExpandableArea.TitleFont") )
		{}
		
		/** Style used to draw this area */
		SLATE_STYLE_ARGUMENT( FExpandableAreaStyle, Style )
		/** Background color to apply to the border image */
		SLATE_ATTRIBUTE( FSlateColor, BorderBackgroundColor )
		/** Border to use around the area */
		SLATE_ARGUMENT( const FSlateBrush*, BorderImage )
		/** Content displayed next to the expansion arrow.  This is always visible */
		SLATE_NAMED_SLOT( FArguments, HeaderContent )
		/** Content displayed inside the area that is expanded */
		SLATE_NAMED_SLOT( FArguments, BodyContent )
		/** The title to display.  Not used if header content is supplied */
		SLATE_TEXT_ATTRIBUTE( AreaTitle )
		/** Whether or not the area is initially collapsed */
		SLATE_ARGUMENT( bool, InitiallyCollapsed )
		/** The maximum height of the area */
		SLATE_ARGUMENT( float, MaxHeight )
		/** The content padding */
		SLATE_ATTRIBUTE( FMargin, Padding )
		/** Called when the area is expanded or collapsed */
		SLATE_EVENT( FOnBooleanValueChanged, OnAreaExpansionChanged )
		/** Sets the font used to draw the title text */
		SLATE_ATTRIBUTE( FSlateFontInfo, AreaTitleFont )
	SLATE_END_ARGS()


public:

	/**
	 * Constructs a new widget.
	 *
	 * @param InArgs Construction arguments.
	 */
	void Construct( const FArguments& InArgs );

	/**
	 * @return true if the area is currently expanded
	 */
	virtual bool IsExpanded() const { return !bAreaCollapsed; }

	/**
	 * Instantly sets the expanded state of the area
	 */
	virtual void SetExpanded( bool bExpanded );

protected:

	/**
	 * Constructs the header area widget
	 * 
	 * @param InArgs	Construction arguments
	 * @param HeaderContent	User specified header content to display
	 */
	virtual TSharedRef<SWidget> ConstructHeaderWidget( const FArguments& InArgs, TSharedRef<SWidget> HeaderContent );

	/**
	 * @return The visibility of this section                   
	 */
	virtual EVisibility OnGetContentVisibility() const;

	FReply OnHeaderClicked();

	/** Toggles selection visibility when the panel is clicked. */
	virtual void OnToggleContentVisibility();

	/**
	 * @return The collapsed/expanded image we should show.                     
	 */
	virtual const FSlateBrush* OnGetCollapseImage() const;

	/**
	 * @return The scale of the content inside this section.  Animated 
	 */
	virtual FVector2D GetSectionScale() const;

	/**
	 * Computes the desired size of this area. Optionally clamping to Max height
	 */
	virtual FVector2D ComputeDesiredSize(float) const override;

protected:

	/** Border widget for the header area */
	TSharedPtr<SBorder> HeaderBorder;

	/** Curved used to simulate a rollout of the section */
	FCurveSequence RolloutCurve;

	/** Delegate that fires when the area becomes expanded or collapsed the user */
	FOnBooleanValueChanged OnAreaExpansionChanged;

	/** The maximum height of this area */
	float MaxHeight;

	/** true if the section is visible */
	bool bAreaCollapsed;

	/** Image to use when the area is collapsed */
	const FSlateBrush* CollapsedImage;

	/** Image to use when the area is expanded */
	const FSlateBrush* ExpandedImage;
};

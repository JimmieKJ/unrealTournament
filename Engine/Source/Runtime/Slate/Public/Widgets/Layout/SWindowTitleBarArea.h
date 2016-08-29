// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class SLATE_API SWindowTitleBarArea
	: public SPanel
{
public:
	
	class SLATE_API FSlot : public TSupportsOneChildMixin<FSlot>, public TSupportsContentAlignmentMixin<FSlot>, public TSupportsContentPaddingMixin<FSlot>
	{
		public:
			FSlot()
			: TSupportsOneChildMixin<FSlot>()
			, TSupportsContentAlignmentMixin<FSlot>(HAlign_Fill, VAlign_Fill)
			{ }
	};

	SLATE_BEGIN_ARGS(SWindowTitleBarArea)
		: _HAlign( HAlign_Fill )
		, _VAlign( VAlign_Fill )
		, _Padding( 0.0f )
		, _Content()
		{ }

		/** Horizontal alignment of content in the area allotted to the SWindowTitleBarArea by its parent */
		SLATE_ARGUMENT( EHorizontalAlignment, HAlign )

		/** Vertical alignment of content in the area allotted to the SWindowTitleBarArea by its parent */
		SLATE_ARGUMENT( EVerticalAlignment, VAlign )

		/** Padding between the SWindowTitleBarArea and the content that it presents. Padding affects desired size. */
		SLATE_ATTRIBUTE( FMargin, Padding )

		/** The widget content presented by the SWindowTitleBarArea */
		SLATE_DEFAULT_SLOT( FArguments, Content )

	SLATE_END_ARGS()

	SWindowTitleBarArea();

	void Construct( const FArguments& InArgs );

	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual FChildren* GetChildren() override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;

	virtual EWindowZone::Type GetWindowZoneOverride() const override;

	/** See the Content slot. */
	void SetContent(const TSharedRef< SWidget >& InContent);

	/** See HAlign argument */
	void SetHAlign(EHorizontalAlignment HAlign);

	/** See VAlign argument */
	void SetVAlign(EVerticalAlignment VAlign);

	/** See Padding attribute */
	void SetPadding(const TAttribute<FMargin>& InPadding);

protected:

	FSlot ChildSlot;
};

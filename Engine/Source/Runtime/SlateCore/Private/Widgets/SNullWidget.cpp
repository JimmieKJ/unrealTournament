// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"


FNoChildren NullWidgetNoChildren;

class SLATECORE_API SNullWidgetContent
	: public SWidget
{
public:

	SLATE_BEGIN_ARGS(SNullWidgetContent)
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs )
	{
		SWidget::Construct( 
			InArgs._ToolTipText,
			InArgs._ToolTip,
			InArgs._Cursor, 
			InArgs._IsEnabled,
			InArgs._Visibility,
			InArgs._RenderTransform,
			InArgs._RenderTransformPivot,
			InArgs._Tag,
			InArgs.MetaData
		);
	}

private:
	virtual void SetVisibility( TAttribute<EVisibility> InVisibility ) override final
	{
		ensureMsg( false, TEXT("Attempting to SetVisibility() on SNullWidget. Mutating SNullWidget is not allowed.") );
	}
public:
	
	// SWidget interface

	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override
	{
		return LayerId;
	}

	virtual FVector2D ComputeDesiredSize(float) const override final
	{
		return FVector2D(0.0f, 0.0f);
	}

	virtual FChildren* GetChildren( ) override final
	{
		return &NullWidgetNoChildren;
	}


	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override final
	{
		// Nothing to arrange; Null Widgets do not have children.
	}

	// End of SWidget interface
};


SLATECORE_API TSharedRef<SWidget> SNullWidget::NullWidget = SNew(SNullWidgetContent).Visibility(EVisibility::Hidden);

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IPropertyTableUtilities.h"
#include "PropertyNode.h"
#include "PropertyEditor.h"

#include "IPropertyTableColumn.h"
#include "IPropertyTableRow.h"
#include "IPropertyTableCellPresenter.h"


class SPropertyTableCell : public SCompoundWidget
{
	public:

	SLATE_BEGIN_ARGS( SPropertyTableCell ) 
		: _Presenter( NULL )
		, _Style( TEXT("PropertyTable") )
	{}
		SLATE_ARGUMENT( TSharedPtr< IPropertyTableCellPresenter >, Presenter)
		SLATE_ARGUMENT( FName, Style )
	SLATE_END_ARGS()

	SPropertyTableCell()
		: DropDownAnchor( NULL )
		, Presenter( NULL )
		, Cell( NULL )
		, bEnterEditingMode( false )
		, Style()
	{ }

	void Construct( const FArguments& InArgs, const TSharedRef< class IPropertyTableCell >& InCell );

	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;

	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;


private:

	TSharedRef< SWidget > ConstructCellContents();

	void SetContent( const TSharedRef< SWidget >& NewContents );

	TSharedRef< class SWidget > ConstructEditModeCellWidget();

	TSharedRef< class SWidget > ConstructEditModeDropDownWidget();

	TSharedRef< class SBorder > ConstructInvalidPropertyWidget();

	const FSlateBrush* GetCurrentCellBorder() const;

	void OnAnchorWindowClosed( const TSharedRef< SWindow >& WindowClosing );

	void EnteredEditMode();

	void ExitedEditMode();

	void OnCellValueChanged( UObject* Object, FPropertyChangedEvent& PropertyChangedEvent );

	/** One-off active timer to trigger entering the editing mode */
	EActiveTimerReturnType TriggerEnterEditingMode(double InCurrentTime, float InDeltaTime);

private:

	TSharedPtr< SMenuAnchor > DropDownAnchor;

	TSharedPtr< class IPropertyTableCellPresenter > Presenter;
	TSharedPtr< class IPropertyTableCell > Cell;
	bool bEnterEditingMode;
	FName Style;

	const FSlateBrush* CellBackground;
};
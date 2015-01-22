// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the message breakpoints panel.
 */
class SMessagingBreakpoints
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMessagingBreakpoints) { }
	SLATE_END_ARGS()


public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InStyle The visual style to use for this widget.
	 * @param InTracer The message tracer.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<ISlateStyle>& InStyle, const IMessageTracerRef& InTracer );

private:

	/** Handles generating a row widget for the endpoint list view. */
	TSharedRef<ITableRow> HandleBreakpointListGenerateRow( IMessageTracerBreakpointPtr EndpointInfo, const TSharedRef<STableViewBase>& OwnerTable );

	/** Handles the selection of endpoints. */
	void HandleBreakpointListSelectionChanged( IMessageTracerBreakpointPtr InItem, ESelectInfo::Type SelectInfo );

private:

	/** Holds the filtered list of historic messages. */
	TArray<IMessageTracerBreakpointPtr> BreakpointList;

	/** Holds the message list view. */
	TSharedPtr<SListView<IMessageTracerBreakpointPtr>> BreakpointListView;

	/** Holds the widget's visual style. */
	TSharedPtr<ISlateStyle> Style;

	/** Holds a pointer to the message bus tracer. */
	IMessageTracerPtr Tracer;
};

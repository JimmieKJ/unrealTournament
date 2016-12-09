// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IMessageTracerBreakpoint.h"
#include "IMessageTracer.h"
#include "Styling/ISlateStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"

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
	void Construct(const FArguments& InArgs, const TSharedRef<ISlateStyle>& InStyle, const TSharedRef<IMessageTracer, ESPMode::ThreadSafe>& InTracer);

private:

	/** Handles generating a row widget for the endpoint list view. */
	TSharedRef<ITableRow> HandleBreakpointListGenerateRow(TSharedPtr<IMessageTracerBreakpoint, ESPMode::ThreadSafe> EndpointInfo, const TSharedRef<STableViewBase>& OwnerTable);

	/** Handles the selection of endpoints. */
	void HandleBreakpointListSelectionChanged(TSharedPtr<IMessageTracerBreakpoint, ESPMode::ThreadSafe> InItem, ESelectInfo::Type SelectInfo);

private:

	/** Holds the filtered list of historic messages. */
	TArray<TSharedPtr<IMessageTracerBreakpoint, ESPMode::ThreadSafe>> BreakpointList;

	/** Holds the message list view. */
	TSharedPtr<SListView<TSharedPtr<IMessageTracerBreakpoint, ESPMode::ThreadSafe>>> BreakpointListView;

	/** Holds the widget's visual style. */
	TSharedPtr<ISlateStyle> Style;

	/** Holds a pointer to the message bus tracer. */
	TSharedPtr<IMessageTracer, ESPMode::ThreadSafe> Tracer;
};

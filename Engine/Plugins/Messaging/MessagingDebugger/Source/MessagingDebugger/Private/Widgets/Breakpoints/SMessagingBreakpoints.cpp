// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MessagingDebuggerPrivatePCH.h"


#define LOCTEXT_NAMESPACE "SMessagingBreakpoints"


/* SMessagingBreakpoints interface
 *****************************************************************************/

void SMessagingBreakpoints::Construct( const FArguments& InArgs, const TSharedRef<ISlateStyle>& InStyle, const IMessageTracerRef& InTracer )
{
	Style = InStyle;
	Tracer = InTracer;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SBorder)
					.BorderImage(InStyle->GetBrush("GroupBorder"))
					.Padding(0.0f)
					[
						// message list
						SAssignNew(BreakpointListView, SListView<IMessageTracerBreakpointPtr>)
							.ItemHeight(24.0f)
							.ListItemsSource(&BreakpointList)
							.SelectionMode(ESelectionMode::Multi)
							.OnGenerateRow(this, &SMessagingBreakpoints::HandleBreakpointListGenerateRow)
							.OnSelectionChanged(this, &SMessagingBreakpoints::HandleBreakpointListSelectionChanged)
							.HeaderRow
							(
								SNew(SHeaderRow)

								+ SHeaderRow::Column("Name")
									.DefaultLabel(LOCTEXT("BreakpointListNameColumnHeader", "Name"))
									.FillWidth(1.0f)

								+ SHeaderRow::Column("HitCount")
									.DefaultLabel(LOCTEXT("BreakpointListHitCountColumnHeader", "Hit Count"))
									.FixedWidth(64.0f)
							)
					]
			]
	];
}


/* SMessagingBreakpoints implementation
 *****************************************************************************/



/* SMessagingBreakpoints callbacks
 *****************************************************************************/

TSharedRef<ITableRow> SMessagingBreakpoints::HandleBreakpointListGenerateRow( IMessageTracerBreakpointPtr Breakpoint, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(SMessagingBreakpointsTableRow, OwnerTable)
		.Breakpoint(Breakpoint)
		.Style(Style);
}


void SMessagingBreakpoints::HandleBreakpointListSelectionChanged( IMessageTracerBreakpointPtr InItem, ESelectInfo::Type SelectInfo )
{

}


#undef LOCTEXT_NAMESPACE

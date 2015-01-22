// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MessagingDebuggerPrivatePCH.h"
#include "SExpandableArea.h"


#define LOCTEXT_NAMESPACE "SMessagingTypes"


/* SMessagingTypes structors
 *****************************************************************************/

SMessagingTypes::~SMessagingTypes()
{
	if (Model.IsValid())
	{
		Model->OnSelectedMessageChanged().RemoveAll(this);
	}

	if (Tracer.IsValid())
	{
		Tracer->OnMessagesReset().RemoveAll(this);
		Tracer->OnTypeAdded().RemoveAll(this);
	}
}


/* SMessagingTypes interface
 *****************************************************************************/

void SMessagingTypes::Construct( const FArguments& InArgs, const FMessagingDebuggerModelRef& InModel, const TSharedRef<ISlateStyle>& InStyle, const IMessageTracerRef& InTracer )
{
	Filter = MakeShareable(new FMessagingDebuggerTypeFilter());
	Model = InModel;
	Style = InStyle;
	Tracer = InTracer;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SExpandableArea)
					.AreaTitle(LOCTEXT("TypeFilterAreaTitle", "Message Type Filter"))
					.InitiallyCollapsed(true)
					.Padding(8.0f)
					.BodyContent()
					[
						// filter bar
						SNew(SMessagingTypesFilterBar, Filter.ToSharedRef())
					]
			]

		+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SBorder)
					.BorderImage(InStyle->GetBrush("GroupBorder"))
					.Padding(0.0f)
					[
						// type list
						SAssignNew(TypeListView, SListView<FMessageTracerTypeInfoPtr>)
							.ItemHeight(24.0f)
							.ListItemsSource(&TypeList)
							.SelectionMode(ESelectionMode::Multi)
							.OnGenerateRow(this, &SMessagingTypes::HandleTypeListGenerateRow)
							.OnSelectionChanged(this, &SMessagingTypes::HandleTypeListSelectionChanged)
							.HeaderRow
							(
								SNew(SHeaderRow)

								+ SHeaderRow::Column("Break")
									.FixedWidth(24.0f)
									.HAlignHeader(HAlign_Center)
									.VAlignHeader(VAlign_Center)
									.HeaderContent()
									[
										SNew(SImage)
											.Image(Style->GetBrush("Break"))
											.ToolTipText(LOCTEXT("TypeListBreakpointsColumnTooltip", "Breakpoints"))
									]

								+ SHeaderRow::Column("Name")
									.DefaultLabel(LOCTEXT("TypeListNameColumnHeader", "Name"))
									.FillWidth(0.9f)

								+ SHeaderRow::Column("Messages")
									.FillWidth(0.1f)
									.HAlignCell(HAlign_Right)
									.HAlignHeader(HAlign_Right)
									.VAlignHeader(VAlign_Center)
									.HeaderContent()
									[
										SNew(SImage)
											.Image(Style->GetBrush("SentMessage"))
											.ToolTipText(LOCTEXT("TypeListMessagesColumnTooltip", "Number of messages per message type"))
									]

								+ SHeaderRow::Column("Visibility")
									.FixedWidth(26.0f)
									.HAlignCell(HAlign_Center)
									.HAlignHeader(HAlign_Center)
									.VAlignHeader(VAlign_Center)
									.HeaderContent()
									[
										SNew(SImage)
											.Image(Style->GetBrush("Visibility"))
											.ToolTipText(LOCTEXT("TypeListVisibilityColumnTooltip", "Visibility"))
									]
							)
					]
			]
	];

	Filter->OnChanged().AddRaw(this, &SMessagingTypes::HandleFilterChanged);
	Model->OnSelectedMessageChanged().AddRaw(this, &SMessagingTypes::HandleModelSelectedMessageChanged);
	Tracer->OnMessagesReset().AddRaw(this, &SMessagingTypes::HandleTracerMessagesReset);
	Tracer->OnTypeAdded().AddRaw(this, &SMessagingTypes::HandleTracerTypeAdded);

	ReloadTypes();
}


/* SMessagingTypes implementation
 *****************************************************************************/

void SMessagingTypes::AddType( const FMessageTracerTypeInfoRef& TypeInfo )
{
	if (Filter->FilterType(TypeInfo))
	{
		TypeList.Add(TypeInfo);
		TypeListView->RequestListRefresh();
	}
}


void SMessagingTypes::ReloadTypes()
{
	TypeList.Reset();
	
	TArray<FMessageTracerTypeInfoPtr> Types;

	if (Tracer->GetMessageTypes(Types) > 0)
	{
		for (TArray<FMessageTracerTypeInfoPtr>::TConstIterator It(Types); It; ++It)
		{
			AddType(It->ToSharedRef());
		}
	}

	TypeListView->RequestListRefresh();
}


/* SMessagingTypes callbacks
 *****************************************************************************/

void SMessagingTypes::HandleFilterChanged()
{
	ReloadTypes();
}


void SMessagingTypes::HandleModelSelectedMessageChanged()
{
	FMessageTracerMessageInfoPtr SelectedMessage = Model->GetSelectedMessage();

	if (SelectedMessage.IsValid())
	{
		TypeListView->SetSelection(SelectedMessage->TypeInfo);
	}
}


void SMessagingTypes::HandleTracerMessagesReset()
{
	ReloadTypes();
}


void SMessagingTypes::HandleTracerTypeAdded( FMessageTracerTypeInfoRef TypeInfo )
{
	AddType(TypeInfo);
}


TSharedRef<ITableRow> SMessagingTypes::HandleTypeListGenerateRow( FMessageTracerTypeInfoPtr TypeInfo, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(SMessagingTypesTableRow, OwnerTable, Model.ToSharedRef())
		.HighlightText(this, &SMessagingTypes::HandleTypeListGetHighlightText)
		.Style(Style)
		.TypeInfo(TypeInfo);
}


FText SMessagingTypes::HandleTypeListGetHighlightText() const
{
	return FText::GetEmpty();
	//return FilterBar->GetFilterText();
}


void SMessagingTypes::HandleTypeListSelectionChanged( FMessageTracerTypeInfoPtr InItem, ESelectInfo::Type SelectInfo )
{

}


#undef LOCTEXT_NAMESPACE

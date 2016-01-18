// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DeviceManagerPrivatePCH.h"
#include "SExpandableArea.h"


#define LOCTEXT_NAMESPACE "SDeviceBrowser"


/* SDeviceBrowser interface
 *****************************************************************************/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SDeviceBrowser::Construct( const FArguments& InArgs, const FDeviceManagerModelRef& InModel, const ITargetDeviceServiceManagerRef& InDeviceServiceManager, const TSharedPtr<FUICommandList>& InUICommandList )
{
	DeviceServiceManager = InDeviceServiceManager;
	Filter = MakeShareable(new FDeviceBrowserFilter());
	Model = InModel;
	NeedsServiceListRefresh = true;
	UICommandList = InUICommandList;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.AutoHeight()
				[
					/*
					SNew(SExpandableArea)
						.AreaTitle(LOCTEXT("FilterBarAreaTitle", "Device Filter").ToString())
						.InitiallyCollapsed(true)
						.Padding(FMargin(8.0f, 8.0f, 8.0f, 4.0f))
						.BodyContent()
						[*/
							// filter bar
							SNew(SDeviceBrowserFilterBar, Filter.ToSharedRef())
						//]
				]

		+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(0.0f)
					[
						// device list
						SAssignNew(DeviceServiceListView, SListView<ITargetDeviceServicePtr>)
							.ItemHeight(20.0f)
							.ListItemsSource(&DeviceServiceList)
							.OnContextMenuOpening(this, &SDeviceBrowser::HandleDeviceServiceListViewContextMenuOpening)
							.OnGenerateRow(this, &SDeviceBrowser::HandleDeviceServiceListViewGenerateRow)
							.OnSelectionChanged(this, &SDeviceBrowser::HandleDeviceServiceListViewSelectionChanged)
							.SelectionMode(ESelectionMode::Single)
							.HeaderRow
							(
								SNew(SHeaderRow)

								+ SHeaderRow::Column("Icon")
									.DefaultLabel( FText::FromString(TEXT(" ")) )
									.FixedWidth(32.0f)

								+ SHeaderRow::Column("Name")
									.DefaultLabel(LOCTEXT("DevicesListNameColumnHeader", "Device"))

								+ SHeaderRow::Column("Platform")
									.DefaultLabel(LOCTEXT("DevicesListPlatformColumnHeader", "Platform"))

								+ SHeaderRow::Column("Status")
									.DefaultLabel(LOCTEXT("DevicesListStatusColumnHeader", "Status"))
									.FixedWidth(128.0f)

								+ SHeaderRow::Column("Claimed")
									.DefaultLabel(LOCTEXT("DevicesListClaimedColumnHeader", "Claimed By"))

								+ SHeaderRow::Column("Share")
									.DefaultLabel(LOCTEXT("DevicesListShareColumnHeader", "Share"))
									.FixedWidth(48.0f)
									.HAlignCell(HAlign_Center)
									.HAlignHeader(HAlign_Center)
							)
					]
			]

		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SExpandableArea)
					.AreaTitle(LOCTEXT("DeviceAdderAreaTitle", "Add An Unlisted Device"))
					.InitiallyCollapsed(true)
					.Padding(FMargin(8.0f, 8.0f, 8.0f, 4.0f))
					.BodyContent()
					[
						// device adder
						SNew(SDeviceBrowserDeviceAdder, InDeviceServiceManager)
					]
			]
	];

	DeviceServiceManager->OnServiceAdded().AddSP(this, &SDeviceBrowser::HandleDeviceServiceManagerServiceAdded);
	DeviceServiceManager->OnServiceRemoved().AddSP(this, &SDeviceBrowser::HandleDeviceServiceManagerServiceRemoved);

	Filter->OnFilterChanged().AddSP(this, &SDeviceBrowser::HandleFilterChanged);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SDeviceBrowser::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	//@TODO Passive - Only happens in response to the addition or removal of a device to the device service manager
	if (NeedsServiceListRefresh)
	{
		ReloadDeviceServiceList(true);
		NeedsServiceListRefresh = false;
	}
}


/* SDeviceBrowser callbacks
 *****************************************************************************/

void SDeviceBrowser::ReloadDeviceServiceList( bool FullyReload )
{
	// reload target device service list
	if (FullyReload)
	{
		AvailableDeviceServices.Reset();

		DeviceServiceManager->GetServices(AvailableDeviceServices);
		Filter->ResetFilter(AvailableDeviceServices);
	}

	// filter target device service list
	DeviceServiceList.Reset();

	for (int32 DeviceServiceIndex = 0; DeviceServiceIndex < AvailableDeviceServices.Num(); ++DeviceServiceIndex)
	{
		const ITargetDeviceServicePtr& DeviceService = AvailableDeviceServices[DeviceServiceIndex];

		if (Filter->FilterDeviceService(DeviceService.ToSharedRef()))
		{
			DeviceServiceList.Add(DeviceService);
		}
	}

	// refresh list view
	DeviceServiceListView->RequestListRefresh();
}


/* SDeviceBrowser callbacks
 *****************************************************************************/


TSharedPtr<SWidget> SDeviceBrowser::HandleDeviceServiceListViewContextMenuOpening( )
{
	TArray<ITargetDeviceServicePtr> SelectedDeviceServices = DeviceServiceListView->GetSelectedItems();

	if (SelectedDeviceServices.Num() > 0)
	{
		return SNew(SDeviceBrowserContextMenu, UICommandList);
	}
	
	return NULL;
}


TSharedRef<ITableRow> SDeviceBrowser::HandleDeviceServiceListViewGenerateRow( ITargetDeviceServicePtr DeviceService, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(SDeviceBrowserDeviceListRow, OwnerTable)
		.DeviceService(DeviceService)
		.HighlightText(this, &SDeviceBrowser::HandleDeviceServiceListViewHighlightText)
		.ToolTip(SNew(SDeviceBrowserTooltip, DeviceService.ToSharedRef()));
}


void SDeviceBrowser::HandleDeviceServiceListViewSelectionChanged( ITargetDeviceServicePtr Selection, ESelectInfo::Type SelectInfo )
{
	Model->SelectDeviceService(Selection);
}


FText SDeviceBrowser::HandleDeviceServiceListViewHighlightText( ) const
{
	return Filter->GetDeviceSearchText();
}


void SDeviceBrowser::HandleDeviceServiceManagerServiceAdded( const ITargetDeviceServiceRef& AddedService )
{
	NeedsServiceListRefresh = true;
}


void SDeviceBrowser::HandleDeviceServiceManagerServiceRemoved( const ITargetDeviceServiceRef& RemovedService )
{
	NeedsServiceListRefresh = true;
}


void SDeviceBrowser::HandleFilterChanged( )
{
	ReloadDeviceServiceList(false);
}


#undef LOCTEXT_NAMESPACE

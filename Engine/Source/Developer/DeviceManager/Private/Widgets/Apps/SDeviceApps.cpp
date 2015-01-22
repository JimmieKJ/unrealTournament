// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DeviceManagerPrivatePCH.h"


#define LOCTEXT_NAMESPACE "SDeviceApps"


/* SMessagingEndpoints structors
*****************************************************************************/

SDeviceApps::~SDeviceApps( )
{
	if (Model.IsValid())
	{
		Model->OnSelectedDeviceServiceChanged().RemoveAll(this);
	}
}


/* SDeviceDetails interface
 *****************************************************************************/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SDeviceApps::Construct( const FArguments& InArgs, const FDeviceManagerModelRef& InModel )
{
	Model = InModel;

	ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
			[
				SNew(SVerticalBox)
					.IsEnabled(this, &SDeviceApps::HandleAppsBoxIsEnabled)

				+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						// applications list
						SNew(SBorder)
							.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
							.Padding(0.0f)
							[
								SAssignNew(AppListView, SListView<TSharedPtr<FString> >)
									.ItemHeight(20.0f)
									.ListItemsSource(&AppList)
									.OnGenerateRow(this, &SDeviceApps::HandleAppListViewGenerateRow)
									.OnSelectionChanged(this, &SDeviceApps::HandleAppListViewSelectionChanged)
									.SelectionMode(ESelectionMode::Single)
									.HeaderRow
									(
										SNew(SHeaderRow)

										+ SHeaderRow::Column("Name")
											.DefaultLabel(LOCTEXT("AppListNameColumnHeader", "Name"))

										+ SHeaderRow::Column("Date")
											.DefaultLabel(LOCTEXT("AppListDeploymentDateColumnHeader", "Date deployed"))

										+ SHeaderRow::Column("Owner")
											.DefaultLabel(LOCTEXT("AppListOwnerColumnHeader", "Deployed by"))
									)
							]
					]
			]

		+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("NotificationList.ItemBackground"))
					.Padding(8.0f)
					.Visibility(this, &SDeviceApps::HandleSelectDeviceOverlayVisibility)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("SelectSessionOverlayText", "Please select a device from the Device Browser"))
					]
			]
	];

	Model->OnSelectedDeviceServiceChanged().AddRaw(this, &SDeviceApps::HandleModelSelectedDeviceServiceChanged);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


/* SDeviceApps callbacks
 *****************************************************************************/

TSharedRef<ITableRow> SDeviceApps::HandleAppListViewGenerateRow( TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(SDeviceAppsAppListRow, OwnerTable);
}


void SDeviceApps::HandleAppListViewSelectionChanged( TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo )
{

}


bool SDeviceApps::HandleAppsBoxIsEnabled( ) const
{
	ITargetDeviceServicePtr DeviceService = Model->GetSelectedDeviceService();

	return (DeviceService.IsValid() && DeviceService->CanStart());
}


void SDeviceApps::HandleModelSelectedDeviceServiceChanged( )
{
}


EVisibility SDeviceApps::HandleSelectDeviceOverlayVisibility( ) const
{
	if (Model->GetSelectedDeviceService().IsValid())
	{
		return EVisibility::Hidden;
	}

	return EVisibility::Visible;
}


#undef LOCTEXT_NAMESPACE

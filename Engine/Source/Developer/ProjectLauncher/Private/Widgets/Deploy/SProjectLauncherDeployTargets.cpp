// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ProjectLauncherPrivatePCH.h"


#define LOCTEXT_NAMESPACE "SProjectLauncherDeployTargets"


/* SSessionSProjectLauncherDeployTargets structors
 *****************************************************************************/

SProjectLauncherDeployTargets::~SProjectLauncherDeployTargets()
{
	if (Model.IsValid())
	{
		const ITargetDeviceProxyManagerRef& DeviceProxyManager = Model->GetDeviceProxyManager();
		DeviceProxyManager->OnProxyAdded().RemoveAll(this);
		DeviceProxyManager->OnProxyRemoved().RemoveAll(this);
	}
}


/* SSessionSProjectLauncherDeployTargets interface
 *****************************************************************************/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SProjectLauncherDeployTargets::Construct(const FArguments& InArgs, const FProjectLauncherModelRef& InModel)
{
	Model = InModel;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			// device list
			SAssignNew(DeviceProxyListView, SListView<ITargetDeviceProxyPtr>)
			.ItemHeight(16.0f)
			.HeaderRow
			(
			SNew(SHeaderRow)

			+ SHeaderRow::Column("CheckBox")
			.DefaultLabel(LOCTEXT("DeviceListCheckboxColumnHeader", " "))
			.FixedWidth(24.0f)

			+ SHeaderRow::Column("Device")
			.DefaultLabel(LOCTEXT("DeviceListDeviceColumnHeader", "Device"))
			.FillWidth(0.35f)

			+ SHeaderRow::Column("Variant")
			.DefaultLabel(LOCTEXT("DeviceListVariantColumnHeader", "Variant"))
			.FillWidth(0.2f)

			+ SHeaderRow::Column("Platform")
			.DefaultLabel(LOCTEXT("DeviceListPlatformColumnHeader", "Platform"))
			.FillWidth(0.15f)

			+ SHeaderRow::Column("Host")
			.DefaultLabel(LOCTEXT("DeviceListHostColumnHeader", "Host"))
			.FillWidth(0.15f)

			+ SHeaderRow::Column("Owner")
			.DefaultLabel(LOCTEXT("DeviceListOwnerColumnHeader", "Owner"))
			.FillWidth(0.15f)
			)
			.ListItemsSource(&DeviceProxyList)
			.OnGenerateRow(this, &SProjectLauncherDeployTargets::HandleDeviceProxyListViewGenerateRow)
			.SelectionMode(ESelectionMode::Single)
			.Visibility(this, &SProjectLauncherDeployTargets::HandleDeviceProxyListViewVisibility)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 12.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			.Visibility(this, &SProjectLauncherDeployTargets::HandleNoDevicesBoxVisibility)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush(TEXT("Icons.Warning")))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SProjectLauncherDeployTargets::HandleNoDevicesTextBlockText)
			]
		]
	];

	const ITargetDeviceProxyManagerRef& DeviceProxyManager = Model->GetDeviceProxyManager();

	DeviceProxyManager->OnProxyAdded().AddSP(this, &SProjectLauncherDeployTargets::HandleDeviceProxyManagerProxyAdded);
	DeviceProxyManager->OnProxyRemoved().AddSP(this, &SProjectLauncherDeployTargets::HandleDeviceProxyManagerProxyRemoved);

	DeviceProxyManager->GetProxies(NAME_None, false, DeviceProxyList);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


/* SProjectLauncherDeployTargets implementation
*****************************************************************************/

void SProjectLauncherDeployTargets::RefreshDeviceProxyList()
{
	Model->GetDeviceProxyManager()->GetProxies(NAME_None, false, DeviceProxyList);

	DeviceProxyListView->RequestListRefresh();
}


/* SSessionSProjectLauncherDeployTargets callbacks
 *****************************************************************************/

ILauncherDeviceGroupPtr SProjectLauncherDeployTargets::HandleDeviceListRowDeviceGroup() const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();
	if (SelectedProfile.IsValid())
	{
		return SelectedProfile->GetDeployedDeviceGroup();
	}
	return nullptr;
}


bool SProjectLauncherDeployTargets::HandleDeviceListRowIsEnabled(ITargetDeviceProxyPtr DeviceProxy) const
{
	if (DeviceProxy.IsValid())
	{
		ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

		if (SelectedProfile.IsValid())
		{
			/*@Todo: Fix this! we should iterate the Devices target platforms and see if any are deployable*/
			//bool RetVal = SelectedProfile->IsDeployablePlatform(DeviceProxy->GetTargetPlatformName(NAME_None));
			return true;
		}
	}

	return false;
}


FText SProjectLauncherDeployTargets::HandleDeviceListRowToolTipText(ITargetDeviceProxyPtr DeviceProxy) const
{
	FTextBuilder Builder;
	Builder.AppendLineFormat(LOCTEXT("DeviceListRowToolTipName", "Name: {0}"), FText::FromString(DeviceProxy->GetName()));
	
	if (DeviceProxy->HasVariant(NAME_None))
	{	
		Builder.AppendLineFormat(LOCTEXT("DeviceListRowToolTipPlatform", "Platform: {0}"), FText::FromString(DeviceProxy->GetTargetPlatformName(NAME_None)));
		Builder.AppendLineFormat(LOCTEXT("DeviceListRowToolTipDeviceId", "Device ID: {0}"), FText::FromString(DeviceProxy->GetTargetDeviceId(NAME_None)));
	}
	else
	{
		Builder.AppendLine(LOCTEXT("InvalidDevice", "Invalid Device"));
	}

	return Builder.ToText();
}

TSharedRef<ITableRow> SProjectLauncherDeployTargets::HandleDeviceProxyListViewGenerateRow(ITargetDeviceProxyPtr InItem, const TSharedRef<STableViewBase>& OwnerTable) const
{
	check(Model->GetSelectedProfile().IsValid());

	return SNew(SProjectLauncherDeployTargetListRow, OwnerTable)
		.DeviceGroup(this, &SProjectLauncherDeployTargets::HandleDeviceListRowDeviceGroup)
		.DeviceProxy(InItem)
		.IsEnabled(this, &SProjectLauncherDeployTargets::HandleDeviceListRowIsEnabled, InItem)
		.ToolTipText(this, &SProjectLauncherDeployTargets::HandleDeviceListRowToolTipText, InItem);
}

EVisibility SProjectLauncherDeployTargets::HandleDeviceProxyListViewVisibility() const
{
	if (DeviceProxyList.Num() > 0)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

EVisibility SProjectLauncherDeployTargets::HandleNoDevicesBoxVisibility() const
{
	if (DeviceProxyList.Num() == 0)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

FText SProjectLauncherDeployTargets::HandleNoDevicesTextBlockText() const
{
	if (DeviceProxyList.Num() == 0)
	{
		return LOCTEXT("NoDevicesText", "No available devices were detected.");
	}

	return FText::GetEmpty();
}

void SProjectLauncherDeployTargets::HandleDeviceProxyManagerProxyAdded(const ITargetDeviceProxyRef& AddedProxy)
{
	RefreshDeviceProxyList();
}

void SProjectLauncherDeployTargets::HandleDeviceProxyManagerProxyRemoved(const ITargetDeviceProxyRef& RemovedProxy)
{
	RefreshDeviceProxyList();
}

#undef LOCTEXT_NAMESPACE

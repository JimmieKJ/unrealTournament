// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ProjectLauncherPrivatePCH.h"
#include "SHyperlink.h"


#define LOCTEXT_NAMESPACE "SProjectLauncherSimpleDeviceListView"


/* SProjectLauncherDeployTargets structors
 *****************************************************************************/

SProjectLauncherSimpleDeviceListView::~SProjectLauncherSimpleDeviceListView( )
{
	if (Model.IsValid())
	{
		const ITargetDeviceProxyManagerRef& DeviceProxyManager = Model->GetDeviceProxyManager();
		DeviceProxyManager->OnProxyAdded().RemoveAll(this);
		DeviceProxyManager->OnProxyRemoved().RemoveAll(this);
	}
}


/* SProjectLauncherDeployTargets interface
 *****************************************************************************/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SProjectLauncherSimpleDeviceListView::Construct(const FArguments& InArgs, const FProjectLauncherModelRef& InModel)
{
	OnProfileRun = InArgs._OnProfileRun;
	IsAdvanced = InArgs._IsAdvanced;
	
	Model = InModel;

	SAssignNew(DeviceProxyListView, SListView<ITargetDeviceProxyPtr>)
		.SelectionMode(ESelectionMode::None)
		.ListItemsSource(&DeviceProxyList)
		.OnGenerateRow(this, &SProjectLauncherSimpleDeviceListView::HandleDeviceProxyListViewGenerateRow)
		.ItemHeight(16.0f);

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SScrollBorder, DeviceProxyListView.ToSharedRef())
			[
				DeviceProxyListView.ToSharedRef()
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2, 4, 2, 4)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.TextStyle(FCoreStyle::Get(), "ToolBar.Keybinding")
				.Text(LOCTEXT("ProjectLauncherDeviceManagerLinkPreamble", "Don't see your device? Verify it's setup and claimed in the "))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.Padding(0.0f, 0.0f)
			[
				// all cultures hyper link
				SNew(SHyperlink)
				.TextStyle(FCoreStyle::Get(), "ToolBar.Keybinding")
				.OnNavigate(this, &SProjectLauncherSimpleDeviceListView::HandleDeviceManagerHyperlinkNavigate)
				.Text(LOCTEXT("ProjectLauncherDeviceManagerLink", "Device Manager."))
				.ToolTipText(LOCTEXT("ProjectLauncherDeviceManagerLinkTooltip", "Open the Device Manager window, where you can setup and claim devices connected to your machine or shared on the network."))
			]
		]
	];

	const ITargetDeviceProxyManagerRef& DeviceProxyManager = Model->GetDeviceProxyManager();

	DeviceProxyManager->OnProxyAdded().AddSP(this, &SProjectLauncherSimpleDeviceListView::HandleDeviceProxyManagerProxyAdded);
	DeviceProxyManager->OnProxyRemoved().AddSP(this, &SProjectLauncherSimpleDeviceListView::HandleDeviceProxyManagerProxyRemoved);

	DeviceProxyManager->GetProxies(NAME_None, false, DeviceProxyList);

}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


/* SProjectLauncherDeployTargets implementation
 *****************************************************************************/

void SProjectLauncherSimpleDeviceListView::RefreshDeviceProxyList()
{
	Model->GetDeviceProxyManager()->GetProxies(NAME_None, false, DeviceProxyList);
	DeviceProxyListView->RequestListRefresh();
}


/* SProjectLauncherDeployTargets callbacks
 *****************************************************************************/

bool SProjectLauncherSimpleDeviceListView::HandleDeviceListRowIsEnabled( ITargetDeviceProxyPtr DeviceProxy ) const
{
	return true;
}

void SProjectLauncherSimpleDeviceListView::HandleDeviceManagerHyperlinkNavigate() const
{
	FGlobalTabmanager::Get()->InvokeTab(FTabId("DeviceManager"));
}

FText SProjectLauncherSimpleDeviceListView::HandleDeviceListRowToolTipText( ITargetDeviceProxyPtr DeviceProxy ) const
{
	FTextBuilder Builder;
	Builder.AppendLineFormat(LOCTEXT("DeviceListRowToolTipName", "Name: {0}"), FText::FromString(DeviceProxy->GetName()));
	//Builder.AppendLineFormat(LOCTEXT("DeviceListRowToolTipPlatform", "Platform: {0}"), FText::FromString(DeviceProxy->GetTargetPlatformName(SimpleProfile->GetDeviceVariant())));
	//Builder.AppendLineFormat(LOCTEXT("DeviceListRowToolTipDeviceId", "Device ID: {0}"), FText::FromString(DeviceProxy->GetTargetDeviceId(SimpleProfile->GetDeviceVariant())));

	return Builder.ToText();
}


TSharedRef<ITableRow> SProjectLauncherSimpleDeviceListView::HandleDeviceProxyListViewGenerateRow(ITargetDeviceProxyPtr InItem, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(SProjectLauncherSimpleDeviceListRow, Model.ToSharedRef(), OwnerTable)
		.OnProfileRun(OnProfileRun)
		.IsAdvanced(IsAdvanced)
		.DeviceProxy(InItem)
		.IsEnabled(this, &SProjectLauncherSimpleDeviceListView::HandleDeviceListRowIsEnabled, InItem)
		.ToolTipText(this, &SProjectLauncherSimpleDeviceListView::HandleDeviceListRowToolTipText, InItem);
}

void SProjectLauncherSimpleDeviceListView::HandleDeviceProxyManagerProxyAdded(const ITargetDeviceProxyRef& AddedProxy)
{
	RefreshDeviceProxyList();
}

void SProjectLauncherSimpleDeviceListView::HandleDeviceProxyManagerProxyRemoved(const ITargetDeviceProxyRef& AddedProxy)
{
	RefreshDeviceProxyList();
}

#undef LOCTEXT_NAMESPACE

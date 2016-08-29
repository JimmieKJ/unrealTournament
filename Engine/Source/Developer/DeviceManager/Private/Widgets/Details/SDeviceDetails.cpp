// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DeviceManagerPrivatePCH.h"


#define LOCTEXT_NAMESPACE "SDeviceDetails"


/* SMessagingEndpoints structors
*****************************************************************************/

SDeviceDetails::~SDeviceDetails( )
{
	if (Model.IsValid())
	{
		Model->OnSelectedDeviceServiceChanged().RemoveAll(this);
	}
}


/* SDeviceDetails interface
 *****************************************************************************/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SDeviceDetails::Construct( const FArguments& InArgs, const FDeviceManagerModelRef& InModel )
{
	Model = InModel;

	ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
			[
				SNew(SVerticalBox)
					.Visibility(this, &SDeviceDetails::HandleDetailsBoxVisibility)

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(4.0f, 2.0f)
					[
						// quick info
						SAssignNew(QuickInfo, SDeviceQuickInfo)
					]

				+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(0.0f, 8.0f, 0.0f, 0.0f)
					[
						SNew(SBorder)
							.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
							.Padding(0.0f)
							[
								// feature list view
								SAssignNew(FeatureListView, SListView<FDeviceDetailsFeaturePtr>)
									.ItemHeight(24.0f)
									.ListItemsSource(&FeatureList)
									.OnGenerateRow(this, &SDeviceDetails::HandleFeatureListGenerateRow)
									.SelectionMode(ESelectionMode::None)
									.HeaderRow
									(
										SNew(SHeaderRow)

										+ SHeaderRow::Column("Feature")
											.DefaultLabel(LOCTEXT("FeatureListFeatureColumnHeader", "Feature"))
											.FillWidth(0.6f)

										+ SHeaderRow::Column("Available")
											.DefaultLabel(LOCTEXT("FeatureListAvailableColumnHeader", "Available"))
											.FillWidth(0.4f)
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
					.Visibility(this, &SDeviceDetails::HandleSelectDeviceOverlayVisibility)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("SelectSessionOverlayText", "Please select a device from the Device Browser"))
					]
			]
	];

	Model->OnSelectedDeviceServiceChanged().AddRaw(this, &SDeviceDetails::HandleModelSelectedDeviceServiceChanged);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


/* SDeviceDetails callbacks
 *****************************************************************************/

EVisibility SDeviceDetails::HandleDetailsBoxVisibility( ) const
{
	if (Model->GetSelectedDeviceService().IsValid())
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}


TSharedRef<ITableRow> SDeviceDetails::HandleFeatureListGenerateRow( FDeviceDetailsFeaturePtr Feature, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(SDeviceDetailsFeatureListRow, OwnerTable, Feature.ToSharedRef());
//		.Style(Style);
}


void SDeviceDetails::HandleModelSelectedDeviceServiceChanged( )
{
	FeatureList.Empty();

	ITargetDeviceServicePtr DeviceService = Model->GetSelectedDeviceService();

	if (DeviceService.IsValid())
	{
		ITargetDevicePtr TargetDevice = DeviceService->GetDevice();

		if (TargetDevice.IsValid())
		{
			const ITargetPlatform& TargetPlatform = TargetDevice->GetTargetPlatform();

			// platform features
			FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("AudioStreaming"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::AudioStreaming))));
			FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("DistanceFieldShadows"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::DistanceFieldShadows))));
			FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("GrayscaleSRGB"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::GrayscaleSRGB))));
			FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("HighQualityLightmaps"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::HighQualityLightmaps))));
			FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("LowQualityLightmaps"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::LowQualityLightmaps))));
			FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("MultipleGameInstances"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::MultipleGameInstances))));
			FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("Packaging"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::Packaging))));
			FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("SdkConnectDisconnect"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::SdkConnectDisconnect))));
			FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("Tessellation"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::Tessellation))));
			FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("TextureStreaming"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::TextureStreaming))));
			FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("UserCredentials"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::UserCredentials))));
			FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("MobileRendering"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::MobileRendering))));
			FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("DeferredRendering"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::DeferredRendering))));

			// device features
			FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("MultiLaunch"), TargetDevice->SupportsFeature(ETargetDeviceFeatures::MultiLaunch))));
			FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("PowerOff"), TargetDevice->SupportsFeature(ETargetDeviceFeatures::PowerOff))));
			FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("PowerOn"), TargetDevice->SupportsFeature(ETargetDeviceFeatures::PowerOn))));
			FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("ProcessSnapshot"), TargetDevice->SupportsFeature(ETargetDeviceFeatures::ProcessSnapshot))));
			FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("Reboot"), TargetDevice->SupportsFeature(ETargetDeviceFeatures::Reboot))));
		}
	}

	FeatureListView->RequestListRefresh();
	QuickInfo->SetDeviceService(DeviceService);
}


EVisibility SDeviceDetails::HandleSelectDeviceOverlayVisibility( ) const
{
	if (Model->GetSelectedDeviceService().IsValid())
	{
		return EVisibility::Hidden;
	}

	return EVisibility::Visible;
}


#undef LOCTEXT_NAMESPACE

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DeviceManagerPrivatePCH.h"


#define LOCTEXT_NAMESPACE "SDeviceBrowserDeviceAdder"


/* SDeviceBrowserDeviceAdder interface
 *****************************************************************************/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SDeviceBrowserDeviceAdder::Construct( const FArguments& InArgs, const ITargetDeviceServiceManagerRef& InDeviceServiceManager )
{
	DeviceServiceManager = InDeviceServiceManager;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)

				// platform selector
				+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Left)
							[
								SNew(STextBlock)
									.Text(LOCTEXT("PlatformLabel", "Platform:"))
							]

						+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Left)
							.Padding(0.0f, 4.0f, 0.0f, 0.0f)
							[
								SAssignNew(PlatformComboBox, SComboBox<TSharedPtr<FString> >)
									.ContentPadding(FMargin(6.0f, 2.0f))
									.OptionsSource(&PlatformList)
									.OnGenerateWidget(this, &SDeviceBrowserDeviceAdder::HandlePlatformComboBoxGenerateWidget)
									.OnSelectionChanged(this, &SDeviceBrowserDeviceAdder::HandlePlatformComboBoxSelectionChanged)
									[
										SNew(STextBlock)
											.Text(this, &SDeviceBrowserDeviceAdder::HandlePlatformComboBoxContentText)
									]
							]
					]

				// device identifier input
				+ SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SVerticalBox)
							.ToolTipText(LOCTEXT("DeviceIdToolTip", "The device's unique identifier. Depending on the selected Platform, this can be a host name, an IP address, a MAC address or some other platform specific unique identifier."))

						+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Left)
							[
								SNew(STextBlock)
									.Text(LOCTEXT("DeviceIdLabel", "Device Identifier:"))
							]

						+ SVerticalBox::Slot()
							.FillHeight(1.0)
							.Padding(0.0f, 4.0f, 0.0f, 0.0f)
							[
								SAssignNew(DeviceIdTextBox, SEditableTextBox)
							]
					]

				// device name input
				+ SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SVerticalBox)
							.ToolTipText(LOCTEXT("DeviceNameToolTip", "A display name for this device. Once the device is connected, this will be replaced with the device's actual name."))

						+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Left)
							[
								SNew(STextBlock)
									.Text(LOCTEXT("DisplayNameLabel", "Display Name:"))
							]

						+ SVerticalBox::Slot()
							.FillHeight(1.0)
							.Padding(0.0f, 4.0f, 0.0f, 0.0f)
							[
								SAssignNew(DeviceNameTextBox, SEditableTextBox)
							]
					]

				// add button
				+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Bottom)
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SAssignNew(AddButton, SButton)
							.ContentPadding(FMargin(9.0, 2.0))
							.IsEnabled(this, &SDeviceBrowserDeviceAdder::HandleAddButtonIsEnabled)
							.Text(LOCTEXT("AddButtonText", "Add"))
							.OnClicked(this, &SDeviceBrowserDeviceAdder::HandleAddButtonClicked)
					]
			]

		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
					.Visibility(this, &SDeviceBrowserDeviceAdder::HandleCredentialsBoxVisibility)

				// user name input
				+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Left)
							[
								SNew(STextBlock)
									.Text(LOCTEXT("UserNameLabel", "User:"))
							]

						+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							.Padding(0.0f, 4.0f, 0.0f, 0.0f)
							[
								SAssignNew(UserNameTextBox, SEditableTextBox)
							]
					]

				// user password input
				+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Left)
							[
								SNew(STextBlock)
									.Text(LOCTEXT("UserPasswordLabel", "Password:"))
							]

						+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							.Padding(0.0f, 4.0f, 0.0f, 0.0f)
							[
								SAssignNew(UserPasswordTextBox, SEditableTextBox)
									.IsPassword(true)
							]
					]
			]
	];

	RefreshPlatformList();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


/* SDeviceBrowserDeviceAdder implementation
 *****************************************************************************/

void SDeviceBrowserDeviceAdder::DetermineAddUnlistedButtonVisibility( )
{
	if (PlatformComboBox->GetSelectedItem().IsValid())
	{
		FString DeviceIdText = DeviceIdTextBox->GetText().ToString();
		DeviceIdText.Trim();

		AddButton->SetEnabled(!DeviceIdText.IsEmpty());
	}
}


void SDeviceBrowserDeviceAdder::RefreshPlatformList( )
{
	TArray<ITargetPlatform*> Platforms = GetTargetPlatformManager()->GetTargetPlatforms();

	PlatformList.Reset();

	for (int32 Index = 0; Index < Platforms.Num(); ++Index)
	{
		PlatformList.Add(MakeShareable(new FString(Platforms[Index]->PlatformName())));
	}

	PlatformComboBox->RefreshOptions();
}


/* SDeviceBrowserDeviceAdder event handlers
 *****************************************************************************/

FReply SDeviceBrowserDeviceAdder::HandleAddButtonClicked( )
{
	ITargetPlatform * TargetPlatform = GetTargetPlatformManager()->FindTargetPlatform(*PlatformComboBox->GetSelectedItem());

	bool bAdded = TargetPlatform->AddDevice(DeviceIdTextBox->GetText().ToString(), false);
	if (bAdded)
	{
		DeviceIdTextBox->SetText(FText::GetEmpty());
		DeviceNameTextBox->SetText(FText::GetEmpty());
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("DeviceAdderFailedToAddDeviceMessage", "Failed to add the device!"));
	}

	return FReply::Handled();
}


bool SDeviceBrowserDeviceAdder::HandleAddButtonIsEnabled( ) const
{
	TSharedPtr<FString> PlatformName = PlatformComboBox->GetSelectedItem();

	if (PlatformName.IsValid())
	{
		FString TextCheck = DeviceNameTextBox->GetText().ToString();
		TextCheck.Trim();

		if (!TextCheck.IsEmpty())
		{
			ITargetPlatform* Platform = GetTargetPlatformManager()->FindTargetPlatform(*PlatformName);

			if (Platform != nullptr)
			{
				if (!Platform->RequiresUserCredentials())
				{
					return true;
				}

				// check user/password as well
				TextCheck = UserNameTextBox->GetText().ToString();
				TextCheck.Trim();

				if (!TextCheck.IsEmpty())
				{
					// do not trim the password
					return !UserPasswordTextBox->GetText().ToString().IsEmpty();
				}
			}
		}
	}

	return false;
}


EVisibility SDeviceBrowserDeviceAdder::HandleCredentialsBoxVisibility( ) const
{
	TSharedPtr<FString> PlatformName = PlatformComboBox->GetSelectedItem();

	if (PlatformName.IsValid())
	{
		ITargetPlatform* Platform = GetTargetPlatformManager()->FindTargetPlatform(*PlatformName);

		if ((Platform != nullptr) && Platform->RequiresUserCredentials())
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}


void SDeviceBrowserDeviceAdder::HandleDeviceNameTextBoxTextChanged (const FString& Text)
{
	DetermineAddUnlistedButtonVisibility();
}


FText SDeviceBrowserDeviceAdder::HandlePlatformComboBoxContentText( ) const
{
	TSharedPtr<FString> SelectedPlatform = PlatformComboBox->GetSelectedItem();

	return SelectedPlatform.IsValid() ? FText::FromString(*SelectedPlatform) : LOCTEXT("SelectAPlatform", "Select a Platform");
}


TSharedRef<SWidget> SDeviceBrowserDeviceAdder::HandlePlatformComboBoxGenerateWidget( TSharedPtr<FString> Item )
{
	const PlatformInfo::FPlatformInfo* const PlatformInfo = PlatformInfo::FindPlatformInfo(**Item);

	return
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			[
				SNew(SBox)
					.WidthOverride(24)
					.HeightOverride(24)
					[
						SNew(SImage)
							.Image((PlatformInfo) ? FEditorStyle::GetBrush(PlatformInfo->GetIconStyleName(PlatformInfo::EPlatformIconSize::Normal)) : FStyleDefaults::GetNoBrush())
					]
			]

		+ SHorizontalBox::Slot()
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
					.Text(FText::FromString(*Item))
			];
}


void SDeviceBrowserDeviceAdder::HandlePlatformComboBoxSelectionChanged( TSharedPtr<FString> StringItem, ESelectInfo::Type SelectInfo )
{

}


#undef LOCTEXT_NAMESPACE

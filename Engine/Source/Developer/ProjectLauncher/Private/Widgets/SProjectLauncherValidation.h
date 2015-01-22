// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE "SProjectLauncherValidation"


/**
 * Implements the launcher's profile validation panel.
 */
class SProjectLauncherValidation
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SProjectLauncherValidation) { }
	SLATE_ATTRIBUTE(ILauncherProfilePtr, LaunchProfile)
	SLATE_END_ARGS()


public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 * @param InModel The data model.
	 */
	void Construct( const FArguments& InArgs)
	{
		LaunchProfileAttr = InArgs._LaunchProfile;

		ChildSlot
		[
			SNew(SVerticalBox)
			
			// build settings
			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("NoBuildGameSelectedError", "A Project must be selected.").ToString(), ELauncherProfileValidationErrors::NoProjectSelected)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("NoBuildConfigurationSelectedError", "A Build Configuration must be selected.").ToString(), ELauncherProfileValidationErrors::NoBuildConfigurationSelected)
				]

			// cook settings
			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("NoCookedPlatformSelectedError", "At least one Platform must be selected when cooking by the book.").ToString(), ELauncherProfileValidationErrors::NoPlatformSelected)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("NoCookedCulturesSelectedError", "At least one Culture must be selected when cooking by the book.").ToString(), ELauncherProfileValidationErrors::NoCookedCulturesSelected)
				]

			// packaging settings

			// deployment settings
			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("CopyToDeviceRequiresCookByTheBookError", "Deployment by copying to device requires 'By The Book' cooking.").ToString(), ELauncherProfileValidationErrors::CopyToDeviceRequiresCookByTheBook)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("DeployedDeviceGroupRequired", "A device group must be selected when deploying builds.").ToString(), ELauncherProfileValidationErrors::DeployedDeviceGroupRequired)
				]

			// launch settings
			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("CustomRolesNotSupportedYet", "Custom launch roles are not supported yet.").ToString(), ELauncherProfileValidationErrors::CustomRolesNotSupportedYet)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("InitialCultureNotAvailable", "The Initial Culture selected for launch is not in the build.").ToString(), ELauncherProfileValidationErrors::InitialCultureNotAvailable)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("InitialMapNotAvailable", "The Initial Map selected for launch is not in the build.").ToString(), ELauncherProfileValidationErrors::InitialMapNotAvailable)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeValidationMessage(TEXT("Icons.Error"), LOCTEXT("NoLaunchRoleDeviceAssigned", "One or more launch roles do not have a device assigned.").ToString(), ELauncherProfileValidationErrors::NoLaunchRoleDeviceAssigned)
				]
		 
			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeCallbackMessage(TEXT("Icons.Error"), ELauncherProfileValidationErrors::NoPlatformSDKInstalled)
				]
		];
	}

protected:

	/**
	 * Creates a widget for a validation message.
	 *
	 * @param IconName The name of the message icon.
	 * @param MessageText The message text.
	 * @param MessageType The message type.
	 */
	TSharedRef<SWidget> MakeValidationMessage( const TCHAR* IconName, const FString& MessageText, ELauncherProfileValidationErrors::Type Message )
	{
		return SNew(SHorizontalBox)
			.Visibility(this, &SProjectLauncherValidation::HandleValidationMessageVisibility, Message)

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0)
			[
				SNew(SImage)
					.Image(FEditorStyle::GetBrush(IconName))
			]

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
					.Text(MessageText)
			];
	}

	/**
	 * Creates a widget for a validation message.
	 *
	 * @param IconName The name of the message icon.
	 * @param MessageType The message type.
	 */
	TSharedRef<SWidget> MakeCallbackMessage( const TCHAR* IconName,ELauncherProfileValidationErrors::Type Message )
	{
		return SNew(SHorizontalBox)
			.Visibility(this, &SProjectLauncherValidation::HandleValidationMessageVisibility, Message)

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0)
			[
				SNew(SImage)
					.Image(FEditorStyle::GetBrush(IconName))
			]

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
					.Text(this, &SProjectLauncherValidation::HandleValidationMessage, Message)
			];
	}

private:

	// Callback for getting the visibility state of a validation message.
	EVisibility HandleValidationMessageVisibility( ELauncherProfileValidationErrors::Type Error ) const
	{
		ILauncherProfilePtr LaunchProfile = LaunchProfileAttr.Get();
		if (!LaunchProfile.IsValid() || LaunchProfile->HasValidationError(Error))
		{
			return EVisibility::Visible;
		}
		return EVisibility::Collapsed;
	}

	FString	HandleValidationMessage( ELauncherProfileValidationErrors::Type Error ) const
	{
		ILauncherProfilePtr LaunchProfile = LaunchProfileAttr.Get();
		if (LaunchProfile.IsValid())
		{
			if (LaunchProfile->HasValidationError(Error))
			{
				return LOCTEXT("NoPlatformSDKInstalled", "A required platform SDK is mising: ").ToString() + LaunchProfile->GetInvalidPlatform();
			}

			return TEXT("");
		}
		return LOCTEXT("InvalidLaunchProfile", "Invalid Launch Profile.").ToString();
	}

private:

	// Attribute for the launch profile this widget shows validation for. 
	TAttribute<ILauncherProfilePtr> LaunchProfileAttr;
};


#undef LOCTEXT_NAMESPACE

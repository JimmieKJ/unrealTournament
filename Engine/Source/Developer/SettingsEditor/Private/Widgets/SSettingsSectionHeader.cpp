// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "SettingsEditorPrivatePCH.h"
#include "SSettingsSectionHeader.h"
#include "NotificationManager.h"
#include "SNotificationList.h"
#include "SSettingsEditorCheckoutNotice.h"
#include "DesktopPlatformModule.h"
#include "IDetailsView.h"

#define LOCTEXT_NAMESPACE "SSettingsEditor"

void SSettingsSectionHeader::Construct(const FArguments& InArgs, const UObject* InSettingsObject, ISettingsEditorModelPtr InModel, TSharedPtr<IDetailsView> InDetailsView)
{
	Model = InModel;
	SettingsObject = InSettingsObject;
	SettingsSection = Model->GetSectionFromSectionObject(InSettingsObject);
	DetailsView = InDetailsView;

	Model->OnSelectionChanged().AddSP(this, &SSettingsSectionHeader::OnSettingsSelectionChanged);

	// Create the watcher widget for the default config file (checks file status / SCC state)
	FileWatcherWidget =
		SNew(SSettingsEditorCheckoutNotice)
		.Visibility(this, &SSettingsSectionHeader::HandleCheckoutNoticeVisibility)
		.OnFileProbablyModifiedExternally(this, &SSettingsSectionHeader::HandleCheckoutNoticeFileProbablyModifiedExternally)
		.ConfigFilePath(this, &SSettingsSectionHeader::HandleCheckoutNoticeConfigFilePath);


	ChildSlot
	.Padding(FMargin(0.0f, 8.0f, 0.0f, 5.0f))
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					// category title
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("SettingsEditor.CatgoryAndSectionFont"))
					.Text(GetSettingsBoxTitleText())
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					// category description
					SNew(STextBlock)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Text(GetSettingsBoxDescriptionText())
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(16.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SSettingsSectionHeader::HandleButtonRowVisibility)
				+SHorizontalBox::Slot()
				[
					// set as default button
					SNew(SButton)
					.Visibility(this, &SSettingsSectionHeader::HandleSetAsDefaultButtonVisibility)
					.IsEnabled(this, &SSettingsSectionHeader::HandleSetAsDefaultButtonEnabled)
					.OnClicked(this, &SSettingsSectionHeader::HandleSetAsDefaultButtonClicked)
					.Text(LOCTEXT("SaveDefaultsButtonText", "Set as Default"))
					.ToolTipText(LOCTEXT("SaveDefaultsButtonTooltip", "Save the values below as the new default settings"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					// export button
					SNew(SButton)
					.IsEnabled(this, &SSettingsSectionHeader::HandleExportButtonEnabled)
					.OnClicked(this, &SSettingsSectionHeader::HandleExportButtonClicked)
					.Text(LOCTEXT("ExportButtonText", "Export..."))
					.ToolTipText(LOCTEXT("ExportButtonTooltip", "Export these settings to a file on your computer"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					// import button
					SNew(SButton)
					.IsEnabled(this, &SSettingsSectionHeader::HandleImportButtonEnabled)
					.OnClicked(this, &SSettingsSectionHeader::HandleImportButtonClicked)
					.Text(LOCTEXT("ImportButtonText", "Import..."))
					.ToolTipText(LOCTEXT("ImportButtonTooltip", "Import these settings from a file on your computer"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					// reset defaults button
					SNew(SButton)
					.Visibility(this, &SSettingsSectionHeader::HandleSetAsDefaultButtonVisibility)
					.IsEnabled(this, &SSettingsSectionHeader::HandleResetToDefaultsButtonEnabled)
					.OnClicked(this, &SSettingsSectionHeader::HandleResetDefaultsButtonClicked)
					.Text(LOCTEXT("ResetDefaultsButtonText", "Reset to Defaults"))
					.ToolTipText(LOCTEXT("ResetDefaultsButtonTooltip", "Reset the settings below to their default values"))
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			FileWatcherWidget.ToSharedRef()
		]
	];

}

FText SSettingsSectionHeader::GetSettingsBoxTitleText() const
{
	if(SettingsSection.IsValid())
	{
		static const FText TitleFmt = FText::FromString(TEXT("{0} - {1}"));
		return FText::Format(TitleFmt, SettingsSection->GetCategory().Pin()->GetDisplayName(), SettingsSection->GetDisplayName());
	}

	return FText::GetEmpty();
}

FText SSettingsSectionHeader::GetSettingsBoxDescriptionText() const
{
	if(SettingsSection.IsValid())
	{
		return SettingsSection->GetDescription();
	}

	return FText::GetEmpty();
}

EVisibility SSettingsSectionHeader::HandleButtonRowVisibility() const
{
	return DetailsView.Pin()->HasActiveSearch() ? EVisibility::Hidden : EVisibility::Visible;
}

FReply SSettingsSectionHeader::HandleExportButtonClicked()
{
	if(SettingsSection.IsValid())
	{
		if(LastExportDir.IsEmpty())
		{
			LastExportDir = FPaths::GetPath(GEditorPerProjectIni);
		}

		FString DefaultFileName = FString::Printf(TEXT("%s Backup %s.ini"), *SettingsSection->GetDisplayName().ToString(), *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H%M%S")));
		TArray<FString> OutFiles;

		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

		if(FDesktopPlatformModule::Get()->SaveFileDialog(ParentWindowHandle, LOCTEXT("ExportSettingsDialogTitle", "Export settings...").ToString(), LastExportDir, DefaultFileName, TEXT("Config files (*.ini)|*.ini"), EFileDialogFlags::None, OutFiles))
		{
			if(SettingsSection->Export(OutFiles[0]))
			{
				ShowNotification(LOCTEXT("ExportSettingsSuccess", "Export settings succeeded"), SNotificationItem::CS_Success);
			}
			else
			{
				ShowNotification(LOCTEXT("ExportSettingsFailure", "Export settings failed"), SNotificationItem::CS_Fail);
			}
		}
	}

	return FReply::Handled();
}


bool SSettingsSectionHeader::HandleExportButtonEnabled() const
{
	if(SettingsSection.IsValid())
	{
		return SettingsSection->CanExport();
	}

	return false;
}


FReply SSettingsSectionHeader::HandleImportButtonClicked()
{
	if(SettingsSection.IsValid())
	{
		TArray<FString> OutFiles;

		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

		if(FDesktopPlatformModule::Get()->OpenFileDialog(ParentWindowHandle, LOCTEXT("ImportSettingsDialogTitle", "Import settings...").ToString(), FPaths::GetPath(GEditorPerProjectIni), TEXT(""), TEXT("Config files (*.ini)|*.ini"), EFileDialogFlags::None, OutFiles))
		{
			if(SettingsSection->Import(OutFiles[0]) && SettingsSection->Save())
			{
				ShowNotification(LOCTEXT("ImportSettingsSuccess", "Import settings succeeded"), SNotificationItem::CS_Success);
			}
			else
			{
				ShowNotification(LOCTEXT("ImportSettingsFailure", "Import settings failed"), SNotificationItem::CS_Fail);
			}
		}
	}

	return FReply::Handled();
}


bool SSettingsSectionHeader::HandleImportButtonEnabled() const
{
	if(SettingsSection.IsValid())
	{
		return SettingsSection->CanEdit() && SettingsSection->CanImport() && !IsDefaultConfigCheckOutNeeded();
	}

	return false;
}


/**
* Gets the absolute path to the Default.ini for the specified object.
*
* @return The path to the file.
*/
FString SSettingsSectionHeader::GetDefaultConfigFilePath() const
{
	FString RelativeConfigFilePath = SettingsObject->GetDefaultConfigFilename();
	return FPaths::ConvertRelativePathToFull(RelativeConfigFilePath);
}


/**
* Checks whether the default config file needs to be checked out for editing.
*
* @return true if the file needs to be checked out, false otherwise.
*/
bool SSettingsSectionHeader::IsDefaultConfigCheckOutNeeded() const
{
	if(SettingsObject.IsValid() && SettingsObject->GetClass()->HasAnyClassFlags(CLASS_Config | CLASS_DefaultConfig))
	{
		return !FileWatcherWidget->IsUnlocked();
	}
	else
	{
		return false;
	}
}

FReply SSettingsSectionHeader::HandleResetDefaultsButtonClicked()
{
	if(SettingsSection.IsValid())
	{
		SettingsSection->ResetDefaults();
	}

	return FReply::Handled();
}


bool SSettingsSectionHeader::HandleResetToDefaultsButtonEnabled() const
{
	if(SettingsSection.IsValid())
	{
		return (SettingsSection->CanEdit() && SettingsSection->CanResetDefaults());
	}

	return false;
}

EVisibility SSettingsSectionHeader::HandleSetAsDefaultButtonVisibility() const
{
	return (SettingsSection.IsValid() && SettingsSection->HasDefaultSettingsObject()) ? EVisibility::Collapsed : EVisibility::Visible;
}

FReply SSettingsSectionHeader::HandleSetAsDefaultButtonClicked()
{
	if(SettingsSection.IsValid())
	{
		if(FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("SaveAsDefaultUserConfirm", "Are you sure you want to update the default settings?")) != EAppReturnType::Yes)
		{
			return FReply::Handled();
		}

		if(IsDefaultConfigCheckOutNeeded())
		{
			if(ISourceControlModule::Get().IsEnabled())
			{
				if(FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("SaveAsDefaultNeedsCheckoutMessage", "The default configuration file for these settings is currently not checked out. Would you like to check it out from source control? \n\nIf checked into source control this would affect other developers.")) != EAppReturnType::Yes)
				{
					return FReply::Handled();
				}

				CheckOutDefaultConfigFile();
			}
			else
			{
				if(FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("SaveAsDefaultsIsReadOnlyMessage", "The default configuration file for these settings is currently not writeable. Would you like to make it writable?")) != EAppReturnType::Yes)
				{
					return FReply::Handled();
				}

				MakeDefaultConfigFileWritable();
			}
		}

		SettingsSection->SaveDefaults();

		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SaveAsDefaultsSucceededMessage", "The default configuration file for these settings was updated successfully. \n\nIf checked into source control this would affect other developers."));
	}

	return FReply::Handled();
}


bool SSettingsSectionHeader::HandleSetAsDefaultButtonEnabled() const
{
	if(SettingsSection.IsValid())
	{
		return SettingsSection->CanSaveDefaults();
	}

	return false;
}

/**
* Checks out the default configuration file for the currently selected settings object.
*
* @return true if the check-out succeeded, false otherwise.
*/
bool SSettingsSectionHeader::CheckOutDefaultConfigFile()
{
	if(!SettingsObject.IsValid())
	{
		return false;
	}

	FText ErrorMessage;

	// check out configuration file
	FString AbsoluteConfigFilePath = GetDefaultConfigFilePath();
	TArray<FString> FilesToBeCheckedOut;
	FilesToBeCheckedOut.Add(AbsoluteConfigFilePath);

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(AbsoluteConfigFilePath, EStateCacheUsage::ForceUpdate);

	if(SourceControlState.IsValid())
	{
		if(SourceControlState->IsDeleted())
		{
			ErrorMessage = LOCTEXT("ConfigFileMarkedForDeleteError", "Error: The configuration file is marked for deletion.");
		}
		else if(!SourceControlState->IsCurrent())
		{
			if(false)
			{
				if(SourceControlProvider.Execute(ISourceControlOperation::Create<FSync>(), FilesToBeCheckedOut) == ECommandResult::Succeeded)
				{
					SettingsObject->ReloadConfig();
				}
				else
				{
					ErrorMessage = LOCTEXT("FailedToSyncConfigFileError", "Error: Failed to sync the configuration file to head revision.");
				}
			}
		}
		else if(SourceControlState->CanCheckout() || SourceControlState->IsCheckedOutOther())
		{
			if(SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), FilesToBeCheckedOut) == ECommandResult::Failed)
			{
				ErrorMessage = LOCTEXT("FailedToCheckOutConfigFileError", "Error: Failed to check out the configuration file.");
			}
		}
	}

	// show errors, if any
	if(!ErrorMessage.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);

		return false;
	}

	return true;
}

/**
* Makes the default configuration file for the currently selected settings object writable.
*
* @return true if it was made writable, false otherwise.
*/
bool SSettingsSectionHeader::MakeDefaultConfigFileWritable()
{
	if(!SettingsObject.IsValid())
	{
		return false;
	}

	FString AbsoluteConfigFilePath = GetDefaultConfigFilePath();

	return FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*AbsoluteConfigFilePath, false);
}

void SSettingsSectionHeader::ShowNotification(const FText& Text, SNotificationItem::ECompletionState CompletionState) const
{
	FNotificationInfo Notification(Text);
	Notification.ExpireDuration = 3.f;
	Notification.bUseSuccessFailIcons = false;

	FSlateNotificationManager::Get().AddNotification(Notification)->SetCompletionState(CompletionState);
}

/** Returns the config file name currently being edited. */
FString SSettingsSectionHeader::HandleCheckoutNoticeConfigFilePath() const
{
	if(SettingsObject.IsValid() && SettingsObject->GetClass()->HasAnyClassFlags(CLASS_Config | CLASS_DefaultConfig))
	{
		return GetDefaultConfigFilePath();
	}
	else
	{
		return FString();
	}
}

/** Reloads the configuration object. */
void SSettingsSectionHeader::HandleCheckoutNoticeFileProbablyModifiedExternally()
{
	if(SettingsObject.IsValid() && SettingsObject->GetClass()->HasAnyClassFlags(CLASS_Config | CLASS_DefaultConfig))
	{
		SettingsObject->ReloadConfig();
	}
}

/** Callback for determining the visibility of the 'Locked' notice. */
EVisibility SSettingsSectionHeader::HandleCheckoutNoticeVisibility() const
{
	if(SettingsObject.IsValid() && SettingsObject->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig) && !DetailsView.Pin()->HasActiveSearch())
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

void SSettingsSectionHeader::OnSettingsSelectionChanged()
{
	FileWatcherWidget->Invalidate();
}


FSettingsDetailRootObjectCustomization::FSettingsDetailRootObjectCustomization(ISettingsEditorModelPtr InModel, TSharedRef<IDetailsView> InDetailsView)
	: Model(InModel)
	, DetailsView(InDetailsView)
{
}

void FSettingsDetailRootObjectCustomization::Initialize()
{
	Model->OnSelectionChanged().AddSP(this, &FSettingsDetailRootObjectCustomization::OnSelectedSectionChanged);
	// Call once to ensure we have a valid section object
	OnSelectedSectionChanged();
}

TSharedPtr<SWidget> FSettingsDetailRootObjectCustomization::CustomizeObjectHeader(const UObject* InRootObject)
{
	return SNew(SSettingsSectionHeader, InRootObject, Model, DetailsView.Pin());
}

bool FSettingsDetailRootObjectCustomization::IsObjectVisible(const UObject* InRootObject) const
{
	return SelectedSectionObject == nullptr || SelectedSectionObject == InRootObject || DetailsView.Pin()->HasActiveSearch();
}

bool FSettingsDetailRootObjectCustomization::ShouldDisplayHeader(const UObject* InRootObject) const
{
	return true;
}

void FSettingsDetailRootObjectCustomization::OnSelectedSectionChanged()
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();
	if(SelectedSection.IsValid())
	{
		SelectedSectionObject = SelectedSection->GetSettingsObject();
	}
	else
	{
		SelectedSectionObject = nullptr;
	}

	DetailsView.Pin()->RefreshRootObjectVisibility();
}

#undef LOCTEXT_NAMESPACE
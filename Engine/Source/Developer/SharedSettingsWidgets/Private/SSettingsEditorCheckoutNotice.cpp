// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SharedSettingsWidgetsPrivatePCH.h"
#include "EditorStyle.h"
#include "SSettingsEditorCheckoutNotice.h"
#include "ISourceControlModule.h"
#include "SWidgetSwitcher.h"
#include "SThrobber.h"
#include "SNotificationList.h"
#include "NotificationManager.h"


#define LOCTEXT_NAMESPACE "SSettingsEditorCheckoutNotice"


/* SSettingsEditorCheckoutNotice interface
 *****************************************************************************/

void SSettingsEditorCheckoutNotice::Construct( const FArguments& InArgs )
{
	OnFileProbablyModifiedExternally = InArgs._OnFileProbablyModifiedExternally;
	ConfigFilePath = InArgs._ConfigFilePath;

	LastDefaultConfigCheckOutTime = 0.0;
	DefaultConfigCheckOutNeeded = false;
	DefaultConfigQueryInProgress = true;

	const int Padding = 8;
	// default configuration notice
	ChildSlot
	[
		SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("SettingsEditor.CheckoutWarningBorder"))
			.BorderBackgroundColor(this, &SSettingsEditorCheckoutNotice::HandleBorderBackgroundColor)
			[
				SNew(SWidgetSwitcher)
					.WidgetIndex(this, &SSettingsEditorCheckoutNotice::HandleNoticeSwitcherWidgetIndex)

				// Locked slot
				+ SWidgetSwitcher::Slot()
					[
						SNew(SHorizontalBox)

						// Locked icon
						+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(Padding)
							.VAlign(VAlign_Center)
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("GenericLock"))
							]

						// Locked notice
						+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(FMargin(0.f, Padding, Padding, Padding))
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
									.Text(this, &SSettingsEditorCheckoutNotice::HandleLockedStatusText)
									.Font(FEditorStyle::GetFontStyle("PropertyWindow.NormalFont"))
									.ShadowColorAndOpacity(FLinearColor::Black.CopyWithNewOpacity(0.3f))
									.ShadowOffset(FVector2D::UnitVector)
							]

						// Check out button
						+ SHorizontalBox::Slot()
							.Padding(FMargin(0.f, 0.f, Padding, 0.f))
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SButton)
									.OnClicked(this, &SSettingsEditorCheckoutNotice::HandleCheckOutButtonClicked)
									.Text(this, &SSettingsEditorCheckoutNotice::HandleCheckOutButtonText)
									.ToolTipText(this, &SSettingsEditorCheckoutNotice::HandleCheckOutButtonToolTip)
									.Visibility(this, &SSettingsEditorCheckoutNotice::HandleCheckOutButtonVisibility)
							]

						// Source control status throbber
						+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0.0f, 1.0f)
							[
								SNew(SThrobber)
									.Visibility(this, &SSettingsEditorCheckoutNotice::HandleThrobberVisibility)
							]
					]

				// Unlocked slot
				+ SWidgetSwitcher::Slot()
					[
						SNew(SHorizontalBox)

						// Unlocked icon
						+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(Padding)
							.VAlign(VAlign_Center)
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("GenericUnlock"))
							]

						// Unlocked notice
						+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(FMargin(0.f, Padding, Padding, Padding))
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
									.Font(FEditorStyle::GetFontStyle("PropertyWindow.NormalFont"))
									.Text(this, &SSettingsEditorCheckoutNotice::HandleUnlockedStatusText)
							]
					]
			]
	];
}


/* SSettingsEditorCheckoutNotice callbacks
 *****************************************************************************/

FReply SSettingsEditorCheckoutNotice::HandleCheckOutButtonClicked()
{
	FString TargetFilePath = ConfigFilePath.Get();

	if (ISourceControlModule::Get().IsEnabled())
	{
		FText ErrorMessage;

		if (!SourceControlHelpers::CheckoutOrMarkForAdd(TargetFilePath, FText::FromString(TargetFilePath), NULL, ErrorMessage))
		{
			FNotificationInfo Info(ErrorMessage);
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}
	else
	{
		if (!FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*TargetFilePath, false))
		{
			FText NotificationErrorText = FText::Format(LOCTEXT("FailedToMakeWritable", "Could not make {0} writable."), FText::FromString(TargetFilePath));

			FNotificationInfo Info(NotificationErrorText);
			Info.ExpireDuration = 3.0f;

			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}

	return FReply::Handled();
}


FText SSettingsEditorCheckoutNotice::HandleCheckOutButtonText( ) const
{
	if (ISourceControlModule::Get().IsEnabled() && ISourceControlModule::Get().GetProvider().IsAvailable())
	{
		return LOCTEXT("CheckOutFile", "Check Out File");
	}

	return LOCTEXT("MakeWritable", "Make Writable");
}


FText SSettingsEditorCheckoutNotice::HandleCheckOutButtonToolTip( ) const
{
	if (ISourceControlModule::Get().IsEnabled() && ISourceControlModule::Get().GetProvider().IsAvailable())
	{
		return LOCTEXT("CheckOutFileTooltip", "Check out the default configuration file that holds these settings.");
	}

	return LOCTEXT("MakeWritableTooltip", "Make the default configuration file that holds these settings writable.");
}

EVisibility SSettingsEditorCheckoutNotice::HandleCheckOutButtonVisibility() const
{
	// Display for checking out the file, or for making writable
	if ((ISourceControlModule::Get().IsEnabled() && ISourceControlModule::Get().GetProvider().IsAvailable() && !DefaultConfigQueryInProgress) || 
		(!ISourceControlModule::Get().IsEnabled() && (DefaultConfigQueryInProgress || DefaultConfigCheckOutNeeded)))
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}

FText SSettingsEditorCheckoutNotice::HandleLockedStatusText() const
{
	FText ConfigFilename = FText::FromString(FPaths::GetCleanFilename(ConfigFilePath.Get()));

	if (DefaultConfigQueryInProgress)
	{
		return FText::Format(LOCTEXT("DefaultSettingsNotice_Source", "These settings are saved in {0}. Checking file state..."), ConfigFilename);
	}
	
	return FText::Format(ISourceControlModule::Get().IsEnabled() ?
		LOCTEXT("DefaultSettingsNotice_WithSourceControl", "These settings are saved in {0}, which is currently NOT checked out.") :
		LOCTEXT("DefaultSettingsNotice_Source", "These settings are saved in {0}, which is currently NOT writable."), ConfigFilename);
}

FText SSettingsEditorCheckoutNotice::HandleUnlockedStatusText() const
{
	FText ConfigFilename = FText::FromString(FPaths::GetCleanFilename(ConfigFilePath.Get()));
	
	return FText::Format(ISourceControlModule::Get().IsEnabled() ?
		LOCTEXT("DefaultSettingsNotice_CheckedOut", "These settings are saved in {0}, which is currently checked out.") :
		LOCTEXT("DefaultSettingsNotice_Writable", "These settings are saved in {0}, which is currently writable."), ConfigFilename);
}


EVisibility SSettingsEditorCheckoutNotice::HandleThrobberVisibility() const
{
	if(ISourceControlModule::Get().IsEnabled())
	{
		return DefaultConfigQueryInProgress ? EVisibility::Visible : EVisibility::Collapsed;
	}
	return EVisibility::Collapsed;
}

FSlateColor SSettingsEditorCheckoutNotice::HandleBorderBackgroundColor() const
{
	static FColor Orange(166, 137, 0);

	const FLinearColor FinalColor = (IsUnlocked() || DefaultConfigQueryInProgress) ? FColor(60, 60, 60) : Orange;
	return FinalColor;
}

void SSettingsEditorCheckoutNotice::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// cache selected settings object's configuration file state (purposely not in an active timer to make sure it appears while the user is active)
	if (InCurrentTime - LastDefaultConfigCheckOutTime >= 1.0f)
	{
		bool NewCheckOutNeeded = false;

		DefaultConfigQueryInProgress = true;
		FString CachedConfigFileName = ConfigFilePath.Get();
		if (!CachedConfigFileName.IsEmpty())
		{
			if (ISourceControlModule::Get().IsEnabled())
			{
				// note: calling QueueStatusUpdate often does not spam status updates as an internal timer prevents this
				ISourceControlModule::Get().QueueStatusUpdate(CachedConfigFileName);

				ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
				FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(CachedConfigFileName, EStateCacheUsage::Use);
				NewCheckOutNeeded = SourceControlState.IsValid() && SourceControlState->CanCheckout();
				DefaultConfigQueryInProgress = SourceControlState.IsValid() && SourceControlState->IsUnknown();
			}
			else
			{
				NewCheckOutNeeded = (FPaths::FileExists(CachedConfigFileName) && IFileManager::Get().IsReadOnly(*CachedConfigFileName));
				DefaultConfigQueryInProgress = false;
			}

			// file has been checked in or reverted
			if ((NewCheckOutNeeded == true) && (DefaultConfigCheckOutNeeded == false))
			{
				OnFileProbablyModifiedExternally.ExecuteIfBound();
			}
		}

		DefaultConfigCheckOutNeeded = NewCheckOutNeeded;
		LastDefaultConfigCheckOutTime = InCurrentTime;
	}
}


bool SSettingsEditorCheckoutNotice::IsUnlocked() const
{
	return !DefaultConfigCheckOutNeeded && !DefaultConfigQueryInProgress;
}

#undef LOCTEXT_NAMESPACE

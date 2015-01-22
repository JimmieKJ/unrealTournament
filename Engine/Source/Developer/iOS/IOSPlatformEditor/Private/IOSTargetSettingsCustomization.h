// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorStyle.h"
#include "PropertyEditorModule.h"
#include "IOSRuntimeSettings.h"

//////////////////////////////////////////////////////////////////////////
// FIOSTargetSettingsCustomization

class FIOSTargetSettingsCustomization : public IDetailCustomization
{
public:
	// Makes a new instance of this detail layout class for a specific detail view requesting it
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	// End of IDetailCustomization interface

private:
	TArray<struct FPlatformIconInfo> IconNames;
	TArray<struct FPlatformIconInfo> LaunchImageNames;

	const FString EngineInfoPath;
	const FString GameInfoPath;

	const FString EngineGraphicsPath;
	const FString GameGraphicsPath;

	IDetailLayoutBuilder* SavedLayoutBuilder;

	// Is the manifest writable?
	TAttribute<bool> SetupForPlatformAttribute;

	bool bProvisionInstalled;
	bool bCertificateInstalled;

	TSharedPtr<FMonitoredProcess> IPPProcess;

private:
	FIOSTargetSettingsCustomization();

	void BuildPListSection(IDetailLayoutBuilder& DetailLayout);
	void BuildIconSection(IDetailLayoutBuilder& DetailLayout);
	void BuildRemoteBuildingSection(IDetailLayoutBuilder& DetailLayout);

	// Navigates to the plist in explorer or finder
	FReply OpenPlistFolder();

	// Copies the setup files for the platform into the project
	void CopySetupFilesIntoProject();

	// Called when a property that is really stored in the plist is modified
	void OnPlistPropertyModified();

	// Builds an image row
	void BuildImageRow(class IDetailLayoutBuilder& DetailLayout, class IDetailCategoryBuilder& Category, const struct FPlatformIconInfo& Info, const FVector2D& MaxDisplaySize);

	// Install the provision
 	FReply OnInstallProvisionClicked();

	// Install the provision
	FReply OnInstallCertificateClicked();

	// certificate request
	FReply OnCertificateRequestClicked();

	// Get the image to display for the provision status
	const FSlateBrush* GetProvisionStatus() const;

	// Get the image to display for the certificate status
	const FSlateBrush* GetCertificateStatus() const;

	// Update the provision status
	void UpdateStatus();

	// status tick delay
	bool UpdateStatusDelegate(float delay);
};

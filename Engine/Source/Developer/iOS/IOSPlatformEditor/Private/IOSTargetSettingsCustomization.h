// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorStyle.h"
#include "PropertyEditorModule.h"
#include "IOSRuntimeSettings.h"

//////////////////////////////////////////////////////////////////////////
// FProvision structure
class FProvision
{
public:
	FString Name;
	FString FileName;
	FString Status;
	bool bDistribution;
	bool bSelected;
};
typedef TSharedPtr<class FProvision> ProvisionPtr;

//////////////////////////////////////////////////////////////////////////
// FCertificate structure
class FCertificate
{
public:
	FString Name;
	FString Status;
	FString Expires;
	bool bSelected;
};
typedef TSharedPtr<class FCertificate> CertificatePtr;

//////////////////////////////////////////////////////////////////////////
// FIOSTargetSettingsCustomization

class FIOSTargetSettingsCustomization : public IDetailCustomization
{
public:
	~FIOSTargetSettingsCustomization();

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
	bool bShowAllProvisions;
	bool bShowAllCertificates;

	TSharedPtr<FMonitoredProcess> IPPProcess;
	FDelegateHandle TickerHandle;
	TArray<ProvisionPtr> ProvisionList;
	TArray<ProvisionPtr> FilteredProvisionList;
	TSharedPtr<SListView<ProvisionPtr> > ProvisionListView;
	TSharedPtr<SWidgetSwitcher > ProvisionInfoSwitcher;
	TArray<CertificatePtr> CertificateList;
	TArray<CertificatePtr> FilteredCertificateList;
	TSharedPtr<SListView<CertificatePtr> > CertificateListView;
	TSharedPtr<SWidgetSwitcher > CertificateInfoSwitcher;
	TAttribute<bool> RunningIPPProcess;

	FString SelectedProvision;
	FString SelectedFile;
	FString SelectedCert;

private:
	FIOSTargetSettingsCustomization();

	void BuildPListSection(IDetailLayoutBuilder& DetailLayout);
	void BuildIconSection(IDetailLayoutBuilder& DetailLayout);
	void BuildRemoteBuildingSection(IDetailLayoutBuilder& DetailLayout);

	// Navigates to the plist in explorer or finder
	FReply OpenPlistFolder();

	// Copies the setup files for the platform into the project
	void CopySetupFilesIntoProject();

	// Builds an image row
	void BuildImageRow(class IDetailLayoutBuilder& DetailLayout, class IDetailCategoryBuilder& Category, const struct FPlatformIconInfo& Info, const FVector2D& MaxDisplaySize);

	// Install the provision
 	FReply OnInstallProvisionClicked();

	// Install the provision
	FReply OnInstallCertificateClicked();

	// certificate request
	FReply OnCertificateRequestClicked();

	// ssh key request
	FReply OnGenerateSSHKey();

	// Get the image to display for the provision status
	const FSlateBrush* GetProvisionStatus() const;

	// Get the image to display for the certificate status
	const FSlateBrush* GetCertificateStatus() const;

	// find the installed certificates and provisions
	void FindRequiredFiles();

	// Update the provision status
	void UpdateStatus();

	// Update the ssh key status
	void UpdateSSHStatus();

	// status tick delay
	bool UpdateStatusDelegate(float delay);

	// handle provision list generate row
	TSharedRef<ITableRow> HandleProvisionListGenerateRow( ProvisionPtr Provision, const TSharedRef<STableViewBase>& OwnerTable );

	// handle certificate list generate row
	TSharedRef<ITableRow> HandleCertificateListGenerateRow( CertificatePtr Certificate, const TSharedRef<STableViewBase>& OwnerTable );

	// handle which set of provisions to view
	void HandleAllProvisionsHyperlinkNavigate( bool AllProvisions );

	// handle which set of provisions to view
	void HandleAllCertificatesHyperlinkNavigate( bool AllCertificates );

	// filter the lists based on the settings
	void FilterLists();

	// returns whether we are importing or not
	bool IsImportEnabled() const;

	// updates the text in the ini file and checks for a valid provision/certificate
	void OnBundleIdentifierChanged(const FText& NewText, ETextCommit::Type, TSharedRef<IPropertyHandle> InPropertyHandle);

	// updates the text in the ini file and checks for a valid provision/certificate
	void OnRemoteServerChanged(const FText& NewText, ETextCommit::Type, TSharedRef<IPropertyHandle> InPropertyHandle);

	// 
	FText GetBundleText(TSharedRef<IPropertyHandle> InPropertyHandle) const;
};

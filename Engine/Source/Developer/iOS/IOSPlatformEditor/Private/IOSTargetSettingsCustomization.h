// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
	bool bManuallySelected;
};
typedef TSharedPtr<class FProvision> ProvisionPtr;
typedef TSharedPtr<TArray<ProvisionPtr>> ProvisionListPtr;

//////////////////////////////////////////////////////////////////////////
// FCertificate structure
class FCertificate
{
public:
	FString Name;
	FString Status;
	FString Expires;
	bool bSelected;
	bool bManuallySelected;
};
typedef TSharedPtr<class FCertificate> CertificatePtr;
typedef TSharedPtr<TArray<CertificatePtr>> CertificateListPtr;

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
	bool bManuallySelected;

	TSharedPtr<FMonitoredProcess> IPPProcess;
	FDelegateHandle TickerHandle;
	TSharedPtr<TArray<ProvisionPtr>> ProvisionList;
	TArray<ProvisionPtr> FilteredProvisionList;
	TSharedPtr<SListView<ProvisionPtr> > ProvisionListView;
	TSharedPtr<SWidgetSwitcher > ProvisionInfoSwitcher;
	TSharedPtr<TArray<CertificatePtr>> CertificateList;
	TArray<CertificatePtr> FilteredCertificateList;
	TSharedPtr<SListView<CertificatePtr> > CertificateListView;
	TSharedPtr<SWidgetSwitcher > CertificateInfoSwitcher;
	TAttribute<bool> RunningIPPProcess;
	TSharedPtr<IPropertyHandle> MobileProvisionProperty;
	TSharedPtr<IPropertyHandle> SignCertificateProperty;

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

	// updates the bundle identifier if it is valid and checks for a matching provision/certificate
	void OnBundleIdentifierChanged(const FText& NewText, ETextCommit::Type, TSharedRef<IPropertyHandle> InPropertyHandle);

	// posts an error if the bundle identifier has become invalid
	void OnBundleIdentifierTextChanged(const FText& NewText, ETextCommit::Type, TSharedRef<IPropertyHandle> InPropertyHandle);

	// returns true if the given string is a valid bundle identifier
	bool IsBundleIdentifierValid(const FString& inIdentifier);

	// updates the text in the ini file and checks for a valid provision/certificate
	void OnRemoteServerChanged(const FText& NewText, ETextCommit::Type, TSharedRef<IPropertyHandle> InPropertyHandle);

	void HandleProvisionChanged(FString Provision);

	void HandleCertificateChanged(FString Certificate);

	// 
	FText GetBundleText(TSharedRef<IPropertyHandle> InPropertyHandle) const;

	TSharedPtr< SEditableTextBox > BundleIdTextBox;
};

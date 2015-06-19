// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "IOSPlatformEditorPrivatePCH.h"
#include "SWidgetSwitcher.h"
#include "IDetailPropertyRow.h"
#include "IOSTargetSettingsCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "PropertyEditing.h"
#include "DesktopPlatformModule.h"
#include "MainFrame.h"

#include "ScopedTransaction.h"
#include "SExternalImageReference.h"
#include "SHyperlinkLaunchURL.h"
#include "SPlatformSetupMessage.h"
#include "PlatformIconInfo.h"
#include "SourceControlHelpers.h"
#include "ManifestUpdateHelper.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "TargetPlatform.h"
#include "GameProjectGenerationModule.h"
#include "SHyperlink.h"
#include "SProvisionListRow.h"
#include "SCertificateListRow.h"

#define LOCTEXT_NAMESPACE "IOSTargetSettings"

bool SProvisionListRow::bInitialized = false;
FCheckBoxStyle SProvisionListRow::ProvisionCheckBoxStyle;

//////////////////////////////////////////////////////////////////////////
// FIOSTargetSettingsCustomization
namespace FIOSTargetSettingsCustomizationConstants
{
	const FText DisabledTip = LOCTEXT("GitHubSourceRequiredToolTip", "This requires GitHub source.");
}


TSharedRef<IDetailCustomization> FIOSTargetSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FIOSTargetSettingsCustomization);
}

FIOSTargetSettingsCustomization::FIOSTargetSettingsCustomization()
	: EngineInfoPath(FString::Printf(TEXT("%sBuild/IOS/UE4Game-Info.plist"), *FPaths::EngineDir()))
	, GameInfoPath(FString::Printf(TEXT("%sBuild/IOS/Info.plist"), *FPaths::GameDir()))
	, EngineGraphicsPath(FString::Printf(TEXT("%sBuild/IOS/Resources/Graphics"), *FPaths::EngineDir()))
	, GameGraphicsPath(FString::Printf(TEXT("%sBuild/IOS/Resources/Graphics"), *FPaths::GameDir()))
{
	new (IconNames) FPlatformIconInfo(TEXT("Icon29.png"), LOCTEXT("SettingsIcon_iPhone", "iPhone Settings Icon"), FText::GetEmpty(), 29, 29, FPlatformIconInfo::Optional);// also iOS6 spotlight search
	new (IconNames) FPlatformIconInfo(TEXT("Icon29@2x.png"), LOCTEXT("SettingsIcon_iPhoneRetina", "iPhone Retina Settings Icon"), FText::GetEmpty(), 58, 58, FPlatformIconInfo::Optional); // also iOS6 spotlight search
	new (IconNames) FPlatformIconInfo(TEXT("Icon40.png"), LOCTEXT("SpotlightIcon_iOS7", "iOS7 Spotlight Icon"), FText::GetEmpty(), 40, 40, FPlatformIconInfo::Optional);
	new (IconNames) FPlatformIconInfo(TEXT("Icon40@2x.png"), LOCTEXT("SpotlightIcon_Retina_iOS7", "Retina iOS7 Spotlight Icon"), FText::GetEmpty(), 80, 80, FPlatformIconInfo::Optional);
	new (IconNames) FPlatformIconInfo(TEXT("Icon50.png"), LOCTEXT("SpotlightIcon_iPad_iOS6", "iPad iOS6 Spotlight Icon"), FText::GetEmpty(), 50, 50, FPlatformIconInfo::Optional);
	new (IconNames) FPlatformIconInfo(TEXT("Icon50@2x.png"), LOCTEXT("SpotlightIcon_iPadRetina_iOS6", "iPad Retina iOS6 Spotlight Icon"), FText::GetEmpty(), 100, 100, FPlatformIconInfo::Optional);
	new (IconNames) FPlatformIconInfo(TEXT("Icon57.png"), LOCTEXT("AppIcon_iPhone_iOS6", "iPhone iOS6 App Icon"), FText::GetEmpty(), 57, 57, FPlatformIconInfo::Required);
	new (IconNames) FPlatformIconInfo(TEXT("Icon57@2x.png"), LOCTEXT("AppIcon_iPhoneRetina_iOS6", "iPhone Retina iOS6 App Icon"), FText::GetEmpty(), 114, 114, FPlatformIconInfo::Required);
	new (IconNames) FPlatformIconInfo(TEXT("Icon60@2x.png"), LOCTEXT("AppIcon_iPhoneRetina_iOS7", "iPhone Retina iOS7 App Icon"), FText::GetEmpty(), 120, 120, FPlatformIconInfo::Required);
	new (IconNames) FPlatformIconInfo(TEXT("Icon72.png"), LOCTEXT("AppIcon_iPad_iOS6", "iPad iOS6 App Icon"), FText::GetEmpty(), 72, 72, FPlatformIconInfo::Required);
	new (IconNames) FPlatformIconInfo(TEXT("Icon72@2x.png"), LOCTEXT("AppIcon_iPadRetina_iOS6", "iPad Retina iOS6 App Icon"), FText::GetEmpty(), 144, 144, FPlatformIconInfo::Required);
	new (IconNames) FPlatformIconInfo(TEXT("Icon76.png"), LOCTEXT("AppIcon_iPad_iOS7", "iPad iOS7 App Icon"), FText::GetEmpty(), 76, 76, FPlatformIconInfo::Required);
	new (IconNames) FPlatformIconInfo(TEXT("Icon76@2x.png"), LOCTEXT("AppIcon_iPadRetina_iOS7", "iPad Retina iOS7 App Icon"), FText::GetEmpty(), 152, 152, FPlatformIconInfo::Required);

	new (LaunchImageNames) FPlatformIconInfo(TEXT("Default.png"), LOCTEXT("LaunchImage_iPhone", "Launch iPhone 4/4S"), FText::GetEmpty(), 320, 480, FPlatformIconInfo::Required);
	new (LaunchImageNames) FPlatformIconInfo(TEXT("Default@2x.png"), LOCTEXT("LaunchImage_iPhoneRetina", "Launch iPhone 4/4S Retina"), FText::GetEmpty(), 640, 960, FPlatformIconInfo::Required);
	new (LaunchImageNames) FPlatformIconInfo(TEXT("Default-568h@2x.png"), LOCTEXT("LaunchImage_iPhone5", "Launch iPhone 5/5S Retina"), FText::GetEmpty(), 640, 1136, FPlatformIconInfo::Required);
	new (LaunchImageNames) FPlatformIconInfo(TEXT("Default-Landscape.png"), LOCTEXT("LaunchImage_iPad_Landscape", "Launch iPad in Landscape"), FText::GetEmpty(), 1024, 768, FPlatformIconInfo::Required);
	new (LaunchImageNames) FPlatformIconInfo(TEXT("Default-Landscape@2x.png"), LOCTEXT("LaunchImage_iPadRetina_Landscape", "Launch iPad Retina in Landscape"), FText::GetEmpty(), 2048, 1536, FPlatformIconInfo::Required);
	new (LaunchImageNames) FPlatformIconInfo(TEXT("Default-Portrait.png"), LOCTEXT("LaunchImage_iPad_Portrait", "Launch iPad in Portrait"), FText::GetEmpty(), 768, 1024, FPlatformIconInfo::Required);
	new (LaunchImageNames) FPlatformIconInfo(TEXT("Default-Portrait@2x.png"), LOCTEXT("LaunchImage_iPadRetina_Portrait", "Launch iPad Retina in Portrait"), FText::GetEmpty(), 1536, 2048, FPlatformIconInfo::Required);
	new (LaunchImageNames) FPlatformIconInfo(TEXT("Default-IPhone6.png"), LOCTEXT("LaunchImage_iPhone6", "Launch iPhone 6"), FText::GetEmpty(), 750, 1334, FPlatformIconInfo::Required);
	new (LaunchImageNames) FPlatformIconInfo(TEXT("Default-IPhone6Plus-Landscape.png"), LOCTEXT("LaunchImage_iPhone6Plus_Landscape", "Launch iPhone 6 Plus in Landscape"), FText::GetEmpty(), 2208, 1242, FPlatformIconInfo::Required);
	new (LaunchImageNames) FPlatformIconInfo(TEXT("Default-IPhone6Plus-Portrait.png"), LOCTEXT("LaunchImage_iPhone6Plus_Portrait", "Launch iPhone 6 Plus in Portrait"), FText::GetEmpty(), 1242, 2208, FPlatformIconInfo::Required);

	bShowAllProvisions = false;
	bShowAllCertificates = false;
}

FIOSTargetSettingsCustomization::~FIOSTargetSettingsCustomization()
{
	if (IPPProcess.IsValid())
	{
		IPPProcess = NULL;
		FTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
}

void FIOSTargetSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	SavedLayoutBuilder = &DetailLayout;

	BuildPListSection(DetailLayout);

	BuildIconSection(DetailLayout);

	BuildRemoteBuildingSection(DetailLayout);

	FindRequiredFiles();
}

static FString OutputMessage;
static void OnOutput(FString Message)
{
	OutputMessage += Message;
	OutputMessage += "\n";
	UE_LOG(LogTemp, Display, TEXT("%s\n"), *Message);
}

void FIOSTargetSettingsCustomization::UpdateStatus()
{
	if (OutputMessage.Len() > 0)
	{
		CertificateList.Reset();
		ProvisionList.Reset();

		// Now split up the log into multiple lines
		TArray<FString> LogLines;
		OutputMessage.ParseIntoArray(LogLines, LINE_TERMINATOR, true);
		
		// format of the line being read here!!
		bool bCerts = false;
		for (int Index = 0; Index < LogLines.Num(); Index++)
		{
			FString& Line = LogLines[Index];
			TArray<FString> Fields;
			Line.ParseIntoArray(Fields, TEXT(","), true);
			if (Line.Contains(TEXT("CERTIFICATE-"), ESearchCase::CaseSensitive))
			{
				CertificatePtr Cert = MakeShareable<FCertificate>(new FCertificate());
				for (int FieldIndex = 0; FieldIndex < Fields.Num(); ++FieldIndex)
				{
					FString Key, Value;
					Fields[FieldIndex].Split(TEXT(":"), &Key, &Value);
					if (Key.Contains("Name"))
					{
						Cert->Name = Value;
					}
					else if (Key.Contains(TEXT("Validity")))
					{
						Cert->Status = Value;
					}
					else if (Key.Contains(TEXT("EndDate")))
					{
						FString Date, Time;
						Value.Split(TEXT("T"), &Date, &Time);
						Cert->Expires = Date;
					}
				}
				CertificatePtr PrevCert = NULL;
				for (int CIndex = 0; CIndex < CertificateList.Num() && !PrevCert.IsValid(); ++CIndex)
				{
					if (CertificateList[CIndex]->Name == Cert->Name)
					{
						PrevCert = CertificateList[CIndex];
						break;
					}
				}
				if (!PrevCert.IsValid())
				{
					CertificateList.Add(Cert);
				}
				else
				{
					FDateTime time1, time2;
					FDateTime::ParseIso8601(*(PrevCert->Expires), time1);
					FDateTime::ParseIso8601(*(Cert->Expires), time2);
					if (time2 > time1)
					{
						PrevCert->Expires = Cert->Expires;
					}
					Cert = NULL;
				}
			}
			else if (Line.Contains(TEXT("PROVISION-"), ESearchCase::CaseSensitive))
			{
				ProvisionPtr Prov = MakeShareable<FProvision>(new FProvision());
				for (int FieldIndex = 0; FieldIndex < Fields.Num(); ++FieldIndex)
				{
					FString Key, Value;
					Fields[FieldIndex].Split(TEXT(":"), &Key, &Value);
					if (Key.Contains("File"))
					{
						Prov->FileName = Value;
					}
					else if (Key.Contains("Name"))
					{
						Prov->Name = Value;
					}
					else if (Key.Contains(TEXT("Validity")))
					{
						Prov->Status = Value;
					}
					else if (Key.Contains(TEXT("Type")))
					{
						Prov->bDistribution = Value.Contains(TEXT("DISTRIBUTION"));
					}
				}
				ProvisionList.Add(Prov);
			}
			else if (Line.Contains(TEXT("MATCHED-"), ESearchCase::CaseSensitive))
			{
				for (int FieldIndex = 0; FieldIndex < Fields.Num(); ++FieldIndex)
				{
					FString Key, Value;
					Fields[FieldIndex].Split(TEXT(":"), &Key, &Value);
					if (Key.Contains("File"))
					{
						SelectedFile = Value;
					}
					else if (Key.Contains("Provision"))
					{
						SelectedProvision = Value;
					}
					else if (Key.Contains(TEXT("Cert")))
					{
						SelectedCert = Value;
					}
				}
			}
		}

		FilterLists();
	}
}

void FIOSTargetSettingsCustomization::UpdateSSHStatus()
{
	// updated SSH key
	const UIOSRuntimeSettings* Settings = GetDefault<UIOSRuntimeSettings>();
	const_cast<UIOSRuntimeSettings*>(Settings)->PostInitProperties();
}

void FIOSTargetSettingsCustomization::BuildPListSection(IDetailLayoutBuilder& DetailLayout)
{
	// Info.plist category
	IDetailCategoryBuilder& ProvisionCategory = DetailLayout.EditCategory(TEXT("Mobile Provision"));
	IDetailCategoryBuilder& AppManifestCategory = DetailLayout.EditCategory(TEXT("Info.plist"));
	IDetailCategoryBuilder& BundleCategory = DetailLayout.EditCategory(TEXT("BundleInformation"));
	IDetailCategoryBuilder& OrientationCategory = DetailLayout.EditCategory(TEXT("Orientation"));
	IDetailCategoryBuilder& RenderCategory = DetailLayout.EditCategory(TEXT("Rendering"));
	IDetailCategoryBuilder& OSInfoCategory = DetailLayout.EditCategory(TEXT("OS Info"));
	IDetailCategoryBuilder& DeviceCategory = DetailLayout.EditCategory(TEXT("Devices"));
	IDetailCategoryBuilder& BuildCategory = DetailLayout.EditCategory(TEXT("Build"));
	IDetailCategoryBuilder& ExtraCategory = DetailLayout.EditCategory(TEXT("Extra PList Data"));

/*	ProvisionCategory.AddCustomRow(TEXT("Certificate Request"), false)
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 1, 0, 1))
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("RequestLabel", "Certificate Request"))
				.Font(DetailLayout.GetDetailFont())
			]
		]
	.ValueContent()
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.OnClicked(this, &FIOSTargetSettingsCustomization::OnCertificateRequestClicked)
				[
					SNew(STextBlock)
					.Text(FText::FromString("Create Certificate Request and a Key Pair"))
				]
			]
		];*/

	ProvisionCategory.AddCustomRow(LOCTEXT("ProvisionLabel", "Provision"), false)
		.WholeRowWidget
		.MinDesiredWidth(0.f)
		.MaxDesiredWidth(0.f)
		.HAlign(HAlign_Fill)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(ProvisionInfoSwitcher, SWidgetSwitcher)
				.WidgetIndex(0)
				// searching for provisions
				+SWidgetSwitcher::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.Padding(4)
					[
						SNew( STextBlock )
						.Text( LOCTEXT( "ProvisionViewerFindingProvisions", "Please wait while we gather information." ) )
						.AutoWrapText( true )
					]
				]
				// importing a provision
				+SWidgetSwitcher::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Center)
					[
						SNew(SBorder)
						.Padding(4)
						[
							SNew( STextBlock )
							.Text( LOCTEXT( "ProvisionViewerImportingProvisions", "Importing Provision.  Please wait..." ) )
							.AutoWrapText( true )
						]
					]
				// no provisions found or no valid provisions
				+SWidgetSwitcher::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.Padding(4)
					[
						SNew( STextBlock )
						.Text( LOCTEXT( "ProvisionViewerNoValidProvisions", "No Provisions Found. Please Import a Provision." ) )
						.AutoWrapText( true )
					]
				]
				+SWidgetSwitcher::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.Padding(FMargin(10, 10, 10, 10))
					.AutoHeight()
					[
						SAssignNew(ProvisionListView, SListView<ProvisionPtr>)
						.ItemHeight(20.0f)
						.ListItemsSource(&FilteredProvisionList)
						.OnGenerateRow(this, &FIOSTargetSettingsCustomization::HandleProvisionListGenerateRow)
						.SelectionMode(ESelectionMode::None)
						.HeaderRow
						(
						SNew(SHeaderRow)
						+ SHeaderRow::Column("Name")
						.DefaultLabel(LOCTEXT("ProvisionListNameColumnHeader", "Provision"))
						.FillWidth(1.0f)
						+ SHeaderRow::Column("File")
						.DefaultLabel(LOCTEXT("ProvisionListFileColumnHeader", "File"))
						+ SHeaderRow::Column("Status")
						.DefaultLabel(LOCTEXT("ProvisionListStatusColumnHeader", "Status"))
						+ SHeaderRow::Column("Distribution")
						.DefaultLabel(LOCTEXT("ProvisionListDistributionColumnHeader", "Distribution"))
						.FixedWidth(75.0f)
						)
					]
					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 6.0f, 0.0f, 4.0f)
						[
							SNew(SSeparator)
							.Orientation(Orient_Horizontal)
						]
					+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SRichTextBlock)
								.Text(LOCTEXT("ProvisionMessage", "<RichTextBlock.TextHighlight>Note</>: The provision in green will be used to provision the IPA."))
								.TextStyle(FEditorStyle::Get(), "MessageLog")
								.DecoratorStyleSet(&FEditorStyle::Get())
								.AutoWrapText(true)
							]

							+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.HAlign(HAlign_Right)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("ViewLabel", "View:"))
								]

							+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(8.0f, 0.0f)
								[
									// all provisions hyper link
									SNew(SHyperlink)
									.OnNavigate(this, &FIOSTargetSettingsCustomization::HandleAllProvisionsHyperlinkNavigate, true)
									.Text(LOCTEXT("AllProvisionsHyperLinkLabel", "All"))
									.ToolTipText(LOCTEXT("AllProvisionsButtonTooltip", "View all provisions."))
								]

							+ SHorizontalBox::Slot()
								.AutoWidth()
								[
									// valid provisions hyper link
									SNew(SHyperlink)
									.OnNavigate(this, &FIOSTargetSettingsCustomization::HandleAllProvisionsHyperlinkNavigate, false)
									.Text(LOCTEXT("ValidProvisionsHyperlinkLabel", "Valid Only"))
									.ToolTipText(LOCTEXT("ValidProvisionsHyperlinkTooltip", "View Valid provisions."))
								]
						]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
					.Padding(FMargin(0, 5, 0, 10))
					.AutoWidth()
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.OnClicked(this, &FIOSTargetSettingsCustomization::OnInstallProvisionClicked)
						.IsEnabled(this, &FIOSTargetSettingsCustomization::IsImportEnabled)
						[
							SNew(STextBlock)
							.Text(FText::FromString("Import Provision"))
						]
					]
			]
		];

	ProvisionCategory.AddCustomRow(LOCTEXT("CertificateLabel", "Certificate"), false)
		.WholeRowWidget
		.MinDesiredWidth(0.f)
		.MaxDesiredWidth(0.f)
		.HAlign(HAlign_Fill)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(CertificateInfoSwitcher, SWidgetSwitcher)
				.WidgetIndex(0)
				// searching for provisions
				+SWidgetSwitcher::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.Padding(4)
					[
						SNew( STextBlock )
						.Text( LOCTEXT( "CertificateViewerFindingProvisions", "Please wait while we gather information." ) )
						.AutoWrapText( true )
					]
				]
				// importing certificate
				+SWidgetSwitcher::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Center)
					[
						SNew(SBorder)
						.Padding(4)
						[
							SNew( STextBlock )
							.Text( LOCTEXT( "CertificateViewerImportingCertificate", "Importing Certificate.  Please wait..." ) )
							.AutoWrapText( true )
						]
					]
				// no provisions found or no valid provisions
				+SWidgetSwitcher::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Center)
					[
						SNew(SBorder)
						.Padding(4)
						[
							SNew( STextBlock )
							.Text( LOCTEXT( "CertificateViewerNoValidProvisions", "No Certificates Found.  Please Import a Certificate." ) )
							.AutoWrapText( true )
						]
					]
				+SWidgetSwitcher::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.Padding(FMargin(10, 10, 10, 10))
							.FillWidth(1.0f)
							[
								SAssignNew(CertificateListView, SListView<CertificatePtr>)
								.ItemHeight(20.0f)
								.ListItemsSource(&FilteredCertificateList)
								.OnGenerateRow(this, &FIOSTargetSettingsCustomization::HandleCertificateListGenerateRow)
								.SelectionMode(ESelectionMode::None)
								.HeaderRow
								(
									SNew(SHeaderRow)
									+ SHeaderRow::Column("Name")
									.DefaultLabel(LOCTEXT("CertificateListNameColumnHeader", "Certificate"))
									+ SHeaderRow::Column("Status")
									.DefaultLabel(LOCTEXT("CertificateListStatusColumnHeader", "Status"))
									.FixedWidth(75.0f)
									+ SHeaderRow::Column("Expires")
									.DefaultLabel(LOCTEXT("CertificateListExpiresColumnHeader", "Expires"))
									.FixedWidth(75.0f)
								)
							]
						]
						+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 6.0f, 0.0f, 4.0f)
							[
								SNew(SSeparator)
								.Orientation(Orient_Horizontal)
							]
						+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew(SRichTextBlock)
									.Text(LOCTEXT("CertificateMessage", "<RichTextBlock.TextHighlight>Note</>: The certificate in green will be used to sign the IPA."))
									.TextStyle(FEditorStyle::Get(), "MessageLog")
									.DecoratorStyleSet(&FEditorStyle::Get())
									.AutoWrapText(true)
								]
								+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.HAlign(HAlign_Right)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("ViewLabel", "View:"))
								]

								+ SHorizontalBox::Slot()
									.AutoWidth()
									.Padding(8.0f, 0.0f)
									[
										// all provisions hyper link
										SNew(SHyperlink)
										.OnNavigate(this, &FIOSTargetSettingsCustomization::HandleAllCertificatesHyperlinkNavigate, true)
										.Text(LOCTEXT("AllCertificatesHyperLinkLabel", "All"))
										.ToolTipText(LOCTEXT("AllCertificatesButtonTooltip", "View all certificates."))
									]

								+ SHorizontalBox::Slot()
									.AutoWidth()
									[
										// valid provisions hyper link
										SNew(SHyperlink)
										.OnNavigate(this, &FIOSTargetSettingsCustomization::HandleAllCertificatesHyperlinkNavigate, false)
										.Text(LOCTEXT("ValidCertificatesHyperlinkLabel", "Valid Only"))
										.ToolTipText(LOCTEXT("ValidCertificatesHyperlinkTooltip", "View Valid certificates."))
									]
							]
					]
				]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(FMargin(0, 5, 0, 10))
				.AutoWidth()
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.OnClicked(this, &FIOSTargetSettingsCustomization::OnInstallCertificateClicked)
					.IsEnabled(this, &FIOSTargetSettingsCustomization::IsImportEnabled)
					[
						SNew(STextBlock)
						.Text(FText::FromString("Import Certificate"))
					]
				]
			]
		];
	
	BundleCategory.AddCustomRow(LOCTEXT("UpgradeInfo", "Upgrade Info"), false)
	.WholeRowWidget
	[
		SNew(SBorder)
		.Padding(1)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(10, 10, 10, 10))
			.FillWidth(1.0f)
			[
				SNew(SRichTextBlock)
				.Text(LOCTEXT("IOSUpgradeInfoMessage", "<RichTextBlock.TextHighlight>Note to users from 4.6 or earlier</>: We now <RichTextBlock.TextHighlight>GENERATE</> an Info.plist when building, so if you have customized your .plist file, you will need to put all of your changes into the below settings. Note that we don't touch the .plist file that is in your project directory, so you can use it as reference."))
				.TextStyle(FEditorStyle::Get(), "MessageLog")
				.DecoratorStyleSet(&FEditorStyle::Get())
				.AutoWrapText(true)
				// + SRichTextBlock::HyperlinkDecorator(TEXT("browser"), FSlateHyperlinkRun::FOnClick::CreateStatic(&OnBrowserLinkClicked))
			 ]
		 ]
	 ];

	// Show properties that are gated by the plist being present and writable
	RunningIPPProcess = false;

#define SETUP_NONROCKET_PROP(PropName, Category) \
	{ \
		TSharedRef<IPropertyHandle> PropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, PropName)); \
		Category.AddProperty(PropertyHandle) \
			.IsEnabled(!FRocketSupport::IsRocket()) \
			.ToolTip(!FRocketSupport::IsRocket() ? PropertyHandle->GetToolTipText() : FIOSTargetSettingsCustomizationConstants::DisabledTip); \
	}

#define SETUP_PLIST_PROP(PropName, Category) \
	{ \
		TSharedRef<IPropertyHandle> PropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, PropName)); \
		Category.AddProperty(PropertyHandle); \
	}

#define SETUP_STATUS_PROP(PropName, Category) \
	{ \
		TSharedRef<IPropertyHandle> PropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, PropName)); \
		Category.AddProperty(PropertyHandle) \
		.Visibility(EVisibility::Hidden); \
		Category.AddCustomRow(LOCTEXT("BundleIdentifier", "BundleIdentifier"), false) \
		.NameContent() \
		[ \
			SNew(SHorizontalBox) \
			+ SHorizontalBox::Slot() \
			.Padding(FMargin(0, 1, 0, 1)) \
			.FillWidth(1.0f) \
			[ \
				SNew(STextBlock) \
				.Text(LOCTEXT("BundleIdentifierLabel", "Bundle Identifier")) \
				.Font(DetailLayout.GetDetailFont()) \
			]\
		] \
		.ValueContent() \
		.MinDesiredWidth( 0.0f ) \
		.MaxDesiredWidth( 0.0f ) \
		[ \
			SNew(SHorizontalBox) \
			+ SHorizontalBox::Slot() \
			.FillWidth(1.0f) \
			.HAlign(HAlign_Fill) \
			[ \
				SNew(SEditableTextBox) \
				.IsEnabled(this, &FIOSTargetSettingsCustomization::IsImportEnabled) \
				.Text(this, &FIOSTargetSettingsCustomization::GetBundleText, PropertyHandle) \
				.Font(DetailLayout.GetDetailFont()) \
				.SelectAllTextOnCommit( true ) \
				.SelectAllTextWhenFocused( true ) \
				.ClearKeyboardFocusOnCommit(false) \
				.ToolTipText(PropertyHandle->GetToolTipText()) \
				.OnTextCommitted(this, &FIOSTargetSettingsCustomization::OnBundleIdentifierChanged, PropertyHandle) \
			] \
		]; \
	}

	const UIOSRuntimeSettings& Settings = *GetDefault<UIOSRuntimeSettings>();

	SETUP_PLIST_PROP(BundleDisplayName, BundleCategory);
	SETUP_PLIST_PROP(BundleName, BundleCategory);
	SETUP_STATUS_PROP(BundleIdentifier, BundleCategory);
	SETUP_PLIST_PROP(VersionInfo, BundleCategory);
	SETUP_PLIST_PROP(bSupportsPortraitOrientation, OrientationCategory);
	SETUP_PLIST_PROP(bSupportsUpsideDownOrientation, OrientationCategory);
	SETUP_PLIST_PROP(bSupportsLandscapeLeftOrientation, OrientationCategory);
	SETUP_PLIST_PROP(bSupportsLandscapeRightOrientation, OrientationCategory);
	
	SETUP_PLIST_PROP(bSupportsMetal, RenderCategory);
	SETUP_PLIST_PROP(bSupportsOpenGLES2, RenderCategory);

	SETUP_PLIST_PROP(bSupportsIPad, DeviceCategory);
	SETUP_PLIST_PROP(bSupportsIPhone, DeviceCategory);

	SETUP_PLIST_PROP(MinimumiOSVersion, OSInfoCategory);

	SETUP_PLIST_PROP(AdditionalPlistData, ExtraCategory);

	SETUP_NONROCKET_PROP(bDevForArmV7, BuildCategory);
	SETUP_NONROCKET_PROP(bDevForArm64, BuildCategory);
	SETUP_NONROCKET_PROP(bDevForArmV7S, BuildCategory);
	SETUP_NONROCKET_PROP(bShipForArmV7, BuildCategory);
	SETUP_NONROCKET_PROP(bShipForArm64, BuildCategory);
	SETUP_NONROCKET_PROP(bShipForArmV7S, BuildCategory);

	SETUP_NONROCKET_PROP(bSupportsMetalMRT, RenderCategory);

#undef SETUP_NONROCKET_PROP
}


void FIOSTargetSettingsCustomization::BuildRemoteBuildingSection(IDetailLayoutBuilder& DetailLayout)
{
#if PLATFORM_WINDOWS
	IDetailCategoryBuilder& BuildCategory = DetailLayout.EditCategory(TEXT("Build"));

	// Sub group we wish to add remote building options to.
	FText RemoteBuildingGroupName = LOCTEXT("RemoteBuildingGroupName", "Remote Build Options");
	IDetailGroup& RemoteBuildingGroup = BuildCategory.AddGroup(*RemoteBuildingGroupName.ToString(), RemoteBuildingGroupName, false);


	// Remote Server Name Property
	TSharedRef<IPropertyHandle> RemoteServerNamePropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, RemoteServerName));
	IDetailPropertyRow& RemoteServerNamePropertyRow = RemoteBuildingGroup.AddPropertyRow(RemoteServerNamePropertyHandle);
	RemoteServerNamePropertyRow
		.ToolTip(LOCTEXT("RemoteServerNameToolTip", "The name or ip address of the remote mac which will be used to build IOS"))
		.CustomWidget()
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 1, 0, 1))
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("RemoteServerNameLabel", "Remote Server Name"))
				.Font(DetailLayout.GetDetailFont())
			]
		]
		.ValueContent()
		.MinDesiredWidth(150.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0.0f, 8.0f))
			[
				SNew(SEditableTextBox)
				.IsEnabled(this, &FIOSTargetSettingsCustomization::IsImportEnabled)
				.Text(this, &FIOSTargetSettingsCustomization::GetBundleText, RemoteServerNamePropertyHandle)
				.Font(DetailLayout.GetDetailFont())
				.SelectAllTextOnCommit(true)
				.SelectAllTextWhenFocused(true)
				.ClearKeyboardFocusOnCommit(false)
				.ToolTipText(RemoteServerNamePropertyHandle->GetToolTipText())
				.OnTextCommitted(this, &FIOSTargetSettingsCustomization::OnRemoteServerChanged, RemoteServerNamePropertyHandle)
			]

		];



	// Add Use RSync Property
	TSharedRef<IPropertyHandle> UseRSyncPropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, bUseRSync));
	BuildCategory.AddProperty(UseRSyncPropertyHandle)
		.Visibility(EVisibility::Hidden);

	TSharedRef<IPropertyHandle> DeltaCopyInstallDirPropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, DeltaCopyInstallPath));
	IDetailPropertyRow& DeltaCopyInstallDirPropertyRow = RemoteBuildingGroup.AddPropertyRow(DeltaCopyInstallDirPropertyHandle);

	// Add RSync Username Property
	TSharedRef<IPropertyHandle> RSyncUsernamePropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, RSyncUsername));
	IDetailPropertyRow& RSyncUsernamePropertyRow = RemoteBuildingGroup.AddPropertyRow(RSyncUsernamePropertyHandle);
	RSyncUsernamePropertyRow
		.ToolTip(LOCTEXT("RSyncUsernameToolTip", "The username of the mac user that matches the specified SSH Key."))
		.CustomWidget()
			.NameContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(FMargin(0, 1, 0, 1))
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RSyncUserNameLabel", "RSync User Name"))
					.Font(DetailLayout.GetDetailFont())
				]
			]
			.ValueContent()
			.MinDesiredWidth(150.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(FMargin(0.0f, 8.0f))
				[
					SNew(SEditableTextBox)
					.IsEnabled(this, &FIOSTargetSettingsCustomization::IsImportEnabled)
					.Text(this, &FIOSTargetSettingsCustomization::GetBundleText, RSyncUsernamePropertyHandle)
					.Font(DetailLayout.GetDetailFont())
					.SelectAllTextOnCommit(true)
					.SelectAllTextWhenFocused(true)
					.ClearKeyboardFocusOnCommit(false)
					.ToolTipText(RSyncUsernamePropertyHandle->GetToolTipText())
					.OnTextCommitted(this, &FIOSTargetSettingsCustomization::OnRemoteServerChanged, RSyncUsernamePropertyHandle)
				]
			];


	// Add existing SSH path label.
	TSharedRef<IPropertyHandle> SSHPrivateKeyLocationPropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, SSHPrivateKeyLocation));
	IDetailPropertyRow& SSHPrivateKeyLocationPropertyRow = RemoteBuildingGroup.AddPropertyRow(SSHPrivateKeyLocationPropertyHandle);
	SSHPrivateKeyLocationPropertyRow
		.ToolTip(LOCTEXT("SSHPrivateKeyLocationToolTip", "The existing location of an SSH Key found by UE4."));


	// Add SSH override path
	TSharedRef<IPropertyHandle> SSHPrivateKeyOverridePathPropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, SSHPrivateKeyOverridePath));
	IDetailPropertyRow& SSHPrivateKeyOverridePathPropertyRow = RemoteBuildingGroup.AddPropertyRow(SSHPrivateKeyOverridePathPropertyHandle);
	SSHPrivateKeyOverridePathPropertyRow
		.ToolTip(LOCTEXT("SSHPrivateKeyOverridePathToolTip", "Override the existing SSH Private Key with one from a specified location."));

	// Add a generate key button
	RemoteBuildingGroup.AddWidgetRow()
		.WholeRowWidget
		.MinDesiredWidth(0.f)
		.MaxDesiredWidth(0.f)
		.HAlign(HAlign_Fill)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(FMargin(0, 5, 0, 10))
					.AutoWidth()
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.OnClicked(this, &FIOSTargetSettingsCustomization::OnGenerateSSHKey)
						.IsEnabled(this, &FIOSTargetSettingsCustomization::IsImportEnabled)
						[
							SNew(STextBlock)
							.Text(FText::FromString("Generate SSH Key"))
						]
					]
				]
		];
#endif
}


void FIOSTargetSettingsCustomization::BuildIconSection(IDetailLayoutBuilder& DetailLayout)
{
	IDetailCategoryBuilder& RequiredIconCategory = DetailLayout.EditCategory(TEXT("Required Icons"));
	IDetailCategoryBuilder& OptionalIconCategory = DetailLayout.EditCategory(TEXT("Optional Icons"));

	// Add the icons
	for (const FPlatformIconInfo& Info : IconNames)
	{
		const FVector2D IconImageMaxSize(Info.IconRequiredSize);
		IDetailCategoryBuilder& IconCategory = (Info.RequiredState == FPlatformIconInfo::Required) ? RequiredIconCategory : OptionalIconCategory;
		BuildImageRow(DetailLayout, IconCategory, Info, IconImageMaxSize);
	}

	// Add the launch images
	IDetailCategoryBuilder& LaunchImageCategory = DetailLayout.EditCategory(TEXT("Launch Images"));
	const FVector2D LaunchImageMaxSize(150.0f, 150.0f);
	for (const FPlatformIconInfo& Info : LaunchImageNames)
	{
		BuildImageRow(DetailLayout, LaunchImageCategory, Info, LaunchImageMaxSize);
	}
}


FReply FIOSTargetSettingsCustomization::OpenPlistFolder()
{
	const FString EditPlistFolder = FPaths::ConvertRelativePathToFull(FPaths::GetPath(GameInfoPath));
	FPlatformProcess::ExploreFolder(*EditPlistFolder);

	return FReply::Handled();
}

void FIOSTargetSettingsCustomization::CopySetupFilesIntoProject()
{
	// First copy the plist, it must get copied
	FText ErrorMessage;
	if (!SourceControlHelpers::CopyFileUnderSourceControl(GameInfoPath, EngineInfoPath, LOCTEXT("InfoPlist", "Info.plist"), /*out*/ ErrorMessage))
	{
		FNotificationInfo Info(ErrorMessage);
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	else
	{
		// Now try to copy all of the icons, etc... (these can be ignored if the file already exists)
		TArray<FPlatformIconInfo> Graphics;
		Graphics.Empty(IconNames.Num() + LaunchImageNames.Num());
		Graphics.Append(IconNames);
		Graphics.Append(LaunchImageNames);

		for (const FPlatformIconInfo& Info : Graphics)
		{
			const FString EngineImagePath = EngineGraphicsPath / Info.IconPath;
			const FString ProjectImagePath = GameGraphicsPath / Info.IconPath;

			if (!FPaths::FileExists(ProjectImagePath))
			{
				SourceControlHelpers::CopyFileUnderSourceControl(ProjectImagePath, EngineImagePath, Info.IconName, /*out*/ ErrorMessage);
			}
		}
	}

	SavedLayoutBuilder->ForceRefreshDetails();
}

void FIOSTargetSettingsCustomization::BuildImageRow(IDetailLayoutBuilder& DetailLayout, IDetailCategoryBuilder& Category, const FPlatformIconInfo& Info, const FVector2D& MaxDisplaySize)
{
	const FString AutomaticImagePath = EngineGraphicsPath / Info.IconPath;
	const FString TargetImagePath = GameGraphicsPath / Info.IconPath;

	Category.AddCustomRow(Info.IconName)
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 1, 0, 1))
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(Info.IconName)
				.Font(DetailLayout.GetDetailFont())
			]
		]
		.ValueContent()
		.MaxDesiredWidth(400.0f)
		.MinDesiredWidth(100.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SExternalImageReference, AutomaticImagePath, TargetImagePath)
				.FileDescription(Info.IconDescription)
				.RequiredSize(Info.IconRequiredSize)
				.MaxDisplaySize(MaxDisplaySize)
			]
		];
}

void FIOSTargetSettingsCustomization::FindRequiredFiles()
{
	const UIOSRuntimeSettings& Settings = *GetDefault<UIOSRuntimeSettings>();
	FString BundleIdentifier = Settings.BundleIdentifier.Replace(TEXT("[PROJECT_NAME]"), FApp::GetGameName());
#if PLATFORM_MAC
	FString CmdExe = TEXT("/bin/sh");
	FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles/Mac/RunMono.sh"));
	FString IPPPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/DotNet/IOS/IPhonePackager.exe"));
	FString CommandLine = FString::Printf(TEXT("\"%s\" \"%s\" certificates Engine -bundlename \"%s\""), *ScriptPath, *IPPPath, *(BundleIdentifier));
#else
	FString CmdExe = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/DotNet/IOS/IPhonePackager.exe"));
	FString CommandLine = FString::Printf(TEXT("certificates Engine -bundlename \"%s\""), *(BundleIdentifier));
#endif
	IPPProcess = MakeShareable(new FMonitoredProcess(CmdExe, CommandLine, true));
	OutputMessage = TEXT("");
	IPPProcess->OnOutput().BindStatic(&OnOutput);
	IPPProcess->Launch();
	TickerHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FIOSTargetSettingsCustomization::UpdateStatusDelegate), 1.0f);
	if (ProvisionInfoSwitcher.IsValid())
	{
		ProvisionInfoSwitcher->SetActiveWidgetIndex(0);
	}
	if (CertificateInfoSwitcher.IsValid())
	{
		CertificateInfoSwitcher->SetActiveWidgetIndex(0);
	}
	RunningIPPProcess = true;
}

FReply FIOSTargetSettingsCustomization::OnInstallProvisionClicked()
{
	// pass the file to IPP to install
	FString ProjectPath = FPaths::IsProjectFilePathSet() ? FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()) : FPaths::RootDir() / FApp::GetGameName() / FApp::GetGameName() + TEXT(".uproject");
	FString ProvisionPath;

	// get the provision by popping up the file dialog
	TArray<FString> OpenFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpened = false;
	int32 FilterIndex = -1;
	FString FileTypes = TEXT("Provision Files (*.mobileprovision)|*.mobileprovision");

	if ( DesktopPlatform )
	{
		void* ParentWindowWindowHandle = NULL;

		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
		if ( MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid() )
		{
			ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}

		bOpened = DesktopPlatform->OpenFileDialog(
			ParentWindowWindowHandle,
			LOCTEXT("ImportDialogTitle", "Import Provision").ToString(),
			FPaths::GetProjectFilePath(),
			TEXT(""),
			FileTypes,
			EFileDialogFlags::None,
			OpenFilenames,
			FilterIndex
			);
	}

	if ( bOpened )
	{
		ProvisionPath = FPaths::ConvertRelativePathToFull(OpenFilenames[0]);

		// see if the provision is already installed
		FString DestName = FPaths::GetBaseFilename(ProvisionPath);
		TCHAR Path[4096];
#if PLATFORM_MAC
		FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"), Path, ARRAY_COUNT(Path));
		FString Destination = FString::Printf(TEXT("\"%s/Library/MobileDevice/Provisioning Profiles/%s.mobileprovision\""), Path, *DestName);
		FString Destination2 = FString::Printf(TEXT("\"%s/Library/MobileDevice/Provisioning Profiles/%s.mobileprovision\""), Path, FApp::GetGameName());
#else
		FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"), Path, ARRAY_COUNT(Path));
		FString Destination = FString::Printf(TEXT("%s\\Apple Computer\\MobileDevice\\Provisioning Profiles\\%s.mobileprovision"), Path, *DestName);
		FString Destination2 = FString::Printf(TEXT("%s\\Apple Computer\\MobileDevice\\Provisioning Profiles\\%s.mobileprovision"), Path, FApp::GetGameName());
#endif
		if (FPaths::FileExists(Destination) || FPaths::FileExists(Destination2))
		{
			FString MessagePrompt = FString::Printf(TEXT("%s mobile provision file already exists.  Do you want to replace this provision?"), *DestName);
			if (FPlatformMisc::MessageBoxExt(EAppMsgType::OkCancel, *MessagePrompt, TEXT("File Exists")) == EAppReturnType::Cancel)
			{
				return FReply::Handled();
			}
		}

		const UIOSRuntimeSettings& Settings = *GetDefault<UIOSRuntimeSettings>();
#if PLATFORM_MAC
		FString CmdExe = TEXT("/bin/sh");
		FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles/Mac/RunMono.sh"));
		FString IPPPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/DotNet/IOS/IPhonePackager.exe"));
		FString CommandLine = FString::Printf(TEXT("\"%s\" \"%s\" Install Engine -project \"%s\" -provision \"%s\" -bundlename \"%s\""), *ScriptPath, *IPPPath, *ProjectPath, *ProvisionPath, *(Settings.BundleIdentifier));
#else
		FString CmdExe = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/DotNet/IOS/IPhonePackager.exe"));
		FString CommandLine = FString::Printf(TEXT("Install Engine -project \"%s\" -provision \"%s\" -bundlename \"%s\""), *ProjectPath, *ProvisionPath, *(Settings.BundleIdentifier));
#endif
		IPPProcess = MakeShareable(new FMonitoredProcess(CmdExe, CommandLine, true));
		OutputMessage = TEXT("");
		IPPProcess->OnOutput().BindStatic(&OnOutput);
		IPPProcess->Launch();
		TickerHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FIOSTargetSettingsCustomization::UpdateStatusDelegate), 10.0f);
		if (ProvisionInfoSwitcher.IsValid())
		{
			ProvisionInfoSwitcher->SetActiveWidgetIndex(1);
		}
		RunningIPPProcess = true;
	}

	return FReply::Handled();
}

FReply FIOSTargetSettingsCustomization::OnInstallCertificateClicked()
{
	// pass the file to IPP to install
	FString ProjectPath = FPaths::IsProjectFilePathSet() ? FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()) : FPaths::RootDir() / FApp::GetGameName() / FApp::GetGameName() + TEXT(".uproject");
	FString CertPath;

	// get the provision by popping up the file dialog
	TArray<FString> OpenFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpened = false;
	int32 FilterIndex = -1;
	FString FileTypes = TEXT("Code Signing Certificates (*.cer;*.p12)|*.cer;*p12");

	if ( DesktopPlatform )
	{
		void* ParentWindowWindowHandle = NULL;

		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
		if ( MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid() )
		{
			ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}

		bOpened = DesktopPlatform->OpenFileDialog(
			ParentWindowWindowHandle,
			LOCTEXT("ImportDialogTitle", "Import Certificate").ToString(),
			FPaths::GetProjectFilePath(),
			TEXT(""),
			FileTypes,
			EFileDialogFlags::None,
			OpenFilenames,
			FilterIndex
			);
	}

	if ( bOpened )
	{
		const UIOSRuntimeSettings& Settings = *GetDefault<UIOSRuntimeSettings>();
		CertPath = FPaths::ConvertRelativePathToFull(OpenFilenames[0]);
#if PLATFORM_MAC
		FString CmdExe = TEXT("/bin/sh");
		FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles/Mac/RunMono.sh"));
		FString IPPPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/DotNet/IOS/IPhonePackager.exe"));
		FString CommandLine = FString::Printf(TEXT("\"%s\" \"%s\" Install Engine -project \"%s\" -certificate \"%s\" -bundlename \"%s\""), *ScriptPath, *IPPPath, *ProjectPath, *CertPath, *(Settings.BundleIdentifier));
#else
		FString CmdExe = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/DotNet/IOS/IPhonePackager.exe"));
		FString CommandLine = FString::Printf(TEXT("Install Engine -project \"%s\" -certificate \"%s\" -bundlename \"%s\""), *ProjectPath, *CertPath, *(Settings.BundleIdentifier));
#endif
		IPPProcess = MakeShareable(new FMonitoredProcess(CmdExe, CommandLine, true));
		OutputMessage = TEXT("");
		IPPProcess->OnOutput().BindStatic(&OnOutput);
		IPPProcess->Launch();
		TickerHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FIOSTargetSettingsCustomization::UpdateStatusDelegate), 10.0f);
		if (CertificateInfoSwitcher.IsValid())
		{
			CertificateInfoSwitcher->SetActiveWidgetIndex(1);
		}
		RunningIPPProcess = true;
	}

	return FReply::Handled();
}

FReply FIOSTargetSettingsCustomization::OnCertificateRequestClicked()
{
	// TODO: bring up an open file dialog and then install the provision
	return FReply::Handled();
}

FReply FIOSTargetSettingsCustomization::OnGenerateSSHKey()
{
	// see if the key is already generated
	const UIOSRuntimeSettings& Settings = *GetDefault<UIOSRuntimeSettings>();
	TCHAR Path[4096];
	FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"), Path, ARRAY_COUNT(Path));
	FString Destination = FString::Printf(TEXT("%s\\Unreal Engine\\UnrealBuildTool\\SSHKeys\\%s\\%s\\RemoteToolChainPrivate.key"), Path, *(Settings.RemoteServerName), *(Settings.RSyncUsername));
	if (FPaths::FileExists(Destination))
	{
		FString MessagePrompt = FString::Printf(TEXT("An SSH Key already exists.  Do you want to replace this key?"));
		if (FPlatformMisc::MessageBoxExt(EAppMsgType::OkCancel, *MessagePrompt, TEXT("Key Exists")) == EAppReturnType::Cancel)
		{
			return FReply::Handled();
		}
	}

	FString CmdExe = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles/MakeAndInstallSSHKey.bat"));
	FString DeltaCopyPath = Settings.DeltaCopyInstallPath.Path;
	if (DeltaCopyPath.IsEmpty())
	{
		// if the user hasn't specified a location for DeltaCopy, try and use the default install location
		TCHAR ProgramPath[4096];
		FPlatformMisc::GetEnvironmentVariable(TEXT("PROGRAMFILES(X86)"), ProgramPath, ARRAY_COUNT(ProgramPath));
		DeltaCopyPath = FPaths::Combine(ProgramPath, TEXT("DeltaCopy"));

	}
	FString CygwinPath = TEXT("/cygdrive/") + FString(Path).Replace(TEXT(":"), TEXT("")).Replace(TEXT("\\"), TEXT("/"));
	FString EnginePath = FPaths::EngineDir();
	FString CommandLine = FString::Printf(TEXT("\"%s\\ssh.exe\" \"%s\\rsync.exe\" %s %s \"%s\" \"%s\" \"%s\""),
		*DeltaCopyPath,
		*DeltaCopyPath,
		*(Settings.RSyncUsername),
		*(Settings.RemoteServerName),
		Path,
		*CygwinPath,
		*EnginePath);

	OutputMessage = TEXT("");
	IPPProcess = MakeShareable(new FMonitoredProcess(CmdExe, CommandLine, false, false));
	IPPProcess->Launch();
	TickerHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FIOSTargetSettingsCustomization::UpdateStatusDelegate), 10.0f);
	RunningIPPProcess = true;

	return FReply::Handled();
}

const FSlateBrush* FIOSTargetSettingsCustomization::GetProvisionStatus() const
{
	if( bProvisionInstalled )
	{
		return FEditorStyle::GetBrush("Automation.Success");
	}
	else
	{
		return FEditorStyle::GetBrush("Automation.Fail");
	}
}

const FSlateBrush* FIOSTargetSettingsCustomization::GetCertificateStatus() const
{
	if( bCertificateInstalled )
	{
		return FEditorStyle::GetBrush("Automation.Success");
	}
	else
	{
		return FEditorStyle::GetBrush("Automation.Fail");
	}
}

bool FIOSTargetSettingsCustomization::UpdateStatusDelegate(float DeltaTime)
{
	if (IPPProcess.IsValid())
	{
		if (IPPProcess->IsRunning())
		{
			return true;
		}
		int RetCode = IPPProcess->GetReturnCode();
		IPPProcess = NULL;
		UpdateStatus();
		UpdateSSHStatus();
	}
	RunningIPPProcess = false;

	return false;
}

TSharedRef<ITableRow> FIOSTargetSettingsCustomization::HandleProvisionListGenerateRow( ProvisionPtr InProvision, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(SProvisionListRow, OwnerTable)
		.Provision(InProvision);
}

TSharedRef<ITableRow> FIOSTargetSettingsCustomization::HandleCertificateListGenerateRow( CertificatePtr InCertificate, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(SCertificateListRow, OwnerTable)
		.Certificate(InCertificate);
}

void FIOSTargetSettingsCustomization::HandleAllProvisionsHyperlinkNavigate( bool AllProvisions )
{
	bShowAllProvisions = AllProvisions;
	FilterLists();
}

void FIOSTargetSettingsCustomization::HandleAllCertificatesHyperlinkNavigate( bool AllCertificates )
{
	bShowAllCertificates = AllCertificates;
	FilterLists();
}

void FIOSTargetSettingsCustomization::FilterLists()
{
	FilteredProvisionList.Reset();
	FilteredCertificateList.Reset();

	for (int Index = 0; Index < ProvisionList.Num(); ++Index)
	{
		if (SelectedProvision.Contains(ProvisionList[Index]->Name) && SelectedFile.Contains(ProvisionList[Index]->FileName))
		{
			ProvisionList[Index]->bSelected = true;
		}
		else
		{
			ProvisionList[Index]->bSelected = false;
		}
		if (bShowAllProvisions || ProvisionList[Index]->Status.Contains("VALID"))
		{
			FilteredProvisionList.Add(ProvisionList[Index]);
		}
	}

	if (ProvisionList.Num() > 0)
	{
		if (ProvisionInfoSwitcher.IsValid())
		{
			ProvisionInfoSwitcher->SetActiveWidgetIndex(3);
		}
		if (FilteredProvisionList.Num() == 0 && !bShowAllProvisions)
		{
			FilteredProvisionList.Append(ProvisionList);
		}
	}
	else
	{
		if (ProvisionInfoSwitcher.IsValid())
		{
			ProvisionInfoSwitcher->SetActiveWidgetIndex(2);
		}
	}

	for (int Index = 0; Index < CertificateList.Num(); ++Index)
	{
		if (SelectedCert.Contains(CertificateList[Index]->Name))
		{
			CertificateList[Index]->bSelected = true;
		}
		else
		{
			CertificateList[Index]->bSelected = false;
		}
		if (bShowAllCertificates || CertificateList[Index]->Status.Contains("VALID"))
		{
			FilteredCertificateList.Add(CertificateList[Index]);
		}
	}

	if (CertificateList.Num() > 0)
	{
		if (CertificateInfoSwitcher.IsValid())
		{
			CertificateInfoSwitcher->SetActiveWidgetIndex(3);
		}
		if (FilteredCertificateList.Num() == 0 && !bShowAllCertificates)
		{
			FilteredCertificateList.Append(CertificateList);
		}
	}
	else
	{
		if (CertificateInfoSwitcher.IsValid())
		{
			CertificateInfoSwitcher->SetActiveWidgetIndex(2);
		}
	}

	CertificateListView->RequestListRefresh();
	ProvisionListView->RequestListRefresh();
}

bool FIOSTargetSettingsCustomization::IsImportEnabled() const
{
	return !RunningIPPProcess.Get();
}

void FIOSTargetSettingsCustomization::OnBundleIdentifierChanged(const FText& NewText, ETextCommit::Type CommitType, TSharedRef<IPropertyHandle> InPropertyHandle)
{
	FText OutText;
	InPropertyHandle->GetValueAsFormattedText(OutText);
	if (OutText.ToString() != NewText.ToString())
	{
		InPropertyHandle->SetValueFromFormattedString( NewText.ToString() );
		FindRequiredFiles();
	}
}

void FIOSTargetSettingsCustomization::OnRemoteServerChanged(const FText& NewText, ETextCommit::Type CommitType, TSharedRef<IPropertyHandle> InPropertyHandle)
{
	FText OutText;
	InPropertyHandle->GetValueAsFormattedText(OutText);
	if (OutText.ToString() != NewText.ToString())
	{
		InPropertyHandle->SetValueFromFormattedString(NewText.ToString());
		OutputMessage = TEXT("");
		UpdateSSHStatus();
	}
}

FText FIOSTargetSettingsCustomization::GetBundleText(TSharedRef<IPropertyHandle> InPropertyHandle) const
{
	FText OutText;
	InPropertyHandle->GetValueAsFormattedText(OutText);
	return OutText;
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE

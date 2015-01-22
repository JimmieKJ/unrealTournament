// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "IOSPlatformEditorPrivatePCH.h"
#include "IOSTargetSettingsCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "PropertyEditing.h"
#include "DesktopPlatformModule.h"
#include "MainFrame.h"
#include "IDetailPropertyRow.h"

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

#define LOCTEXT_NAMESPACE "IOSTargetSettings"

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
}

void FIOSTargetSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	SavedLayoutBuilder = &DetailLayout;

	UpdateStatus();

	BuildPListSection(DetailLayout);

	BuildIconSection(DetailLayout);

	BuildRemoteBuildingSection(DetailLayout);
}

void FIOSTargetSettingsCustomization::UpdateStatus()
{
	// get the provision status
	bProvisionInstalled = bCertificateInstalled = false;
	const ITargetPlatform* const Platform = GetTargetPlatformManager()->FindTargetPlatform("IOS");
	if (Platform)
	{
		FString NotInstalledTutorialLink;
		FString ProjectPath = FPaths::IsProjectFilePathSet() ? FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()) : FPaths::RootDir() / FApp::GetGameName() / FApp::GetGameName() + TEXT(".uproject");
		int32 Result = Platform->CheckRequirements(ProjectPath, false, NotInstalledTutorialLink);
		bProvisionInstalled = !(Result & ETargetPlatformReadyStatus::ProvisionNotFound);
		bCertificateInstalled = !(Result & ETargetPlatformReadyStatus::SigningKeyNotFound);
	}
}

void FIOSTargetSettingsCustomization::BuildPListSection(IDetailLayoutBuilder& DetailLayout)
{
	// Info.plist category
	IDetailCategoryBuilder& ProvisionCategory = DetailLayout.EditCategory(TEXT("Mobile Provision"));
	IDetailCategoryBuilder& AppManifestCategory = DetailLayout.EditCategory(TEXT("Info.plist"));
	IDetailCategoryBuilder& BundleCategory = DetailLayout.EditCategory(TEXT("Bundle Information"));
	IDetailCategoryBuilder& OrientationCategory = DetailLayout.EditCategory(TEXT("Orientation"));
	IDetailCategoryBuilder& RenderCategory = DetailLayout.EditCategory(TEXT("Rendering"));
	IDetailCategoryBuilder& OSInfoCategory = DetailLayout.EditCategory(TEXT("OS Info"));
	IDetailCategoryBuilder& DeviceCategory = DetailLayout.EditCategory(TEXT("Devices"));
	IDetailCategoryBuilder& BuildCategory = DetailLayout.EditCategory(TEXT("Build"));

	TSharedRef<SPlatformSetupMessage> PlatformSetupMessage = SNew(SPlatformSetupMessage, GameInfoPath)
		.PlatformName(LOCTEXT("iOSPlatformName", "iOS"))
		.OnSetupClicked(this, &FIOSTargetSettingsCustomization::CopySetupFilesIntoProject);

	SetupForPlatformAttribute = PlatformSetupMessage->GetReadyToGoAttribute();

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
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 1, 0, 1))
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ProvisionLabel", "Provision"))
				.Font(DetailLayout.GetDetailFont())
			]
		]
		.ValueContent()
			.MinDesiredWidth(300.0f)
			.MaxDesiredWidth(350.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(8,3,8,3))
			.AutoWidth()
			[
				SNew(SImage)
				.Image(this, &FIOSTargetSettingsCustomization::GetProvisionStatus)
			]
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 0, 0, 0))
			.AutoWidth()
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnClicked(this, &FIOSTargetSettingsCustomization::OnInstallProvisionClicked)
				[
					SNew(STextBlock)
					.Text(FText::FromString("Import Provision"))
				]
			]
		];

	ProvisionCategory.AddCustomRow(LOCTEXT("CertificateLabel", "Certificate"), false)
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 1, 0, 1))
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CertificateLabel", "Certificate"))
				.Font(DetailLayout.GetDetailFont())
			]
		]
		.ValueContent()
			.MinDesiredWidth(300.0f)
			.MaxDesiredWidth(350.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(8,3,8,3))
				[
					SNew(SImage)
					.Image(this, &FIOSTargetSettingsCustomization::GetCertificateStatus)
				]
				+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.OnClicked(this, &FIOSTargetSettingsCustomization::OnInstallCertificateClicked)
						[
							SNew(STextBlock)
							.Text(FText::FromString("Import Certificate"))
						]
					]
			];

	AppManifestCategory.AddCustomRow(LOCTEXT("Warning", "Warning"), false)
		.WholeRowWidget
		[
			PlatformSetupMessage
		];

	AppManifestCategory.AddCustomRow(LOCTEXT("InfoPlistHyperlink", "Info.plist Hyperlink"), false)
		.WholeRowWidget
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			[
				SNew(SHyperlinkLaunchURL, TEXT("https://developer.apple.com/library/ios/documentation/general/Reference/InfoPlistKeyReference/Articles/AboutInformationPropertyListFiles.html"))
				.Text(LOCTEXT("ApplePlistPage", "About Information Property List Files"))
				.ToolTipText(LOCTEXT("ApplePlistPageTooltip", "Opens a page that discusses Info.plist"))
			]
		];


	AppManifestCategory.AddCustomRow(LOCTEXT("InfoPlist", "Info.plist"), false)
		.IsEnabled(SetupForPlatformAttribute)
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 1, 0, 1))
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PlistLabel", "Info.plist"))
				.Font(DetailLayout.GetDetailFont())
			]
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("OpenPlistFolderButton", "Open PList Folder"))
				.ToolTipText(LOCTEXT("OpenPlistFolderButton_Tooltip", "Opens the folder containing the plist for the current project in Explorer or Finder"))
				.OnClicked(this, &FIOSTargetSettingsCustomization::OpenPlistFolder)
			]
		];

	// Show properties that are gated by the plist being present and writable
	FSimpleDelegate PlistModifiedDelegate = FSimpleDelegate::CreateRaw(this, &FIOSTargetSettingsCustomization::OnPlistPropertyModified);
	FGameProjectGenerationModule& GameProjectModule = FModuleManager::LoadModuleChecked<FGameProjectGenerationModule>(TEXT("GameProjectGeneration"));
	bool bHasCode = GameProjectModule.Get().ProjectHasCodeFiles();

#define SETUP_NONROCKET_PROP(PropName, Category, Tip, DisabledTip) \
	{ \
		TSharedRef<IPropertyHandle> PropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, PropName)); \
		Category.AddProperty(PropertyHandle) \
			.EditCondition(SetupForPlatformAttribute, NULL) \
			.IsEnabled(!FRocketSupport::IsRocket()) \
			.ToolTip(!FRocketSupport::IsRocket() ? Tip : DisabledTip); \
	}

#define SETUP_PLIST_PROP(PropName, Category, Tip) \
	{ \
		TSharedRef<IPropertyHandle> PropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, PropName)); \
		PropertyHandle->SetOnPropertyValueChanged(PlistModifiedDelegate); \
		Category.AddProperty(PropertyHandle) \
			.EditCondition(SetupForPlatformAttribute, NULL) \
			.ToolTip(Tip); \
	}

	SETUP_PLIST_PROP(BundleDisplayName, BundleCategory, LOCTEXT("BundleDisplayNameToolTip", "Specifies the the display name for the application. This will be displayed under the icon on the device."));
	SETUP_PLIST_PROP(BundleName, BundleCategory, LOCTEXT("BundleNameToolTip", "Specifies the the name of the application bundle. This is the short name for the application bundle."));
	SETUP_PLIST_PROP(BundleIdentifier, BundleCategory, LOCTEXT("BundleIdentifierToolTip", "Specifies the bundle identifier for the application."));
	SETUP_PLIST_PROP(VersionInfo, BundleCategory, LOCTEXT("VersionInfoToolTip", "Specifies the version for the application."));
	SETUP_PLIST_PROP(bSupportsPortraitOrientation, OrientationCategory, LOCTEXT("SupportsPortraitOrientationToolTip", "Supports default portrait orientation. Landscape will not be supported."));
	SETUP_PLIST_PROP(bSupportsUpsideDownOrientation, OrientationCategory, LOCTEXT("SupportsUpsideDownOrientationToolTip", "Supports upside down portrait orientation. Landscape will not be supported."));
	SETUP_PLIST_PROP(bSupportsLandscapeLeftOrientation, OrientationCategory, LOCTEXT("SupportsLandscapeLeftOrientationToolTip", "Supports left landscape orientation. Protrait will not be supported."));
	SETUP_PLIST_PROP(bSupportsLandscapeRightOrientation, OrientationCategory, LOCTEXT("SupportsLandscapeRightOrientationToolTip", "Supports right landscape orientation. Protrait will not be supported."));
	
	SETUP_PLIST_PROP(bSupportsMetal, RenderCategory, LOCTEXT("SupportsMetalToolTip", "Whether or not to add support for Metal API (requires IOS8 and A7 processors)."));
	SETUP_PLIST_PROP(bSupportsOpenGLES2, RenderCategory, LOCTEXT("SupportsOpenGLES2ToolTip", "Whether or not to add support for OpenGL ES2 (if this is false, then your game should specify minimum IOS8 version and use \"metal\" instead of \"opengles-2\" in UIRequiredDeviceCapabilities)"));

	SETUP_PLIST_PROP(bSupportsIPad, DeviceCategory, LOCTEXT("SupportsIPadToolTip", "Whether or not to add support for iPad devices"));
	SETUP_PLIST_PROP(bSupportsIPhone, DeviceCategory, LOCTEXT("SupportsIPhoneToolTip", "Whether or not to add support for iPhone devices"));

	SETUP_PLIST_PROP(MinimumiOSVersion, OSInfoCategory, LOCTEXT("MinimumiOSVersionToolTip", "Minimum iOS version this game supports"));

	SETUP_NONROCKET_PROP(bDevForArmV7, BuildCategory, LOCTEXT("DevForArmV7ToolTip", "Enable ArmV7 support? (this will be used if all type are unchecked)"), FIOSTargetSettingsCustomizationConstants::DisabledTip);
	SETUP_NONROCKET_PROP(bDevForArm64, BuildCategory, LOCTEXT("DevForArm64ToolTip", "Enable Arm64 support?"), FIOSTargetSettingsCustomizationConstants::DisabledTip);
	SETUP_NONROCKET_PROP(bDevForArmV7S, BuildCategory, LOCTEXT("DevForArmV7SToolTip", "Enable ArmV7s support?"), FIOSTargetSettingsCustomizationConstants::DisabledTip);
	SETUP_NONROCKET_PROP(bShipForArmV7, BuildCategory, LOCTEXT("ShipForArmV7ToolTip", "Enable ArmV7 support? (this will be used if all type are unchecked)"), FIOSTargetSettingsCustomizationConstants::DisabledTip);
	SETUP_NONROCKET_PROP(bShipForArm64, BuildCategory, LOCTEXT("ShipForArm64ToolTip", "Enable Arm64 support?"), FIOSTargetSettingsCustomizationConstants::DisabledTip);
	SETUP_NONROCKET_PROP(bShipForArmV7S, BuildCategory, LOCTEXT("ShipForArmV7SToolTip", "Enable ArmV7s support?"), FIOSTargetSettingsCustomizationConstants::DisabledTip);

#undef SETUP_PLIST_PROP
#undef SETUP_NONROCKET_PROP
}


void FIOSTargetSettingsCustomization::BuildRemoteBuildingSection(IDetailLayoutBuilder& DetailLayout)
{
	IDetailCategoryBuilder& BuildCategory = DetailLayout.EditCategory(TEXT("Build"));

	// Sub group we wish to add remote building options to.
	FText RemoteBuildingGroupName = LOCTEXT("RemoteBuildingGroupName", "Remote Build Options");
	IDetailGroup& RemoteBuildingGroup = BuildCategory.AddGroup(*RemoteBuildingGroupName.ToString(), RemoteBuildingGroupName, false);

	// Remote Server Name Property
	TSharedRef<IPropertyHandle> RemoteServerNamePropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, RemoteServerName));
	IDetailPropertyRow& RemoteServerNamePropertyRow = RemoteBuildingGroup.AddPropertyRow(RemoteServerNamePropertyHandle);
	RemoteServerNamePropertyRow
		.EditCondition(SetupForPlatformAttribute, NULL)
		.IsEnabled(!FRocketSupport::IsRocket())
		.ToolTip(!FRocketSupport::IsRocket() ? LOCTEXT("RemoteServerNameToolTip", "The name or ip address of the remote mac which will be used to build IOS") : FIOSTargetSettingsCustomizationConstants::DisabledTip);

	
	// Add Use RSync Property
	TSharedRef<IPropertyHandle> UseRSyncPropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, bUseRSync));
	IDetailPropertyRow& UseRSyncPropertRow = RemoteBuildingGroup.AddPropertyRow(UseRSyncPropertyHandle);
	UseRSyncPropertRow
		.EditCondition(SetupForPlatformAttribute, NULL)
		.IsEnabled(!FRocketSupport::IsRocket())
		.ToolTip(!FRocketSupport::IsRocket() ? LOCTEXT("UseRSyncToolTip", "Use RSync instead of RPCUtility") : FIOSTargetSettingsCustomizationConstants::DisabledTip);

	
	// Add RSync Username Property
	TSharedRef<IPropertyHandle> RSyncUsernamePropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, RSyncUsername));
	IDetailPropertyRow& RSyncUsernamePropertyRow = RemoteBuildingGroup.AddPropertyRow(RSyncUsernamePropertyHandle);
	RSyncUsernamePropertyRow
		.EditCondition(SetupForPlatformAttribute, NULL)
		.IsEnabled(!FRocketSupport::IsRocket())
		.ToolTip(!FRocketSupport::IsRocket() ? LOCTEXT("RSyncUsernameToolTip", "The username of the mac user that matches the specified SSH Key.") : FIOSTargetSettingsCustomizationConstants::DisabledTip);


	// Add existing SSH path label.
	TSharedRef<IPropertyHandle> SSHPrivateKeyLocationPropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, SSHPrivateKeyLocation));
	IDetailPropertyRow& SSHPrivateKeyLocationPropertyRow = RemoteBuildingGroup.AddPropertyRow(SSHPrivateKeyLocationPropertyHandle);
	SSHPrivateKeyLocationPropertyRow
		.EditCondition(SetupForPlatformAttribute, NULL)
		.IsEnabled(!FRocketSupport::IsRocket())
		.ToolTip(!FRocketSupport::IsRocket() ? LOCTEXT("SSHPrivateKeyLocationToolTip", "The existing location of an SSH Key found by UE4.") : FIOSTargetSettingsCustomizationConstants::DisabledTip);


	// Add SSH override path
	TSharedRef<IPropertyHandle> SSHPrivateKeyOverridePathPropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, SSHPrivateKeyOverridePath));
	IDetailPropertyRow& SSHPrivateKeyOverridePathPropertyRow = RemoteBuildingGroup.AddPropertyRow(SSHPrivateKeyOverridePathPropertyHandle);
	SSHPrivateKeyOverridePathPropertyRow
		.EditCondition(SetupForPlatformAttribute, NULL)
		.IsEnabled(!FRocketSupport::IsRocket())
		.ToolTip(!FRocketSupport::IsRocket() ? LOCTEXT("SSHPrivateKeyOverridePathToolTip", "Override the existing SSH Private Key with one from a specified location.") : FIOSTargetSettingsCustomizationConstants::DisabledTip);
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

void FIOSTargetSettingsCustomization::OnPlistPropertyModified()
{
	check(SetupForPlatformAttribute.Get() == true);
	const UIOSRuntimeSettings& Settings = *GetDefault<UIOSRuntimeSettings>();

	FManifestUpdateHelper Updater(GameInfoPath);

	// The text we're trying to replace looks like this:
	// 	<key>UISupportedInterfaceOrientations</key>
	// 	<array>
	// 		<string>UIInterfaceOrientationLandscapeRight</string>
	// 		<string>UIInterfaceOrientationLandscapeLeft</string>
	// 	</array>
	const FString InterfaceOrientations(TEXT("<key>UISupportedInterfaceOrientations</key>"));
	const FString ClosingArray(TEXT("</array>"));

	// Build the replacement array
	FString OrientationArrayBody = TEXT("\n\t<array>\n");
	if (Settings.bSupportsPortraitOrientation)
	{
		OrientationArrayBody += TEXT("\t\t<string>UIInterfaceOrientationPortrait</string>\n");
	}
	if (Settings.bSupportsUpsideDownOrientation)
	{
		OrientationArrayBody += TEXT("\t\t<string>UIInterfaceOrientationPortraitUpsideDown</string>\n");
	}
	if (Settings.bSupportsLandscapeLeftOrientation && (!Settings.bSupportsPortraitOrientation && !Settings.bSupportsUpsideDownOrientation))
	{
		OrientationArrayBody += TEXT("\t\t<string>UIInterfaceOrientationLandscapeLeft</string>\n");
	}
	if (Settings.bSupportsLandscapeRightOrientation && (!Settings.bSupportsPortraitOrientation && !Settings.bSupportsUpsideDownOrientation))
	{
		OrientationArrayBody += TEXT("\t\t<string>UIInterfaceOrientationLandscapeRight</string>\n");
	}
	OrientationArrayBody += TEXT("\t");
	Updater.ReplaceKey(InterfaceOrientations, ClosingArray, OrientationArrayBody);

	// build the replacement bundle display name
	const FString BundleDisplayNameKey(TEXT("<key>CFBundleDisplayName</key>"));
	const FString ClosingString(TEXT("</string>"));
	FString BundleDisplayNameBody = TEXT("\n\t<string>") + Settings.BundleDisplayName;
	Updater.ReplaceKey(BundleDisplayNameKey, ClosingString, BundleDisplayNameBody);

	// build the replacement bundle display name
	const FString BundleNameKey(TEXT("<key>CFBundleName</key>"));
	FString BundleNameBody = TEXT("\n\t<string>") + Settings.BundleName;
	Updater.ReplaceKey(BundleNameKey, ClosingString, BundleNameBody);

	// build the replacement bundle identifier
	const FString BundleIdentifierKey(TEXT("<key>CFBundleIdentifier</key>"));
	FString BundleIdentifierBody = TEXT("\n\t<string>") + Settings.BundleIdentifier;
	Updater.ReplaceKey(BundleIdentifierKey, ClosingString, BundleIdentifierBody);

	// build the replacement version info
	const FString BundleShortVersionKey(TEXT("<key>CFBundleShortVersionString</key>"));
	FString VersionInfoBody = TEXT("\n\t<string>") + Settings.VersionInfo;
	Updater.ReplaceKey(BundleShortVersionKey, ClosingString, VersionInfoBody);

	// build the replacement required device caps
	const FString RequiredDeviceCaps(TEXT("<key>UIRequiredDeviceCapabilities</key>"));
	FString DeviceCapsArrayBody = TEXT("\n\t<array>\n");
	// automatically add armv7 for now
	DeviceCapsArrayBody += TEXT("\t\t<string>armv7</string>\n");
	if (Settings.bSupportsOpenGLES2)
	{
		DeviceCapsArrayBody += TEXT("\t\t<string>opengles-2</string>\n");
	}
	else if (Settings.bSupportsMetal)
	{
		DeviceCapsArrayBody += TEXT("\t\t<string>metal</string>\n");
	}
	DeviceCapsArrayBody += TEXT("\t");
	Updater.ReplaceKey(RequiredDeviceCaps, ClosingArray, DeviceCapsArrayBody);

	// build the replacement device families
	const FString DeviceFamilyKey(TEXT("<key>UIDeviceFamily</key>"));
	FString FamilyKeyBody = TEXT("\n\t<array>\n");
	// automatically add armv7 for now
	if (Settings.bSupportsIPhone)
	{
		FamilyKeyBody += TEXT("\t\t<integer>1</integer>\n");
	}
	if (Settings.bSupportsIPad)
	{
		FamilyKeyBody += TEXT("\t\t<integer>2</integer>\n");
	}
	FamilyKeyBody += TEXT("\t");
	Updater.ReplaceKey(DeviceFamilyKey, ClosingArray, FamilyKeyBody);

	// build the replacement min iOS version
	const FString MiniOSVersionKey(TEXT("<key>MinimumOSVersion</key>"));
	FString iOSVersionBody = TEXT("\n\t<string>");
	switch (Settings.MinimumiOSVersion)
	{
	case EIOSVersion::IOS_61:
		iOSVersionBody += TEXT("6.1");
		break;
	case EIOSVersion::IOS_7:
		iOSVersionBody += TEXT("7.0");
		break;
	case EIOSVersion::IOS_8:
		iOSVersionBody += TEXT("8.0");
		break;
	}
	Updater.ReplaceKey(MiniOSVersionKey, ClosingString, iOSVersionBody);

	// Write out the updated .plist
	Updater.Finalize(GameInfoPath, true, FFileHelper::EEncodingOptions::ForceUTF8);
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

static FString OutputMessage;
static void OnOutput(FString Message)
{
	OutputMessage += Message;
	UE_LOG(LogTemp, Display, TEXT("%s\n"), *Message);
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
#if PLATFORM_MAC
		FString CmdExe = TEXT("/bin/sh");
		FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles/Mac/RunMono.sh"));
		FString IPPPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/DotNet/IOS/IPhonePackager.exe"));
		FString CommandLine = FString::Printf(TEXT("\"%s\" \"%s\" Install Engine -project \"%s\" -provision \"%s\""), *ScriptPath, *IPPPath, *ProjectPath, *ProvisionPath);
#else
		FString CmdExe = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/DotNet/IOS/IPhonePackager.exe"));
		FString CommandLine = FString::Printf(TEXT("Install Engine -project \"%s\" -provision \"%s\""), *ProjectPath, *ProvisionPath);
#endif
		IPPProcess = MakeShareable(new FMonitoredProcess(CmdExe, CommandLine, true));
		OutputMessage = TEXT("");
		IPPProcess->OnOutput().BindStatic(&OnOutput);
		IPPProcess->Launch();
		FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FIOSTargetSettingsCustomization::UpdateStatusDelegate), 10.0f);
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
		CertPath = FPaths::ConvertRelativePathToFull(OpenFilenames[0]);
#if PLATFORM_MAC
		FString CmdExe = TEXT("/bin/sh");
		FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles/Mac/RunMono.sh"));
		FString IPPPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/DotNet/IOS/IPhonePackager.exe"));
		FString CommandLine = FString::Printf(TEXT("\"%s\" \"%s\" Install Engine -project \"%s\" -certificate \"%s\""), *ScriptPath, *IPPPath, *ProjectPath, *CertPath);
#else
		FString CmdExe = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/DotNet/IOS/IPhonePackager.exe"));
		FString CommandLine = FString::Printf(TEXT("Install Engine -project \"%s\" -certificate \"%s\""), *ProjectPath, *CertPath);
#endif
		IPPProcess = MakeShareable(new FMonitoredProcess(CmdExe, CommandLine, true));
		OutputMessage = TEXT("");
		IPPProcess->OnOutput().BindStatic(&OnOutput);
		IPPProcess->Launch();
		FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FIOSTargetSettingsCustomization::UpdateStatusDelegate), 10.0f);
	}

	return FReply::Handled();
}

FReply FIOSTargetSettingsCustomization::OnCertificateRequestClicked()
{
	// TODO: bring up an open file dialog and then install the provision
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
	if (IPPProcess->IsRunning())
	{
		return true;
	}
	int RetCode = IPPProcess->GetReturnCode();
	IPPProcess = NULL;
	ensure(RetCode == 0);
	UpdateStatus();

	return false;
}
//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
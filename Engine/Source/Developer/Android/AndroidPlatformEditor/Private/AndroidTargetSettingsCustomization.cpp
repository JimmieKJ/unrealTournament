// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AndroidPlatformEditorPrivatePCH.h"
#include "AndroidTargetSettingsCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "PropertyEditing.h"

#include "ScopedTransaction.h"
#include "SExternalImageReference.h"
#include "SHyperlinkLaunchURL.h"
#include "SPlatformSetupMessage.h"
#include "PlatformIconInfo.h"
#include "SourceControlHelpers.h"
#include "ManifestUpdateHelper.h"
#include "SNotificationList.h"
#include "NotificationManager.h"

#define LOCTEXT_NAMESPACE "AndroidRuntimeSettings"

//////////////////////////////////////////////////////////////////////////
// FAndroidTargetSettingsCustomization
namespace FAndroidTargetSettingsCustomizationConstants
{
	const FText DisabledTip = LOCTEXT("GitHubSourceRequiredToolTip", "This requires GitHub source.");
}

TSharedRef<IDetailCustomization> FAndroidTargetSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FAndroidTargetSettingsCustomization);
}

FAndroidTargetSettingsCustomization::FAndroidTargetSettingsCustomization()
	: AndroidRelativePath(TEXT(""))
	, EngineAndroidPath(FPaths::EngineDir() + TEXT("Build/Android/Java"))
	, GameAndroidPath(FPaths::GameDir() + TEXT("Build/Android"))
	, EngineManifestPath(EngineAndroidPath / TEXT("AndroidManifest.xml"))
	, GameManifestPath(GameAndroidPath / TEXT("AndroidManifest.xml"))
	, EngineGooglePlayAppIDPath(EngineAndroidPath / TEXT("res") / TEXT("values") / TEXT("GooglePlayAppID.xml"))
	, GameGooglePlayAppIDPath(GameAndroidPath / TEXT("res") / TEXT("values") / TEXT("GooglePlayAppID.xml"))
	, EngineSigningConfigPath(EngineAndroidPath / TEXT("SigningConfig.xml"))
	, GameSigningConfigPath(GameAndroidPath / TEXT("SigningConfig.xml"))
	, EngineProguardPath(EngineAndroidPath / TEXT("proguard-project.txt"))
	, GameProguardPath(GameAndroidPath / TEXT("proguard-project.txt"))
	, EngineProjectPropertiesPath(EngineAndroidPath / TEXT("project.properties"))
	, GameProjectPropertiesPath(GameAndroidPath / TEXT("project.properties"))
{
	new (IconNames) FPlatformIconInfo(TEXT("res/drawable/icon.png"), LOCTEXT("SettingsIcon", "Icon"), FText::GetEmpty(), 48, 48, FPlatformIconInfo::Required);
	new (IconNames) FPlatformIconInfo(TEXT("res/drawable-ldpi/icon.png"), LOCTEXT("SettingsIcon_LDPI", "LDPI Icon"), FText::GetEmpty(), 36, 36, FPlatformIconInfo::Required);
	new (IconNames) FPlatformIconInfo(TEXT("res/drawable-mdpi/icon.png"), LOCTEXT("SettingsIcon_MDPI", "MDPI Icon"), FText::GetEmpty(), 48, 48, FPlatformIconInfo::Required);
	new (IconNames) FPlatformIconInfo(TEXT("res/drawable-hdpi/icon.png"), LOCTEXT("SettingsIcon_HDPI", "HDPI Icon"), FText::GetEmpty(), 72, 72, FPlatformIconInfo::Required);
	new (IconNames) FPlatformIconInfo(TEXT("res/drawable-xhdpi/icon.png"), LOCTEXT("SettingsIcon_XHDPI", "XHDPI Icon"), FText::GetEmpty(), 96, 96, FPlatformIconInfo::Required);
}

void FAndroidTargetSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	SavedLayoutBuilder = &DetailLayout;

	BuildAppManifestSection(DetailLayout);
	BuildIconSection(DetailLayout);
}

void FAndroidTargetSettingsCustomization::BuildAppManifestSection(IDetailLayoutBuilder& DetailLayout)
{
	// Cache some categories
	IDetailCategoryBuilder& AppManifestCategory = DetailLayout.EditCategory(TEXT("AppManifest"));
	IDetailCategoryBuilder& BuildCategory = DetailLayout.EditCategory(TEXT("Build"));

	TSharedRef<SPlatformSetupMessage> PlatformSetupMessage = SNew(SPlatformSetupMessage, GameManifestPath)
		.PlatformName(LOCTEXT("AndroidPlatformName", "Android"))
		.OnSetupClicked(this, &FAndroidTargetSettingsCustomization::CopySetupFilesIntoProject);

	SetupForPlatformAttribute = PlatformSetupMessage->GetReadyToGoAttribute();

	AppManifestCategory.AddCustomRow(LOCTEXT("Warning", "Warning"), false)
		.WholeRowWidget
		[
			PlatformSetupMessage
		];

	AppManifestCategory.AddCustomRow(LOCTEXT("AppManifestHyperlink", "App Manifest Hyperlink"), false)
		.WholeRowWidget
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			[
				SNew(SHyperlinkLaunchURL, TEXT("http://developer.android.com/guide/topics/manifest/manifest-intro.html"))
				.Text(LOCTEXT("AndroidDeveloperManifestPage", "Android Developer Page on the App Manifest"))
				.ToolTipText(LOCTEXT("AndroidDeveloperManifestPageTooltip", "Opens a page that discusses the App Manifest"))
			]
		];

	AppManifestCategory.AddCustomRow(LOCTEXT("AppManifestLabel", "App Manifest"), false)
		.IsEnabled(SetupForPlatformAttribute)
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 1, 0, 1))
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AppManifestLabel", "App Manifest"))
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
				.Text(LOCTEXT("OpenManifestFolderButton", "Open Manifest Folder"))
				.ToolTipText(LOCTEXT("OpenManifestFolderButton_Tooltip", "Opens the folder containing the manifest (AndroidManifest.xml) in Explorer or Finder"))
				.OnClicked(this, &FAndroidTargetSettingsCustomization::OpenManifestFolder)
			]
		];

	// Show properties that are gated by the manifest file being present and writable
	TSharedRef<IPropertyHandle> OrientationProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UAndroidRuntimeSettings, Orientation));
	OrientationProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateRaw(this, &FAndroidTargetSettingsCustomization::OnOrientationModified));
	AppManifestCategory.AddProperty(OrientationProperty)
		.EditCondition(SetupForPlatformAttribute, NULL);

	TSharedRef<IPropertyHandle> DepthBufferPreferenceProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UAndroidRuntimeSettings, DepthBufferPreference));
	DepthBufferPreferenceProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateRaw(this, &FAndroidTargetSettingsCustomization::OnDepthBufferPreferenceModified));
	AppManifestCategory.AddProperty(DepthBufferPreferenceProperty)
		.EditCondition(SetupForPlatformAttribute, NULL);

	// Google Play category
	IDetailCategoryBuilder& GooglePlayCategory = DetailLayout.EditCategory(TEXT("GooglePlayServices"));
	
	TSharedRef<SPlatformSetupMessage> GooglePlaySetupMessage = SNew(SPlatformSetupMessage, GameGooglePlayAppIDPath)
		.PlatformName(LOCTEXT("GooglePlayPlatformName", "Google Play services"))
		.OnSetupClicked(this, &FAndroidTargetSettingsCustomization::CopyGooglePlayAppIDFileIntoProject);

	SetupForGooglePlayAttribute = GooglePlaySetupMessage->GetReadyToGoAttribute();

	GooglePlayCategory.AddCustomRow(LOCTEXT("Warning", "Warning"), false)
		.WholeRowWidget
		[
			GooglePlaySetupMessage
		];

	GooglePlayCategory.AddCustomRow(LOCTEXT("AppIDHyperlink", "App ID Hyperlink"), false)
		.WholeRowWidget
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			[
				SNew(SHyperlinkLaunchURL, TEXT("http://developer.android.com/google/index.html"))
				.Text(LOCTEXT("GooglePlayDeveloperPage", "Android Developer Page on Google Play services"))
				.ToolTipText(LOCTEXT("GooglePlayDeveloperPageTooltip", "Opens a page that discusses Google Play services"))
			]
		];

	TSharedRef<IPropertyHandle> EnabledProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UAndroidRuntimeSettings, bEnableGooglePlaySupport));
	GooglePlayCategory.AddProperty(EnabledProperty)
		.EditCondition(SetupForGooglePlayAttribute, NULL);

	TSharedRef<IPropertyHandle> AppIDProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UAndroidRuntimeSettings, GamesAppID));
	AppIDProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateRaw(this, &FAndroidTargetSettingsCustomization::OnAppIDModified));
	GooglePlayCategory.AddProperty(AppIDProperty)
		.EditCondition(SetupForGooglePlayAttribute, NULL);

	TSharedRef<IPropertyHandle> AdMobAdUnitIDProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UAndroidRuntimeSettings, AdMobAdUnitID));
	GooglePlayCategory.AddProperty(AdMobAdUnitIDProperty)
		.EditCondition(SetupForGooglePlayAttribute, NULL);

	TSharedRef<IPropertyHandle> GooglePlayLicenseKeyProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UAndroidRuntimeSettings, GooglePlayLicenseKey));
	GooglePlayCategory.AddProperty(GooglePlayLicenseKeyProperty)
		.EditCondition(SetupForGooglePlayAttribute, NULL);


#define SETUP_NONROCKET_PROP(PropName, Category, Tip) \
	{ \
		TSharedRef<IPropertyHandle> PropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UAndroidRuntimeSettings, PropName)); \
		Category.AddProperty(PropertyHandle) \
			.EditCondition(SetupForPlatformAttribute, NULL) \
			.IsEnabled(!FRocketSupport::IsRocket()) \
			.ToolTip(!FRocketSupport::IsRocket() ? Tip : FAndroidTargetSettingsCustomizationConstants::DisabledTip); \
	}
	SETUP_NONROCKET_PROP(bBuildForArmV7, BuildCategory, LOCTEXT("BuildForArmV7ToolTip", "Enable ArmV7 CPU architecture support? (this will be used if all CPU architecture types are unchecked)"));
	SETUP_NONROCKET_PROP(bBuildForX86, BuildCategory, LOCTEXT("BuildForX86ToolTip", "Enable X86 CPU architecture support?"));
	SETUP_NONROCKET_PROP(bBuildForES2, BuildCategory, LOCTEXT("BuildForES2ToolTip", "Enable OpenGL ES2 rendering support? (this will be used if rendering types are unchecked)"));
	SETUP_NONROCKET_PROP(bBuildForES31, BuildCategory, LOCTEXT("BuildForES31ToolTip", "Enable OpenGL ES31 + AEP (Android Extension Pack) rendering support?"));
	// @todo android fat binary: Put back in when we expose those
//	SETUP_NONROCKET_PROP(bSplitIntoSeparateApks, BuildCategory, LOCTEXT("SplitIntoSeparateAPKsToolTip", "If checked, CPU architectures and rendering types will be split into separate .apk files"));
}

void FAndroidTargetSettingsCustomization::BuildIconSection(IDetailLayoutBuilder& DetailLayout)
{
	// Icon category
	IDetailCategoryBuilder& IconCategory = DetailLayout.EditCategory(TEXT("Icons"));

	IconCategory.AddCustomRow(LOCTEXT("IconsHyperlink", "Icons Hyperlink"), false)
		.WholeRowWidget
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			[
				SNew(SHyperlinkLaunchURL, TEXT("http://developer.android.com/design/style/iconography.html"))
				.Text(LOCTEXT("AndroidDeveloperIconographyPage", "Android Developer Page on Iconography"))
				.ToolTipText(LOCTEXT("AndroidDeveloperIconographyPageTooltip", "Opens a page on Android Iconography"))
			]
		];

	for (const FPlatformIconInfo& Info : IconNames)
	{
		const FString AutomaticImagePath = EngineAndroidPath / Info.IconPath;
		const FString TargetImagePath = GameAndroidPath / Info.IconPath;

		IconCategory.AddCustomRow(Info.IconName)
		.NameContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.Padding( FMargin( 0, 1, 0, 1 ) )
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
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SExternalImageReference, AutomaticImagePath, TargetImagePath)
				.FileDescription(Info.IconDescription)
				.RequiredSize(Info.IconRequiredSize)
				.MaxDisplaySize(FVector2D(Info.IconRequiredSize))
			]
		];
	}
}

FReply FAndroidTargetSettingsCustomization::OpenManifestFolder()
{
	const FString EditAppManifestFolder = FPaths::ConvertRelativePathToFull(FPaths::GetPath(GameManifestPath));
	FPlatformProcess::ExploreFolder(*EditAppManifestFolder);

	return FReply::Handled();
}

void FAndroidTargetSettingsCustomization::CopySetupFilesIntoProject()
{
	// First copy the manifest, it must get copied
	FText ErrorMessage;
	if (!SourceControlHelpers::CopyFileUnderSourceControl(GameManifestPath, EngineManifestPath, LOCTEXT("AppManifest", "App Manifest"), /*out*/ ErrorMessage))
	{
		FNotificationInfo Info(ErrorMessage);
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	else
	{
		// Now try to copy all of the icons, etc... (these can be ignored if the file already exists)
		for (const FPlatformIconInfo& Info : IconNames)
		{
			const FString EngineImagePath = EngineAndroidPath / Info.IconPath;
			const FString ProjectImagePath = GameAndroidPath / Info.IconPath;

			if (!FPaths::FileExists(ProjectImagePath))
			{
				SourceControlHelpers::CopyFileUnderSourceControl(ProjectImagePath, EngineImagePath, Info.IconName, /*out*/ ErrorMessage);
			}
		}

		// and copy the other files (aren't required)
		SourceControlHelpers::CopyFileUnderSourceControl(GameSigningConfigPath, EngineSigningConfigPath, LOCTEXT("SigningConfig", "Distribution Signing Config"), /*out*/ ErrorMessage);
		SourceControlHelpers::CopyFileUnderSourceControl(GameProguardPath, EngineProguardPath, LOCTEXT("Proguard", "Proguard Settings"), /*out*/ ErrorMessage);
		SourceControlHelpers::CopyFileUnderSourceControl(GameProjectPropertiesPath, EngineProjectPropertiesPath, LOCTEXT("ProjectProperties", "Project Properties"), /*out*/ ErrorMessage);
	}

	SavedLayoutBuilder->ForceRefreshDetails();
}

void FAndroidTargetSettingsCustomization::CopyGooglePlayAppIDFileIntoProject()
{
	FText ErrorMessage;
	if (!SourceControlHelpers::CopyFileUnderSourceControl(GameGooglePlayAppIDPath, EngineGooglePlayAppIDPath, LOCTEXT("GooglePlayAppID", "GooglePlayAppID.xml"), /*out*/ ErrorMessage))
	{
		FNotificationInfo Info(ErrorMessage);
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	SavedLayoutBuilder->ForceRefreshDetails();
}

void FAndroidTargetSettingsCustomization::OnOrientationModified()
{
	check(SetupForPlatformAttribute.Get() == true);


	FManifestUpdateHelper Updater(GameManifestPath);

	const FString OrientationTag(TEXT("android:screenOrientation=\""));
	const FString ClosingQuote(TEXT("\""));
	const FString NewOrientationString = OrientationToString(GetDefault<UAndroidRuntimeSettings>()->Orientation);
	Updater.ReplaceKey(OrientationTag, ClosingQuote, NewOrientationString);

	Updater.Finalize(GameManifestPath);
}

FString FAndroidTargetSettingsCustomization::OrientationToString(const EAndroidScreenOrientation::Type Orientation)
{
	extern ANDROIDRUNTIMESETTINGS_API class UEnum* Z_Construct_UEnum_UAndroidRuntimeSettings_EAndroidScreenOrientation();
	
	UEnum* Enum = Z_Construct_UEnum_UAndroidRuntimeSettings_EAndroidScreenOrientation();
	check(Enum != nullptr);
	
	return Enum->GetMetaData(TEXT("ManifestValue"), (int32)Orientation);
}

void FAndroidTargetSettingsCustomization::OnDepthBufferPreferenceModified()
{
	check(SetupForPlatformAttribute.Get() == true);


	FManifestUpdateHelper Updater(GameManifestPath);

	const FString PrefixTag(TEXT("meta-data android:name=\"com.epicgames.ue4.GameActivity.DepthBufferPreference\" android:value=\""));
	const FString ClosingQuote(TEXT("\""));
	const FString NewDepthBufferPreferenceString = DepthBufferPreferenceToString(GetDefault<UAndroidRuntimeSettings>()->DepthBufferPreference);

	// First check if the key needs to be created
	if (!Updater.HasKey(PrefixTag))
	{
		// Add the key for value replacement
		const FString CloseActivityTag(TEXT("</activity"));
		const FString CloseBracket(TEXT(">"));
		const FString NewKeyString(TEXT(">\n\t\t<meta-data android:name=\"com.epicgames.ue4.GameActivity.DepthBufferPreference\" android:value=\"0\"/"));
		Updater.ReplaceKey(CloseActivityTag, CloseBracket, NewKeyString);
	}

	Updater.ReplaceKey(PrefixTag, ClosingQuote, NewDepthBufferPreferenceString);

	Updater.Finalize(GameManifestPath);
}

FString FAndroidTargetSettingsCustomization::DepthBufferPreferenceToString(const EAndroidDepthBufferPreference::Type DepthBufferPreference)
{
	extern ANDROIDRUNTIMESETTINGS_API class UEnum* Z_Construct_UEnum_UAndroidRuntimeSettings_EAndroidDepthBufferPreference();

	UEnum* Enum = Z_Construct_UEnum_UAndroidRuntimeSettings_EAndroidDepthBufferPreference();
	check(Enum != nullptr);

	return Enum->GetMetaData(TEXT("ManifestValue"), (int32)DepthBufferPreference);
}

void FAndroidTargetSettingsCustomization::OnAppIDModified()
{
	check(SetupForPlatformAttribute.Get() == true);


	FManifestUpdateHelper Updater(GameGooglePlayAppIDPath);

	const FString AppIDTag(TEXT("name=\"app_id\">"));
	const FString ClosingTag(TEXT("</string>"));
	const FString NewIDString = GetDefault<UAndroidRuntimeSettings>()->GamesAppID;
	Updater.ReplaceKey(AppIDTag, ClosingTag, NewIDString);

	Updater.Finalize(GameGooglePlayAppIDPath);
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
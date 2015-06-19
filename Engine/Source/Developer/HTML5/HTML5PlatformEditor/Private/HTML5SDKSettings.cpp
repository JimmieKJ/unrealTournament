// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "HTML5PlatformEditorPrivatePCH.h"

//#include "EngineTypes.h"
#include "HTML5SDKSettings.h"
#include "IHTML5TargetPlatformModule.h"
#include "PropertyHandle.h"
#include "DesktopPlatformModule.h"
#include "PlatformFilemanager.h"
#include "GenericPlatformFile.h"
#include "PlatformInfo.h"

#define LOCTEXT_NAMESPACE "FHTML5PlatformEditorModule"

DEFINE_LOG_CATEGORY_STATIC(HTML5SDKSettings, Log, All);

UHTML5SDKSettings::UHTML5SDKSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TargetPlatformModule(nullptr)
{
}

#if WITH_EDITOR
void UHTML5SDKSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (!TargetPlatformModule)
	{
		TargetPlatformModule = FModuleManager::LoadModulePtr<IHTML5TargetPlatformModule>("HTML5TargetPlatform");
	}
	
	if (TargetPlatformModule) 
	{
		UpdateGlobalUserConfigFile();//SaveConfig();
		TargetPlatformModule->RefreshAvailableDevices();
	}
}

void UHTML5SDKSettings::QueryKnownBrowserLocations()
{
	if (DeviceMap.Num() != 0) 
	{
		return;
	}

	struct FBrowserLocation 
	{
		FString Name;
		FString Path;
	} PossibleLocations[] = 
	{
#if PLATFORM_WINDOWS
		{TEXT("Nightly(64bit)"), TEXT("C:/Program Files/Nightly/firefox.exe")},
		{TEXT("Nightly"), TEXT("C:/Program Files (x86)/Nightly/firefox.exe")},
		{TEXT("Firefox"), TEXT("C:/Program Files (x86)/Mozilla Firefox/firefox.exe")},
		{TEXT("Chrome"), TEXT("C:/Program Files (x86)/Google/Chrome/Application/chrome.exe")},
#elif PLATFORM_MAC
		{TEXT("Safari"), TEXT("/Applications/Safari.app")},
		{TEXT("Firefox"),TEXT("/Applications/Firefox.app")},
		{TEXT("Chrome"),TEXT("/Applications/Google Chrome.app")},
#elif PLATFORM_LINUX
		{TEXT("Firefox"), TEXT("/usr/bin/firefox")},
#else
		{TEXT("Firefox"), TEXT("")},
#endif
	};

	for (const auto& Loc : PossibleLocations)
	{
		if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*Loc.Path) || FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*Loc.Path))
		{
			FHTML5DeviceMapping NewDevice;
			NewDevice.DeviceName = Loc.Name;
			NewDevice.DevicePath.FilePath = Loc.Path;
			DeviceMap.Add(NewDevice);
		}
	}
}

TSharedRef<IPropertyTypeCustomization> FHTML5SDKPathCustomization::MakeInstance()
{
	return MakeShareable(new FHTML5SDKPathCustomization());
}

void FHTML5SDKPathCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	PathProperty = StructPropertyHandle->GetChildHandle("SDKPath");
	VersionProperty = StructPropertyHandle->GetChildHandle("EmscriptenVersion");

	InvalidVersion = MakeShareable(new FHTML5Version());
	InvalidVersion->Text = LOCTEXT("IncorrectSDKDirectory", "Incorrect SDK Directory");

	UpdateAvailableVersions();

	if (PathProperty.IsValid())
	{
		HeaderRow.ValueContent()
			.MinDesiredWidth(125.0f)
			.MaxDesiredWidth(600.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.MinDesiredWidth(200)
					[
						PathProperty->CreatePropertyValueWidget()
					]
				]
				+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
					.VAlign(VAlign_Center)
					[
						SAssignNew(BrowseButton, SButton)
						.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
						.ToolTipText(LOCTEXT("FolderButtonToolTipText", "Choose a directory from this computer"))
						.OnClicked(FOnClicked::CreateSP(this, &FHTML5SDKPathCustomization::OnPickDirectory, PathProperty.ToSharedRef()))
						.ContentPadding(2.0f)
						.ForegroundColor(FSlateColor::UseForeground())
						.IsFocusable(false)
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
							.ColorAndOpacity(FSlateColor::UseForeground())
						]
					]
				+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
					.VAlign(VAlign_Center)
					[
						SAssignNew(SDKSelector, SComboBox< TSharedPtr<FHTML5Version> >)
						.OptionsSource(&Options)
						.InitiallySelectedItem(Options[0])
						.OnSelectionChanged(this, &FHTML5SDKPathCustomization::OnSelectionChanged)
						.ContentPadding(2)
						.OnGenerateWidget(this, &FHTML5SDKPathCustomization::OnGenerateWidget)
						[
							SNew(STextBlock)
							.Text(this, &FHTML5SDKPathCustomization::GetSelectedText)
						]
					]
			]
		.NameContent()
			[
				StructPropertyHandle->CreatePropertyNameWidget()
			];
	}
}

void FHTML5SDKPathCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

FReply FHTML5SDKPathCustomization::OnPickDirectory(TSharedRef<IPropertyHandle> PropertyHandle)// const
{
	FString Directory;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	auto& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	auto* Settings = GetMutableDefault<UHTML5SDKSettings>();
	if (!DesktopPlatform || !Settings)
	{
		return FReply::Handled();
	}

	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(BrowseButton.ToSharedRef());
	void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

	FString StartDirectory = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_IMPORT);
	for (;;)
	{
		if (DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, LOCTEXT("FolderDialogTitle", "Choose a directory").ToString(), StartDirectory, Directory))
		{
			FText FailureReason;
 			if (PlatformFile.DirectoryExists(*Directory))
 			{
				// TODO: Update config value too
				Settings->EmscriptenRoot.SDKPath = Directory;
				PathProperty->SetValue(Directory);
				UpdateAvailableVersions();
				SDKSelector->SetSelectedItem(SelectedVersionItem);
				SDKSelector->RefreshOptions();
 			}
 			else
			{
				StartDirectory = Directory;
				FMessageDialog::Open(EAppMsgType::Ok, FailureReason);
				continue;
			}
		}
		break;
	}

	return FReply::Handled();
}

TSharedRef<SWidget> FHTML5SDKPathCustomization::OnGenerateWidget(TSharedPtr<FHTML5Version> InItem)
{
	return SNew(STextBlock)
		.Text(InItem->Text);
}

FText FHTML5SDKPathCustomization::GetSelectedText() const
{
	return SelectedVersionItem->Text;
}

void FHTML5SDKPathCustomization::OnSelectionChanged(TSharedPtr<FHTML5Version> InItem, ESelectInfo::Type InSeletionInfo)
{
	SelectedVersionItem = InItem;
	auto* Settings = GetMutableDefault<UHTML5SDKSettings>();
	VersionProperty->SetValue(InItem->Version.VersionNumberAsString());
}

void FHTML5SDKPathCustomization::UpdateAvailableVersions()
{
	Options.Empty();
	Options.Add(InvalidVersion);

	//Take the cheap option here. SDK is now uninstalled. Don't query UBT!
	// If we clear this function we're good to install again
	PlatformInfo::UpdatePlatformSDKStatus(TEXT("HTML5"), PlatformInfo::EPlatformSDKStatus::NotInstalled);

	SelectedVersionItem = Options[0];

	auto* Settings = GetMutableDefault<UHTML5SDKSettings>();
	auto& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TargetPlatformModule = FModuleManager::LoadModulePtr<IHTML5TargetPlatformModule>("HTML5TargetPlatform");
	if (!Settings || !TargetPlatformModule)
	{
		return;
	}

	TSharedPtr<FHTML5Version> PrevSelectedVersion(new FHTML5Version);
	PrevSelectedVersion->Version.VersionNumberFromString(*Settings->EmscriptenRoot.EmscriptenVersion);

	TArray<FHTML5SDKVersionNumber> SDKVersions;
	TargetPlatformModule->GetInstalledSDKVersions(*(Settings->EmscriptenRoot.SDKPath), SDKVersions);

	if (SDKVersions.Num() == 0) 
	{
		//Try the environment variable EMSCRIPTEN - This assumes latest version.
		TCHAR SDKDirectory[32768] = { 0 };
		FPlatformMisc::GetEnvironmentVariable(TEXT("EMSCRIPTEN"), SDKDirectory, ARRAY_COUNT(SDKDirectory));
		TargetPlatformModule->GetInstalledSDKVersions(SDKDirectory, SDKVersions);
		PrevSelectedVersion->Version.MakeLatestVersionNumber();
	}

	if (SDKVersions.Num() == 0)
	{
		return;
	}
	Options.Empty();
	for (const auto& i : SDKVersions)
	{
		TSharedPtr<FHTML5Version> Ver(new FHTML5Version);
		Ver->Text = FText::FromString(i.VersionNumberAsString());
		Ver->Version = i;
		Options.Add(Ver);
	}

	TSharedPtr<FHTML5Version> Latest(new FHTML5Version);
	*Latest = *Options.Last();
	Latest->Text = LOCTEXT("LastestVesion", "Latest");
	Latest->Version.MakeLatestVersionNumber();
	Options.Add(Latest);

	int32 FoundIndex = Options.IndexOfByPredicate([&](const TSharedPtr<FHTML5Version>& RHS)
	{
		return RHS->Version == PrevSelectedVersion->Version;
	});
	if (FoundIndex != INDEX_NONE)
	{
		SelectedVersionItem = Options[FoundIndex];
	}
	else
	{
		SelectedVersionItem = Options.Last();
	}

	//Take the cheap option here. If we got here, there is a valid install selected. 
	// Don't query UBT, it'll repeat the same test via a separate process! :(
	PlatformInfo::UpdatePlatformSDKStatus(TEXT("HTML5"), PlatformInfo::EPlatformSDKStatus::Installed);
}
#endif //WITH_EDITOR
#undef LOCTEXT_NAMESPACE
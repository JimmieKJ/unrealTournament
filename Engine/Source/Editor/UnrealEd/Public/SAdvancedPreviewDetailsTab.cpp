// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "SlateBasics.h"
#include "SAdvancedPreviewDetailsTab.h"
#include "STextComboBox.h"
#include "IDetailsView.h"
#include "ModuleManager.h"
#include "PropertyEditorModule.h"

#include "AssetViewerSettings.h"
#include "ScopedTransaction.h"

#include "AdvancedPreviewScene.h"

#define LOCTEXT_NAMESPACE "SPrettyPreview"

SAdvancedPreviewDetailsTab::SAdvancedPreviewDetailsTab()
{
	DefaultSettings = UAssetViewerSettings::Get();
	if (DefaultSettings)
	{
		RefreshDelegate = DefaultSettings->OnAssetViewerSettingsChanged().AddRaw(this, &SAdvancedPreviewDetailsTab::OnAssetViewerSettingsRefresh);
			
		AddRemoveProfileDelegate = DefaultSettings->OnAssetViewerProfileAddRemoved().AddLambda([this]() { this->Refresh(); });
	}

	PerProjectSettings = GetMutableDefault<UEditorPerProjectUserSettings>();

}

SAdvancedPreviewDetailsTab::~SAdvancedPreviewDetailsTab()
{
	DefaultSettings = UAssetViewerSettings::Get();
	if (DefaultSettings)
	{
		DefaultSettings->OnAssetViewerSettingsChanged().Remove(RefreshDelegate);
		DefaultSettings->OnAssetViewerProfileAddRemoved().Remove(AddRemoveProfileDelegate);
		DefaultSettings->Save();
	}
}

void SAdvancedPreviewDetailsTab::Construct(const FArguments& InArgs)
{
	PreviewScene = InArgs._PreviewScenePtr;	
	DefaultSettings = UAssetViewerSettings::Get();
	ProfileIndex = PerProjectSettings->AssetViewerProfileIndex;
		
	CreateSettingsView();

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(2.0f, 1.0f, 2.0f, 1.0f )
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SceneProfileSettingsLabel", "Scene Profile Settings"))				
			]
		]
		+SVerticalBox::Slot()		
		.Padding(2.0f, 1.0f, 2.0f, 1.0f)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.Padding(2.0f)
			[
				SAssignNew(ProfileComboBox, STextComboBox)
				.OptionsSource(&ProfileNames)
				.OnSelectionChanged(this, &SAdvancedPreviewDetailsTab::ComboBoxSelectionChanged)				
				.ToolTipText(LOCTEXT("SceneProfileComboBoxToolTip", "Allows for switching between scene profiles."))
				.IsEnabled_Lambda([this]() -> bool
				{
					return ProfileNames.Num() > 1;
				})
			]
			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &SAdvancedPreviewDetailsTab::AddProfileButtonClick)
				.Text(LOCTEXT("AddProfileButton", "Add Profile"))
				.ToolTipText(LOCTEXT("SceneProfileAddProfile", "Adds a new profile."))
			]
			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &SAdvancedPreviewDetailsTab::RemoveProfileButtonClick)
				.Text(LOCTEXT("RemoveProfileButton", "Remove Profile"))
				.ToolTipText(LOCTEXT("SceneProfileRemoveProfile", "Removes the currently selected profile."))
				.IsEnabled_Lambda([this]()->bool
				{
					return ProfileNames.Num() > 1; 
				})
			]
		]

		+ SVerticalBox::Slot()
		.Padding(2.0f, 1.0f, 2.0f, 1.0f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				SettingsView->AsShared()
			]
		]
	];

	UpdateProfileNames();
	UpdateSettingsView();
}

void SAdvancedPreviewDetailsTab::ComboBoxSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type /*SelectInfo*/)
{
	int32 NewSelectionIndex;

	if (ProfileNames.Find(NewSelection, NewSelectionIndex))
	{
		ProfileIndex = NewSelectionIndex;
		PerProjectSettings->AssetViewerProfileIndex = ProfileIndex;
		UpdateSettingsView();	
		PreviewScene->SetProfileIndex(ProfileIndex);
	}
}

void SAdvancedPreviewDetailsTab::UpdateSettingsView()
{	
	SettingsView->SetObject(DefaultSettings, true);
}

void SAdvancedPreviewDetailsTab::UpdateProfileNames()
{
	checkf(DefaultSettings->Profiles.Num(), TEXT("There should always be at least one profile available"));
	ProfileNames.Empty();
	for (FPreviewSceneProfile& Profile : DefaultSettings->Profiles)
	{
		ProfileNames.Add(TSharedPtr<FString>(new FString(Profile.ProfileName)));
	}

	ProfileComboBox->RefreshOptions();
	ProfileComboBox->SetSelectedItem(ProfileNames[ProfileIndex]);
}

FReply SAdvancedPreviewDetailsTab::AddProfileButtonClick()
{
	// Add new profile to settings instance
	DefaultSettings->Profiles.AddDefaulted();
	FPreviewSceneProfile& NewProfile = DefaultSettings->Profiles.Last();
	NewProfile.ProfileName = FString::Printf(TEXT("Profile_%i"), DefaultSettings->Profiles.Num());
	DefaultSettings->PostEditChange();
	
	return FReply::Handled();
}

FReply SAdvancedPreviewDetailsTab::RemoveProfileButtonClick()
{
	// Remove currently selected profile 
	DefaultSettings->Profiles.RemoveAt(ProfileIndex);
	ProfileIndex = DefaultSettings->Profiles.IsValidIndex(ProfileIndex - 1 ) ? ProfileIndex - 1 : 0;
	PerProjectSettings->AssetViewerProfileIndex = ProfileIndex;
	DefaultSettings->PostEditChange();
	
	return FReply::Handled();
}

void SAdvancedPreviewDetailsTab::OnAssetViewerSettingsRefresh(const FName& InPropertyName)
{
	const bool bUpdateEnvironment = (InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, EnvironmentCubeMap)) || (InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, LightingRigRotation) || (InPropertyName == GET_MEMBER_NAME_CHECKED(UAssetViewerSettings, Profiles)));
	const bool bUpdateSkyLight = bUpdateEnvironment || (InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, SkyLightIntensity) || (InPropertyName == GET_MEMBER_NAME_CHECKED(UAssetViewerSettings, Profiles)));
	const bool bUpdateDirectionalLight = (InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, DirectionalLightIntensity)) || (InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, DirectionalLightColor));
	const bool bUpdatePostProcessing = (InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, PostProcessingSettings)) || (InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, bPostProcessingEnabled));

	if (InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, ProfileName))
	{
		UpdateProfileNames();
	}

	PreviewScene->UpdateScene(DefaultSettings->Profiles[ProfileIndex], bUpdateSkyLight, bUpdateEnvironment, bUpdatePostProcessing, bUpdateDirectionalLight);
}

void SAdvancedPreviewDetailsTab::CreateSettingsView()
{	
	// Create a property view
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs(
		/*bUpdateFromSelection=*/ false,
		/*bLockable=*/ false,
		/*bAllowSearch=*/ false,
		FDetailsViewArgs::HideNameArea,
		/*bHideSelectionTip=*/ true,
		/*InNotifyHook=*/ nullptr,
		/*InSearchInitialKeyFocus=*/ false,
		/*InViewIdentifier=*/ NAME_None);
	DetailsViewArgs.DefaultsOnlyVisibility = FDetailsViewArgs::EEditDefaultsOnlyNodeVisibility::Automatic;
	DetailsViewArgs.bShowOptions = false;

	SettingsView = EditModule.CreateDetailView(DetailsViewArgs);
	UpdateSettingsView();
}

void SAdvancedPreviewDetailsTab::Refresh()
{	
	ProfileIndex = PerProjectSettings->AssetViewerProfileIndex;	
	UpdateProfileNames();
	PreviewScene->SetProfileIndex(ProfileIndex);
	UpdateSettingsView();
}

#undef LOCTEXT_NAMESPACE
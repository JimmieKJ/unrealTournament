// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SAdvancedPreviewDetailsTab.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/STextComboBox.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"

#include "AssetViewerSettings.h"

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

void SAdvancedPreviewDetailsTab::Construct(const FArguments& InArgs, const TSharedRef<FAdvancedPreviewScene>& InPreviewScene)
{
	PreviewScenePtr = InPreviewScene;
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
		PreviewScenePtr.Pin()->SetProfileIndex(ProfileIndex);
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
	const FScopedTransaction Transaction(LOCTEXT("AddSceneProfile", "Adding Preview Scene Profile"));
	DefaultSettings->Modify();

	// Add new profile to settings instance
	DefaultSettings->Profiles.AddDefaulted();
	FPreviewSceneProfile& NewProfile = DefaultSettings->Profiles.Last();
	
	// Try to create a valid profile name when one is added
	bool bFoundValidName = false;
	int ProfileAppendNum = DefaultSettings->Profiles.Num();
	FString NewProfileName;
	while (!bFoundValidName)
	{
		NewProfileName = FString::Printf(TEXT("Profile_%i"), ProfileAppendNum);

		bool bValidName = true;
		for (const FPreviewSceneProfile& Profile : DefaultSettings->Profiles)
		{
			if (Profile.ProfileName == NewProfileName)
			{
				bValidName = false;				
				break;
			}
		}

		if (!bValidName)
		{
			++ProfileAppendNum;
		}

		bFoundValidName = bValidName;
	}

	NewProfile.ProfileName = NewProfileName;
	DefaultSettings->PostEditChange();

	// Change selection to new profile so the user directly sees the profile that was added
	Refresh();
	ProfileComboBox->SetSelectedItem(ProfileNames.Last());
	
	return FReply::Handled();
}

FReply SAdvancedPreviewDetailsTab::RemoveProfileButtonClick()
{
	const FScopedTransaction Transaction(LOCTEXT("RemoveSceneProfile", "Remove Preview Scene Profile"));
	DefaultSettings->Modify();

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

	PreviewScenePtr.Pin()->UpdateScene(DefaultSettings->Profiles[ProfileIndex], bUpdateSkyLight, bUpdateEnvironment, bUpdatePostProcessing, bUpdateDirectionalLight);
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
	PerProjectSettings->AssetViewerProfileIndex = DefaultSettings->Profiles.IsValidIndex(PerProjectSettings->AssetViewerProfileIndex) ? PerProjectSettings->AssetViewerProfileIndex : 0;
	ProfileIndex = PerProjectSettings->AssetViewerProfileIndex;
	UpdateProfileNames();
	PreviewScenePtr.Pin()->SetProfileIndex(ProfileIndex);
	UpdateSettingsView();
}

#undef LOCTEXT_NAMESPACE

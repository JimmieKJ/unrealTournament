// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PersonaPrivatePCH.h"
#include "SBlendProfilePicker.h"
#include "Animation/BlendProfile.h"
#include "STextEntryPopup.h"
#include "ScopedTransaction.h"


#define LOCTEXT_NAMESPACE "BlendProfilePicker"

//////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SBlendProfilePicker::Construct(const FArguments& InArgs)
{
	bShowNewOption = InArgs._AllowNew;
	bShowClearOption = InArgs._AllowClear;
	TargetSkeleton = InArgs._TargetSkeleton;

	if(TargetSkeleton)
	{
		if(InArgs._InitialProfile == nullptr || TargetSkeleton->BlendProfiles.Contains(InArgs._InitialProfile))
		{
			SelectedProfile = InArgs._InitialProfile;
		}
		else
		{
			SelectedProfile = nullptr;
		}
	}
	else
	{
		SelectedProfile = nullptr;
	}

	BlendProfileSelectedDelegate = InArgs._OnBlendProfileSelected;

	ChildSlot
	[
		SNew(SComboButton)
		.ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
		.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
		.ContentPadding(2.0f)
		.OnGetMenuContent(this, &SBlendProfilePicker::GetMenuContent)
		.ButtonContent()
		[
			SNew(STextBlock)
			.TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
			.Font(FEditorStyle::GetFontStyle("PropertyWindow.NormalFont"))
			.Text(this, &SBlendProfilePicker::GetSelectedProfileName)
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FText SBlendProfilePicker::GetSelectedProfileName() const
{
	if(SelectedProfile)
	{
		return FText::Format(FText(LOCTEXT("SelectedNameEntry", "{0}")), FText::FromString(SelectedProfile->GetName()));
	}
	return FText(LOCTEXT("NoSelectionEntry", "None"));
}

TSharedRef<SWidget> SBlendProfilePicker::GetMenuContent()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	const bool bHasSettingsSection = bShowNewOption || bShowClearOption;

	if(bHasSettingsSection)
	{
		MenuBuilder.BeginSection(NAME_None, LOCTEXT("MenuSettings", "Settings"));
		{
			if(bShowNewOption)
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("CreateNew", "Create New Blend Profile"),
					LOCTEXT("CreateNew_ToolTip", "Creates a new blend profile inside the skeleton."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateSP(this, &SBlendProfilePicker::OnCreateNewProfile)));
			}

			if(bShowClearOption)
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("Clear", "Clear"),
					LOCTEXT("Clear_ToolTip", "Clear the selected blend profile."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateSP(this, &SBlendProfilePicker::OnClearSelection)));
			}
		}
		MenuBuilder.EndSection();
	}

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("Profiles", "Available Blend Profiles"));
	{
		for(UBlendProfile* Profile : TargetSkeleton->BlendProfiles)
		{
			MenuBuilder.AddMenuEntry(
				FText::Format(LOCTEXT("ProfileEntry", "{0}"), FText::FromString(Profile->GetName())),
				LOCTEXT("ProfileEntry_ToolTip", "Select this profile for editing."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SBlendProfilePicker::OnProfileSelected, Profile)));
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SBlendProfilePicker::OnProfileSelected(UBlendProfile* Profile)
{
	SelectedProfile = Profile;

	BlendProfileSelectedDelegate.ExecuteIfBound(SelectedProfile);
}

void SBlendProfilePicker::OnClearSelection()
{
	SelectedProfile = nullptr;

	BlendProfileSelectedDelegate.ExecuteIfBound(SelectedProfile);
}

void SBlendProfilePicker::OnCreateNewProfile()
{
	TSharedRef<STextEntryPopup> TextEntry = SNew(STextEntryPopup)
		.Label(LOCTEXT("NewProfileName", "Profile Name"))
		.OnTextCommitted(this, &SBlendProfilePicker::OnCreateNewProfileComitted);

	FSlateApplication::Get().PushMenu(
		AsShared(),
		FWidgetPath(),
		TextEntry,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup));
}

void SBlendProfilePicker::OnCreateNewProfileComitted(const FText& NewName, ETextCommit::Type CommitType)
{
	FSlateApplication::Get().DismissAllMenus();

	if(CommitType == ETextCommit::OnEnter && TargetSkeleton)
	{
		FScopedTransaction Transaction(LOCTEXT("Trans_NewProfile", "Create new blend profile."));

		FName NameToUse = FName(*NewName.ToString());

		// Only create if we don't have a matching profile
		if(UBlendProfile* FoundProfile = TargetSkeleton->GetBlendProfile(NameToUse))
		{
			OnProfileSelected(FoundProfile);
		}
		else if(UBlendProfile* NewProfile = TargetSkeleton->CreateNewBlendProfile(NameToUse))
		{
			OnProfileSelected(NewProfile);
		}
	}
}

void SBlendProfilePicker::SetSelectedProfile(UBlendProfile* InProfile, bool bBroadcast /*= true*/)
{
	if(TargetSkeleton->BlendProfiles.Contains(InProfile))
	{
		SelectedProfile = InProfile;
		if(bBroadcast)
		{
			BlendProfileSelectedDelegate.ExecuteIfBound(InProfile);
		}
	}
	else if(!InProfile)
	{
		OnClearSelection();
	}
}

#undef LOCTEXT_NAMESPACE

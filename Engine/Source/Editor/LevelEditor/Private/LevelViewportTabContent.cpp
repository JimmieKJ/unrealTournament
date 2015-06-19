// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LevelEditor.h"
#include "LevelViewportTabContent.h"
#include "LevelViewportLayout2x2.h"
#include "LevelViewportLayoutOnePane.h"
#include "LevelViewportLayoutTwoPanes.h"
#include "LevelViewportLayoutThreePanes.h"
#include "LevelViewportLayoutFourPanes.h"
#include "SLevelViewport.h"
#include "SDockTab.h"


// FLevelViewportTabContent ///////////////////////////

TSharedPtr< class FLevelViewportLayout > FLevelViewportTabContent::ConstructViewportLayoutByTypeName(const FName& TypeName, bool bSwitchingLayouts)
{
	TSharedPtr< class FLevelViewportLayout > ViewportLayout;

	// The items in these ifs should match the names in namespace LevelViewportConfigurationNames
	if (TypeName == LevelViewportConfigurationNames::FourPanes2x2) ViewportLayout = MakeShareable(new FLevelViewportLayout2x2);
	else if (TypeName == LevelViewportConfigurationNames::TwoPanesVert) ViewportLayout = MakeShareable(new FLevelViewportLayoutTwoPanesVert);
	else if (TypeName == LevelViewportConfigurationNames::TwoPanesHoriz) ViewportLayout = MakeShareable(new FLevelViewportLayoutTwoPanesHoriz);
	else if (TypeName == LevelViewportConfigurationNames::ThreePanesLeft) ViewportLayout = MakeShareable(new FLevelViewportLayoutThreePanesLeft);
	else if (TypeName == LevelViewportConfigurationNames::ThreePanesRight) ViewportLayout = MakeShareable(new FLevelViewportLayoutThreePanesRight);
	else if (TypeName == LevelViewportConfigurationNames::ThreePanesTop) ViewportLayout = MakeShareable(new FLevelViewportLayoutThreePanesTop);
	else if (TypeName == LevelViewportConfigurationNames::ThreePanesBottom) ViewportLayout = MakeShareable(new FLevelViewportLayoutThreePanesBottom);
	else if (TypeName == LevelViewportConfigurationNames::FourPanesLeft) ViewportLayout = MakeShareable(new FLevelViewportLayoutFourPanesLeft);
	else if (TypeName == LevelViewportConfigurationNames::FourPanesRight) ViewportLayout = MakeShareable(new FLevelViewportLayoutFourPanesRight);
	else if (TypeName == LevelViewportConfigurationNames::FourPanesBottom) ViewportLayout = MakeShareable(new FLevelViewportLayoutFourPanesBottom);
	else if (TypeName == LevelViewportConfigurationNames::FourPanesTop) ViewportLayout = MakeShareable(new FLevelViewportLayoutFourPanesTop);
	else if (TypeName == LevelViewportConfigurationNames::OnePane) ViewportLayout = MakeShareable(new FLevelViewportLayoutOnePane);

	check (ViewportLayout.IsValid());
	ViewportLayout->SetIsReplacement(bSwitchingLayouts);
	return ViewportLayout;
}

void FLevelViewportTabContent::Initialize(TSharedPtr<class SLevelEditor> InParentLevelEditor, TSharedPtr<SDockTab> InParentTab, const FString& InLayoutString)
{
	ParentTab = InParentTab;
	ParentLevelEditor = InParentLevelEditor;
	LayoutString = InLayoutString;

	InParentTab->SetOnPersistVisualState( SDockTab::FOnPersistVisualState::CreateSP(this, &FLevelViewportTabContent::SaveLayoutString, LayoutString) );

	const FString& IniSection = FLayoutSaveRestore::GetAdditionalLayoutConfigIni();

	FString LayoutTypeString;
	if(LayoutString.IsEmpty() ||
		!GConfig->GetString(*IniSection, *(InLayoutString + TEXT(".LayoutType")), LayoutTypeString, GEditorPerProjectIni))
	{
		LayoutTypeString = LevelViewportConfigurationNames::FourPanes2x2.ToString();
	}
	FName LayoutType(*LayoutTypeString);
	SetViewportConfiguration(LayoutType);
}

bool FLevelViewportTabContent::IsVisible() const
{
	if (ActiveLevelViewportLayout.IsValid())
	{
		return ActiveLevelViewportLayout->IsVisible();
	}
	return false;
}

const TArray< TSharedPtr< SLevelViewport > >* FLevelViewportTabContent::GetViewports() const
{
	if (ActiveLevelViewportLayout.IsValid())
	{
		return &ActiveLevelViewportLayout->GetViewports();
	}
	return NULL;
}

void FLevelViewportTabContent::SetViewportConfiguration(const FName& ConfigurationName)
{
	bool bSwitchingLayouts = ActiveLevelViewportLayout.IsValid();

	if (bSwitchingLayouts)
	{
		ActiveLevelViewportLayout->SaveLayoutString(LayoutString);
		ActiveLevelViewportLayout.Reset();
	}

	ActiveLevelViewportLayout = ConstructViewportLayoutByTypeName(ConfigurationName, bSwitchingLayouts);
	check (ActiveLevelViewportLayout.IsValid());

	UpdateViewportTabWidget();
}

bool FLevelViewportTabContent::IsViewportConfigurationSet(const FName& ConfigurationName) const
{
	if (ActiveLevelViewportLayout.IsValid())
	{
		return ActiveLevelViewportLayout->GetLayoutTypeName() == ConfigurationName;
	}
	return false;
}

bool FLevelViewportTabContent::BelongsToTab(TSharedRef<class SDockTab> InParentTab) const
{
	TSharedPtr<SDockTab> ParentTabPinned = ParentTab.Pin();
	return ParentTabPinned == InParentTab;
}

void FLevelViewportTabContent::UpdateViewportTabWidget()
{
	TSharedPtr<SDockTab> ParentTabPinned = ParentTab.Pin();
	if (ParentTabPinned.IsValid() && ActiveLevelViewportLayout.IsValid())
	{
		TSharedRef<SWidget> LayoutWidget = ActiveLevelViewportLayout->BuildViewportLayout(ParentTabPinned, SharedThis(this), LayoutString, ParentLevelEditor);

		ParentTabPinned->SetContent(LayoutWidget);
	}
}

void FLevelViewportTabContent::SaveLayoutString(const FString InLayoutString) const
{
	if (ActiveLevelViewportLayout.IsValid() && !InLayoutString.IsEmpty())
	{
		FString LayoutTypeString = ActiveLevelViewportLayout->GetLayoutTypeName().ToString();

		const FString& IniSection = FLayoutSaveRestore::GetAdditionalLayoutConfigIni();
		GConfig->SetString(*IniSection, *(InLayoutString + TEXT(".LayoutType")), *LayoutTypeString, GEditorPerProjectIni);

		ActiveLevelViewportLayout->SaveLayoutString(InLayoutString);
	}
}

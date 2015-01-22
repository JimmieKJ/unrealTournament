// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ProjectLauncherPrivatePCH.h"
#include "SExpandableArea.h"


#define LOCTEXT_NAMESPACE "SProjectLauncherPackagingSettings"


/* SProjectLauncherPackagingSettings structors
 *****************************************************************************/

SProjectLauncherPackagingSettings::~SProjectLauncherPackagingSettings( )
{
	if (Model.IsValid())
	{
		Model->OnProfileSelected().RemoveAll(this);
	}
}


/* SProjectLauncherPackagingSettings interface
 *****************************************************************************/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SProjectLauncherPackagingSettings::Construct( const FArguments& InArgs, const FProjectLauncherModelRef& InModel )
{
	Model = InModel;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.FillHeight(1.0)
			[
				SNew(SBorder)
					.Padding(8.0)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(STextBlock)
									.Text(LOCTEXT("RepositoryPathLabel", "Repository Path:"))
							]

						+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0, 4.0, 0.0, 0.0)
							[
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
									.FillWidth(1.0)
									.Padding(0.0, 0.0, 0.0, 3.0)
									[
										// repository path text box
										SAssignNew(RepositoryPathTextBox, SEditableTextBox)
									]

								+ SHorizontalBox::Slot()
									.AutoWidth()
									.HAlign(HAlign_Right)
									.Padding(4.0, 0.0, 0.0, 0.0)
									[
										// browse button
										SNew(SButton)
											.ContentPadding(FMargin(6.0, 2.0))
											.IsEnabled(false)
											.Text(LOCTEXT("BrowseButtonText", "Browse..."))
											.ToolTipText(LOCTEXT("BrowseButtonToolTip", "Browse for the repository"))
											//.OnClicked(this, &SProjectLauncherPackagePage::HandleBrowseButtonClicked)
									]
							]

						+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0, 8.0, 0.0, 0.0)
							[
								SNew(STextBlock)
									.Text(LOCTEXT("DescriptionTextBoxLabel", "Description:"))
							]

						+ SVerticalBox::Slot()
							.FillHeight(1.0)
							.Padding(0.0, 4.0, 0.0, 0.0)
							[
								SNew(SEditableTextBox)
							]
					]
			]

		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0, 8.0, 0.0, 0.0)
			[
				SNew(SExpandableArea)
					.AreaTitle(LOCTEXT("IncrementalPackagingAreaTitle", "Incremental Packaging"))
					.InitiallyCollapsed(true)
					.Padding(8.0)
					.BodyContent()
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
							.AutoHeight()
							[
								// incremental packaging check box
								SNew(SCheckBox)
									//.IsChecked(this, &SProjectLauncherLaunchPage::HandleIncrementalCheckBoxIsChecked)
									//.OnCheckStateChanged(this, &SProjectLauncherLaunchPage::HandleIncrementalCheckBoxCheckStateChanged)
									.Content()
									[
										SNew(STextBlock)
											.Text(LOCTEXT("IncrementalCheckBoxText", "This is an incremental build"))
									]
							]

						+ SVerticalBox::Slot()
							.FillHeight(1.0)
							.Padding(18.0, 12.0, 0.0, 0.0)
							[
								SNew(SVerticalBox)
							]
					]
			]
	];

	Model->OnProfileSelected().AddSP(this, &SProjectLauncherPackagingSettings::HandleProfileManagerProfileSelected);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


/* SProjectLauncherPackagingSettings callbacks
 *****************************************************************************/

void SProjectLauncherPackagingSettings::HandleProfileManagerProfileSelected( const ILauncherProfilePtr& SelectedProfile, const ILauncherProfilePtr& PreviousProfile )
{

}


#undef LOCTEXT_NAMESPACE

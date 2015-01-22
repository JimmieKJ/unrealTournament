// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ProjectLauncherPrivatePCH.h"


#define LOCTEXT_NAMESPACE "SProjectLauncherProjectPage"


/* SProjectLauncherProjectPage interface
 *****************************************************************************/

void SProjectLauncherProjectPage::Construct( const FArguments& InArgs, const FProjectLauncherModelRef& InModel, bool InShowConfig )
{
	Model = InModel;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("WhichProjectToUseText", "Which project would you like to use?"))
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0, 8.0, 0.0, 0.0)
		[
			// project loading area
			SNew(SProjectLauncherProjectPicker, InModel)
			.LaunchProfile(InArgs._LaunchProfile)
		]
	];
}


#undef LOCTEXT_NAMESPACE

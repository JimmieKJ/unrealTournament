// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ProjectLauncherPrivatePCH.h"


#define LOCTEXT_NAMESPACE "SProjectLauncherLaunchCustomRoles"


/* SProjectLauncherLaunchCustomRoles structors
 *****************************************************************************/

SProjectLauncherLaunchCustomRoles::~SProjectLauncherLaunchCustomRoles( )
{
	if (Model.IsValid())
	{
		Model->OnProfileSelected().RemoveAll(this);
	}
}


/* SProjectLauncherLaunchCustomRoles interface
 *****************************************************************************/

void SProjectLauncherLaunchCustomRoles::Construct( const FArguments& InArgs, const FProjectLauncherModelRef& InModel )
{
	Model = InModel;

	ChildSlot
	[
		SNullWidget::NullWidget
	];
}


#undef LOCTEXT_NAMESPACE

// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SPersonaDetails.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

void SPersonaDetails::Construct(const FArguments& InArgs)
{
	FDetailsViewArgs DetailsViewArgs(false, false, true, FDetailsViewArgs::HideNameArea, true);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	ChildSlot
	[
		DetailsView.ToSharedRef()
	];
}

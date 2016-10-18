// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieSceneToolsProjectSettingsCustomization.h"
#include "MovieSceneToolsProjectSettings.h"

#include "PropertyEditorModule.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"

//#include "Editor/PropertyEditor/Public/IDetailsView.h"
//#include "IDetailCustomization.h"
//#include "DetailLayoutBuilder.h"
//#include "PropertyHandle.h"


TSharedRef<IDetailCustomization> FMovieSceneToolsProjectSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FMovieSceneToolsProjectSettingsCustomization);
}

void FMovieSceneToolsProjectSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TSharedRef<IPropertyHandle> TakeSeparatorProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMovieSceneToolsProjectSettings, TakeSeparator));
	TakeSeparatorProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateRaw(this, &FMovieSceneToolsProjectSettingsCustomization::OnTakeSeparatorUpdated));
}

void FMovieSceneToolsProjectSettingsCustomization::OnTakeSeparatorUpdated()
{
	UMovieSceneToolsProjectSettings* ProjectSettings = GetMutableDefault<UMovieSceneToolsProjectSettings>();

	FString TakeSeparator = ProjectSettings->TakeSeparator;

	// Make sure the take separator is a valid single character
	FText OutErrorMessage;
	if (!FName(*TakeSeparator).IsValidXName(INVALID_OBJECTNAME_CHARACTERS INVALID_LONGPACKAGE_CHARACTERS, &OutErrorMessage))
	{
		ProjectSettings->TakeSeparator = TEXT("_");
	}
	else if (ProjectSettings->TakeSeparator.Len() > 1)
	{
		ProjectSettings->TakeSeparator = ProjectSettings->TakeSeparator.LeftChop(ProjectSettings->TakeSeparator.Len()-1);
	}
}

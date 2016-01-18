// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Engine/RendererSettings.h"

URendererSettings::URendererSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SectionName = TEXT("Rendering");
	TranslucentSortAxis = FVector(0.0f, -1.0f, 0.0f);
}

void URendererSettings::PostInitProperties()
{
	Super::PostInitProperties();

	if ( IsTemplate() )
	{
		// If we have UI scale curve data in the renderer settings config, move it to the user interface settings.
		if ( FRichCurve* OldCurve = UIScaleCurve_DEPRECATED.GetRichCurve() )
		{
			if ( OldCurve->GetNumKeys() != 0 )
			{
				UUserInterfaceSettings* UISettings = GetMutableDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass());
				UISettings->UIScaleRule = UIScaleRule_DEPRECATED;
				UISettings->UIScaleCurve = UIScaleCurve_DEPRECATED;

				// Remove all old keys so that we don't attempt to load from renderer settings again.
				OldCurve->Reset();
			}
		}
	}

#if WITH_EDITOR
	if (IsTemplate())
	{
		ImportConsoleVariableValues();
	}
#endif // #if WITH_EDITOR
}

#if WITH_EDITOR
void URendererSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		ExportValuesToConsoleVariables(PropertyChangedEvent.Property);

		// If the renderer settings are updated and saved, we need to update the user interface settings too, to prevent data loss
		// from old curve data for UI settings.
		if ( IsTemplate() && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive )
		{
			UUserInterfaceSettings* UISettings = GetMutableDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass());
			UISettings->UpdateDefaultConfigFile();
		}
	}
}
#endif // #if WITH_EDITOR

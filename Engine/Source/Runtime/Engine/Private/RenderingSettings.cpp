// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Engine/RendererSettings.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"

/** The editor object. */
extern UNREALED_API class UEditorEngine* GEditor;
#endif // #if WITH_EDITOR

URendererSettings::URendererSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SectionName = TEXT("Rendering");
	TranslucentSortAxis = FVector(0.0f, -1.0f, 0.0f);
	bSupportStationarySkylight = true;
	bSupportPointLightWholeSceneShadows = true;
	bSupportAtmosphericFog = true;
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

	SanatizeReflectionCaptureResolution();

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

	SanatizeReflectionCaptureResolution();

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

		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(URendererSettings, ReflectionCaptureResolution) && 
			PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
		{
			GEditor->UpdateReflectionCaptures();
		}
	}
}
#endif // #if WITH_EDITOR

void URendererSettings::SanatizeReflectionCaptureResolution()
{
	static const int32 MaxReflectionCaptureResolution = 1024;
	static const int32 MinReflectionCaptureResolution = 64;
	ReflectionCaptureResolution = FMath::Clamp(int32(FMath::RoundUpToPowerOfTwo(ReflectionCaptureResolution)), MinReflectionCaptureResolution, MaxReflectionCaptureResolution);
}

URendererOverrideSettings::URendererOverrideSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SectionName = TEXT("Rendering Overrides");	
}

void URendererOverrideSettings::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	if (IsTemplate())
	{
		ImportConsoleVariableValues();
	}
#endif // #if WITH_EDITOR
}

#if WITH_EDITOR
void URendererOverrideSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);	

	if (PropertyChangedEvent.Property)
	{
		ExportValuesToConsoleVariables(PropertyChangedEvent.Property);		
	}
}
#endif // #if WITH_EDITOR

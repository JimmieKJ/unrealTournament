// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Engine/RendererSettings.h"

URendererSettings::URendererSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SectionName = TEXT("Rendering");
	TranslucentSortAxis = FVector(0.0f, -1.0f, 0.0f);
}

static FName RenderingConsoleVariableFName(TEXT("ConsoleVariable"));

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
		for (UProperty* Property = GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext)
		{
			if (!Property->HasAnyPropertyFlags(CPF_Config))
			{
				continue;
			}

			FString CVarName = Property->GetMetaData(RenderingConsoleVariableFName);
			if (!CVarName.IsEmpty())
			{
				IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
				if (CVar)
				{
					if (Property->ImportText(*CVar->GetString(), Property->ContainerPtrToValuePtr<uint8>(this, 0), PPF_ConsoleVariable, this) == NULL)
					{
						UE_LOG(LogTemp, Error, TEXT("URendererSettings import failed for %s on console variable %s (=%s)"), *Property->GetName(), *CVarName, *CVar->GetString());
					}
				}
				else
				{
					UE_LOG(LogTemp, Fatal, TEXT("URendererSettings failed to find console variable %s for %s"), *CVarName, *Property->GetName());
				}
			}
		}
	}
#endif // #if WITH_EDITOR
}

#if WITH_EDITOR
void URendererSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		FString CVarName = PropertyChangedEvent.Property->GetMetaData(RenderingConsoleVariableFName);
		if (!CVarName.IsEmpty())
		{
			IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
			if (CVar && (CVar->GetFlags() & ECVF_ReadOnly) == 0)
			{
				UByteProperty* ByteProperty = Cast<UByteProperty>(PropertyChangedEvent.Property);
				if (ByteProperty != NULL && ByteProperty->Enum != NULL)
				{
					CVar->Set(ByteProperty->GetPropertyValue_InContainer(this), ECVF_SetByProjectSetting);
				}
				else if (UBoolProperty* BoolProperty = Cast<UBoolProperty>(PropertyChangedEvent.Property))
				{
					CVar->Set((int32)BoolProperty->GetPropertyValue_InContainer(this), ECVF_SetByProjectSetting);
				}
				else if (UIntProperty* IntProperty = Cast<UIntProperty>(PropertyChangedEvent.Property))
				{
					CVar->Set(IntProperty->GetPropertyValue_InContainer(this), ECVF_SetByProjectSetting);
				}
				else if (UFloatProperty* FloatProperty = Cast<UFloatProperty>(PropertyChangedEvent.Property))
				{
					CVar->Set(FloatProperty->GetPropertyValue_InContainer(this), ECVF_SetByProjectSetting);
				}
			}
			else
			{
				UE_LOG(LogInit, Warning, TEXT("CVar named '%s' marked up in URenderingSettings was not found or is set to read-only"), *CVarName);
			}
		}

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

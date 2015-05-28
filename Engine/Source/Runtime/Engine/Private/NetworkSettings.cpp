// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Engine/NetworkSettings.h"

UNetworkSettings::UNetworkSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SectionName = TEXT("Network");
}

static FName NetworkConsoleVariableFName(TEXT("ConsoleVariable"));

void UNetworkSettings::PostInitProperties()
{
	Super::PostInitProperties();
#if WITH_EDITOR
	if (IsTemplate())
	{
		for (UProperty* Property = GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext)
		{
			if (!Property->HasAnyPropertyFlags(CPF_Config))
			{
				continue;
			}

			FString CVarName = Property->GetMetaData(NetworkConsoleVariableFName);
			if (!CVarName.IsEmpty())
			{
				IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
				if (CVar)
				{
					if (Property->ImportText(*CVar->GetString(), Property->ContainerPtrToValuePtr<uint8>(this, 0), PPF_ConsoleVariable, this) == NULL)
					{
						UE_LOG(LogTemp, Error, TEXT("UNetworkSettings import failed for %s on console variable %s (=%s)"), *Property->GetName(), *CVarName, *CVar->GetString());
					}
				}
				else
				{
					UE_LOG(LogTemp, Fatal, TEXT("UNetworkSettings failed to find console variable %s for %s"), *CVarName, *Property->GetName());
				}
			}
		}
	}
#endif // #if WITH_EDITOR
}

#if WITH_EDITOR
void UNetworkSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property)
	{
		FString CVarName = PropertyChangedEvent.Property->GetMetaData(NetworkConsoleVariableFName);
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
		}
	}
}
#endif // #if WITH_EDITOR


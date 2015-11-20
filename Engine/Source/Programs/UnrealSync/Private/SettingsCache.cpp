// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealSync.h"
#include "SettingsCache.h"

DECLARE_LOG_CATEGORY_CLASS(LogSettings, Log, All);

const FString& FSettingsCache::GetSettingsFileName()
{
	const static FString HardCodedSettingsFileName = "UnrealSync.settings";

	return HardCodedSettingsFileName;
}

FSettingsCache::FSettingsCache()
{
	// Read the file to a string.
	FString FileContents;
	if (!FFileHelper::LoadFileToString(FileContents, *GetSettingsFileName()))
	{
		UE_LOG(LogSettings, Log, TEXT("Couldn't find settings file \"%s\"."), *GetSettingsFileName());
		return;
	}

	// Deserialize a JSON object from the string
	TSharedPtr<FJsonObject> Object;
	TSharedRef<TJsonReader<> > Reader = TJsonReaderFactory<>::Create(FileContents);
	if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
	{
		UE_LOG(LogSettings, Error, TEXT("Settings file JSON parsing failed: \"%s\"."), *Reader->GetErrorMessage());
		return;
	}

	TMap<FString, TSharedRef<FJsonValue> > TempSettings;
	for (const auto& KeyValuePair : Object->Values)
	{
		if (!KeyValuePair.Value.IsValid())
		{
			UE_LOG(LogSettings, Error, TEXT("Bad format of settings file: \"%s\"."), *Reader->GetErrorMessage());
			return;
		}

		TempSettings.Add(KeyValuePair.Key, KeyValuePair.Value.ToSharedRef());
	}

	Settings = MoveTemp(TempSettings);
}

void FSettingsCache::Save()
{
	TSharedRef<FJsonObject> Object(new FJsonObject());

	for (const auto& Setting : Settings)
	{
		Object->SetField(Setting.Key, Setting.Value);
	}

	FString Buffer;
	TSharedRef<TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&Buffer);
	if (!FJsonSerializer::Serialize(Object, Writer))
	{
		UE_LOG(LogSettings, Error, TEXT("Failed serializing settings using JSON."));
		return;
	}

	auto bNeedsWrite = true;

	if (FPaths::FileExists(GetSettingsFileName()))
	{
		// Read the file to a string.
		FString FileContents;
		bNeedsWrite = !(FFileHelper::LoadFileToString(FileContents, *GetSettingsFileName()) && FileContents == Buffer);
	}

	if (bNeedsWrite && !FFileHelper::SaveStringToFile(Buffer, *GetSettingsFileName()))
	{
		UE_LOG(LogSettings, Error, TEXT("Failed writing settings to file: \"%s\"."), *GetSettingsFileName());
		return;
	}
}

void FSettingsCache::SetSetting(const FString& Name, TSharedRef<FJsonValue> Value)
{
	auto* ParamPtr = Settings.Find(Name);
	if (ParamPtr != nullptr)
	{
		*ParamPtr = Value;
	}
	else
	{
		Settings.Add(Name, Value);
	}
}

bool FSettingsCache::GetSetting(TSharedPtr<FJsonValue>& Value, const FString& Name)
{
	auto* ParamPtr = Settings.Find(Name);
	if (ParamPtr == nullptr)
	{
		return false;
	}

	Value = *ParamPtr;
	return true;
}

FSettingsCache& FSettingsCache::Get()
{
	static FSettingsCache Cache;
	return Cache;
}

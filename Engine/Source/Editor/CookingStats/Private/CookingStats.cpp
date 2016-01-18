// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "CookingStatsPCH.h"
#include "ModuleManager.h"

DEFINE_LOG_CATEGORY(LogCookingStats);

FCookingStats::FCookingStats()
{
	RunGuid = FName(*FString::Printf(TEXT("RunID%s"), *FGuid::NewGuid().ToString()));
}

FCookingStats::~FCookingStats()
{
}

void FCookingStats::AddRunTag(const FName& Tag, const FString& Value)
{
	AddTagValue(RunGuid, Tag, Value);
}

void FCookingStats::AddTag(const FName& Key, const FName& Tag)
{
	AddTagValue(Key, Tag, FString());
}

void FCookingStats::AddTagValue(const FName& Key, const FName& Tag, const FString& TagValue)
{
	FScopeLock ScopeLock(&SyncObject);
	auto Value = KeyTags.Find(Key);

	if (Value == nullptr)
	{
		Value = &KeyTags.Add(Key);
	}

	Value->Add(Tag, TagValue);

}


bool FCookingStats::GetTagValue(const FName& Key, const FName& TagName, FString& OutValue) const
{
	FScopeLock ScopeLock(&SyncObject);
	auto Tags = KeyTags.Find(Key);

	if (Tags == nullptr)
	{
		return false;
	}

	auto* Value = Tags->Find(TagName);

	if (Value == nullptr)
	{
		return false;
	}
	OutValue = *Value;
	return true;
}

bool FCookingStats::SaveStatsAsCSV(const FString& Filename) const
{
	FString Output;
	FScopeLock ScopeLock(&SyncObject);

	for (const auto& Stat : KeyTags)
	{
		Output += Stat.Key.ToString();

		for (const auto& Tag : Stat.Value)
		{
			Output += TEXT(", ");
			Output += Tag.Key.ToString();
			if (Tag.Value.IsEmpty() == false)
			{
				Output += TEXT("=");
				Output += Tag.Value;
			}
			
		}

		for (const auto& Tag : GlobalTags)
		{
			Output += TEXT(", ");
			Output += Tag.Key.ToString();
			if (Tag.Value.IsEmpty() == false)
			{
				Output += TEXT("=");
				Output += Tag.Value;
			}
		}

		Output += FString::Printf(TEXT(", RunGuid=%s\r\n"), *RunGuid.ToString());
	}

	FFileHelper::SaveStringToFile(Output, *Filename);
	return true;
}


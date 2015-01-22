// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "ScriptPluginPrivatePCH.h"
#include "ScriptBlueprint.h"
#if WITH_EDITOR
#include "BlueprintEditorUtils.h"
#endif

/////////////////////////////////////////////////////
// UScriptBlueprint

UScriptBlueprint::UScriptBlueprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
UClass* UScriptBlueprint::GetBlueprintClass() const
{
	return UScriptBlueprintGeneratedClass::StaticClass();
}

bool UScriptBlueprint::ValidateGeneratedClass(const UClass* InClass)
{
	bool Result = Super::ValidateGeneratedClass(InClass);

	const UScriptBlueprintGeneratedClass* GeneratedClass = Cast<const UScriptBlueprintGeneratedClass>(InClass);
	if ( !ensure(GeneratedClass) )
	{
		return false;
	}
	const UScriptBlueprint* Blueprint = Cast<UScriptBlueprint>(GetBlueprintFromClass(GeneratedClass));
	if ( !ensure(Blueprint) )
	{
		return false;
	}

	return Result;
}

void UScriptBlueprint::UpdateScriptStatus()
{
	FDateTime OldTimeStamp;
	bool bCodeDirty = !FDateTime::Parse(SourceFileTimestamp, OldTimeStamp);
	
	if (!bCodeDirty)
	{
		FDateTime TimeStamp = IFileManager::Get().GetTimeStamp(*SourceFilePath);
		bCodeDirty = TimeStamp > OldTimeStamp;
	}

	if (bCodeDirty)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(this);
	}
}

void UScriptBlueprint::UpdateSourceCodeIfChanged()
{
	FDateTime OldTimeStamp;	
	bool bCodeDirty = !FDateTime::Parse(SourceFileTimestamp, OldTimeStamp);
	FDateTime TimeStamp = IFileManager::Get().GetTimeStamp(*SourceFilePath);
	bCodeDirty = bCodeDirty || (TimeStamp > OldTimeStamp);

	if (bCodeDirty)
	{
		FString NewScript;
		if (FFileHelper::LoadFileToString(NewScript, *SourceFilePath))
		{
			SourceCode = NewScript;
			SourceFileTimestamp = TimeStamp.ToString();
		}
	}
}
#endif
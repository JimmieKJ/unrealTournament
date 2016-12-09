// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "VersionManifest.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "Serialization/JsonTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Modules/ModuleManager.h"

FVersionManifest::FVersionManifest()
{
	Changelist = 0;
	CompatibleChangelist = 0;
}

FString FVersionManifest::GetFileName(const FString& DirectoryName, bool bIsGameFolder)
{
	FString FileName = DirectoryName / FPlatformProcess::ExecutableName();
	if(FApp::GetBuildConfiguration() == EBuildConfigurations::DebugGame && bIsGameFolder)
	{
		FileName += FString::Printf(TEXT("-%s-DebugGame"), FPlatformProcess::GetBinariesSubdirectory());
	}
	return FileName + TEXT(".modules");
}

bool FVersionManifest::TryRead(const FString& FileName, FVersionManifest& Manifest)
{
	// Read the file to a string
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FileName))
	{
		return false;
	}

	// Deserialize a JSON object from the string
	TSharedPtr< FJsonObject > ObjectPtr;
	TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, ObjectPtr) || !ObjectPtr.IsValid() )
	{
		return false;
	}
	FJsonObject& Object = *ObjectPtr.Get();

	// Read the changelist and build id
	if(!Object.TryGetNumberField(TEXT("Changelist"), Manifest.Changelist) || !Object.TryGetStringField(TEXT("BuildId"), Manifest.BuildId))
	{
		return false;
	}

	// Read the compatible changelist, or default to the current changelist if it's not set
	if(Manifest.Changelist == 0 || !Object.TryGetNumberField(TEXT("CompatibleChangelist"), Manifest.CompatibleChangelist))
	{
		Manifest.CompatibleChangelist = Manifest.Changelist;
	}

	// Read the module mappings
	TSharedPtr<FJsonObject> ModulesObject = Object.GetObjectField(TEXT("Modules"));
	if(ModulesObject.IsValid())
	{
		for(TPair<FString, TSharedPtr<FJsonValue>>& Pair: ModulesObject->Values)
		{
			if(Pair.Value->Type == EJson::String)
			{
				Manifest.ModuleNameToFileName.FindOrAdd(Pair.Key) = Pair.Value->AsString();
			}
		}
	}

	return true;
}





FVersionedModuleEnumerator::FVersionedModuleEnumerator()
{
}

bool FVersionedModuleEnumerator::RegisterWithModuleManager()
{
	FString VersionManifestFileName = FVersionManifest::GetFileName(FPlatformProcess::BaseDir(), false);
	if(!FVersionManifest::TryRead(VersionManifestFileName, InitialManifest))
	{
		return false;
	}

	FModuleManager::Get().QueryModulesDelegate.BindRaw(this, &FVersionedModuleEnumerator::QueryModules);
	return true;
}

const FVersionManifest& FVersionedModuleEnumerator::GetInitialManifest() const
{
	return InitialManifest;
}

void FVersionedModuleEnumerator::QueryModules(const FString& InDirectoryName, bool bIsGameDirectory, TMap<FString, FString>& OutModules) const
{
	FVersionManifest Manifest;
	if(FVersionManifest::TryRead(FVersionManifest::GetFileName(InDirectoryName, bIsGameDirectory), Manifest) && IsMatchingVersion(Manifest))
	{
		OutModules = Manifest.ModuleNameToFileName;
	}
}

bool FVersionedModuleEnumerator::IsMatchingVersion(const FVersionManifest& Manifest) const
{
	if(InitialManifest.Changelist == 0)
	{
		return Manifest.Changelist == 0 && (InitialManifest.BuildId == Manifest.BuildId || Manifest.BuildId.Len() == 0);
	}
	else
	{
		return InitialManifest.Changelist == Manifest.Changelist;
	}
}


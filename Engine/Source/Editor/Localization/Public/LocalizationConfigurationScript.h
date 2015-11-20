// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConfigCacheIni.h"
#include "LocalizationTargetTypes.h"

struct LOCALIZATION_API FLocalizationConfigurationScript : public FConfigFile
{
	FConfigSection& CommonSettings()
	{
		return FindOrAdd(TEXT("CommonSettings"));
	}

	FConfigSection& GatherTextStep(const uint32 Index)
	{
		return FindOrAdd( FString::Printf( TEXT("GatherTextStep%u"), Index) );
	}
};

namespace LocalizationConfigurationScript
{
	LOCALIZATION_API FString MakePathRelativeForCommandletProcess(const FString& Path, const bool IsUsingProjectFile);

	LOCALIZATION_API FString GetScriptDirectory(const ULocalizationTarget* const Target);
	LOCALIZATION_API FString GetDataDirectory(const ULocalizationTarget* const Target);
	LOCALIZATION_API TArray<FString> GetScriptPaths(const ULocalizationTarget* const Target);

	LOCALIZATION_API FString GetManifestPath(const ULocalizationTarget* const Target);
	LOCALIZATION_API FString GetArchivePath(const ULocalizationTarget* const Target, const FString& CultureName);
	LOCALIZATION_API FString GetDefaultPOFileName(const ULocalizationTarget* const Target);
	LOCALIZATION_API FString GetDefaultPOPath(const ULocalizationTarget* const Target, const FString& CultureName);
	LOCALIZATION_API FString GetLocResPath(const ULocalizationTarget* const Target, const FString& CultureName);
	LOCALIZATION_API FString GetWordCountCSVPath(const ULocalizationTarget* const Target);
	LOCALIZATION_API FString GetConflictReportPath(const ULocalizationTarget* const Target);

	LOCALIZATION_API FLocalizationConfigurationScript GenerateGatherScript(const ULocalizationTarget* const Target);
	LOCALIZATION_API FString GetGatherScriptPath(const ULocalizationTarget* const Target);

	LOCALIZATION_API FLocalizationConfigurationScript GenerateImportScript(const ULocalizationTarget* const Target, const TOptional<FString> CultureName = TOptional<FString>(), const TOptional<FString> OutputPathOverride = TOptional<FString>());
	LOCALIZATION_API FString GetImportScriptPath(const ULocalizationTarget* const Target, const TOptional<FString> CultureName = TOptional<FString>());

	LOCALIZATION_API FLocalizationConfigurationScript GenerateExportScript(const ULocalizationTarget* const Target, const TOptional<FString> CultureName = TOptional<FString>(), const TOptional<FString> OutputPathOverride = TOptional<FString>());
	LOCALIZATION_API FString GetExportScriptPath(const ULocalizationTarget* const Target, const TOptional<FString> CultureName = TOptional<FString>());

	LOCALIZATION_API FLocalizationConfigurationScript GenerateWordCountReportScript(const ULocalizationTarget* const Target);
	LOCALIZATION_API FString GetWordCountReportScriptPath(const ULocalizationTarget* const Target);

	LOCALIZATION_API FLocalizationConfigurationScript GenerateCompileScript(const ULocalizationTarget* const Target, const TOptional<FString> CultureName = TOptional<FString>());
	LOCALIZATION_API FString GetCompileScriptPath(const ULocalizationTarget* const Target, const TOptional<FString> CultureName = TOptional<FString>());

	LOCALIZATION_API FLocalizationConfigurationScript GenerateRegenerateResourcesScript(const ULocalizationTarget* const Target);
	LOCALIZATION_API FString GetRegenerateResourcesScriptPath(const ULocalizationTarget* const Target);
}
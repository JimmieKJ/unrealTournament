// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConfigCacheIni.h"
#include "LocalizationTargetTypes.h"

struct FLocalizationConfigurationScript : public FConfigFile
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
	FString MakePathRelativeForCommandletProcess(const FString& Path, const bool IsUsingProjectFile);

	FString GetScriptDirectory(const ULocalizationTarget* const Target);
	FString GetDataDirectory(const ULocalizationTarget* const Target);
	TArray<FString> GetScriptPaths(const ULocalizationTarget* const Target);

	FString GetManifestPath(const ULocalizationTarget* const Target);
	FString GetArchivePath(const ULocalizationTarget* const Target, const FString& CultureName);
	FString GetDefaultPOFileName(const ULocalizationTarget* const Target);
	FString GetDefaultPOPath(const ULocalizationTarget* const Target, const FString& CultureName);
	FString GetLocResPath(const ULocalizationTarget* const Target, const FString& CultureName);
	FString GetWordCountCSVPath(const ULocalizationTarget* const Target);
	FString GetConflictReportPath(const ULocalizationTarget* const Target);

	FLocalizationConfigurationScript GenerateGatherScript(const ULocalizationTarget* const Target);
	FString GetGatherScriptPath(const ULocalizationTarget* const Target);

	FLocalizationConfigurationScript GenerateImportScript(const ULocalizationTarget* const Target, const TOptional<FString> CultureName = TOptional<FString>(), const TOptional<FString> OutputPathOverride = TOptional<FString>());
	FString GetImportScriptPath(const ULocalizationTarget* const Target, const TOptional<FString> CultureName = TOptional<FString>());

	FLocalizationConfigurationScript GenerateExportScript(const ULocalizationTarget* const Target, const TOptional<FString> CultureName = TOptional<FString>(), const TOptional<FString> OutputPathOverride = TOptional<FString>());
	FString GetExportScriptPath(const ULocalizationTarget* const Target, const TOptional<FString> CultureName = TOptional<FString>());

	FLocalizationConfigurationScript GenerateWordCountReportScript(const ULocalizationTarget* const Target);
	FString GetWordCountReportScriptPath(const ULocalizationTarget* const Target);

	FLocalizationConfigurationScript GenerateCompileScript(const ULocalizationTarget* const Target, const TOptional<FString> CultureName = TOptional<FString>());
	FString GetCompileScriptPath(const ULocalizationTarget* const Target, const TOptional<FString> CultureName = TOptional<FString>());
}
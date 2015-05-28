// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "LocalizationTargetTypes.h"

namespace LocalizationCommandletTasks
{
	bool GatherTargets(const TSharedRef<SWindow>& ParentWindow, const TArray<ULocalizationTarget*>& Targets);
	bool GatherTarget(const TSharedRef<SWindow>& ParentWindow, ULocalizationTarget* const Target);

	bool ImportTargets(const TSharedRef<SWindow>& ParentWindow, const TArray<ULocalizationTarget*>& Targets, const TOptional<FString> DirectoryPath = TOptional<FString>());
	bool ImportTarget(const TSharedRef<SWindow>& ParentWindow, ULocalizationTarget* const Target, const TOptional<FString> DirectoryPath = TOptional<FString>());
	bool ImportCulture(const TSharedRef<SWindow>& ParentWindow, ULocalizationTarget* const Target, const FString& CultureName, const TOptional<FString> FilePath = TOptional<FString>());

	bool ExportTargets(const TSharedRef<SWindow>& ParentWindow, const TArray<ULocalizationTarget*>& Targets, const TOptional<FString> DirectoryPath = TOptional<FString>());
	bool ExportTarget(const TSharedRef<SWindow>& ParentWindow, ULocalizationTarget* const Target, const TOptional<FString> DirectoryPath = TOptional<FString>());
	bool ExportCulture(const TSharedRef<SWindow>& ParentWindow, ULocalizationTarget* const Target, const FString& CultureName, const TOptional<FString> FilePath = TOptional<FString>());

	bool GenerateWordCountReportsForTargets(const TSharedRef<SWindow>& ParentWindow, const TArray<ULocalizationTarget*>& Targets);
	bool GenerateWordCountReportForTarget(const TSharedRef<SWindow>& ParentWindow, ULocalizationTarget* const Target);

	bool CompileTargets(const TSharedRef<SWindow>& ParentWindow, const TArray<ULocalizationTarget*>& Targets);
	bool CompileTarget(const TSharedRef<SWindow>& ParentWindow, ULocalizationTarget* const Target);
	bool CompileCulture(const TSharedRef<SWindow>& ParentWindow, ULocalizationTarget* const Target, const FString& CultureName);
}
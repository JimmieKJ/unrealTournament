// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LocalizationPrivatePCH.h"
#include "LocalizationCommandletTasks.h"
#include "LocalizationCommandletExecution.h"
#include "LocalizationConfigurationScript.h"

#define LOCTEXT_NAMESPACE "LocalizationCommandletTasks"

bool LocalizationCommandletTasks::GatherTargets(const TSharedRef<SWindow>& ParentWindow, const TArray<ULocalizationTarget*>& Targets)
{
	TArray<LocalizationCommandletExecution::FTask> Tasks;

	for (ULocalizationTarget* Target : Targets)
	{
		const bool ShouldUseProjectFile = !Target->IsMemberOfEngineTargetSet();

		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("TargetName"), FText::FromString(Target->Settings.Name));

		const FText GatherTaskName = FText::Format(LOCTEXT("GatherTaskNameFormat", "Gather Text for {TargetName}"), Arguments);
		const FString GatherScriptPath = LocalizationConfigurationScript::GetGatherScriptPath(Target);
		LocalizationConfigurationScript::GenerateGatherScript(Target).Write(GatherScriptPath);
		Tasks.Add(LocalizationCommandletExecution::FTask(GatherTaskName, GatherScriptPath, ShouldUseProjectFile));
	}

	return LocalizationCommandletExecution::Execute(ParentWindow, LOCTEXT("GatherAllTargetsWindowTitle", "Gather Text for All Targets"), Tasks);
}

bool LocalizationCommandletTasks::GatherTarget(const TSharedRef<SWindow>& ParentWindow, ULocalizationTarget* const Target)
{
	TArray<LocalizationCommandletExecution::FTask> Tasks;
	const bool ShouldUseProjectFile = !Target->IsMemberOfEngineTargetSet();

	const FString GatherScriptPath = LocalizationConfigurationScript::GetGatherScriptPath(Target);
	LocalizationConfigurationScript::GenerateGatherScript(Target).Write(GatherScriptPath);
	Tasks.Add(LocalizationCommandletExecution::FTask(LOCTEXT("GatherTaskName", "Gather Text"), GatherScriptPath, ShouldUseProjectFile));

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("TargetName"), FText::FromString(Target->Settings.Name));

	const FText WindowTitle = FText::Format(LOCTEXT("GatherTargetWindowTitle", "Gather Text for Target {TargetName}"), Arguments);
	return LocalizationCommandletExecution::Execute(ParentWindow, WindowTitle, Tasks);
}


bool LocalizationCommandletTasks::ImportTargets(const TSharedRef<SWindow>& ParentWindow, const TArray<ULocalizationTarget*>& Targets, const TOptional<FString> DirectoryPath)
{
	TArray<LocalizationCommandletExecution::FTask> Tasks;

	for (ULocalizationTarget* Target : Targets)
	{
		const bool ShouldUseProjectFile = !Target->IsMemberOfEngineTargetSet();

		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("TargetName"), FText::FromString(Target->Settings.Name));

		const FText ImportTaskName = FText::Format(LOCTEXT("ImportTaskNameFormat", "Import Translations for {TargetName}"), Arguments);
		const FString ImportScriptPath = LocalizationConfigurationScript::GetImportScriptPath(Target, TOptional<FString>());
		const TOptional<FString> DirectoryPathForTarget = DirectoryPath.IsSet() ? DirectoryPath.GetValue() / Target->Settings.Name : TOptional<FString>();
		LocalizationConfigurationScript::GenerateImportScript(Target, TOptional<FString>(), DirectoryPathForTarget).Write(ImportScriptPath);
		Tasks.Add(LocalizationCommandletExecution::FTask(ImportTaskName, ImportScriptPath, ShouldUseProjectFile));

		const FText ReportTaskName = FText::Format(LOCTEXT("ReportTaskNameFormat", "Generate Reports for {TargetName}"), Arguments);
		const FString ReportScriptPath = LocalizationConfigurationScript::GetWordCountReportScriptPath(Target);
		LocalizationConfigurationScript::GenerateWordCountReportScript(Target).Write(ReportScriptPath);
		Tasks.Add(LocalizationCommandletExecution::FTask(ReportTaskName, ReportScriptPath, ShouldUseProjectFile));
	}

	return LocalizationCommandletExecution::Execute(ParentWindow, LOCTEXT("ImportForAllTargetsWindowTitle", "Import Translations for All Targets"), Tasks);
}

bool LocalizationCommandletTasks::ImportTarget(const TSharedRef<SWindow>& ParentWindow, ULocalizationTarget* const Target, const TOptional<FString> DirectoryPath)
{
	TArray<LocalizationCommandletExecution::FTask> Tasks;
	const bool ShouldUseProjectFile = !Target->IsMemberOfEngineTargetSet();

	const FString ImportScriptPath = LocalizationConfigurationScript::GetImportScriptPath(Target, TOptional<FString>());
	LocalizationConfigurationScript::GenerateImportScript(Target, TOptional<FString>(), DirectoryPath).Write(ImportScriptPath);
	Tasks.Add(LocalizationCommandletExecution::FTask(LOCTEXT("ImportTaskName", "Import Translations"), ImportScriptPath, ShouldUseProjectFile));

	const FString ReportScriptPath = LocalizationConfigurationScript::GetWordCountReportScriptPath(Target);
	LocalizationConfigurationScript::GenerateWordCountReportScript(Target).Write(ReportScriptPath);
	Tasks.Add(LocalizationCommandletExecution::FTask(LOCTEXT("ReportTaskName", "Generate Reports"), ReportScriptPath, ShouldUseProjectFile));

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("TargetName"), FText::FromString(Target->Settings.Name));

	const FText WindowTitle = FText::Format(LOCTEXT("ImportForTargetWindowTitle", "Import Translations for Target {TargetName}"), Arguments);
	return LocalizationCommandletExecution::Execute(ParentWindow, WindowTitle, Tasks);
}

bool LocalizationCommandletTasks::ImportCulture(const TSharedRef<SWindow>& ParentWindow, ULocalizationTarget* const Target, const FString& CultureName, const TOptional<FString> FilePath)
{
	FCulturePtr Culture = FInternationalization::Get().GetCulture(CultureName);
	if (!Culture.IsValid())
	{
		return false;
	}

	TArray<LocalizationCommandletExecution::FTask> Tasks;
	const bool ShouldUseProjectFile = !Target->IsMemberOfEngineTargetSet();

	const FString DefaultImportScriptPath = LocalizationConfigurationScript::GetImportScriptPath(Target, TOptional<FString>(CultureName));
	const FString ImportScriptPath = FPaths::CreateTempFilename(*FPaths::GetPath(DefaultImportScriptPath), *FPaths::GetBaseFilename(DefaultImportScriptPath), *FPaths::GetExtension(DefaultImportScriptPath, true));
	LocalizationConfigurationScript::GenerateImportScript(Target, TOptional<FString>(CultureName), FilePath).Write(ImportScriptPath);
	Tasks.Add(LocalizationCommandletExecution::FTask(LOCTEXT("ImportTaskName", "Import Translations"), ImportScriptPath, ShouldUseProjectFile));

	const FString ReportScriptPath = LocalizationConfigurationScript::GetWordCountReportScriptPath(Target);
	LocalizationConfigurationScript::GenerateWordCountReportScript(Target).Write(ReportScriptPath);
	Tasks.Add(LocalizationCommandletExecution::FTask(LOCTEXT("ReportTaskName", "Generate Reports"), ReportScriptPath, ShouldUseProjectFile));

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("CultureName"), FText::FromString(Culture->GetDisplayName()));
	Arguments.Add(TEXT("TargetName"), FText::FromString(Target->Settings.Name));

	const FText WindowTitle = FText::Format(LOCTEXT("ImportCultureForTargetWindowTitle", "Import {CultureName} Translations for Target {TargetName}"), Arguments);

	bool HasSucceeeded = LocalizationCommandletExecution::Execute(ParentWindow, WindowTitle, Tasks);
	IFileManager::Get().Delete(*ImportScriptPath); // Don't clutter up the loc config directory with scripts for individual cultures.
	return HasSucceeeded;

}

bool LocalizationCommandletTasks::ExportTargets(const TSharedRef<SWindow>& ParentWindow, const TArray<ULocalizationTarget*>& Targets, const TOptional<FString> DirectoryPath)
{
	TArray<LocalizationCommandletExecution::FTask> Tasks;

	for (ULocalizationTarget* Target : Targets)
	{
		const bool ShouldUseProjectFile = !Target->IsMemberOfEngineTargetSet();

		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("TargetName"), FText::FromString(Target->Settings.Name));

		const FText ExportTaskName = FText::Format(LOCTEXT("ExportTaskNameFormat", "Export Translations for {TargetName}"), Arguments);
		const FString ExportScriptPath = LocalizationConfigurationScript::GetExportScriptPath(Target, TOptional<FString>());
		const TOptional<FString> DirectoryPathForTarget = DirectoryPath.IsSet() ? DirectoryPath.GetValue() / Target->Settings.Name : TOptional<FString>();
		LocalizationConfigurationScript::GenerateExportScript(Target, TOptional<FString>(), DirectoryPathForTarget).Write(ExportScriptPath);
		Tasks.Add(LocalizationCommandletExecution::FTask(ExportTaskName, ExportScriptPath, ShouldUseProjectFile));
	}

	return LocalizationCommandletExecution::Execute(ParentWindow, LOCTEXT("ExportForAllTargetsWindowTitle", "Export Translations for All Targets"), Tasks);
}

bool LocalizationCommandletTasks::ExportTarget(const TSharedRef<SWindow>& ParentWindow, ULocalizationTarget* const Target, const TOptional<FString> DirectoryPath)
{
	TArray<LocalizationCommandletExecution::FTask> Tasks;
	const bool ShouldUseProjectFile = !Target->IsMemberOfEngineTargetSet();

	const FString ExportScriptPath = LocalizationConfigurationScript::GetExportScriptPath(Target, TOptional<FString>());
	LocalizationConfigurationScript::GenerateExportScript(Target, TOptional<FString>(), DirectoryPath).Write(ExportScriptPath);
	Tasks.Add(LocalizationCommandletExecution::FTask(LOCTEXT("ExportTaskName", "Export Translations"), ExportScriptPath, ShouldUseProjectFile));

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("TargetName"), FText::FromString(Target->Settings.Name));

	const FText WindowTitle = FText::Format(LOCTEXT("ExportForTargetWindowTitle", "Export Translations for Target {TargetName}"), Arguments);
	return LocalizationCommandletExecution::Execute(ParentWindow, WindowTitle, Tasks);
}

bool LocalizationCommandletTasks::ExportCulture(const TSharedRef<SWindow>& ParentWindow, ULocalizationTarget* const Target, const FString& CultureName, const TOptional<FString> FilePath)
{
	FCulturePtr Culture = FInternationalization::Get().GetCulture(CultureName);
	if (!Culture.IsValid())
	{
		return false;
	}

	TArray<LocalizationCommandletExecution::FTask> Tasks;
	const bool ShouldUseProjectFile = !Target->IsMemberOfEngineTargetSet();

	const FString DefaultExportScriptPath = LocalizationConfigurationScript::GetExportScriptPath(Target, TOptional<FString>(CultureName));
	const FString ExportScriptPath = FPaths::CreateTempFilename(*FPaths::GetPath(DefaultExportScriptPath), *FPaths::GetBaseFilename(DefaultExportScriptPath), *FPaths::GetExtension(DefaultExportScriptPath, true));
	LocalizationConfigurationScript::GenerateExportScript(Target, TOptional<FString>(CultureName), FilePath).Write(ExportScriptPath);
	Tasks.Add(LocalizationCommandletExecution::FTask(LOCTEXT("ExportTaskName", "Export Translations"), ExportScriptPath, ShouldUseProjectFile));

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("CultureName"), FText::FromString(Culture->GetDisplayName()));
	Arguments.Add(TEXT("TargetName"), FText::FromString(Target->Settings.Name));

	const FText WindowTitle = FText::Format(LOCTEXT("ExportCultureForTargetWindowTitle", "Export {CultureName} Translations for Target {TargetName}"), Arguments);

	bool HasSucceeeded = LocalizationCommandletExecution::Execute(ParentWindow, WindowTitle, Tasks);
	IFileManager::Get().Delete(*ExportScriptPath); // Don't clutter up the loc config directory with scripts for individual cultures.
	return HasSucceeeded;
}

bool LocalizationCommandletTasks::GenerateWordCountReportsForTargets(const TSharedRef<SWindow>& ParentWindow, const TArray<ULocalizationTarget*>& Targets)
{
	TArray<LocalizationCommandletExecution::FTask> Tasks;

	for (ULocalizationTarget* Target : Targets)
	{
		const bool ShouldUseProjectFile = !Target->IsMemberOfEngineTargetSet();

		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("TargetName"), FText::FromString(Target->Settings.Name));

		const FText ReportTaskName = FText::Format(LOCTEXT("ReportTaskNameFormat", "Generate Word Count Report for {TargetName}"), Arguments);
		const FString ReportScriptPath = LocalizationConfigurationScript::GetWordCountReportScriptPath(Target);
		LocalizationConfigurationScript::GenerateWordCountReportScript(Target).Write(ReportScriptPath);
		Tasks.Add(LocalizationCommandletExecution::FTask(ReportTaskName, ReportScriptPath, ShouldUseProjectFile));
	}

	return LocalizationCommandletExecution::Execute(ParentWindow, LOCTEXT("GenerateReportsForAllTargetsWindowTitle", "Generate Word Count Reports for All Targets"), Tasks);
}

bool LocalizationCommandletTasks::GenerateWordCountReportForTarget(const TSharedRef<SWindow>& ParentWindow, ULocalizationTarget* const Target)
{
	TArray<LocalizationCommandletExecution::FTask> Tasks;
	const bool ShouldUseProjectFile = !Target->IsMemberOfEngineTargetSet();

	const FString ReportScriptPath = LocalizationConfigurationScript::GetWordCountReportScriptPath(Target);
	LocalizationConfigurationScript::GenerateWordCountReportScript(Target).Write(ReportScriptPath);
	Tasks.Add(LocalizationCommandletExecution::FTask(LOCTEXT("ReportTaskName", "Generate Word Count Report"), ReportScriptPath, ShouldUseProjectFile));

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("TargetName"), FText::FromString(Target->Settings.Name));

	const FText WindowTitle = FText::Format(LOCTEXT("GenerateReportForTargetWindowTitle", "Generate Word Count Report for Target {TargetName}"), Arguments);
	return LocalizationCommandletExecution::Execute(ParentWindow, WindowTitle, Tasks);
}

bool LocalizationCommandletTasks::CompileTargets(const TSharedRef<SWindow>& ParentWindow, const TArray<ULocalizationTarget*>& Targets)
{
	TArray<LocalizationCommandletExecution::FTask> Tasks;

	for (ULocalizationTarget* Target : Targets)
	{
		const bool ShouldUseProjectFile = !Target->IsMemberOfEngineTargetSet();

		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("TargetName"), FText::FromString(Target->Settings.Name));

		const FText CompileTaskName = FText::Format(LOCTEXT("CompileTaskNameFormat", "Compile Translations for {TargetName}"), Arguments);
		const FString CompileScriptPath = LocalizationConfigurationScript::GetCompileScriptPath(Target);
		LocalizationConfigurationScript::GenerateCompileScript(Target).Write(CompileScriptPath);
		Tasks.Add(LocalizationCommandletExecution::FTask(CompileTaskName, CompileScriptPath, ShouldUseProjectFile));
	}

	return LocalizationCommandletExecution::Execute(ParentWindow, LOCTEXT("GenerateLocResForAllTargetsWindowTitle", "Compile Translations for All Targets"), Tasks);
}

bool LocalizationCommandletTasks::CompileTarget(const TSharedRef<SWindow>& ParentWindow, ULocalizationTarget* const Target)
{
	TArray<LocalizationCommandletExecution::FTask> Tasks;
	const bool ShouldUseProjectFile = !Target->IsMemberOfEngineTargetSet();

	const FString CompileScriptPath = LocalizationConfigurationScript::GetCompileScriptPath(Target);
	LocalizationConfigurationScript::GenerateCompileScript(Target).Write(CompileScriptPath);
	Tasks.Add(LocalizationCommandletExecution::FTask(LOCTEXT("CompileTaskName", "Compile Translations"), CompileScriptPath, ShouldUseProjectFile));

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("TargetName"), FText::FromString(Target->Settings.Name));

	const FText WindowTitle = FText::Format(LOCTEXT("GenerateLocResForTargetWindowTitle", "Compile Translations for Target {TargetName}"), Arguments);
	return LocalizationCommandletExecution::Execute(ParentWindow, WindowTitle, Tasks);
}

bool LocalizationCommandletTasks::CompileCulture(const TSharedRef<SWindow>& ParentWindow, ULocalizationTarget* const Target, const FString& CultureName)
{
	FCulturePtr Culture = FInternationalization::Get().GetCulture(CultureName);
	if (!Culture.IsValid())
	{
		return false;
	}

	TArray<LocalizationCommandletExecution::FTask> Tasks;
	const bool ShouldUseProjectFile = !Target->IsMemberOfEngineTargetSet();

	const FString DefaultCompileScriptPath = LocalizationConfigurationScript::GetCompileScriptPath(Target, TOptional<FString>(CultureName));
	const FString CompileScriptPath = FPaths::CreateTempFilename(*FPaths::GetPath(DefaultCompileScriptPath), *FPaths::GetBaseFilename(DefaultCompileScriptPath), *FPaths::GetExtension(DefaultCompileScriptPath, true));
	LocalizationConfigurationScript::GenerateCompileScript(Target, TOptional<FString>(CultureName)).Write(CompileScriptPath);
	Tasks.Add(LocalizationCommandletExecution::FTask(LOCTEXT("CompileTaskName", "Compile Translations"), CompileScriptPath, ShouldUseProjectFile));

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("CultureName"), FText::FromString(Culture->GetDisplayName()));
	Arguments.Add(TEXT("TargetName"), FText::FromString(Target->Settings.Name));

	const FText WindowTitle = FText::Format(LOCTEXT("CompileCultureForTargetWindowTitle", "Compile {CultureName} Translations for Target {TargetName}"), Arguments);

	bool HasSucceeeded = LocalizationCommandletExecution::Execute(ParentWindow, WindowTitle, Tasks);
	IFileManager::Get().Delete(*CompileScriptPath); // Don't clutter up the loc config directory with scripts for individual cultures.
	return HasSucceeeded;
}

#undef LOCTEXT_NAMESPACE
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LocalizationDashboardPrivatePCH.h"
#include "LocalizationConfigurationScript.h"
#include "LocalizationTargetTypes.h"
#include "LocalizationDashboardSettings.h"

namespace
{
	FString GetConfigDir(const ULocalizationTarget* const Target)
	{
		return Target->IsMemberOfEngineTargetSet() ? FPaths::EngineConfigDir() : FPaths::GameConfigDir();
	}

	FString GetContentDir(const ULocalizationTarget* const Target)
	{
		return Target->IsMemberOfEngineTargetSet() ? FPaths::EngineContentDir() : FPaths::GameContentDir();
	}
}

namespace LocalizationConfigurationScript
{
	FString MakePathRelativeForCommandletProcess(const FString& Path, const bool IsUsingProjectFile)
	{
		FString Result = Path;
		const FString ProjectDir = !IsUsingProjectFile ? FPaths::EngineDir() : FPaths::GameDir();
		FPaths::MakePathRelativeTo(Result, *ProjectDir);
		return Result;
	}

	FString GetScriptDirectory(const ULocalizationTarget* const Target)
	{
		return GetConfigDir(Target) / TEXT("Localization");
	}

	FString GetDataDirectory(const ULocalizationTarget* const Target)
	{
		return GetContentDir(Target) / TEXT("Localization") / Target->Settings.Name;
	}

	TArray<FString> GetScriptPaths(const ULocalizationTarget* const Target)
	{
		TArray<FString> Result;
		Result.Add(GetGatherScriptPath(Target));
		Result.Add(GetImportScriptPath(Target));
		Result.Add(GetExportScriptPath(Target));
		Result.Add(GetWordCountReportScriptPath(Target));
		return Result;
	}

	FString GetManifestPath(const ULocalizationTarget* const Target)
	{
		return GetDataDirectory(Target) / FString::Printf( TEXT("%s.%s"), *Target->Settings.Name, TEXT("manifest") );
	}

	FString GetArchivePath(const ULocalizationTarget* const Target, const FString& CultureName)
	{
		return GetDataDirectory(Target) / CultureName / FString::Printf( TEXT("%s.%s"), *Target->Settings.Name, TEXT("archive") );
	}

	FString GetDefaultPOFileName(const ULocalizationTarget* const Target)
	{
		return FString::Printf( TEXT("%s.%s"), *Target->Settings.Name, TEXT("po") );
	}

	FString GetDefaultPOPath(const ULocalizationTarget* const Target, const FString& CultureName)
	{
		return GetDataDirectory(Target) / CultureName / GetDefaultPOFileName(Target);
	}

	FString GetLocResPath(const ULocalizationTarget* const Target, const FString& CultureName)
	{
		return GetDataDirectory(Target) / CultureName / FString::Printf( TEXT("%s.%s"), *Target->Settings.Name, TEXT("locres") );
	}

	FString GetWordCountCSVPath(const ULocalizationTarget* const Target)
	{
		return GetDataDirectory(Target) / FString::Printf( TEXT("%s.%s"), *Target->Settings.Name, TEXT("csv") );
	}

	FString GetConflictReportPath(const ULocalizationTarget* const Target)
	{
		return GetDataDirectory(Target) / FString::Printf( TEXT("%s_Conflicts.%s"), *Target->Settings.Name, TEXT("txt") );
	}

	FLocalizationConfigurationScript GenerateGatherScript(const ULocalizationTarget* const Target)
	{
		FLocalizationConfigurationScript Script;

		const FString ConfigDirRelativeToGameDir = MakePathRelativeForCommandletProcess(GetConfigDir(Target), !Target->IsMemberOfEngineTargetSet());
		const FString ContentDirRelativeToGameDir = MakePathRelativeForCommandletProcess(GetContentDir(Target), !Target->IsMemberOfEngineTargetSet());

		// CommonSettings
		{
			FConfigSection& ConfigSection = Script.CommonSettings();

			const ULocalizationTargetSet* const LocalizationTargetSet = GetDefault<ULocalizationTargetSet>(ULocalizationTargetSet::StaticClass());
			for (const FGuid& TargetDependencyGuid : Target->Settings.TargetDependencies)
			{
				TArray<ULocalizationTarget*> AllLocalizationTargets;
				ULocalizationTargetSet* EngineTargetSet = ULocalizationDashboardSettings::GetEngineTargetSet();
				if (EngineTargetSet != LocalizationTargetSet)
				{
					AllLocalizationTargets.Append(EngineTargetSet->TargetObjects);
				}
				AllLocalizationTargets.Append(LocalizationTargetSet->TargetObjects);

				ULocalizationTarget* const * OtherTarget = AllLocalizationTargets.FindByPredicate([&TargetDependencyGuid](ULocalizationTarget* const InOtherTarget)->bool{return InOtherTarget->Settings.Guid == TargetDependencyGuid;});
				if (OtherTarget)
				{
					ConfigSection.Add( TEXT("ManifestDependencies"), MakePathRelativeForCommandletProcess(GetManifestPath(*OtherTarget), !Target->IsMemberOfEngineTargetSet()) );
				}
			}

			for (const FFilePath& Path : Target->Settings.AdditionalManifestDependencies)
			{
				ConfigSection.Add( TEXT("ManifestDependencies"), MakePathRelativeForCommandletProcess(Path.FilePath, !Target->IsMemberOfEngineTargetSet()) );
			}

			for (const FString& ModuleName : Target->Settings.RequiredModuleNames)
			{
				ConfigSection.Add( TEXT("ModulesToPreload"), ModuleName );
			}

			const FString SourcePath = ContentDirRelativeToGameDir / TEXT("Localization") / Target->Settings.Name;
			ConfigSection.Add( TEXT("SourcePath"), SourcePath );
			const FString DestinationPath = ContentDirRelativeToGameDir / TEXT("Localization") / Target->Settings.Name;
			ConfigSection.Add( TEXT("DestinationPath"), DestinationPath );

			ConfigSection.Add( TEXT("ManifestName"), FPaths::GetCleanFilename(GetManifestPath(Target)) );
			ConfigSection.Add( TEXT("ArchiveName"), FPaths::GetCleanFilename(GetArchivePath(Target, FString())) );

			if (Target->Settings.SupportedCulturesStatistics.IsValidIndex(Target->Settings.NativeCultureIndex))
			{
			ConfigSection.Add( TEXT("NativeCulture"), Target->Settings.SupportedCulturesStatistics[Target->Settings.NativeCultureIndex].CultureName );
			}
			for (const FCultureStatistics& CultureStatistics : Target->Settings.SupportedCulturesStatistics)
			{
				ConfigSection.Add( TEXT("CulturesToGenerate"), CultureStatistics.CultureName );
			}
		}

		uint32 GatherTextStepIndex = 0;
		// GatherTextFromSource
		if (Target->Settings.GatherFromTextFiles.IsEnabled)
		{
			FConfigSection& ConfigSection = Script.GatherTextStep(GatherTextStepIndex++);

			// CommandletClass
			ConfigSection.Add( TEXT("CommandletClass"), TEXT("GatherTextFromSource") );

			// Include Paths
			for (const auto& IncludePath : Target->Settings.GatherFromTextFiles.SearchDirectories)
			{
				ConfigSection.Add( TEXT("SearchDirectoryPaths"), IncludePath.Path );
			}

			// Exclude Paths
			ConfigSection.Add( TEXT("ExcludePathFilters"), TEXT("Config/Localization/*") );
			for (const auto& ExcludePath : Target->Settings.GatherFromTextFiles.ExcludePathWildcards)
			{
				ConfigSection.Add( TEXT("ExcludePathFilters"), ExcludePath.Pattern );
			}

			// Source File Search Filters
			for (const auto& FileExtension : Target->Settings.GatherFromTextFiles.FileExtensions)
			{
				ConfigSection.Add( TEXT("FileNameFilters"), FString::Printf( TEXT("*.%s"), *FileExtension.Pattern) );
			}
		}

		// GatherTextFromAssets
		if (Target->Settings.GatherFromPackages.IsEnabled)
		{
			FConfigSection& ConfigSection = Script.GatherTextStep(GatherTextStepIndex++);

			// CommandletClass
			ConfigSection.Add( TEXT("CommandletClass"), TEXT("GatherTextFromAssets") );

			// Include Paths
			for (const auto& IncludePath : Target->Settings.GatherFromPackages.IncludePathWildcards)
			{
				ConfigSection.Add( TEXT("IncludePathFilters"), IncludePath.Pattern );
			}

			// Exclude Paths
			ConfigSection.Add( TEXT("ExcludePathFilters"), TEXT("Content/Localization/*") );
			for (const auto& ExcludePath : Target->Settings.GatherFromPackages.ExcludePathWildcards)
			{
				ConfigSection.Add( TEXT("ExcludePathFilters"), ExcludePath.Pattern );
			}

			// Package Extensions
			for (const auto& FileExtension : Target->Settings.GatherFromPackages.FileExtensions)
			{
				ConfigSection.Add( TEXT("PackageFileNameFilters"), FString::Printf( TEXT("*.%s"), *FileExtension.Pattern) );
			}
		}

		// GatherTextFromMetadata
		if (Target->Settings.GatherFromMetaData.IsEnabled)
		{
			FConfigSection& ConfigSection = Script.GatherTextStep(GatherTextStepIndex++);

			// CommandletClass
			ConfigSection.Add( TEXT("CommandletClass"), TEXT("GatherTextFromMetadata") );

			// Include Paths
			for (const auto& IncludePath : Target->Settings.GatherFromMetaData.IncludePathWildcards)
			{
				ConfigSection.Add( TEXT("IncludePathFilters"), IncludePath.Pattern );
			}

			// Exclude Paths
			for (const auto& ExcludePath : Target->Settings.GatherFromMetaData.ExcludePathWildcards)
			{
				ConfigSection.Add( TEXT("ExcludePathFilters"), ExcludePath.Pattern );
			}

			// Package Extensions
			for (const FMetaDataKeyGatherSpecification& Specification : Target->Settings.GatherFromMetaData.KeySpecifications)
			{
				ConfigSection.Add( TEXT("InputKeys"), Specification.MetaDataKey.Name );
				ConfigSection.Add( TEXT("OutputNamespaces"), Specification.TextNamespace );
				ConfigSection.Add( TEXT("OutputKeys"), FString::Printf(TEXT("\"%s\""), *Specification.TextKeyPattern.Pattern) );
			}
		}

		// GenerateGatherManifest
		{
			FConfigSection& ConfigSection = Script.GatherTextStep(GatherTextStepIndex++);

			// CommandletClass
			ConfigSection.Add( TEXT("CommandletClass"), TEXT("GenerateGatherManifest") );
		}

		// GenerateGatherArchive
		{
			FConfigSection& ConfigSection = Script.GatherTextStep(GatherTextStepIndex++);

			// CommandletClass
			ConfigSection.Add( TEXT("CommandletClass"), TEXT("GenerateGatherArchive") );
		}

		// GenerateTextLocalizationReport
		{
			FConfigSection& ConfigSection = Script.GatherTextStep(GatherTextStepIndex++);

			// CommandletClass
			ConfigSection.Add( TEXT("CommandletClass"), TEXT("GenerateTextLocalizationReport") );

			ConfigSection.Add( TEXT("bWordCountReport"), TEXT("true") );
			ConfigSection.Add( TEXT("WordCountReportName"), FPaths::GetCleanFilename( GetWordCountCSVPath(Target) ) );

			ConfigSection.Add( TEXT("bConflictReport"), TEXT("true") );
			ConfigSection.Add( TEXT("ConflictReportName"), FPaths::GetCleanFilename( GetConflictReportPath(Target) ) );
		}

		Script.Dirty = true;

		return Script;
	}

	FString GetGatherScriptPath(const ULocalizationTarget* const Target)
	{
		return GetScriptDirectory(Target) / FString::Printf( TEXT("%s_Gather.%s"), *(Target->Settings.Name), TEXT("ini") );
	}

	FLocalizationConfigurationScript GenerateImportScript(const ULocalizationTarget* const Target, const TOptional<FString> CultureName, const TOptional<FString> OutputPathOverride)
	{
		FLocalizationConfigurationScript Script;

		const FString ContentDirRelativeToGameDir = MakePathRelativeForCommandletProcess(GetContentDir(Target), !Target->IsMemberOfEngineTargetSet());

		// GatherTextStep0 - InternationalizationExport
		{
			FConfigSection& ConfigSection = Script.GatherTextStep(0);

			// CommandletClass
			ConfigSection.Add( TEXT("CommandletClass"), TEXT("InternationalizationExport") );

			ConfigSection.Add( TEXT("bImportLoc"), TEXT("true") );

			FString SourcePath;
			// Overriding output path changes the source directory for the PO file.
			if (OutputPathOverride.IsSet())
			{
				// The output path for a specific culture is a file path.
				if (CultureName.IsSet())
				{
					SourcePath = MakePathRelativeForCommandletProcess( FPaths::GetPath(OutputPathOverride.GetValue()), !Target->IsMemberOfEngineTargetSet() );
				}
				// Otherwise, it is a directory path.
				else
				{
					SourcePath = MakePathRelativeForCommandletProcess( OutputPathOverride.GetValue(), !Target->IsMemberOfEngineTargetSet() );
				}
			}
			// Use the default PO file's directory path.
			else
			{
				SourcePath = ContentDirRelativeToGameDir / TEXT("Localization") / Target->Settings.Name;
			}
			ConfigSection.Add( TEXT("SourcePath"), SourcePath );

			const FString DestinationPath = ContentDirRelativeToGameDir / TEXT("Localization") / Target->Settings.Name;
			ConfigSection.Add( TEXT("DestinationPath"), DestinationPath );

			const auto& AddCultureToGenerate = [&](const int32 Index)
			{
				ConfigSection.Add( TEXT("CulturesToGenerate"), Target->Settings.SupportedCulturesStatistics[Index].CultureName );
			};

			// Export for a specific culture.
			if (CultureName.IsSet())
			{
				ConfigSection.Add( TEXT("CulturesToGenerate"), CultureName.GetValue() );
			}
			// Export for all cultures.
			else
			{
				for (const FCultureStatistics& CultureStatistics : Target->Settings.SupportedCulturesStatistics)
				{
					ConfigSection.Add( TEXT("CulturesToGenerate"), CultureStatistics.CultureName );
				}
			}

			// Do not use culture subdirectories if importing a single culture to a specific directory.
			if (CultureName.IsSet() && OutputPathOverride.IsSet())
			{
				ConfigSection.Add( TEXT("bUseCultureDirectory"), "false" );
			}

			ConfigSection.Add( TEXT("ManifestName"), FPaths::GetCleanFilename(GetManifestPath(Target)) );
			ConfigSection.Add( TEXT("ArchiveName"), FPaths::GetCleanFilename(GetArchivePath(Target, FString())) );

			FString POFileName;
			// The output path for a specific culture is a file path.
			if (CultureName.IsSet() && OutputPathOverride.IsSet())
			{
				POFileName =  FPaths::GetCleanFilename( OutputPathOverride.GetValue() );
			}
			// Use the default PO file's name.
			else
			{
				POFileName = FPaths::GetCleanFilename( GetDefaultPOFileName( Target ) );
			}
			ConfigSection.Add( TEXT("PortableObjectName"), POFileName );
		}

		Script.Dirty = true;

		return Script;
	}

	FString GetImportScriptPath(const ULocalizationTarget* const Target, const TOptional<FString> CultureName)
	{
		const FString ConfigFileDirectory = GetScriptDirectory(Target);
		FString ConfigFilePath;
		if (CultureName.IsSet())
		{
			ConfigFilePath = ConfigFileDirectory / FString::Printf( TEXT("%s_Import_%s.%s"), *Target->Settings.Name, *CultureName.GetValue(), TEXT("ini") );
		}
		else
		{
			ConfigFilePath = ConfigFileDirectory / FString::Printf( TEXT("%s_Import.%s"), *Target->Settings.Name, TEXT("ini") );
		}
		return ConfigFilePath;
	}

	FLocalizationConfigurationScript GenerateExportScript(const ULocalizationTarget* const Target, const TOptional<FString> CultureName, const TOptional<FString> OutputPathOverride)
	{
		FLocalizationConfigurationScript Script;

		const FString ContentDirRelativeToGameDir = MakePathRelativeForCommandletProcess(GetContentDir(Target), !Target->IsMemberOfEngineTargetSet());

		// GatherTextStep0 - InternationalizationExport
		{
			FConfigSection& ConfigSection = Script.GatherTextStep(0);

			// CommandletClass
			ConfigSection.Add( TEXT("CommandletClass"), TEXT("InternationalizationExport") );

			ConfigSection.Add( TEXT("bExportLoc"), TEXT("true") );

			const FString SourcePath = ContentDirRelativeToGameDir / TEXT("Localization") / Target->Settings.Name;
			ConfigSection.Add( TEXT("SourcePath"), SourcePath );

			FString DestinationPath;
			// Overriding output path changes the destination directory for the PO file.
			if (OutputPathOverride.IsSet())
			{
				// The output path for a specific culture is a file path.
				if (CultureName.IsSet())
				{
					DestinationPath = MakePathRelativeForCommandletProcess( FPaths::GetPath(OutputPathOverride.GetValue()), !Target->IsMemberOfEngineTargetSet() );
				}
				// Otherwise, it is a directory path.
				else
				{
					DestinationPath = MakePathRelativeForCommandletProcess( OutputPathOverride.GetValue(), !Target->IsMemberOfEngineTargetSet() );
				}
			}
			// Use the default PO file's directory path.
			else
			{
				DestinationPath = ContentDirRelativeToGameDir / TEXT("Localization") / Target->Settings.Name;
			}
			ConfigSection.Add( TEXT("DestinationPath"), DestinationPath );

			const auto& AddCultureToGenerate = [&](const int32 Index)
			{
				ConfigSection.Add( TEXT("CulturesToGenerate"), Target->Settings.SupportedCulturesStatistics[Index].CultureName );
			};

			// Export for a specific culture.
			if (CultureName.IsSet())
			{
				const int32 CultureIndex = Target->Settings.SupportedCulturesStatistics.IndexOfByPredicate([CultureName](const FCultureStatistics& Culture) { return Culture.CultureName == CultureName.GetValue(); });
				AddCultureToGenerate(CultureIndex);
			}
			// Export for all cultures.
			else
			{
				for (int32 CultureIndex = 0; CultureIndex < Target->Settings.SupportedCulturesStatistics.Num(); ++CultureIndex)
				{
					AddCultureToGenerate(CultureIndex);
				}
			}

			// Do not use culture subdirectories if exporting a single culture to a specific directory.
			if (CultureName.IsSet() && OutputPathOverride.IsSet())
			{
				ConfigSection.Add( TEXT("bUseCultureDirectory"), "false" );
			}


			ConfigSection.Add( TEXT("ManifestName"), FPaths::GetCleanFilename(GetManifestPath(Target)) );
			ConfigSection.Add( TEXT("ArchiveName"), FPaths::GetCleanFilename(GetArchivePath(Target, FString())) );
			FString POFileName;
			// The output path for a specific culture is a file path.
			if (CultureName.IsSet() && OutputPathOverride.IsSet())
			{
				POFileName =  FPaths::GetCleanFilename( OutputPathOverride.GetValue() );
			}
			// Use the default PO file's name.
			else
			{
				POFileName = FPaths::GetCleanFilename( GetDefaultPOPath( Target, CultureName.Get( TEXT("") ) ) );
			}
			ConfigSection.Add( TEXT("PortableObjectName"), POFileName );
		}

		Script.Dirty = true;

		return Script;
	}

	FString GetExportScriptPath(const ULocalizationTarget* const Target, const TOptional<FString> CultureName)
	{
		const FString ConfigFileDirectory = GetScriptDirectory(Target);
		FString ConfigFilePath;
		if (CultureName.IsSet())
		{
			ConfigFilePath = ConfigFileDirectory / FString::Printf( TEXT("%s_Export_%s.%s"), *Target->Settings.Name, *CultureName.GetValue(), TEXT("ini") );
		}
		else
		{
			ConfigFilePath = ConfigFileDirectory / FString::Printf( TEXT("%s_Export.%s"), *Target->Settings.Name, TEXT("ini") );
		}
		return ConfigFilePath;
	}

	FLocalizationConfigurationScript GenerateWordCountReportScript(const ULocalizationTarget* const Target)
	{
		FLocalizationConfigurationScript Script;

		const FString ContentDirRelativeToGameDir = MakePathRelativeForCommandletProcess(GetContentDir(Target), !Target->IsMemberOfEngineTargetSet());

		// GatherTextStep0 - GenerateTextLocalizationReport
		{
			FConfigSection& ConfigSection = Script.GatherTextStep(0);

			// CommandletClass
			ConfigSection.Add( TEXT("CommandletClass"), TEXT("GenerateTextLocalizationReport") );

			const FString SourcePath = ContentDirRelativeToGameDir / TEXT("Localization") / Target->Settings.Name;
			ConfigSection.Add( TEXT("SourcePath"), SourcePath );
			const FString DestinationPath = ContentDirRelativeToGameDir / TEXT("Localization") / Target->Settings.Name;
			ConfigSection.Add( TEXT("DestinationPath"), DestinationPath );

			ConfigSection.Add( TEXT("ManifestName"), FString::Printf( TEXT("%s.%s"), *Target->Settings.Name, TEXT("manifest") ) );

			for (const FCultureStatistics& CultureStatistics : Target->Settings.SupportedCulturesStatistics)
			{
				ConfigSection.Add( TEXT("CulturesToGenerate"), CultureStatistics.CultureName );
			}

			ConfigSection.Add( TEXT("bWordCountReport"), TEXT("true") );

			ConfigSection.Add( TEXT("WordCountReportName"), FPaths::GetCleanFilename( GetWordCountCSVPath(Target) ) );
		}

		Script.Dirty = true;

		return Script;
	}

	FString GetWordCountReportScriptPath(const ULocalizationTarget* const Target)
	{
		return GetScriptDirectory(Target) / FString::Printf( TEXT("%s_GenerateReports.%s"), *(Target->Settings.Name), TEXT("ini") );
	}

	FLocalizationConfigurationScript GenerateCompileScript(const ULocalizationTarget* const Target, const TOptional<FString> CultureName)
	{
		FLocalizationConfigurationScript Script;

		const FString ContentDirRelativeToGameDir = MakePathRelativeForCommandletProcess(GetContentDir(Target), !Target->IsMemberOfEngineTargetSet());

		// GatherTextStep0 - GenerateTextLocalizationResource
		{
			FConfigSection& ConfigSection = Script.GatherTextStep(0);

			// CommandletClass
			ConfigSection.Add( TEXT("CommandletClass"), TEXT("GenerateTextLocalizationResource") );

			const FString SourcePath = ContentDirRelativeToGameDir / TEXT("Localization") / Target->Settings.Name;
			ConfigSection.Add( TEXT("SourcePath"), SourcePath );
			const FString DestinationPath = ContentDirRelativeToGameDir / TEXT("Localization") / Target->Settings.Name;
			ConfigSection.Add( TEXT("DestinationPath"), DestinationPath );

			ConfigSection.Add( TEXT("ManifestName"), FString::Printf( TEXT("%s.%s"), *Target->Settings.Name, TEXT("manifest") ) );
			ConfigSection.Add( TEXT("ResourceName"), FString::Printf( TEXT("%s.%s"), *Target->Settings.Name, TEXT("locres") ) );

			const auto& AddCultureToGenerate = [&](const int32 Index)
			{
				ConfigSection.Add( TEXT("CulturesToGenerate"), Target->Settings.SupportedCulturesStatistics[Index].CultureName );
			};

			// Compile a specific culture.
			if (CultureName.IsSet())
			{
				const int32 CultureIndex = Target->Settings.SupportedCulturesStatistics.IndexOfByPredicate([CultureName](const FCultureStatistics& Culture) { return Culture.CultureName == CultureName.GetValue(); });
				AddCultureToGenerate(CultureIndex);
			}
			// Compile all cultures.
			else
			{
				for (int32 CultureIndex = 0; CultureIndex < Target->Settings.SupportedCulturesStatistics.Num(); ++CultureIndex)
			{
					AddCultureToGenerate(CultureIndex);
				}
			}
		}

		Script.Dirty = true;

		return Script;
	}

	FString GetCompileScriptPath(const ULocalizationTarget* const Target, const TOptional<FString> CultureName)
	{
		const FString ConfigFileDirectory = GetScriptDirectory(Target);
		FString ConfigFilePath;
		if (CultureName.IsSet())
		{
			ConfigFilePath = ConfigFileDirectory / FString::Printf( TEXT("%s_Compile_%s.%s"), *Target->Settings.Name, *CultureName.GetValue(), TEXT("ini") );
		}
		else
	{
			ConfigFilePath = ConfigFileDirectory / FString::Printf( TEXT("%s_Compile.%s"), *Target->Settings.Name, TEXT("ini") );
		}
		return ConfigFilePath;
	}
}
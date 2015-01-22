// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE "FMainFrameTranslationEditorMenu"


/**
 * Static helper class for populating the "Cook Content" menu.
 */
class FMainFrameTranslationEditorMenu
{
public:

	/**
	 * Creates the main frame translation editor sub menu.
	 *
	 * @param MenuBuilder The builder for the menu that owns this menu.
	 */
	static void MakeMainFrameTranslationEditorSubMenu( FMenuBuilder& MenuBuilder )
	{
		// Directories to search for localization
		// Directory Structure should look like this:
		// ProjectRootFolder/Content/Localization/  <-- This is the directory to add to the array below
		// ProjectRootFolder/Content/Localization/LocalizationProjectName/
		// ProjectRootFolder/Content/Localization/LocalizationProjectName/LocalizationProjectName.manifest
		// ProjectRootFolder/Content/Localization/LocalizationProjectName/en/LocalizationProjectName.archive
		// ProjectRootFolder/Content/Localization/LocalizationProjectName/jp/LocalizationProjectName.archive
		// ProjectRootFolder/Content/Localization/LocalizationProjectName/ko/LocalizationProjectName.archive
		// ...

		// Add some specific directories 
		TArray<FString> DirectoriesToSearch;
		DirectoriesToSearch.Add(FPaths::EngineContentDir() / TEXT("Localization"));
		DirectoriesToSearch.Add(FPaths::GameContentDir() / TEXT("Localization"));
		DirectoriesToSearch.Add(FPaths::EngineDir() / "Programs/NoRedist/UnrealEngineLauncher/Content/Localization");

		// Also check any of these .ini specified localization paths
		TArray < TArray<FString> > LocalizationPathArrays;
		LocalizationPathArrays.Add(FPaths::GetGameLocalizationPaths());
		LocalizationPathArrays.Add(FPaths::GetEditorLocalizationPaths());
		LocalizationPathArrays.Add(FPaths::GetEngineLocalizationPaths());
		LocalizationPathArrays.Add(FPaths::GetPropertyNameLocalizationPaths());
		LocalizationPathArrays.Add(FPaths::GetToolTipLocalizationPaths());
		for (TArray<FString> LocalizationPathArray : LocalizationPathArrays)
		{
			for (FString LocalizationPath : LocalizationPathArray)
			{
				DirectoriesToSearch.AddUnique(LocalizationPath);
			}
		}

		// Find Manifest files
		TArray<FString> ManifestFileNames;
		for (FString DirectoryToSearch : DirectoriesToSearch)
		{
			// Search for .manifest files in this directory
			const FString ManifestExtension = TEXT("manifest");
			const FString TopFolderManifestFileWildcard = FString::Printf(TEXT("%s/*.%s"), *DirectoryToSearch, *ManifestExtension);

			TArray<FString> TopFolderManifestFileNames;
			IFileManager::Get().FindFiles(TopFolderManifestFileNames, *TopFolderManifestFileWildcard, true, false);

			for (FString& CurrentTopFolderManifestFileName : TopFolderManifestFileNames)
			{
				// Add root back on
				CurrentTopFolderManifestFileName = DirectoryToSearch / CurrentTopFolderManifestFileName;
				ManifestFileNames.AddUnique(CurrentTopFolderManifestFileName);
			}

			// Search for .manifest files 1 directory deep
			const FString DirectoryToSearchWildcard = FString::Printf(TEXT("%s/*"), *DirectoryToSearch);
			TArray<FString> ProjectFolders;
			IFileManager::Get().FindFiles(ProjectFolders, *DirectoryToSearchWildcard, false, true);

			for (FString ProjectFolder : ProjectFolders)
			{
				// Add root back on
				ProjectFolder = DirectoryToSearch / ProjectFolder;
				const FString ProjectFolderManifestFileWildcard = FString::Printf(TEXT("%s/*.%s"), *ProjectFolder, *ManifestExtension);

				TArray<FString> CurrentFolderManifestFileNames;
				IFileManager::Get().FindFiles(CurrentFolderManifestFileNames, *ProjectFolderManifestFileWildcard, true, false);

				for (FString& CurrentFolderManifestFileName : CurrentFolderManifestFileNames)
				{
					// Add root back on
					CurrentFolderManifestFileName = ProjectFolder / CurrentFolderManifestFileName;
					ManifestFileNames.AddUnique(CurrentFolderManifestFileName);
				}
			}
		}

		MenuBuilder.BeginSection("Project", LOCTEXT("Translation_ChooseProject", "Choose Project:"));
		{
			for (FString ManifestFileName : ManifestFileNames)
			{
				FString ManifestName = FPaths::GetBaseFilename(ManifestFileName);

				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("ManifestName"), FText::FromString(ManifestName));
				MenuBuilder.AddSubMenu(
					FText::Format(LOCTEXT("TranslationEditorSubMenuProjectLabel", "{ManifestName} Translations"), Arguments),
					FText::Format(LOCTEXT("TranslationEditorSubMenuProjectToolTip", "Edit Translations for {ManifestName} Strings"), Arguments),
					FNewMenuDelegate::CreateStatic(&FMainFrameTranslationEditorMenu::MakeMainFrameTranslationEditorSubMenuEditorProject, ManifestFileName)
				);
			}
		}

		MenuBuilder.EndSection();
	}

	/**
	 * Creates the main frame translation editor sub menu for editor translations.
	 *
	 * @param MenuBuilder The builder for the menu that owns this menu.
	 */
	static void MakeMainFrameTranslationEditorSubMenuEditorProject( FMenuBuilder& MenuBuilder, const FString ManifestFileName )
	{
		const FString ManifestFolder = FPaths::GetPath(ManifestFileName);
		const FString ManifestFolderWildcard = FString::Printf(TEXT("%s/*"), *ManifestFolder);

		TArray<FString> ArchiveFolders;
		IFileManager::Get().FindFiles(ArchiveFolders, *ManifestFolderWildcard, false, true);

		TArray<FString> ArchiveFileNames;
		for (FString ArchiveFolder : ArchiveFolders)
		{
			// Add root back on
			ArchiveFolder = ManifestFolder / ArchiveFolder;

			const FString ArchiveExtension = TEXT("archive");
			const FString ArchiveFileWildcard = FString::Printf(TEXT("%s/*.%s"), *ArchiveFolder, *ArchiveExtension);

			TArray<FString> CurrentFolderArchiveFileNames;
			IFileManager::Get().FindFiles(CurrentFolderArchiveFileNames, *ArchiveFileWildcard, true, false);

			for (FString& CurrentFolderArchiveFileName : CurrentFolderArchiveFileNames)
			{
				// Add root back on
				CurrentFolderArchiveFileName = ArchiveFolder / CurrentFolderArchiveFileName;
			}

			ArchiveFileNames.Append(CurrentFolderArchiveFileNames);
		}

		MenuBuilder.BeginSection("Language", LOCTEXT("Translation_ChooseLanguage", "Choose Language:"));
		{
			for (FString ArchiveFileName : ArchiveFileNames)
			{
				FString ArchiveName = FPaths::GetBaseFilename(FPaths::GetPath(ArchiveFileName));
				FString ManifestName = FPaths::GetBaseFilename(ManifestFileName);

				FCulturePtr Culture = FInternationalization::Get().GetCulture(ArchiveName);
				if (Culture.IsValid())
				{
					ArchiveName = Culture->GetDisplayName();
				}

				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("ArchiveName"), FText::FromString(ArchiveName));
				Arguments.Add(TEXT("ManifestName"), FText::FromString(ManifestName));

				MenuBuilder.AddMenuEntry(
					FText::Format(LOCTEXT("TranslationEditorSubMenuLanguageLabel", "{ArchiveName} Translations"), Arguments),
					FText::Format(LOCTEXT("TranslationEditorSubMenuLanguageToolTip", "Edit the {ArchiveName} Translations for the {ManifestName} Project"), Arguments),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateStatic(&FMainFrameTranslationEditorMenu::HandleMenuEntryExecute, ManifestFileName, ArchiveFileName))
					);
			}
		}

		MenuBuilder.EndSection();
	}

	private:

	// Handles clicking a menu entry.
	static void HandleMenuEntryExecute( FString ProjectName, FString Language)
	{
		FModuleManager::LoadModuleChecked<ITranslationEditor>("TranslationEditor").OpenTranslationEditor(ProjectName, Language);
	}
};


#undef LOCTEXT_NAMESPACE

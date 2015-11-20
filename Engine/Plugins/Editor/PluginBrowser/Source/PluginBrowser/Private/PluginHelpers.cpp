// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PluginBrowserPrivatePCH.h"
#include "PluginHelpers.h"
#include "GameProjectUtils.h"
#include "GenericPlatformFile.h"

#define LOCTEXT_NAMESPACE "PluginHelpers"

bool FPluginHelpers::CopyPluginTemplateFolder(const TCHAR* DestinationDirectory, const TCHAR* Source, const FString& PluginName)
{
	check(DestinationDirectory);
	check(Source);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	FString DestDir(DestinationDirectory);
	FPaths::NormalizeDirectoryName(DestDir);

	FString SourceDir(Source);
	FPaths::NormalizeDirectoryName(SourceDir);

	// Does Source dir exist?
	if (!PlatformFile.DirectoryExists(*SourceDir))
	{
		return false;
	}

	// Destination directory exists already or can be created ?
	if (!PlatformFile.DirectoryExists(*DestDir) &&
		!PlatformFile.CreateDirectory(*DestDir))
	{
		return false;
	}

	// Copy all files and directories, renaming specific sections to the plugin name
	struct FCopyPluginFilesAndDirs : public IPlatformFile::FDirectoryVisitor
	{
		IPlatformFile& PlatformFile;
		const TCHAR* SourceRoot;
		const TCHAR* DestRoot;
		const FString& PluginName;
		TArray<FString> NameReplacementFileTypes;
		TArray<FString> IgnoredFileTypes;

		FCopyPluginFilesAndDirs(IPlatformFile& InPlatformFile, const TCHAR* InSourceRoot, const TCHAR* InDestRoot, const FString& InPluginName)
			: PlatformFile(InPlatformFile)
			, SourceRoot(InSourceRoot)
			, DestRoot(InDestRoot)
			, PluginName(InPluginName)
		{
			// Which file types we want to replace instances of PLUGIN_NAME with the new Plugin Name
			NameReplacementFileTypes.Add(TEXT("cs"));
			NameReplacementFileTypes.Add(TEXT("cpp"));
			NameReplacementFileTypes.Add(TEXT("h"));
			NameReplacementFileTypes.Add(TEXT("vcxproj"));

			// Which file types do we want to ignore
			IgnoredFileTypes.Add(TEXT("opensdf"));
			IgnoredFileTypes.Add(TEXT("sdf"));
			IgnoredFileTypes.Add(TEXT("user"));
			IgnoredFileTypes.Add(TEXT("suo"));
		}

		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			FString NewName(FilenameOrDirectory);
			// change the root and rename paths/files
			NewName.RemoveFromStart(SourceRoot);
			NewName = NewName.Replace(TEXT("PLUGIN_NAME"), *PluginName, ESearchCase::CaseSensitive);
			NewName = FPaths::Combine(DestRoot, *NewName);

			if (bIsDirectory)
			{
				// create new directory structure
				if (!PlatformFile.CreateDirectoryTree(*NewName) && !PlatformFile.DirectoryExists(*NewName))
				{
					return false;
				}
			}
			else
			{
				FString NewExt = FPaths::GetExtension(FilenameOrDirectory);

				if (!IgnoredFileTypes.Contains(NewExt))
				{
					// Delete destination file if it exists
					if (PlatformFile.FileExists(*NewName))
					{
						PlatformFile.DeleteFile(*NewName);
					}

					// If file of specified extension - open the file as text and replace PLUGIN_NAME in there before saving
					if (NameReplacementFileTypes.Contains(NewExt))
					{
						FString OutFileContents;
						if (!FFileHelper::LoadFileToString(OutFileContents, FilenameOrDirectory))
						{
							return false;
						}

						OutFileContents = OutFileContents.Replace(TEXT("PLUGIN_NAME"), *PluginName, ESearchCase::CaseSensitive);

						if (!FFileHelper::SaveStringToFile(OutFileContents, *NewName))
						{
							return false;
						}
					}
					else
					{
						// Copy file from source
						if (!PlatformFile.CopyFile(*NewName, FilenameOrDirectory))
						{
							// Not all files could be copied
							return false;
						}
					}
				}
			}
			return true; // continue searching
		}
	};

	// copy plugin files and directories visitor
	FCopyPluginFilesAndDirs CopyFilesAndDirs(PlatformFile, *SourceDir, *DestDir, PluginName);

	// create all files subdirectories and files in subdirectories!
	return PlatformFile.IterateDirectoryRecursively(*SourceDir, CopyFilesAndDirs);
}

#undef LOCTEXT_NAMESPACE
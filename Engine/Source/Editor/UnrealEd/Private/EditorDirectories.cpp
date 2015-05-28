// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "EditorDirectories.h"

FEditorDirectories& FEditorDirectories::Get()
{
	static FEditorDirectories Directories;
	return Directories;
}


void FEditorDirectories::LoadLastDirectories()
{
		// Initialize "last dir" array.
	const FString DefaultDir = FPaths::GameContentDir();
	for( int32 CurDirectoryIndex = 0; CurDirectoryIndex < ARRAY_COUNT( LastDir ); ++CurDirectoryIndex )
	{
		// Default all directories to the game content folder
		LastDir[ CurDirectoryIndex ] = DefaultDir;
	}

	const FString DefaultMapDir = FPaths::GameContentDir() / TEXT("Maps");
	if( IFileManager::Get().DirectoryExists( *DefaultMapDir ) )
	{
		LastDir[ELastDirectory::LEVEL] = DefaultMapDir;
	}

	LastDir[ELastDirectory::PROJECT] = FPaths::RootDir();

	// NOTE: We append a "2" to the section name to enforce backwards compatibility.  "Directories" is deprecated.
	GConfig->GetString( TEXT("Directories2"), TEXT("UNR"),				LastDir[ELastDirectory::UNR],					GEditorPerProjectIni );
	GConfig->GetString( TEXT("Directories2"), TEXT("BRUSH"),			LastDir[ELastDirectory::BRUSH],				GEditorPerProjectIni );
	GConfig->GetString( TEXT("Directories2"), TEXT("FBX"),				LastDir[ELastDirectory::FBX],					GEditorPerProjectIni );
	GConfig->GetString( TEXT("Directories2"), TEXT("FBXAnim"),			LastDir[ELastDirectory::FBX_ANIM],			GEditorPerProjectIni );
	GConfig->GetString( TEXT("Directories2"), TEXT("GenericImport"),	LastDir[ELastDirectory::GENERIC_IMPORT],		GEditorPerProjectIni );
	GConfig->GetString( TEXT("Directories2"), TEXT("GenericExport"),	LastDir[ELastDirectory::GENERIC_EXPORT],		GEditorPerProjectIni );
	GConfig->GetString( TEXT("Directories2"), TEXT("GenericOpen"),		LastDir[ELastDirectory::GENERIC_OPEN],		GEditorPerProjectIni );
	GConfig->GetString( TEXT("Directories2"), TEXT("GenericSave"),		LastDir[ELastDirectory::GENERIC_SAVE],		GEditorPerProjectIni );
	GConfig->GetString( TEXT("Directories2"), TEXT("MeshImportExport"),	LastDir[ELastDirectory::MESH_IMPORT_EXPORT],	GEditorPerProjectIni );
	GConfig->GetString( TEXT("Directories2"), TEXT("WorldRoot"),		LastDir[ELastDirectory::WORLD_ROOT],			GEditorPerProjectIni );
	GConfig->GetString( TEXT("Directories2"), TEXT("Level"),			LastDir[ELastDirectory::LEVEL],					GEditorPerProjectIni );
	GConfig->GetString( TEXT("Directories2"), TEXT("Project"),			LastDir[ELastDirectory::PROJECT],				GEditorPerProjectIni );

}

/** Writes the current "LastDir" array back out to the config files */
void FEditorDirectories::SaveLastDirectories()
{
	// Save out default file directories
	GConfig->SetString( TEXT("Directories2"), TEXT("UNR"),				*LastDir[ELastDirectory::UNR],				GEditorPerProjectIni );
	GConfig->SetString( TEXT("Directories2"), TEXT("BRUSH"),			*LastDir[ELastDirectory::BRUSH],				GEditorPerProjectIni );
	GConfig->SetString( TEXT("Directories2"), TEXT("FBX"),				*LastDir[ELastDirectory::FBX],				GEditorPerProjectIni );
	GConfig->SetString( TEXT("Directories2"), TEXT("FBXAnim"),			*LastDir[ELastDirectory::FBX_ANIM],			GEditorPerProjectIni );
	GConfig->SetString( TEXT("Directories2"), TEXT("GenericImport"),	*LastDir[ELastDirectory::GENERIC_IMPORT],		GEditorPerProjectIni );
	GConfig->SetString( TEXT("Directories2"), TEXT("GenericExport"),	*LastDir[ELastDirectory::GENERIC_EXPORT],		GEditorPerProjectIni );
	GConfig->SetString( TEXT("Directories2"), TEXT("GenericOpen"),		*LastDir[ELastDirectory::GENERIC_OPEN],		GEditorPerProjectIni );
	GConfig->SetString( TEXT("Directories2"), TEXT("GenericSave"),		*LastDir[ELastDirectory::GENERIC_SAVE],		GEditorPerProjectIni );
	GConfig->SetString( TEXT("Directories2"), TEXT("MeshImportExport"),	*LastDir[ELastDirectory::MESH_IMPORT_EXPORT],	GEditorPerProjectIni );
	GConfig->SetString( TEXT("Directories2"), TEXT("WorldRoot"),		*LastDir[ELastDirectory::WORLD_ROOT],			GEditorPerProjectIni );
	GConfig->SetString( TEXT("Directories2"), TEXT("Level"),			*LastDir[ELastDirectory::LEVEL],				GEditorPerProjectIni );
	GConfig->SetString( TEXT("Directories2"), TEXT("Project"),			*LastDir[ELastDirectory::PROJECT],				GEditorPerProjectIni );
}

FString FEditorDirectories::GetLastDirectory( const ELastDirectory::Type InLastDir ) const
{
	if ( InLastDir >= 0 && InLastDir < ARRAY_COUNT( LastDir ) )
	{
		return LastDir[InLastDir];
	}
	return FPaths::GameContentDir();
}

void FEditorDirectories::SetLastDirectory( const ELastDirectory::Type InLastDir, const FString& InLastStr )
{
	if ( InLastDir >= 0 && InLastDir < ARRAY_COUNT( LastDir ) )
	{
		LastDir[InLastDir] = InLastStr;
	}
}

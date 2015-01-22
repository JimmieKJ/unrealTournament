// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AddContentDialogPCH.h"

#include "FeaturePackContentSourceProvider.h"
#include "FeaturePackContentSource.h"

#include "ModuleManager.h"

class FFillArrayDirectoryVisitor : public IPlatformFile::FDirectoryVisitor
{
public:
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
	{
		if (bIsDirectory)
		{
			Directories.Add(FilenameOrDirectory);
		}
		else
		{
			Files.Add(FilenameOrDirectory);
		}
		return true;
	}

	TArray<FString> Directories;
	TArray<FString> Files;
};

FFeaturePackContentSourceProvider::FFeaturePackContentSourceProvider()
{
	FeaturePackPath = FPaths::Combine(*FPaths::RootDir(), TEXT("FeaturePacks"));
	StartUpDirectoryWatcher();
	RefreshFeaturePacks();
}

const TArray<TSharedRef<IContentSource>> FFeaturePackContentSourceProvider::GetContentSources()
{
	return ContentSources;
}

void FFeaturePackContentSourceProvider::SetContentSourcesChanged(FOnContentSourcesChanged OnContentSourcesChangedIn)
{
	OnContentSourcesChanged = OnContentSourcesChangedIn;
}

void FFeaturePackContentSourceProvider::StartUpDirectoryWatcher()
{
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();
	if (DirectoryWatcher)
	{
		// If the path doesn't exist on disk, make it so the watcher will work.
		IFileManager::Get().MakeDirectory(*FeaturePackPath);
		DirectoryChangedDelegate = IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FFeaturePackContentSourceProvider::OnFeaturePackDirectoryChanged);
		DirectoryWatcher->RegisterDirectoryChangedCallback(FeaturePackPath, DirectoryChangedDelegate);
	}
}

void FFeaturePackContentSourceProvider::ShutDownDirectoryWatcher()
{
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>( TEXT( "DirectoryWatcher" ) );
	IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();
	if ( DirectoryWatcher )
	{
		DirectoryWatcher->UnregisterDirectoryChangedCallback(FeaturePackPath, DirectoryChangedDelegate);
	}
}

void FFeaturePackContentSourceProvider::OnFeaturePackDirectoryChanged( const TArray<FFileChangeData>& FileChanges )
{
	RefreshFeaturePacks();
}

void FFeaturePackContentSourceProvider::RefreshFeaturePacks()
{
	ContentSources.Empty();
	IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FFillArrayDirectoryVisitor DirectoryVisitor;
	PlatformFile.IterateDirectory( *FeaturePackPath, DirectoryVisitor );
	for ( auto FeaturePackFile : DirectoryVisitor.Files )
	{
		TUniquePtr<FFeaturePackContentSource> NewContentSource = MakeUnique<FFeaturePackContentSource>( FeaturePackFile );
		if ( NewContentSource->IsDataValid() )
		{
			ContentSources.Add( MakeShareable( NewContentSource.Release() ) );
		}
	}
	OnContentSourcesChanged.ExecuteIfBound();
}

FFeaturePackContentSourceProvider::~FFeaturePackContentSourceProvider()
{
	ShutDownDirectoryWatcher();
}
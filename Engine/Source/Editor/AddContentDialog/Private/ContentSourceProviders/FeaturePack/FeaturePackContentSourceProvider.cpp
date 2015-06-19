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
		DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(FeaturePackPath, DirectoryChangedDelegate, DirectoryChangedDelegateHandle);
	}
}

void FFeaturePackContentSourceProvider::ShutDownDirectoryWatcher()
{
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>( TEXT( "DirectoryWatcher" ) );
	IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();
	if ( DirectoryWatcher )
	{
		DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(FeaturePackPath, DirectoryChangedDelegateHandle);
	}
}

void FFeaturePackContentSourceProvider::OnFeaturePackDirectoryChanged( const TArray<FFileChangeData>& FileChanges )
{
	RefreshFeaturePacks();
}

/** Sorting function sort keys. */
struct FFeaturePackCompareSortKey
{
	FORCEINLINE bool operator()( TSharedRef<IContentSource> const& A, TSharedRef<IContentSource> const& B) const { return A->GetSortKey() < B->GetSortKey(); }
};

void FFeaturePackContentSourceProvider::RefreshFeaturePacks()
{
	ContentSources.Empty();
	IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FFillArrayDirectoryVisitor DirectoryVisitor;
	PlatformFile.IterateDirectory( *FeaturePackPath, DirectoryVisitor );
	for ( auto FeaturePackFile : DirectoryVisitor.Files )
	{
		if( FeaturePackFile.EndsWith(TEXT(".upack")) == true)
		{
			TUniquePtr<FFeaturePackContentSource> NewContentSource = MakeUnique<FFeaturePackContentSource>(FeaturePackFile);
			if (NewContentSource->IsDataValid())
			{
				ContentSources.Add(MakeShareable(NewContentSource.Release()));
			}
		}
	}
	ContentSources.Sort(FFeaturePackCompareSortKey());
	OnContentSourcesChanged.ExecuteIfBound();
}

FFeaturePackContentSourceProvider::~FFeaturePackContentSourceProvider()
{
	ShutDownDirectoryWatcher();
}
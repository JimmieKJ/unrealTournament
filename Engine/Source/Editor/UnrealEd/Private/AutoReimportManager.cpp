// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "AssetToolsModule.h"

#include "Developer/DirectoryWatcher/Public/DirectoryWatcherModule.h"
#include "Engine/CurveTable.h"
#include "Engine/DataTable.h"


#define LOCTEXT_NAMESPACE "AutoReimportManager"

DEFINE_LOG_CATEGORY_STATIC(LogAutoReimportManager, Log, All);


UAutoReimportManager::UAutoReimportManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ReimportTime = 0.0f;
}

void UAutoReimportManager::AddReimportDelegate( const FString& FileType, const FAutoReimportDelegate& Delegate )
{
	if(ReimportDelegates* Del = ExtensionToReimporter.Find(FileType))
	{
		Del->Add(Delegate);
	}
	else
	{
		ReimportDelegates Delegates;
		Delegates.Add(Delegate);
		ExtensionToReimporter.Add(FileType, Delegates);
	}
}

void UAutoReimportManager::RemoveReimportDelegate( const FString& FileType, const FAutoReimportDelegate& Delegate )
{
	if(ReimportDelegates* Del = ExtensionToReimporter.Find(FileType))
	{
		Del->Remove(Delegate);
	}
}

void UAutoReimportManager::Initialize()
{
	//Add all auto reimport delegates

	//csv types
	AddReimportDelegate("csv", FAutoReimportDelegate::CreateStatic(&UAutoReimportManager::AutoReimportCurve));
	AddReimportDelegate("csv", FAutoReimportDelegate::CreateStatic(&UAutoReimportManager::AutoReimportCurveTable));
	AddReimportDelegate("csv", FAutoReimportDelegate::CreateStatic(&UAutoReimportManager::AutoReimportDataTable));
	
	//textures
	AddReimportDelegate( "tga", FAutoReimportDelegate::CreateStatic( &UAutoReimportManager::AutoReimportTexture ) );
	AddReimportDelegate( "dds", FAutoReimportDelegate::CreateStatic( &UAutoReimportManager::AutoReimportTexture ) );
	AddReimportDelegate( "png", FAutoReimportDelegate::CreateStatic( &UAutoReimportManager::AutoReimportTexture ) );
	AddReimportDelegate( "jpg", FAutoReimportDelegate::CreateStatic( &UAutoReimportManager::AutoReimportTexture ) );
	AddReimportDelegate( "jpeg", FAutoReimportDelegate::CreateStatic( &UAutoReimportManager::AutoReimportTexture ) );
	AddReimportDelegate( "psd", FAutoReimportDelegate::CreateStatic( &UAutoReimportManager::AutoReimportTexture ) );
	AddReimportDelegate( "bmp", FAutoReimportDelegate::CreateStatic( &UAutoReimportManager::AutoReimportTexture ) );

	//Want notification when files change in specific directories
	GetMutableDefault<UEditorLoadingSavingSettings>()->OnSettingChanged().AddUObject(this, &UAutoReimportManager::HandleLoadingSavingSettingChanged);

	AutoReimportDirectories = GetDefault<UEditorLoadingSavingSettings>()->AutoReimportDirectories;
	for(auto It(AutoReimportDirectories.CreateIterator()); It; ++It)
	{
		//Fix up existing user paths
		*It = FixUserPath(*It);

		//watch it
		WatchedDirectories.Add(this, *It);
	}
	
}

FString UAutoReimportManager::FixUserPath(  FString File )
{
	FPaths::NormalizeFilename(File);
	if(File.EndsWith(TEXT("/")) == false)
	{
		File += TEXT("/");
	}
	return File;
}

void UAutoReimportManager::BeginDestroy( void ) 
{
	for(auto It(AutoReimportDirectories.CreateIterator());It;++It)
	{
		FString Directory = *It;
		WatchedDirectories.Remove(this, Directory);
	}
	Super::BeginDestroy();
}

void UAutoReimportManager::Tick( float DeltaTime ) 
{
	ReimportTime += DeltaTime;

	//we wait this amount of time after a modification to re-import, this is because some tools save the file multiple times
	const double DeltaSinceModified = 1.0;

	//last modification must have been prior to this time
	const double CanImportTime = ReimportTime - DeltaSinceModified;

	//Reimport files that have been modified
	for (auto It(ModifiedFiles.CreateIterator());It;++It)
	{
		if(It.Value() < CanImportTime)
		{
			if(ReimportDelegates* Reimporters = ExtensionToReimporter.Find(FPaths::GetExtension(It.Key())))
			{
				OnFileModified(*Reimporters, It.Key());
			}
			It.RemoveCurrent();
		}
	}
}

TStatId UAutoReimportManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAutoReimportManager, STATGROUP_Tickables);
}

bool UAutoReimportManager::ReimportFile(UObject* Obj, const FString File)
{
	UPackage* Package = Obj->GetOutermost();
	check(Package);

	//found a match, reimport!
	if(FReimportManager::Instance()->Reimport( Obj ))
	{
		return true;
	}

	return false;
}

void UAutoReimportManager::HandleLoadingSavingSettingChanged( FName PropertyName )
{
	if (PropertyName.ToString() == "AutoReimportDirectories")
	{
		for (auto It(AutoReimportDirectories.CreateIterator()); It; ++It)
		{
			WatchedDirectories.Remove(this, *It);
		}

		//Fix up slash issues with whatever user entered
		for (auto It(GetMutableDefault<UEditorLoadingSavingSettings>()->AutoReimportDirectories.CreateIterator()); It; ++It)
		{
			*It = FixUserPath(*It);
		}

		AutoReimportDirectories = GetDefault<UEditorLoadingSavingSettings>()->AutoReimportDirectories;
		for (auto It(AutoReimportDirectories.CreateIterator()); It; ++It)
		{
			WatchedDirectories.Add(this, *It);
		}
	}
}

void UAutoReimportManager::OnDirectoryChanged( const TArray<struct FFileChangeData>& FileChanges )
{
	for (int32 FileIdx = 0; FileIdx < FileChanges.Num(); ++FileIdx)
	{
	    FString File = FileChanges[FileIdx].Filename;
		FPaths::NormalizeFilename(File);
		const FString FileExtension = FPaths::GetExtension(File);

		if ((FileChanges[FileIdx].Action != FFileChangeData::FCA_Removed) && ExtensionToReimporter.Contains(FileExtension))
		{
			ModifiedFiles.Add(File,ReimportTime);//will overwrite with most recent modified time
		} 	
	}
}

bool UAutoReimportManager::OnFileModified(ReimportDelegates& Delegates,  const FString& File )
{
	for( TObjectIterator<UObject> ObjIt; ObjIt; ++ObjIt )
	{
		UObject* Object  = *ObjIt;
		for( auto It = Delegates.CreateIterator();It;++It)
		{
			FAutoReimportDelegate& Delegate = *It;
			if(Delegate.Execute(Object, File))
			{
				return true;
			}
		}
	}
	return false;
}

bool UAutoReimportManager::JustFileNameMatches(const FString& ObjectPath, const FString& FileName)
{
	return FPaths::GetCleanFilename(ObjectPath) == FPaths::GetCleanFilename(FileName);
}

bool UAutoReimportManager::IsSamePath(const FString& ObjectPath, const FString& FileName)
{
	if(ObjectPath.Len() == 0)
	{
		return false;
	}

	if(FPaths::GetBaseFilename(ObjectPath) != FPaths::GetBaseFilename(FileName))
	{
		return false;
	}

	FString NormalizedName = ObjectPath;
	FPaths::NormalizeFilename(NormalizedName);

	if(ObjectPath == FileName) 
	{
		return true;
	}	

	const bool ObjIsRelative = FPaths::IsRelative(NormalizedName);
	const bool FileIsRelative = FPaths::IsRelative(FileName);

	if(ObjIsRelative && !FileIsRelative)
	{
		return FPaths::ConvertRelativePathToFull(NormalizedName) == FileName;
	}
	else if(!ObjIsRelative && FileIsRelative)
	{
		return NormalizedName == FPaths::ConvertRelativePathToFull(FileName);
	}

	return false;

}

void UAutoReimportManager::FDirectories::Add( UAutoReimportManager* AutoReimportManager, const FString&  FileName )
{
	bool bFound = false;
	for(auto It(Paths.CreateIterator());It;++It)
	{
		FDirectory& Path = *It;	
		if(Path.Path == FileName)
		{
			Path.Count++;
			bFound = true;
			break;
		}
	}
	if(!bFound)
	{
		Paths.Add(FDirectory(FileName));
		AutoReimportManager->ActivateAutoReimport(FileName);
	}
}

void UAutoReimportManager::FDirectories::Remove( UAutoReimportManager* AutoReimportManager,  const FString& FileName )
{
	bool bFound = false;
	for(auto It(Paths.CreateIterator());It;++It)
	{
		FDirectory& Path = *It;	
		if(Path.Path == FileName)
		{
			Path.Count--;
			if(0 == Path.Count)
			{
				Paths.RemoveAt(It.GetIndex());
				AutoReimportManager->DeactivateAutoReimport(FileName);
			}
			bFound = true;
			break;
		}
	}

	check(bFound);
}

void UAutoReimportManager::ActivateAutoReimport( const FString& Directory)
{
	// Register for notifications when files change in the content folders
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::Get().LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();
	if (DirectoryWatcher)
	{
		if(Directory.IsEmpty() == false)
		{
			DirectoryWatcher->RegisterDirectoryChangedCallback(Directory, IDirectoryWatcher::FDirectoryChanged::CreateUObject(this, &UAutoReimportManager::OnDirectoryChanged));
		}
	}
}

void UAutoReimportManager::DeactivateAutoReimport( const FString& Directory )
{
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::Get().LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();
	if (DirectoryWatcher)
	{
		if(Directory.IsEmpty() == false)
		{
			DirectoryWatcher->UnregisterDirectoryChangedCallback(Directory, IDirectoryWatcher::FDirectoryChanged::CreateUObject(this, &UAutoReimportManager::OnDirectoryChanged));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Auto Reimport handlers

bool UAutoReimportManager::AutoReimportCurve( UObject* Object, const FString& FileName )
{
	if (GetDefault<UEditorLoadingSavingSettings>()->bAutoReimportCSV)
	{
		if (UCurveBase* Curve = Cast<UCurveBase>(Object))
		{
			if (JustFileNameMatches(Curve->ImportPath, FileName))
			{
				Curve->ImportPath = FReimportManager::SanitizeImportFilename(FileName, Curve);
				ReimportFile(Curve,FileName);

				return true;
			}
		}
	}

	return false;
}

bool UAutoReimportManager::AutoReimportCurveTable( UObject* Object, const FString& FileName )
{
	if (GetDefault<UEditorLoadingSavingSettings>()->bAutoReimportCSV)
	{
		if (UCurveTable* CurveTable = Cast<UCurveTable>(Object))
		{
			if (JustFileNameMatches(CurveTable->ImportPath, FileName))
			{
				CurveTable->ImportPath = FReimportManager::SanitizeImportFilename(FileName, CurveTable);
				ReimportFile(CurveTable, FileName);

				return true;
			}
		}
	}

	return false;
}

bool UAutoReimportManager::AutoReimportDataTable( UObject* Object, const FString& FileName )
{
	if (GetDefault<UEditorLoadingSavingSettings>()->bAutoReimportCSV)
	{
		if (UDataTable* DataTable = Cast<UDataTable>(Object))
		{
			if (JustFileNameMatches(DataTable->ImportPath, FileName))
			{
				DataTable->ImportPath = FReimportManager::SanitizeImportFilename(FileName, DataTable);
				ReimportFile(DataTable, FileName);

				return true;
			}
		}
	}

	return false;
}

bool UAutoReimportManager::AutoReimportTexture( UObject* Object, const FString& FileName )
{
	if (GetDefault<UEditorLoadingSavingSettings>()->bAutoReimportTextures)
	{
		if (UTexture* Texture = Cast<UTexture>(Object))
		{
			if (IsSamePath(FReimportManager::ResolveImportFilename(Texture->SourceFilePath, Texture), FileName))
			{
				// Prevent the texture from being compressed immediately, so the user can see the results
				auto SavedCompressionSetting = Texture->DeferCompression;
				Texture->DeferCompression = true;

				if (ReimportFile(Texture, FileName))
				{
					return true;
				}

				Texture->DeferCompression  = SavedCompressionSetting;
			}
		}
	}

	return false;
}


#undef LOCTEXT_NAMESPACE

